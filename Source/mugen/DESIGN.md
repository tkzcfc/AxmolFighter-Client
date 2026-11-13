# mugen 战斗模拟 ECS 重写设计（sim v2）

> 状态：草案 v1（2026-11-09），待评审。
> 适用范围：`Source/mugen/sim`（新实现）、`Source/mugen/sim_legacy`（冻结的旧实现）、`core/ecs` 小幅增补、`conf/battle` 新运行时表、`AxmolFighter-Tools/config_converter` 降级流水线。
> 阅读顺序：§1 → §3（帧调度）→ §4（数据）→ 其余按需。§11 是需要用户拍板的问题清单。
> 约定：本文位于 `Source/**` 下，按仓库规则不写参照产品名，统一称“参照工程”；参照语义见仓库根 `Reference/` 下的“结构 / 战斗逻辑 / 战斗配置 / AI 与行为树”四篇文档。代码注释不得引用本文路径。

---

## 0. 术语

| 术语 | 含义 |
|---|---|
| 旧实现 / legacy | 现在的 `sim/`。迁移开始时机械改名为 `sim_legacy/`（命名空间 `mugen::sim_legacy`）并冻结，只接受影响 golden 输出的修复 |
| 新 sim | 本文设计的 `sim/`（命名空间 `mugen::sim`），从零编写，不链接旧实现 |
| 参照工程 | Reference/ 文档描述的原 Lua 工程，战斗语义的最终来源 |
| 阶段（Phase） | 帧内的有序区段。阶段之间、阶段内的系统之间都有固定顺序 |
| 系统（System） | 一个自由函数 `void runXxx(World&)`，本身不持有状态，只读写组件/单例/事件队列 |
| 规则库（rules） | 无状态函数库（伤害公式、Buff 准入、扣血、施法开始/结束……），只写本域组件并可向队列追加事件，可被多个系统调用 |
| 请求 / 事实 | 两类事件。请求（`XxxRequest`）可能被拒绝，由唯一的所有者系统排空；事实（`XxxEvent`）描述已经发生的事，可被多个系统只读 |
| 同步点 | 把生成请求实体化（command buffer 回放）的固定位置，见 §3.2 中的 `SpawnFlushSystem` |
| 出生阶段 | 实体被实体化时所在的同步点，决定它本帧是否参与推进（规则 R3） |
| 效果摘要 | 旧实现导出的 golden 文本，用于松等价比对，格式见附录 A |

---

## 1. 目标、非目标与决策

### 1.1 目标

| 编号 | 目标 | 衡量方式 |
|---|---|---|
| G1 | 真 ECS：数据（组件、单例）与逻辑（系统函数）分离；一个文件（`sim/Schedule.cpp`）给出整帧调度 | 读 `Schedule.cpp` 即可知道一帧里谁在何时读写什么 |
| G2 | 系统之间只经组件与确定性事件队列通信；共享逻辑放在无状态规则库 | 系统 `.cpp` 之间无 include；规则库依赖方向单向（§2.4） |
| G3 | 可扩展：新 Buff = 配置里组合 op；新技能 = 时间轴数据；新怪物行为 = 规则表加一行 | 143 条在用规则全部由约 25 种 op 表达（§5.8） |
| G4 | 保持既有确定性约束：无浮点、序列化或决定遍历顺序的状态里无 unordered 容器、无 `std::function`、唯一 RNG、多实例 | `check_rules.ps1` 覆盖 `sim/`；快照往返与哈希测试 |
| G5 | 与旧实现松等价（D0-3） | 附录 A 的效果摘要比对全部通过 |
| G6 | 学习友好：每个 ECS 概念（阶段、查询、事件、命令缓冲、标签组件、单例）在代码里有一个清楚的落点 | 本文 §3、§4 与代码一一对应 |

### 1.2 非目标

- 不做教条式纯 ECS：角色状态机保留为“组件数据 + 转移系统”，状态体可以用 `switch`。
- 不做多线程/并行调度，不做 archetype 存储；沿用现有 sparse-set `ComponentPool`。
- 不追求与旧实现比特级一致（不比哈希），不比较 AI 决策。
- 不移植参照工程中未加载的系统；数据中零使用的功能不实现（例如 `skill.skillType=1` 抬起释放：2252 行全部为 0）。
- 新 sim 不再用行为树：特效动作改由时间轴驱动，`core/bt` 保留但新 sim 不依赖。
- 本次重写不改网络协议与回放输入格式（P-8）。

### 1.3 已定决策（用户确认，视为约束）

| 编号 | 决策 |
|---|---|
| D0-1 | 同仓重写。旧 `sim/` 机械改名为 `sim_legacy/` 并冻结（只接受影响 golden 输出的修复）；新代码直接使用最终名 `sim/`。`core/`、`conf/`、`avatar/` 复用；`core/ecs` 允许小幅增补（系统调度、事件队列） |
| D0-2 | 新 sim 不链接旧实现。等价性经文件形式的 golden“效果摘要”检查，导出工具由独立通道构建 |
| D0-3 | 松等价：帧数据（命中帧、取消窗口、无敌帧、顿帧）±1 帧；RNG 受控（暴击强制 0%/100%）时伤害数值相等；Buff 施加/到期 ±1 帧；位移终点在小容差内；AI 决策不比较，只要求运行合理。同帧内部顺序可自由选择，但必须成为显式文档规则（§3.3） |
| D0-4 | 运行时使用新配置格式；参照形状的 Lua 表由 `config_converter` 映射。共存期间转换器同时输出新旧两套 |
| D0-5 | 首批内容：PVP 英雄 + 一个 PVE 关卡。旧实现未移植的 Buff 规则与“攻击变身”只在新 sim 实现，规格取自参照工程 |
| D0-6 | 顺序：无头 sim + 测试先行；垂直切片 1 之后接渲染；网络/回滚最后；战斗服最后切换；然后一次提交删除 `sim_legacy`（打 tag `legacy-sim`） |
| D0-7 | 目标是清晰结构、可读、可扩展，是学习 ECS 的项目。不教条：角色状态机保留为组件数据 + 转移系统 |

### 1.4 本文提出、需要评审的设计决策

| 编号 | 决策 | 理由 | 详见 |
|---|---|---|---|
| P-1 | **同步规则链例外**：命中微管线、施法开始/结束、动作片段进入/退出、Buff 计时这些点上，直接调用规则库（Buff 触发、Buff 准入、扣血、强制打断），立即生效；其余跨阶段通信一律走队列 | 旧实现的伤害与扣费依赖“前置触发 → 立即生效 → 本次计算”的顺序（例如 BeforeHit 时加的子 Buff 影响本次伤害；PrepareSkill 打开的 TP 消耗窗口影响本次扣费；同帧多段命中逐段叠层）。全部改成队列批处理会改变伤害数值，违反 D0-3 | §4.6、§6.6 |
| P-2 | **出生阶段门控**：实体记录出生帧与出生同步点；只有“角色时间轴”生成的实体在出生帧继续参与推进，其余下一帧起行动 | 精确复刻旧实现“组内创建”的帧语义。否则 `next_effect` 链（4893 个特效配置了后继）每代累积 1 帧偏差，超出 ±1 容差 | §3.3 R3 |
| P-3 | 角色状态改为单值枚举 `RoleStateId` + 唯一转移系统；旧的状态位掩码作为派生值提供给条件与 SkillAi | 位掩码混合（Run 与 Idle 同时置位、Idle 与 Hit 同时置位）难读且转移分散；条件仍需要位语义 | §6.4 |
| P-4 | Buff 生命周期状态机（execute_type / times / interval / reset_type / 叠层）原样移植；“规则类”改为数据化 op；数值修正改为“按来源登记的修正栈”，失活时按来源整体撤销 | 生命周期已被旧实现与测试验证；op 化消除 162 个类；登记式修正消除“加了又减错”的撤销缺陷 | §5 |
| P-5 | 特效不再使用行为树，特效动作与技能动作共用一套时间轴（`TimelineSystem`） | 特效树本质是“动作片段 × n → 销毁”的顺序执行，正好是时间轴 | §6.2、§6.8 |
| P-6 | 特殊能力（`special_ability`，854 行）并入同一套“触发点 + 条件 + op”解释器 | 它与 Buff 的触发模型相同（事件、条件、概率、CD、生成特效/挂 Buff） | §5.10 |
| P-7 | 表现层（`render/`、`ui/`）只依赖 `sim/view/*`、`sim/present/PresentationEvent.h`、`sim/World.h` 外观与网络/回滚外观，不 include 组件头 | 组件布局可自由演进；UI 现在直接 include `sim/skill/Skill.h`、`sim/buff/BuffPool.h` | §8.4 |
| P-8 | `FrameInput`、`PlayerInput`、`PlayerCommand` 的反射字段与旧版逐一相同；新 sim 的 `PlayerSetup` 去掉 `m_class`，职业由 `roleId` 推出 | `NetCodec` 字节不变、协议不变、场景脚本与 `.mgrp` 输入可复用；只有世界快照版本号变化 | §8.5 |
| P-9 | 配置：仍是单一 `config.bin`、单一登记表；新运行时表放 `conf/battle/`；删除旧实现时，参照形状的战斗表结构迁入转换器作为“源表结构”，不再进入运行时 | 少一个加载器与文件；终态运行时只含新格式 | §9 |

---

## 2. 目录布局与命名约定

### 2.1 源码树

```text
Source/mugen/
├── core/                      复用。core/ecs 增补 EventQueue.h、Schedule.h（§4.1）
├── conf/                      复用。新增 conf/battle/*.h（新运行时表，§9）
├── avatar/                    复用（MotionPlayer、.motion/.box、avatar.bin）
├── render/                    渲染接入切片后只依赖 sim/view 与 PresentationEvent（P-7）
├── sim_legacy/                旧实现，冻结，命名空间 mugen::sim_legacy
└── sim/                       新实现，命名空间 mugen::sim
    ├── World.h / World.cpp    世界外观：init / tick / serialize / deserialize / clone / hash
    ├── Schedule.h / .cpp      ★唯一帧调度表 kSchedule（阶段、系统名、函数指针），§3.2 的代码形态
    ├── Components.h           ★唯一组件登记 MG_COMPONENT_LIST（上限 64，计划约 37 个，§4.2）
    ├── Singletons.h           ★唯一单例登记 MG_SINGLETON_LIST（含全部事件队列，§4.3）
    ├── Events.h               ★全部事件结构体（请求、事实，§4.4）
    ├── WorldSetup.h           与旧版同形，但 PlayerSetup 只保留 roleId（职业由 roleId 推出）
    ├── base/                  帧/毫秒换算、SpawnStage、强类型配置 id、eachActive 查询小工具
    ├── input/                 FrameInput、PlayerCommand、ControlQuadrant；ControlManagerCore（客户端按键→命令，不进 World）
    ├── intent/                PlayerControlComponent、IntentComponent 组件；PlayerIntentSystem
    ├── ai/                    AiBrain、kMonsterRules、AiFacts；AiSystem
    ├── skill/                 SkillBookComponent、SkillCastComponent、ActionTimelineComponent；SkillRequest / Cooldown / Timeline 系统；
    │                          skill::rules（beginCast / endCast / forceInterrupt / 消耗 / 取消判定）
    ├── role/                  RoleStateComponent、HitStateComponent；RoleFsm / HitReaction / Locomotion 系统；派生状态位
    ├── motion/                TransformComponent、DisplacementComponent、FollowComponent；DisplacementSystem / FollowSystem；MapRegion
    ├── anim/                  MotionStateComponent、HitboxesComponent；Animation / Hitbox 系统
    ├── combat/                ContactMemoryComponent、HitTrackerComponent、HealthComponent、PauseComponent；Collision / Combat 系统；
    │                          formula/（伤害、闪避、标准伤害纯函数）；health::rules（扣血、护盾、锁血、死亡判定）
    ├── stats/                 StatsComponent（基础值 + 修正栈）、StatusFlagsComponent；StatId 表；stats::rules
    ├── buff/                  BuffListComponent、AbilityListComponent；BuffTickSystem；buff::rules（准入、触发分派、op 解释器）；
    │                          ValueExpr、Predicate
    ├── life/                  SummonComponent、TransformLinkComponent、LifetimeComponent、InactiveComponent/DespawningComponent 标签；
    │                          DeathSystem / TransformSystem / DespawnSystem、SpawnFlushSystem；EntityFactory（原型装配）
    ├── room/                  RoomState、障碍/物品/传送门组件；RoomDirector / Obstacle / Interact 系统；PVP 规则
    ├── present/               PresentationEvent（载荷沿用旧版并少量新增）；PresentationSystem
    ├── view/                  只读查询：WorldView（可视实体）、HudView（HUD 数据）
    ├── net/                   LockstepAuthority、NetCodec、NetBattleSync（切片 6 迁入）
    ├── rollback/              RollbackSession（切片 6 迁入）
    ├── replay/                .mgrp v5、场景脚本读取
    └── debug/                 EffectSummaryWriter（附录 A）、TraceRecorder（不入快照）
```

每个领域目录内的组件写在 `XxxComponents.h`，由根 `Components.h` 统一 include 后登记。

### 2.2 构建目标与共存

| 目标 | 内容 | 使用者 |
|---|---|---|
| `mugen_core` / `conf` / `avatar` | 不变 | 全部 |
| `mugen_sim_legacy` | `sim_legacy/**` | 客户端（直到渲染接入切片）、战斗服（直到切片 7）、旧测试、摘要导出工具 |
| `mugen_sim` | `sim/**` | `mugen_sim_tests`、`mugen_sim_cli`；切片 5 起客户端；切片 7 起战斗服 |
| `mugen_legacy_cli export-summary` | 效果摘要导出（独立通道） | CI、开发者本地 |
| `mugen_sim_cli compare-summary` | 用新 sim 跑同一场景并与 golden 比对 | CI |

- 任何二进制都不同时链接两套 sim。客户端在切片 5 用编译开关 `MG_NEW_SIM` 选择；战斗服在切片 7 直接切换。
- 机械改名同时改命名空间（`mugen::sim` → `mugen::sim_legacy`）、include 路径、CMake 目标与所有使用方（`ui/`、`render/`、战斗服、测试、工具），之后旧实现不再改。
- `check_rules.ps1` 的默认路径加上 `sim_legacy`；新 `sim` 沿用同一套禁令。

### 2.3 命名约定

| 对象 | 约定 | 例 |
|---|---|---|
| 组件 | 名词 + Component；可拷贝的反射结构体；可有只读辅助成员函数；无虚函数、无拥有指针 | `TransformComponent`、`HealthComponent`、`BuffListComponent` |
| 标签组件 | 空结构体，`MG_REFLECT(T)` 支持零字段 | `InactiveComponent`、`DespawningComponent` |
| 系统 | 动词/名词 + System；文件 `XxxSystem.cpp`，入口 `void runXxx(World&)`；文件头注释四行“读 / 写 / 消费 / 产生”，与 §3.2 表一致 | `CombatSystem`、`runCombat` |
| 规则库 | `namespace mugen::sim::<领域>::rules`，函数名动词开头 | `buff::rules::admit` |
| 请求事件 | `XxxRequest` | `SpawnRequest` |
| 事实事件 | `XxxEvent`，字段是已发生的值 | `HitEvent` |
| 运行时配置 | `conf::battle::XxxDef` | `BuffDef`、`ClipDef` |
| 枚举 | `enum class`；与配置数值对应时在注释写出对应关系 | `TriggerPoint::BeforeHit = 3` |
| 成员 | 统一 `m_` 前缀（组件字段也一样，旧实现的无前缀字段不再沿用） | `m_hp` |

### 2.4 依赖规则

- `sim/` 不 include `sim_legacy/`。`core/` 不依赖 `sim/`。
- 系统之间不 include 彼此；系统只依赖组件头、`Events.h`、规则库头。
- 规则库依赖单向：`combat::rules` → `buff::rules` → `stats::rules` / `health::rules`；`skill::rules` → `buff::rules`。`buff::rules` 不依赖 combat/skill（所需上下文由 `TriggerContext` 传值）。
- `render/`、`ui/` 只 include：`sim/World.h`、`sim/view/*`、`sim/present/PresentationEvent.h`、`sim/input/*`、`sim/net/*`、`sim/rollback/*`、`sim/replay/*`。

---

## 3. 帧调度

### 3.1 阶段总览

```text
P0 FrameBegin → P1 Input → P2 Tick → P3 Intent → P4 Action → P5 Movement → P6 Animation
   → P7 Collision → P8 Combat → P9 Lifecycle → P10 Room → P11 Present → P12 FrameEnd
```

与最初草图（输入 → 意图 → 技能时间轴 → 状态机 → 移动 → 动画/采样 → 碰撞 → 命中 → 伤害/Buff → 死亡/生命周期 → 表现）的两处调整：

1. 增加 **P2 Tick**（CD、Buff 计时、资源回复、顿帧倒计时），放在意图之前。旧实现每个角色的更新都先跑这些计时再消费输入，Buff 到期、眩晕解除与按键在同一帧内的先后关系由此保持。
2. **状态机在时间轴之前**。时间轴是 Attack 状态的“状态体”：先由 FSM 决定本帧是否仍在 Attack（上一帧被打、已死亡则不再推进施法），再推进时间轴，避免被取消的施法多跑一帧。时间轴只设置“完成/切招”标志，由 FSM 在下一帧转移（与旧实现一致）。

“伤害”与“Buff 施加”不是两个独立阶段，而是 P8 中逐个接触执行的命中微管线（§6.6，P-1）。

### 3.2 调度表

本表就是 `sim/Schedule.cpp` 中 `kSchedule` 的内容。`SpawnFlushSystem` 是同步点，执行同一个 `runSpawnFlush`，并给新实体标记当前出生阶段（R3）。

| # | 阶段 | 系统 | 读 | 写 | 消费 | 产生 |
|---|---|---|---|---|---|---|
| 0.1 | P0 FrameBegin | ClockSystem | `FrameInput` 系统命令（暂停、慢放）、`SlowMotion` | `WorldClock`、`SlowMotion`、`RoomState.m_paused`；清空 `PresentationEvents` | — | — |
| 0.2 | P0 | ScheduledActionSystem | `ScheduledActions`、`WorldClock` | `PauseComponent`（延迟顿帧）、`RoomState`（波次到时） | — | — |
| 1.1 | P1 Input | InputSystem | `FrameInput`、`PlayerSlots` | `PlayerControlComponent.m_commands`；木偶的 `TransformComponent`/`MotionStateComponent`/`RoleStateComponent` | — | `RoomRequest` |
| 2.1 | P2 Tick | CooldownSystem | `SkillBookComponent`、`StatsComponent`(CD 缩放) | `SkillBookComponent`（CD、次数、SkillAi 计时、技能组/变化计时） | — | `CastEvent`(ColdEnd)；同步触发 SkillColdEnd |
| 2.2 | P2 | BuffTickSystem | `BuffListComponent`、`AbilityListComponent`、`BuffDef` | 两个列表的计时；经 op 写 `StatsComponent`/`HealthComponent`/`StatusFlagsComponent`/`SkillBookComponent` | — | `BuffEvent`、`HpEvent`、`DeathRequest`、`SpawnRequest`、`CueEvent` |
| 2.3 | P2 | VitalsTickSystem | `HealthComponent`、`StatsComponent`、`SkillBookComponent`(曝气)、`RageStateComponent`、`SummonComponent`、`HitStateComponent`、`ComboMeterComponent` | MP 回复、曝气 EP 消耗、伙伴怒气冻结递减（Idle/移动态才回退变身）、狂暴计时、受击保护计时、连击超时与衰减、召唤寿命、`MoveClock`、生命/能量采样 | — | `DeathRequest`(召唤到期)、`CastEvent`(EpZero/AfterCrazy) |
| 2.4 | P2 | PauseTickSystem | `PauseComponent` | `PauseComponent`（顿帧/定身递减并计算 `m_paused`） | — | — |
| 3.1 | P3 Intent | AiSystem | `AiBrain`、`TransformComponent`、`HealthComponent`、`RoleStateComponent`、`SkillBookComponent`、`TauntTableComponent`、`HitStateComponent`(受击记录) | `IntentComponent`、`AiBrain`；RNG | — | — |
| 3.2 | P3 | PlayerIntentSystem | `PlayerControlComponent`、`StatusFlagsComponent`(canControl)、`PauseComponent`、`RoleStateComponent` | `IntentComponent`；未暂停时排空 `PlayerControlComponent.m_commands`；跑动意图粘滞，Attack 中只清移动模式、保留象限供 FreeMove 片段读取 | — | — |
| 4.1 | P4 Action | SkillRequestSystem | `IntentComponent`、`SkillBookComponent`、`SkillCastComponent`、`StatsComponent`、`HealthComponent`、`StatusFlagsComponent`、`RoleStateComponent` | `SkillCastComponent`（当前施法/缓存下一招/nextRun）、`SkillBookComponent` | — | `CastEvent`(Prepared)；同步触发 BeforePrepareSkill |
| 4.2 | P4 | RoleFsmSystem | `RoleStateComponent`、`SkillCastComponent`、`IntentComponent.m_move`、`HitStateComponent`、`HealthComponent`、`DisplacementComponent` 事件位、`MotionStateComponent` 完成位 | `RoleStateComponent`；进入/退出动作写 `MotionStateComponent`、`DisplacementComponent`、`SkillCastComponent`、`ActionTimelineComponent`（经 `skill::rules::beginCast/endCast`） | — | `StateEvent`、`CastEvent`(Began/Ended)；同步触发施法、连招、行为状态、ColdStart、UseTp/UseEp |
| 4.3a | P4 | TimelineSystem（角色遍，仅 `RoleStateComponent==Attack` 时推进） | `ActionTimelineComponent`、`SkillCastComponent`、`MotionStateComponent` 完成位、`StatsComponent`(动作速率) | `ActionTimelineComponent`、`SkillCastComponent`（取消窗口、切招）、`TransformComponent`（瞬移/转向）、`DisplacementComponent`（片段位移） | — | `SpawnRequest`、`TransformRequest`、`CueEvent`、`ActionEvent`；同步：片段 Buff、BeforeRoleSkillAction/After |
| 4.3s | P4 | SpawnFlushSystem | `SpawnRequest` | 新实体（出生阶段 RoleTimeline：本帧参与 4.3b、P5、P6） | `SpawnRequest` | `SpawnEvent` |
| 4.3b | P4 | TimelineSystem（特效遍） | 同上（特效实体）；`DisplacementComponent` 事件位、`IntentComponent`(片段控制) | 同上；特效片段放完时经 `life::rules::despawn` 打 `DespawningComponent`；autoRelease 2/3 命中数达标在特效遍末尾回收 | — | `SpawnRequest`（子特效、召唤）、`DespawnRequest`、`CueEvent` |
| 4.3t | P4 | SpawnFlushSystem | | 新实体（出生阶段 EffectTimeline：下一帧起行动） | `SpawnRequest` | `SpawnEvent` |
| 4.4 | P4 | HitReactionSystem | `RoleStateComponent`（受击族、死亡）、`HitStateComponent`、`DisplacementComponent` 事件位 | `HitStateComponent`（硬直/倒地计时）、`DisplacementComponent`（落地反弹策略）、挣脱请求 | — | — |
| 4.5 | P4 | LocomotionSystem | `RoleStateComponent`（移动/待机/被推/木偶）、`IntentComponent.m_move`、`StatsComponent`(移速)、`StatusFlagsComponent`(canMove) | `DisplacementComponent`（控制速度）、`TransformComponent`（朝向） | — | — |
| 5.1 | P5 Movement | DisplacementSystem | `DisplacementComponent`、`TransformComponent`、`StatsComponent`、`StatusFlagsComponent`、`MapRegion` | `TransformComponent`、`DisplacementComponent`（积分、事件位、内建策略反应） | — | — |
| 5.2 | P5 | FollowSystem | `FollowComponent`、主人/目标的 `TransformComponent` | `TransformComponent` | — | — |
| 6.1 | P6 Animation | AnimationSystem | `MotionStateComponent`、`StatsComponent`(动作速率)、`PauseComponent` | `MotionStateComponent`（时间、循环、完成位） | — | — |
| 6.2 | P6 | HitboxSystem | `MotionStateComponent`、`TransformComponent`、avatar 资源（只读进程缓存） | `HitboxesComponent` | — | — |
| 7.1 | P7 Collision | CollisionSystem | `HitboxesComponent`、`TransformComponent`、`IdentityComponent`(阵营)、`ContactMemoryComponent` | `ContactMemoryComponent` | — | `ContactEvent` |
| 8.1 | P8 Combat | CombatSystem（命中微管线，§6.6） | `ContactEvent`、`EffectInfoComponent`、`HitTrackerComponent`、`HitDef`、`StatsComponent`、`BuffListComponent`、`StatusFlagsComponent`、`RoleStateComponent`、`SkillCastComponent` | `HitTrackerComponent`、`HealthComponent`、`BuffListComponent`(同步准入)、`PauseComponent`、`HitStateComponent`(受击请求与记录)、`SkillCastComponent`(强制打断)、`ComboMeterComponent`、`RageStateComponent` | `ContactEvent`(特效→角色/水晶) | `HitEvent`、`HpEvent`、`DeathRequest`、`SpawnRequest`(受击特效)、`DespawnRequest`、`CueEvent`；RNG |
| 8.2 | P8 | ObstacleSystem | `ContactEvent`(特效→障碍、角色↔障碍) | `ObstacleState`、`TransformComponent`(推回) | `ContactEvent` | `DespawnRequest`、`SpawnRequest`(掉落) |
| 8.3 | P8 | InteractSystem | `ContactEvent`(物品、传送门) | `GoodsState`、`PortalState`、`BuffListComponent`(拾取) | `ContactEvent` | `RoomRequest`(传送) |
| 8.s | P8 | SpawnFlushSystem | | 新实体（出生阶段 Combat） | `SpawnRequest` | `SpawnEvent` |
| 9.1 | P9 Lifecycle | DeathSystem | `DeathRequest`、`HealthComponent`、`IdentityComponent`、`TransformLinkComponent`、`SummonComponent` | `RoleStateComponent`(死亡请求位)、`ComboMeterComponent`(击杀) | `DeathRequest` | `DeathEvent`、`SpawnRequest`(ko/死亡特效、掉落) |
| 9.2 | P9 | TransformSystem | `TransformRequest`、`IdentityComponent`、`TransformComponent`、`HealthComponent`、`PlayerSlots` | 新形态实体、旧形态加 `InactiveComponent`、`TransformLinkComponent`（互链） | `TransformRequest` | `SpawnEvent`、`CueEvent`(TransformSwap) |
| 9.3 | P9 | DespawnSystem | `DespawningComponent`、`LifetimeComponent`、`EffectInfoComponent`、`TransformLinkComponent`、`DespawnRequest` | 主人消失时的随主特效、Buff 清理、`ScheduledActions` 取消、变身组任一形态回收时其余一并回收 | `DespawnRequest` | `SpawnRequest`(后继特效；`m_next` 且非撞墙 EndClip)、`DespawnEvent` |
| 9.s | P9 | SpawnFlushSystem | | 新实体（出生阶段 Lifecycle） | `SpawnRequest` | `SpawnEvent` |
| 10.1 | P10 Room | RoomDirectorSystem（含 PVP 规则） | `DeathEvent`、`RoomRequest`、`RoomState`、`IdentityComponent`、`HealthComponent` | `RoomState`、`SlowMotion`(KO 慢放)、复活 | `RoomRequest` | `RoomEvent`、`SpawnRequest`(波次) |
| 10.s | P10 | SpawnFlushSystem | | 新实体（出生阶段 Room） | `SpawnRequest` | `SpawnEvent` |
| 11.1 | P11 Present | PresentationSystem | 全部事实队列 | `PresentationEvents` | 事实（只读） | `PresentationEvent` |
| 11.2 | P11 | DebugSinkSystem（可选，挂外部输出，不入快照） | 全部事实队列 | 外部 `TraceSink` / 效果摘要 | — | — |
| 12.1 | P12 FrameEnd | RoomTransferSystem | `RoomState` 换房请求 | 整个世界（保留玩家与跨房角色） | — | — |
| 12.2 | P12 | FlushSystem | — | `flushDestroyed()`；清空全部请求/事实队列（Debug 断言请求队列已排空） | — | — |

“同步触发”指在该系统内直接调用 `buff::rules::fire(...)`（P-1）。

S4 状态：表中 AiSystem（3.1）、ObstacleSystem（8.2）、InteractSystem（8.3）、RoomDirectorSystem（10.1）、RoomTransferSystem（12.1）已按本表在 S4 落地（以代码为准）；其余系统在 S0–S3 落地。P2 Tick 的 Cooldown/BuffTick/VitalsTick/PauseTick、P4 的 SkillRequest/RoleFsm/Timeline/HitReaction/Locomotion、P8 的 Combat/Obstacle/Interact、P9 的 Death/Transform/Despawn 全部已实现。

### 3.3 同帧规则

| 编号 | 规则 | 来源 / 理由 |
|---|---|---|
| R1 | 阶段与系统顺序是全序。系统内按实体创建顺序遍历。`TimelineSystem` 分角色遍与特效遍，复刻旧实现“角色组先于特效组” | 旧实现 `World::tick` 组顺序 |
| R2 | 实体只在同步点创建，组件增删只在遍历之外进行（先收集再应用）。生成请求在业务发生处产生（时间轴结束处、命中处），同步点按入队顺序实体化，所以“创建顺序 = 入队顺序” | 避免遍历中组件池扩容使引用失效；创建顺序决定后续所有遍历顺序 |
| R3 | **出生门控**：`IdentityComponent.m_born = {frame, stage}`。出生帧内，`stage > RoleTimeline` 的实体不参与任何逐实体推进（时间轴、位移、跟随、动画、受击框）；所有出生帧内的实体都不参与本帧碰撞。效果：角色时间轴生成的特效本帧即跑特效遍、位移、动画；特效生成的子特效、受击特效、掉落、召唤物、波次怪下一帧起行动 | 复刻旧实现（组内创建、注册帧不碰撞），P-2 |
| R4 | **销毁即时可见**：请求销毁时立即加 `DespawningComponent` 标签；本帧之后的所有系统（碰撞、命中、触发分派、AI 选目标、跟随）都跳过它；物理回收在 12.2。遍历中打 `DespawningComponent` 标签是 R2 唯一允许的例外；同步规则链里不得增删其他组件（需要的组件由 EntityFactory 在生成时挂好） | 旧实现 `destroyRequested` 语义 |
| R5 | **命中顺序与互殴**：接触按（攻击实体创建序，目标创建序）排序，逐个走完整微管线。先处理的命中若打断对方施法，对方本技能 `autoRelease=0` 的特效立即 `DespawningComponent`，本帧不再命中（先手胜）；其余情况双方都命中（相杀），各自顿帧。攻击者本帧死亡不取消其在途特效 | 旧实现碰撞遍历顺序与 `clearSkillEffectsByInterrupt` |
| R6 | **伤害与死亡判定**：扣血即时生效；hp≤0 时同步触发 BeforeDeath（免死、锁血可改变结果）；仍≤0 则立即 `HealthComponent.m_dead = true`，本帧后续指向它的接触在门控处被拒；死亡状态由 DeathSystem 提请、下一帧 FSM 执行（受击族在自然出口转入，§6.5） | 参照工程 `modifyHp` 与“目标已死则返回” |
| R7 | **Buff 同帧增删**：准入同步生效；一次触发分派开始时固定实例序号列表，分派中新增的实例不接收本次触发；同帧先加后删 → Enter 与 Exit 都执行，两条 `BuffEvent` 都发；先删后加同 id → 新实例而非叠层；持续时间到期在 P2，先于本帧命中（到期的 Buff 不影响本帧伤害） | 旧实现 `dispatchEvent` 与更新顺序 |
| R8 | 顿帧/定身期间哪些计时继续，见 §3.4 | 参照工程冻结短路位置 |
| R9 | **暂停期间命令保留**：`PlayerControlComponent.m_commands` 不消费，解冻首帧按原顺序消费 | 参照工程控制事件队列在冻结检查之后消费 |
| R10 | **动画完成延迟一帧**：AnimationSystem 写完成位，时间轴与 FSM 下一帧读取 | 旧实现同序 |
| R11 | **位移事件延迟一帧**：P5 写事件位（brake/air/floor/lie/wall），状态体与 FSM 下一帧读；位移策略的内建反应（反弹、停止、贴墙、穿越、结束片段标志）在 P5 当帧执行 | 用数据策略替代旧实现的同步回调 |
| R12 | 跟随在全部位移之后（FollowSystem 读主人最终位置） | 旧实现特效组在角色组之后 |
| R13 | **顿帧写入**：攻击者与特效的顿帧“覆盖写”（后处理的命中覆盖先处理的）；受击者顿帧经 `ScheduledActions` 延迟 `freeze_time_delay` 后写入，新命中取消旧的待生效顿帧；顿帧从下一帧 P2 开始递减 | 旧实现 `_hitReaction` |
| R14 | 状态请求优先级：Death > Revive > 受击（含追击）> 挣脱 > Attack > Jostled > 移动 > Idle | 参照工程 ROLE 树分支顺序 |
| R15 | **RNG**：唯一的 `RngState`，只经 `rng::draw(world, DrawKind)` 访问。每次命中抽样顺序固定为：属性闪避 → （触发 op 中的概率抽样）→ 暴击 → 浮动。Buff 准入概率在准入时抽样；AI 在 P3 抽样。golden 运行用 `DrawPolicy` 固定各类抽样（附录 A） | 参照工程“闪避 → 暴击 → 浮动” |
| R16 | **触发重入**：同步触发链最大深度 8；同一（实例，触发点）在一条链内只执行一次；超深度记 `MG_LOG_E` 并截断 | 防止 AddBuff ↔ 触发循环 |
| R17 | **表现事件序号**：PresentationSystem 按事实入队顺序翻译，赋帧内 `ordinal`（每帧从 0）；回滚去重沿用（帧，实体，ordinal，载荷） | 旧实现去重规则 |
| R18 | **特效顿帧统一**：特效与角色同规则（P2 递减、当帧判定）。偏差：旧实现特效晚一帧进入冻结，在 ±1 帧容差内 | 简化 |

### 3.4 顿帧 / 定身期间的计时

“暂停”指 `PauseComponent.m_paused`（顿帧 `freeze` 或定身 `static` 任一大于 0）。

| 计时或行为 | 暂停期间 | 说明 |
|---|---|---|
| 技能 CD、次数恢复、SkillAi load/check | 继续 | 参照工程在冻结检查之前更新 |
| Buff interval / times / 持续、Buff 添加 CD、特殊能力 CD 与周期 | 继续 | 同上 |
| MP 回复、曝气 EP 消耗、怒气冻结、狂暴计时、受击保护计时、连击超时 | 继续 | 同上 |
| 召唤物寿命 | 继续 | 同上 |
| 顿帧、定身倒计时本身 | 继续 | |
| 输入命令消费 | **暂停**，命令保留（R9） | |
| AI 决策 | **暂停**（新规则；旧实现照常决策，但意图要等解冻才生效，无可观察差异） | AI 不在比对范围 |
| 时间轴片段时间、cue、取消窗口 | 暂停 | |
| FSM 转移（写 `m_state`） | **仍发生** | 先设顿帧再同帧进受击/死亡态（对齐参考语义：`_hitReaction` 先冻结后受击） |
| 受击硬直/倒地计时、enter 动作（动画/位移） | 暂停 | 状态已转移但 enter 被推迟：`HitStateComponent::m_enterPending` 标记，解冻后补做 |
| 位移积分、跟随 | 暂停 | |
| 动画时间、受击框采样 | 暂停；受击框保留上一次采样，**仍参与碰撞** | 旧实现同 |
| 特效命中间隔 `hit_interval` | 特效暂停时暂停 | |
| 全局慢放 | 所有计时都使用缩放后的 dt | `WorldClock.m_dtMs` |

---

## 4. 数据：组件、单例、事件

### 4.1 `core/ecs` 增补

| 文件 | 内容 |
|---|---|
| `EventQueue.h` | `template <class T> struct EventQueue { std::vector<T> m_items; push/clear/size/empty; MG_REFLECT(EventQueue, m_items) }`。作为单例登记，进入快照。若 `MG_REFLECT` 在类模板内不可用，改为每种事件一个具名队列结构体 |
| `Schedule.h` | `template <class W> struct SystemEntry { const char* m_name; uint8_t m_phase; void (*m_run)(W&); }` 与 `runSchedule(W&, std::span<const SystemEntry<W>>)`；可选的逐系统观察钩子（函数指针，默认空，用于耗时统计/追踪） |
| 不变 | 组件上限 64（签名是 `std::bitset<64>`），新 sim 计划约 37 个组件 |
| `System.h` | 新 sim 不用（虚接口与“无虚函数”约定冲突）；随旧实现一起删除（目前只有一个旧测试 include） |

### 4.2 组件

| # | 组件 | 字段要点 | 写入方（所有者） | 主要读取方 |
|---|---|---|---|---|
| 1 | `IdentityComponent` | `m_kind`(Role/Effect/Obstacle/Goods/Portal)、`m_configId`、`m_camp`、`m_hostileMask`、`m_roleType`、`m_level`、`m_aiDifficulty`(AI 难度，1-based，玩家/无 AI 为 1)、`m_playerSlot`(可选)、`m_owner`、`m_rootOwner`、`m_born{frame, stage}`、`m_ordinal`（同配置第几个，用于语义键） | EntityFactory（只在生成时写） | 全部 |
| 2 | `InactiveComponent` | 标签：变身后休眠、跨房缓冲、房间挂起 | TransformSystem / Room 系统 | 全部（跳过） |
| 3 | `DespawningComponent` | 标签：本帧待回收（R4） | `life::rules::despawn` | 全部（跳过） |
| 4 | `Puppet` | `m_playerId` | InputSystem | Locomotion、view |
| 5 | `PlayerControlComponent` | `m_slot`、`m_commands`（未消费命令，保序） | Input / PlayerIntent | — |
| 6 | `IntentComponent` | `m_move{mode: None/Stop/Walk/Run/MoveTo, quadrant, angle, targetPos, moveKind: Patrol/Chase/Alert/Wander/Finding}`、`m_buttons[]{event, pressType, angle}`、`m_autoCast`、`m_target` | PlayerIntent / Ai（每帧重写） | SkillRequest、RoleFsm、Locomotion |
| 7 | `AiBrainComponent`（S4 已实现） | 警戒、上次动作、规则冷却[8]、技能间隔[]、仇恨计时、目标、正在执行的移动规则 | AiSystem | AiSystem |
| 8 | `TransformComponent` | `m_position`、`m_towardX`、`m_vectorX`、`m_vectorZ`、`m_scale` | DisplacementSystem、FollowSystem；FSM/时间轴（瞬移、转向）；Combat（受击转向） | 全部 |
| 9 | `DisplacementComponent` | 曲线 id、各轴已用时/速度/加速度、重力、反弹、策略{wall, floor, endOnBrake}、事件位{brake, air, floor, lie, wall}、追踪目标、控制速度、`m_noFloorBounce`(浮空耗尽落地不反弹) | DisplacementSystem（积分）；FSM/时间轴/Locomotion（启动、设置控制速度） | 状态体、FSM |
| 10 | `FollowComponent` | `m_target`、`m_mode`(0/1/2)、`m_offset` | EntityFactory | FollowSystem |
| 11 | `MotionStateComponent` | 动作文件、名称、entry、`m_timeMs`、`m_rate`、`m_loop`、`m_completedLoops`、`m_completedThisFrame` | Animation（推进）；FSM/时间轴（播放新动作） | 时间轴、FSM、Hitbox、view |
| 12 | `HitboxesComponent` | 攻击框[]、受击框[]、`m_radius`、`m_sampledFrame` | HitboxSystem | Collision |
| 13 | `ContactMemoryComponent` | 上一帧接触 `{other, point}[]` | Collision | Collision |
| 14 | `StatsComponent` | `m_base[StatId]`、`m_mods[]{stat, value, source}`、`m_final[]`（缓存）、`m_dirty` | `stats::rules`（Buff op 登记修正）、EntityFactory（基础值） | Combat、Movement、Skill、Vitals |
| 15 | `HealthComponent` | `m_hp`、`m_mp`、`m_ep`、`m_maxMp`、`m_maxEp`、`m_shield{amount, layers}`、`m_dead`、`m_beHitEnabled`、采样 `m_prevHp`/`m_prevEp` | `health::rules`（Combat、Buff、Vitals、Room 调用） | 多数 |
| 16 | `StatusFlagsComponent` | 引用计数：SuperArmor、Invincible、Stun、Durance、Palsy、Frozen、Silence、Stealth、NoHeal、NoCrystalCost；派生 `canMove()`、`canControl()` | `buff::rules` | 多数 |
| 17 | `PauseComponent` | `m_freezeMs`、`m_staticMs`、`m_paused`、`m_staticResetAtMs` | Combat/时间轴写入，PauseTick 递减 | 推进类系统 |
| 18 | `HitStateComponent` | 浮空值/重量/疲劳/受击计数、硬直 ms、待处理受击请求 `{hitDefId, towardX, kind}`、受击保护、最近攻击者与伤害记录（供 AI 仇恨）、接触点、`m_hitType`(0 击退/1 击倒/2 击飞/−1 无受击)、`m_enterPending`(顿帧推迟 enter) | Combat（请求与记录）、HitReaction（计时）、FSM（消费请求） | FSM、AI、条件 |
| 19 | `ComboMeterComponent` | 连击数、评价等级/经验、超时、已结算击杀目标 | Combat（命中/击杀）、Vitals（超时衰减） | Buff 条件、view |
| 20 | `TauntTableComponent` | `{applicator, weight}[]`（首次出现顺序） | `buff::rules` | AI |
| 21 | `RageStateComponent` | `m_rage`、`m_freezeMs`/`m_maxFreezeMs`（变身冻结）、`m_transformed`（变身态，伙伴 107） | Combat（命中 grant）、Skill（施法收束进变身）、Vitals（冻结递减、Idle/移动回退） | Skill、view |
| 22 | `SummonComponent` | `m_owner`（直接主人）、`m_root`（最终主人，主人也是召唤时取其根）、`m_remainingMs` | EntityFactory、Vitals | 伤害（标准伤害取根）、Death |
| 23 | `TransformLinkComponent` | `m_members[]`（全部形态实体）、`m_index` | TransformSystem | Death、Despawn |
| 24 | `RoleStateComponent` | `m_state`(RoleStateId)、`m_prev`、`m_stateMs`、`m_data`（小型 variant）、`m_deathRequested`、`m_reviveRequested` | RoleFsm（唯一转移者）；Death/Room 只写请求位 | 多数 |
| 25 | `SkillBookComponent` | 技能运行数据[]{configId, 地址, cd, 次数, 缩放, SkillAi 状态, 禁用计数}、槽位表、技能组/变化、曝气、晶体、目标、全局修正登记 | SkillRequest、Cooldown、`skill::rules`；`buff::rules`（技能修正） | Skill、AI、view |
| 26 | `SkillCastComponent` | 当前施法{技能下标, 方向列, 管线}、缓存下一招、允许打断/至尊打断、`m_switch`（取消窗切招到下一帧）、`m_nextRun`、`m_interrupted`、`m_crazy`(曝气)、伙伴怒气待领取 `m_partnerRagePendingGain(+Active)` 与回退位 `m_partnerRageRecoverPending` | `skill::rules`（SkillRequest、FSM、时间轴、Combat 强制打断） | 时间轴、FSM、条件 |
| 27 | `ActionTimelineComponent` | 片段 id 列表、当前下标、片段时间 ms、尾延时 ms、下一个 cue 下标、片段 Buff 实例序号[]、召唤/定身/变身已触发位、定身复位时间 | TimelineSystem | FSM（完成位） |
| 28 | `EffectInfoComponent` | `m_configId`、`m_owner`、来源上下文{skillId, baseSkillId, slot, artifactId, artifactIndex}、`m_capabilityTrigger` | EntityFactory | Combat、Despawn |
| 29 | `HitTrackerComponent` | 本帧命中数、命中间隔剩余、已命中目标[] | Combat | Combat |
| 30 | `LifetimeComponent` | autoRelease 策略、是否随主人销毁、是否允许后继 | EntityFactory、Combat | Despawn |
| 31 | `BuffListComponent` | 实例[]{serial, id, initialId, lifecycle, context, latched, 规则小状态}、免疫登记、添加 CD、`m_nextSerial` | `buff::rules`、BuffTick | Combat（条件）、view |
| 32 | `AbilityListComponent` | 能力实例[]（CD、周期、结束倒计时、上下文） | `buff::rules`、BuffTick | — |
| 33 | `ObstacleStateComponent`（S4 已实现） | 剩余受击次数、动作计时 | ObstacleSystem | view |
| 34 | `GoodsStateComponent`（S4 已实现） | 拾取状态、抛射 | InteractSystem | view |
| 35 | `PortalStateComponent`（S4 已实现） | Close/Active/Transfer 三阶段 | Room、Interact | view |
| 36 | `DropTableComponent`（S4 已实现） | 死亡掉落配置 | EntityFactory | Death |
| 37 | `MoveClock` | 连续移动/站立秒数（条件 171/172 使用） | Vitals | 条件 |

### 4.3 单例

| 单例 | 内容 | 所有者 |
|---|---|---|
| `WorldClock` | `m_frame`、`m_elapsedMs`、`m_dtMs`、`m_dtSec` | Clock |
| `RngState` | 唯一的 `Random` | 只经 `rng::draw` |
| `SlowMotion` | 外部定长慢放 + KO 曲线 | Clock、Room |
| `PlayerSlots` | 槽位 → 实体 | Spawn、Room |
| `MapRegion` | 地图可行走区域 | Room（换房时重载） |
| `RoomState` | 房间流程（沿用旧版字段） | Room |
| `ScheduledActions` | 有序列表 `{dueMs, seq, owner, action}`；`action` 为 variant（延迟顿帧、波次到时等）；取代旧 `LogicTimers` | 各系统登记，ScheduledActionSystem 执行 |
| `SpawnRegistry` | 下一个生成序号、各配置的 ordinal 计数（语义键） | SpawnFlushSystem |
| 事件队列（§4.4） | 每种事件一个 `EventQueue<T>` | 见 §4.4 |
| `PresentationEvents` | `m_events`、`m_nextSeq` | Present |

`DrawPolicy`（测试用的 RNG 覆盖）和外部追踪输出不是单例，作为 `World` 的非序列化成员（与旧实现 `WorldHooks` 同类）。`World::clone()` 只复制 ECS 状态（组件与单例），不复制非序列化成员（抽样策略、追踪行、本帧输入指针）；克隆体需要时由调用方重新挂接。追踪行由 DebugSinkSystem 在帧初清空，玩法系统不碰。

### 4.4 事件类型

| 事件 | 类别 | 字段 | 生产者 | 消费者 |
|---|---|---|---|---|
| `SpawnRequest` | 请求 | kind、configId、owner、来源上下文、生成时已解析的位置/朝向/向量、寿命 ms、继承掩码 | 时间轴、Combat、Death、Despawn、Room、op `Apply`(不生成实体) 之外的生成点 | SpawnFlushSystem |
| `DespawnRequest` | 请求 | entity、reason（片段结束/命中次数/主人消失/打断/死亡动作结束） | 多处（同时立即打 `DespawningComponent`） | DespawnSystem |
| `TransformRequest` | 请求 | entity、目标角色 id、type、actionId | 时间轴 | TransformSystem |
| `DeathRequest` | 请求 | entity、killer、effect | `health::rules` | DeathSystem |
| `RoomRequest` | 请求 | kind（复活/回城/警告确认/传送）、参数 | Input、Interact | RoomDirector |
| `SpawnEvent` | 事实 | entity、kind、configId、owner、语义键 | SpawnFlushSystem、TransformSystem | Present、Debug |
| `ContactEvent` | 事实 | a、b、phase(Began/Continue/End)、point | Collision | Combat、Obstacle、Interact |
| `HitEvent` | 事实 | attacker、effect、target、hitDefId、result(Hit/Evade/Miss/NoDamage)、damage、critical、killed、contact | Combat | Present、Debug |
| `HpEvent` | 事实 | entity、delta、cause(Hit/Dot/Heal/Cost/Shield)、source | `health::rules` | Present（DOT 飘字）、Debug |
| `DeathEvent` | 事实 | entity、killer | DeathSystem | Room、Present、Debug |
| `CastEvent` | 事实 | entity、skillId、slot、phase(Prepared/Began/Ended/Cancelled/ColdStart/ColdEnd/EpZero/AfterCrazy) | Skill、Cooldown、Vitals | Present、Debug |
| `ActionEvent` | 事实 | entity、actionId、phase(Begin/End) | 时间轴 | Debug |
| `StateEvent` | 事实 | entity、from、to | RoleFsm | Present（受击亮度等）、Debug |
| `BuffEvent` | 事实 | entity、buffId、serial、phase(Added/Stacked/Removed) | `buff::rules` | Present（BuffSpine、图标）、Debug |
| `CueEvent` | 事实 | entity、kind（音效/震屏/残影/提示/对白/全屏演出/白框/隐身视图/霸体描边…）、ids | 时间轴、`buff::rules`、Combat | Present |
| `RoomEvent` | 事实 | kind（状态变化/胜利/失败/警告/PVP 结束）、载荷 | Room | Present |
| `DespawnEvent` | 事实 | entity、reason | DespawnSystem | Present、Debug |
| `DisplacementEvent` | 事实 | entity、phase(Began/Ended/Replaced)、curveId | `displacement::rules`、DisplacementSystem | Present、Debug（终点坐标由 DebugSinkSystem 在 P11 读 `TransformComponent`） |
| `HitStopEvent` | 事实 | entity、freezeMs | `combat::rules`（命中微管线）、ScheduledAction（延迟顿帧到时） | Present、Debug（毫秒→帧数在 DebugSinkSystem 换算） |

事件只能是**玩法事实**或**请求**：凡是登记为单例队列的事件，都必须是表现层或玩法也可用的事实，不得为 golden 摘要专门造事件。摘要需要的派生信息（帧数换算、终点坐标、动作名差分）一律在 DebugSinkSystem / 导出器里从事实与组件推出，不写回任何玩法组件或 `World` 的序列化状态。

### 4.5 队列语义

| 方面 | 规则 |
|---|---|
| 顺序 | 入队顺序就是处理顺序。生产系统按 §3.2 顺序运行、按创建顺序遍历，所以队列内容完全确定 |
| 生命周期 | 请求与事实只在本帧有效，12.2 清空。`PresentationEvents` 例外：保留到下一帧 0.1 才清空，供表现层取走。跨帧需要一律用组件状态或 `ScheduledActions`（二者都进快照） |
| 读写 | 事实可多读、只读；请求由唯一所有者系统排空 |
| 句柄 | 事件只存值与 `Entity`；消费时检查 `alive()` 且不是 `DespawningComponent` |
| 快照与回滚 | 队列是单例、进入快照，但帧边界上为空（Debug 断言），快照不携带在途事件；回滚无需特殊处理 |
| 重入 | 系统不消费自己本遍产生的同类事件。唯一的环（Despawn → SpawnRequest → SpawnFlushSystem）被同步点顺序打断 |

### 4.6 通信原则与同步规则链（P-1）

1. 系统之间不互相调用；系统可以调用规则库。
2. 规则库只写本领域组件，其余影响一律追加事件（生成、销毁请求、表现）。
3. **同步规则链**是唯一允许的即时连锁，只出现在下列点上：命中微管线（§6.6）、施法开始/结束、动作片段进入/退出、Buff 计时与触发 op 执行。在这些点上可直接调用 `buff::rules::fire / admit / remove`、`health::rules::applyDelta`、`skill::rules::forceInterrupt`。
4. 同步链的约束：不创建或回收实体（只发请求、只打 `DespawningComponent`），不推进时间，深度不超过 8（R16）。

未采用的备选：“命中批处理”——先为全部接触触发 BeforeHit，再统一算伤害，再统一触发 AfterHit。它更“纯”，但同帧多段命中、命中叠层、目标在第一段已死这些情况下伤害结果与旧实现不同，违反 D0-3。如果用户更看重纯队列，可接受“同帧多命中场景伤害不再逐一相等”，届时只需把 CombatSystem 拆成三个系统（§11 Q1）。

---

## 5. Buff：可组合 op 模型

### 5.1 运行时模型

转换器把 `buff.lua` + `buff_rule.lua` 的每一行降级为一个 `conf::battle::BuffDef`：

```text
BuffDef
├─ 准入 admission     stackKey(规则族, subType)、priority、maxStacks、removeAllStacks、resetType(0..7)、
│                     cd/cdPvp、probability/probabilityRepeat、immunityClass(buffType)、areaMask、
│                     target 选择器、binding、inherit 掩码、addType、destroyType
├─ 生命周期 lifecycle executeType(1/2/3)、times、intervalSec、innerCdSec（语义原样移植旧实现 BuffLifecycle）
├─ 激活 activation    beginOn / endOn(TriggerPoint)、事件目标过滤、oneShot、conditions(Predicate[])、
│                     latchOn 掩码(E/T/B)、releaseOn 掩码(X/N)
├─ 常驻 persistent    Op[]：激活时应用，失活时按来源（实例 serial）整体撤销
├─ 瞬时 instant       onEnter / onTick / onBegin / onEnd / onExit：Op[]
├─ 监视 watch         Watch / StateTimer（可选）
└─ 表现 presentation  spine / audio / icon / tips 原样透传给表现层
```

运行时只有一个解释器 `buff::rules`，没有“每条规则一个类/函数”。钩子沿用旧实现的五个：E=Enter、X=Exit、T=Update（周期）、B=Begin（began 事件）、N=End（ended 事件）。

- 常驻 op 的“激活/失活”由 `latchOn/releaseOn` 决定。例：霸体 `[E/X]`（只认 Enter/Exit），减速 `[EB/XN]`（Enter 或 Begin 激活，Exit 或 End 撤销），血量越少伤害越高 `[B/N]`（只认事件窗口）。实例上有一个 `latched` 门闩，重复激活不重复叠加——这正是旧实现与参照工程里随处可见的 `bTrigger` 模式。
- 撤销不再“减回去一个数”，而是删除该实例登记的全部修正（§5.7），所以不存在“加了 0.3 减了 0.2”的漂移。

### 5.2 生命周期与准入（原样移植）

准入顺序（`buff::rules::admit`）：

1. 接收者 `InactiveComponent` / `DespawningComponent` → 拒绝。
2. 被免疫（规则 id 或全免）、该 Buff id 仍在添加 CD、带减益免疫且 `buffType ∈ {3,4}`、区域过滤不通过 → 拒绝。
3. 概率：`p = probability`（`probabilityRepeat=1` 时 ×已有层数），`rng::draw(BuffProbability) < p` 才继续。
4. 按 `(规则族, subType)` 找已有实例（`subType = -1` 不查重）：没有 → 新实例并按 executeType 处理（1 立即激活并执行 Enter；2 激活等周期；3 不激活等事件）；已有且 `old.priority ≤ new.priority` → 按 resetType 刷新或叠层。
5. 记录添加 CD；同步触发 AddBuff（R7：新实例不接收正在进行的分派）。

接收者选择（`target` 字段）：1 自身、2 事件目标、3 事件队友、4 队伍成员、5 敌方阵营、6 Boss、7 怪物与精英、8 事件目标为自身、9 全阵营、10 敌方阵营除自身。

周期（每帧 P2，秒制）：

| times | interval | 行为 |
|---|---|---|
| −1 | >0 | 每 interval 执行一次 T；有 innerCd 时再节流 |
| −1 | 0 | 每帧执行 T |
| 0 | >0 | 纯持续：到期移除一层 |
| >0 | >0 | 每 interval 执行一次 E，满 times 后移除一层 |
| >0 | 0 | 每帧执行 E，满 times 后移除一层 |
| <−1 或 interval<0 | — | 不计时 |

重复添加（resetType）：0 不变；1 重置次数与间隔；2 重置次数；3 重置间隔；4 叠层；5 叠层 + 重置间隔；6 叠层 + 重置次数；7 叠层 + 全部重置。DOT 类（规则 `buffType=1`）重置时只在剩余次数更少时采用新值。新旧 id 不同时先对旧数据执行 Exit 再对新数据执行 Enter。

移除：每去掉一层执行一次 X；`removeRepeatAll` 一次去掉全部；层数为 0 时实例退出。**逻辑上立即失效，消失动画只在表现层播放**（旧实现在动画期间仍保留实例，属于表现驱动逻辑，不再沿用）。

### 5.3 触发点与激活方式

`began/ended` 的数值与旧 `BFEvent` 相同，新代码用 `TriggerPoint` 枚举。触发点在哪里被“同步触发”（P-1）：

| 值 | 触发点 | 发生处 | began / ended 行数 |
|---|---|---|---|
| 1 | BeforePrepareSkill | SkillRequestSystem：预约成功前（搓招在可施放检查之前） | 144 / 0 |
| 2 | BeforeCastSkill | FSM 进入 Attack：`beginCast` 完成 CD 与扣费之后 | 285 / 55 |
| 3 / 4 | BeforeHit / AfterHit | 命中微管线，攻击方 | 1556 / 0、613 / 1485 |
| 5 / 6 | BeforeToBeHit / AfterToBeHit | 命中微管线，受击方 | 116 / 0、46 / 107 |
| 7 | AfterCastSkill | `endCast` | 99 / 291 |
| 8 | AfterPrepareSkill | 已废弃，不触发（转换器告警） | 2 / 1 |
| 9 / 10 | BeforeNextSkill / AfterNextSkill | 新一招首段 `beginCast` / 末段 `endCast` | 29 / 3、0 / 35 |
| 11 / 12 | BeforeCrazy / AfterCrazy | 进入/退出曝气 | 22 / 0、1 / 24 |
| 13 | EvadeSuccess | 命中微管线：闪避技能躲开 | 17 / 0 |
| 14 | BeforeDeath | `health::rules`：hp≤0 | 16 / 0 |
| 15 | EpZero | VitalsTick：曝气 EP 归零 | 0 / 2 |
| 16 / 22 | UseTp / UseEp | `beginCast` 扣费 | 5 / 0、1 / 0 |
| 17 | ShieldClean | `health::rules`：护盾破裂 | 0 / 31 |
| 18 | AttackMiss | 命中微管线：属性闪避 | 5 / 0 |
| 19 / 20 | SkillColdStart / SkillColdEnd | `beginCast` 开始 CD / CooldownSystem CD 结束 | 1 / 36、0 / 0 |
| 21 | HpChange | `health::rules`：变化后仍存活 | 29 / 0 |
| 23 / 24 | BeforeArtifactSkill / AfterArtifactSkill | 神器施法（神器系统未在范围内，见 §11 Q6） | 252 / 0、17 / 7 |
| 25 / 26 | AddBuff / RemoveBuff | `buff::rules::admit / remove` | 8 / 6、0 / 6 |
| 27 / 28 | BeforeRoleSkillAction / AfterRoleSkillAction | 时间轴片段进入/退出 | 1 / 0、0 / 1 |
| 50 / 51 | BehaviorStateStart / End | FSM 状态进入/退出，只分派给 StateTimer | — |

数据中的四种激活方式（9335 行）：

| 方式 | 行数 | 主要形态 |
|---|---|---|
| 常驻（无 began/ended） | 5965 | 出生/命中/拾取挂上即生效，到期或被移除失效 |
| 窗口（began 与 ended 都有） | 1983 | BeforeHit→AfterHit 1437（只影响这一次命中）、BeforeCastSkill→AfterCastSkill 244、BeforeToBeHit→AfterToBeHit 97、BeforePrepareSkill→BeforeCastSkill 54、其余 151 |
| 脉冲（只有 began） | 1282 | AfterHit 609、BeforeArtifactSkill 245、BeforeHit 116、AfterCastSkill 96、AfterToBeHit 44、其余 172 |
| 只有 ended | 107 | 常驻激活，事件到来时结束（AfterHit 48、ShieldClean 29 等） |

事件过滤：`eventParam[1]` 为 0 不过滤，8 表示事件目标就是自己，其余按事件目标阵营位匹配（1975 行用 2）；`eventParam[2] = 1` 表示触发一次后退出（148 行）；`binding = 1` 要求事件技能等于绑定技能。条件 `conditions` 在触发时求值（§5.6）。

### 5.4 Op 集

常驻 op（激活时应用，失活时按来源撤销）：

| Op | 参数 | 作用 |
|---|---|---|
| `Stat` | stat、value(ValueExpr)、live | 向 `StatsComponent.m_mods` 登记 `{stat, value, source}`；live 时每次 T 重算并替换 |
| `Flag` | flag | `StatusFlagsComponent` 引用计数 +1。Stun、Frozen 同时在激活瞬间执行 `Interrupt` |
| `SkillMod` | field(CdScale/MpCostScale/EpPlus/DisableSlot/DisableArtifact)、scope(All/Bound/Demand/Slots[]/Global)、value | 登记技能级修正 |
| `Immune` | selector(Rules[]/BuffTypes[]/All) | 登记准入免疫 |
| `HpLock` | value | 登记锁血下限（多个锁相加） |
| `Shield` | amount、layers | 设置护盾值与层数；撤销时清零 |
| `Taunt` | weight | 在接收者的 `TauntTableComponent` 中为施加者加权 |
| `Kids` | ids[] | 激活时准入子 Buff，失活时移除（全层） |
| `Visual` | kind | 发出表现开/关（白框、隐身视图、霸体描边、墨汁屏） |

瞬时 op（在指定钩子执行，不撤销）：

| Op | 参数 | 作用 |
|---|---|---|
| `Dot` | coeff | `floor(标准伤害(施加者根) × coeff × (1+addition)) × 层数`，不计防御、暴击、浮动；经 `health::rules` |
| `Resource` | res(Hp/Mp/Ep/Anger)、amount、floor | 资源增减；治疗受 `NoHeal`、`HealingScale` 影响；扣血可选无视护盾 |
| `Apply` | ids[]、to(Self/Applicator/EventTarget/Area{sel, rx, rz})、n(ValueExpr)、pick(All/Weighted[]) | 准入 Buff。未显式给 `to` 时按子 Buff 自己的 target 字段分派（旧实现语义） |
| `Remove` | by(Id/Rule/Type)、keys[]、stacks(All/N[]) | 移除，结果计数写入寄存器 r0（X1） |
| `Cooldown` | target(Slots/All/Bound/Demand/ArtifactSlots)、mode(Reset{count, threshold, random}/SetRatio{base}/Add{sec}/Reduce{sec}) | 冷却操作 |
| `Extend` | rule、extraTimes | 该规则首个实例的次数上限 += n |
| `Ability` | id | 授予/触发特殊能力（`AbilityListComponent`） |
| `SpawnEffect` | effectId、mode | 生成特效（特殊能力用），标记 capabilityTrigger |
| `EnterCrazy` | — | 未在曝气时进入曝气 |
| `Interrupt` | — | `skill::rules::forceInterrupt` |
| `SetCost` | res(Crystal)、value | 设定技能晶体消耗 |
| `Sound` | audioId | 表现音效 |
| `ExitSelf` | — | 结束本实例（移除类规则执行完即退出） |

监视器：

| Op | 参数 | 作用 |
|---|---|---|
| `Watch` | predicates、kids、mode(Latched/Edge) | 每次 T 求值：条件不满足时挂子 Buff、满足时移除。Latched 只在结果变化时动作；Edge 每次按结果动作（旧实现 LoopCheck / LoopCheck2 语义） |
| `StateTimer` | kind(GetUp/HitRepel/HitFly)、seconds、kids | 监听 BehaviorState：在对应受击状态累计 ≥ seconds 时挂子 Buff 并触发能力（扩展 X2） |

所有 op 还可带一个前置谓词（例：BuffSeparate 的 `Dot` 只在“正在移动”时执行）。

### 5.5 取值表达式 ValueExpr

| 表达式 | 值 |
|---|---|
| `Const(p)` | p |
| `PerStack(p)` | p × 层数 |
| `MaxRatio(p, res)` | 比例时 p × 上限，否则 p。阈值按旧实现：Hp 为 0≤p<1；Ep、Mp 为 p≤1 |
| `HpAmount(p, floor)` | BuffHP 专用：上一行规则，且 p<0、floor>0 时结果不低于 `floor − hp` |
| `BaseMaxHp(p)` | 不含 Buff 加成的生命上限 × p |
| `Lost(p, res)` | p × (上限 − 当前) / 上限 |
| `Cur(p, res)` | p × 当前 / 上限 |
| `SpeedBonus(a, b, cap)` | clamp((MoveScaleX − 1) / a × b, 0, cap) |
| `TypeCount(pool, types[], per, cap)` | min(池中 buffType 属于 types 的实例数 × per, cap)；pool = 自身或事件目标 |
| `CoolingSlots(per, cap)` | min(冷却中的基础槽数 × per, cap) |
| `CoolingSlotTable(t[])` | t[冷却中的基础槽数] |
| `ComboTable(t[])` | t[当前评价等级] |
| `LastDamage(p)` | 触发上下文中本次伤害 × p |
| `Reg(p)` | 寄存器 r0（上一个 `Remove`/`Count` 的计数）× p（X1） |

默认在激活时求值并快照；`live` 的 `Stat` 每次 T 重算。

### 5.6 条件谓词

`buff.lua` 实际用到 93 种条件编号，转换器把它们降为下面 13 种谓词；逐条语义以旧实现 `BuffCondition.cpp` 为准（逐条移植并配单测）。

| 谓词 | 覆盖的条件编号（举例） |
|---|---|
| `HasRule{subject, rule}` | 108–262 中全部 “Target/EntityBFxxx”，256/257 |
| `HasBuff{subject, id, minStacks}` | 198、258 |
| `HasBuffType{subject, type}` | 10、161、162 |
| `HasFlag{subject, flag}` | 100–107、116/117、246/247、259/260 |
| `Motion{subject, kind}` | 112–115（加/减速）、122/123（浮空）、241/242（非浮空） |
| `Ratio{subject, res, min, max}` | 124–127、144/145、239/240 |
| `Delta{subject, res, lost/recovered}` | 154、163–169 |
| `RoleType{subject, types/signs}` | 3、4、173、174 |
| `Skill{kind, …}` | 5（搓招）、9、11–15（连段首/末/同槽）、175、243（技能判定表）、238（技能 id）、248（动作 id）、249（在 CD） |
| `Artifact{…}` | 232、233（依赖神器系统） |
| `Event{kind}` | 1（目标死亡）、2（暴击）、150/151（暗/光受击特效）、155（用 TP） |
| `Timer{moved/standing ≥ s}` | 171、172 |
| `Misc{…}` | 6（MP 足够）、7（晶体足够）、8（连击评价）、263（怒气变身） |

### 5.7 Stat 修正栈

`StatsComponent.m_final[s] = m_base[s] + Σ m_mods(stat = s).value`。定点加法与顺序无关，所以修正的登记顺序不影响结果；`m_mods` 仍按登记顺序保存（快照稳定）。读取统一走 `stats::rules::get(world, e, StatId)`，脏标记时重算。

| StatId | 来源 | 读取方 |
|---|---|---|
| 16 个基础属性（力量…基础伤害）、PvpAtk、PvpDef | 属性表 × attribute_rate | 伤害公式 |
| AddHurt / AvoidHurt | 扩展属性 100 / 101 | 伤害（攻/受） |
| AddCrit / AvoidCrit | 102 / 103 | 暴击率 |
| AddMaxHp | 106 | 生命上限 |
| AddDodge / AddHit | 109 / 111 | 属性闪避 |
| AddCritHurt / AddCritHurtDef | 112 / 113 | 暴伤倍率 |
| AddArtifactHit | 114 | 神器或能力来源伤害 |
| AddFloorTime | 115 | 硬直/倒地时间 |
| MoveScaleX / MoveScaleZ | 移速倍率（基础 1） | 位移；`!canMove()` 时有效值为 0（派生，不再命令式清零/恢复） |
| ActionScale | 动作速率（基础 1） | 时间轴、动画 |
| MpRecoveryScale / EpRecoveryScale | 回复倍率 | Vitals、命中回 EP |
| MpCostScale / EpCostScale / CpCostScale | 全局消耗倍率 | 施法扣费、曝气消耗 |
| HealingScale | 治疗效率（基础 0，治疗 × (1+Σ)） | `health::rules` |
| ComboExpScale | 连击经验倍率（基础 1） | Combo |
| ArtifactCdRate | 神器 CD 倍率 | 神器系统 |
| HpLock | 锁血下限之和 | `health::rules` |

技能级修正（CD 倍率、TP 消耗倍率、额外 EP、禁用计数）登记在 `SkillBookComponent`，同样按来源撤销。

### 5.8 143 条在用规则 → op

统计：`buff.lua` 共 9337 行，2 行 ruleId 非法；其余 9335 行使用 143 个规则类。旧实现分派表覆盖 79 类，其中 18 类是空标记（只供条件按规则查询），`AddByState` 的三个钩子是空函数；64 类未移植（404 行）。

图例：
- 旧：✓ 已移植；○ 空标记（已移植）；△ 旧实现为空或与参照实现有偏差；✗ 未移植（新 sim 按参照工程实现，无 golden）。
- 映射：● 核心 op 可表达；◐ 需要 §5.9 的扩展；✖ 不可表达或死规则，需要决策。
- `[EB/XN]` 表示常驻 op 的激活/撤销钩子；`E:` `T:` `B:` 前缀表示瞬时 op 在哪些钩子执行；p1、p2… 是 `paramValue` 下标（从 1 起），pv2 是 `paramValue2`。

**汇总**：● 128 类（9234 行）；◐ 12 类（97 行）；✖ 3 类（4 行）。

#### 5.8.1 状态、控制与标记（rule 1–63）

| id | 类名 | 行 | 旧 | 映射 | 备注 |
|---|---|---|---|---|---|
| 1 | BuffSuperArmor | 62 | ✓ | ● `[E/X] Flag(SuperArmor)`；E: `Sound`；`spine_step[1]=1` 时 `[E/X] Visual(霸体描边)` | 描边只是表现 |
| 2 | BuffCrazy | 1 | ✓ | ● E: `EnterCrazy` | |
| 3 | BuffInvincible | 50 | ✓ | ● `[E/X] Flag(Invincible)`；E: `Sound` | |
| 4 | BuffBurn | 122 | ✓ | ● E: `Dot(p1)` | 周期由 times/interval 驱动 E |
| 5 | BuffPoison | 33 | ✓ | ● E: `Dot(p1)` | |
| 6 | BuffSneer | 2 | ✓ | ● `[E/X] Taunt(p1)` | 权重记在施加者名下 |
| 7 | BuffStun | 6 | ✓ | ● `[E/X] Flag(Stun)`；E: `Interrupt`、`Sound` | 移动锁由 `canMove()` 派生 |
| 8 | BuffHPLock | 26 | ✓ | ● `[E/X] HpLock(MaxRatio(p1,Hp))` | |
| 9 | BuffSpeed | 165 | ✓ | ● `[EB/XN] Stat(MoveScaleX,p1) Stat(MoveScaleZ,p2)` | |
| 10 | BuffBleeding | 72 | ✓ | ● E: `Dot(p1)` | |
| 11 | BuffDurance | 54 | ✓ | ● `[EB/XN] Flag(Durance)` | 不打断施法 |
| 12 | BuffDark | 4 | ○ | ● 标记（无 op） | 供 `HasRule` |
| 13 | BuffThunder1 | 5 | ○ | ● 标记 | |
| 14 | BuffThunder2 | 3 | ○ | ● 标记 | |
| 15 | BuffThunder3 | 1 | ✗ | ● 标记 | 参照类为空 |
| 16 | BuffPlague1 | 1 | ✗ | ● E: `Dot(p1)` | |
| 17 | BuffPlague2 | 1 | ✗ | ● E: `Dot(p1)` | |
| 18 | BuffPlague3 | 1 | ✗ | ● E: `Dot(p1)` | |
| 19 | BuffBreakArmor | 3 | ✓ | ● `[EB/XN] Stat(AvoidHurt,p1)` | |
| 20 | BuffMagicUp | 1 | ✗ | ● `[EB/XN] Stat(AddHurt,p1) Stat(MpCostScale,p2)` | |
| 21 | BuffDark2 | 1 | ✗ | ● 标记 | |
| 22 | BuffSeparate | 12 | ✓ | ● E: `Dot(p1)`，前置谓词 `Timer(moved>0)` | |
| 23 | BuffCold | 42 | ✓ | ● E: `Dot(p1)` | |
| 24 | BuffPawMark | 6 | ✗ | ● 标记 | |
| 25 | BuffDeathMark | 12 | ○ | ● 标记 | |
| 26 | BuffPurplePoison | 12 | ✓ | ● E,B: `Resource(Ep, MaxRatio(p1,Ep))` | |
| 27 | BuffPassivation | 1 | ✗ | ● `[EB/XN] Stat(AvoidCrit,p1)` | |
| 29 | BuffRepent | 8 | ✓ | ● E: `Dot(p1)` | |
| 30 | BuffFall | 12 | ✓ | ● `[EB/XN] Stat(MoveScaleX,p1) Stat(MoveScaleZ,p2) Stat(AddHurt,p3)` | 第三参按旧实现加到 AddHurt |
| 31 | BuffPalsy | 10 | ✓ | ● `[EB/XN] Flag(Palsy)` | |
| 32 | BuffBurn2 | 2 | ✗ | ● E: `Dot(p1)` | |
| 33 | BuffChill | 9 | ✓ | ● E: `Dot(p1)`；`[E/X] Flag(NoHeal)` | 禁疗是参照语义，旧实现未做（已知偏差） |
| 34 | BuffFlaw | 3 | ✗ | ● `[EB/XN] Stat(AvoidHurt,p1)` | |
| 35 | BuffStarShield | 15 | ✓ | ● `[EB/XN] Stat(AvoidHurt,p1)` | |
| 36 | BuffGhostWalk | 15 | ✓ | ● `[EB/XN] Stat(MoveScaleX,p1) Stat(MoveScaleZ,p2) Stat(AddDodge,p3)` | |
| 37 | BuffGuard | 1 | ✗ | ● 标记 | |
| 38 | BuffBloodBeast | 6 | ✗ | ● `[EB/XN] Stat(AvoidHurt,p1) Stat(AddHurt,p2)` | |
| 40 | BuffRedPython | 5 | ✗ | ● 标记 | |
| 41 | BuffMeltingArmor | 2 | ✗ | ● E: `Dot(p1)` | |
| 43 | BuffCutSakura | 10 | ○ | ● 标记 | |
| 44 | BuffSilence | 1 | ✓ | ● `[E/X] Flag(Silence)` | |
| 45 | BuffLightMark | 5 | ○ | ● 标记 | |
| 46 | BuffStoneSkin | 6 | ○ | ● 标记 | |
| 47 | BuffAdjudication | 5 | ○ | ● 标记 | |
| 48 | BuffReverse | 5 | ○ | ● 标记 | |
| 49 | BuffElectricEye | 1 | ○ | ● 标记 | |
| 50 | BuffLightWing | 1 | ✗ | ● 标记 | |
| 51 | BuffSpeedStep | 1 | ✓ | ● `[EB/XN] Stat(MoveScaleX,p1) Stat(MoveScaleZ,p2)` | 参照实现在不可移动时不加；按旧实现无条件登记 |
| 53 | BuffUnstable | 2 | ○ | ● 标记 | |
| 54 | BuffDarkMark | 1 | ○ | ● 标记 | |
| 55 | BuffSuperBlind | 3 | ✓ | ● `[EB/XN] Stat(AddHit,p1)`；`Visual(墨汁屏)` | |
| 56 | BuffMight | 2 | ○ | ● 标记 | |
| 57 | BuffElectron | 1 | ✓ | ● E: `Dot(p1)` | |
| 58 | BuffElectron2 | 1 | ○ | ● 标记 | |
| 59 | BuffStealth | 10 | ✓ | ● `[E/X] Flag(Stealth)`；`Visual(隐身视图)` | 敌我可见度由表现层按阵营决定 |
| 60 | BuffFrozen | 3 | ✓ | ● `[E/X] Flag(Frozen)`；E: `Interrupt` | |
| 63 | BuffDarkMark2 | 1 | ○ | ● 标记 | |

#### 5.8.2 施加、移除与监视（rule 100–751）

| id | 类名 | 行 | 旧 | 映射 | 备注 |
|---|---|---|---|---|---|
| 100 | BuffAddByApplicator | 3397 | ✓ | ● `[ETB/N] Kids(p[])` | Exit 不撤销子 Buff；T 有门闩按旧实现（参照实现每次 T 都添加，已知偏差） |
| 102 | BuffAddByApplicator3 | 94 | ✓ | ● `[ETB/XN] Kids(p[])` | |
| 103 | BuffAddByApplicator4 | 4 | ✓ | ● B: `Apply(p[], to=Applicator)` | 挂给事件中的攻击者 |
| 104 | BuffAddBySelect | 1 | ✗ | ● E,T,B: `Apply(p[], pick=Weighted(pv2[1]))` | RNG 类别 Choice |
| 300 | BuffAddByDetector | 38 | ✓ | ● E,T,B: `Apply(p[4..], to=Area{sel=p1, rx=p2, rz=p3})` | 选择器 4 友 / 5 敌 / 6 Boss / 7 怪与精英 / 9 全体 |
| 400 | BuffRemoveByType | 20 | ✓ | ● E: `Remove(Type, p[])`、`ExitSelf` | |
| 401 | BuffRemoveByTypeAddExtra | 3 | ✗ | ◐ `[EB/XN]` 激活：`Remove(Type,p1)→r0`（上限 p2）、常驻 `Stat(AddHurt, Reg(p3))`、瞬时 `Resource(Mp, 上限×r0×p4)` | X1 |
| 500 | BuffAddByState | 9 | △ | ◐ `StateTimer(kind=p1, sec=p2, kids=p[3..])`；X: 撤销子 Buff | 旧实现三个钩子都是空函数；X2 |
| 600 | BuffAddByComboEvaluate | 3 | ✗ | ● T: `Watch(Latched, conditions, kids=p[])` | |
| 700 | BuffRemoveByRule | 6 | ✓ | ● E: `Remove(Rule, p[])`、`ExitSelf` | |
| 750 | BuffRemoveById | 52 | ✓ | ● E: `Remove(Id, p[])`、`ExitSelf` | |
| 751 | BuffRemoveById2 | 2 | ✓ | ● E: `Remove(Id, p[], stacks=pv2[i][1])`、`ExitSelf` | 按层数移除 |

#### 5.8.3 伤害、暴击与防御修正（rule 800–1230）

| id | 类名 | 行 | 旧 | 映射 | 备注 |
|---|---|---|---|---|---|
| 800 | BuffAttributeCombo | 7 | ✗ | ● `[EB/XN]` 按 p1：1 `Stat(AddHurt)`、2 `SkillMod(CdScale, Demand/All)`、3 `Stat(MpCostScale)`、4 `Stat(AddCrit)`、5 `Stat(AddCritHurt)`；值 `ComboTable(p[2..])` | 转换器按 p1 展开成单个 op |
| 900 | BuffAttributeSlotCD | 11 | ✗ | ● `[EB/XN]` 按 p1：1 AddHurt、2 AddCrit、3 AddCritHurt；值 `CoolingSlotTable(p[2..])` | |
| 1000 | BuffDamageHurt | 865 | ✓ | ● `[EB/XN] Stat(AddHurt, PerStack(p1))` | |
| 1001 | BuffDamageHurt2 | 597 | ✓ | ● `[EB/XN] Stat(AddHurt, p1)` | |
| 1002 | BuffDamageHurt3 | 79 | ✗ | ● `[EB/XN] Stat(AddArtifactHit, p1)` | 只作用于神器/能力来源伤害 |
| 1010 | BuffDamageCrit | 250 | ✓ | ● `[EB/XN] Stat(AddCritHurt, p1)` | |
| 1011 | BuffDamageCritDef | 2 | ✗ | ● `[EB/XN] Stat(AddCritHurtDef, p1)` | |
| 1040 | BuffDamageReduction | 487 | ✓ | ● `[EB/XN] Stat(AvoidHurt, p1)` | |
| 1050 | BuffDamageHPLoss | 27 | ✓ | ● `[B/N] Stat(AddHurt, Lost(p1,Hp))` | |
| 1051 | BuffDamageCritHPLoss | 6 | ✗ | ● `[B/XN] Stat(AddCritHurt, Lost(p1,Hp))` | |
| 1070 | BuffDamageTPLoss | 11 | ✗ | ● `[B/XN] Stat(AddHurt, Lost(p1,Mp))` | |
| 1071 | BuffCurtTPLoss | 2 | ✗ | ● `[EB/XN] Stat(AddCritHurt, Lost(p1,Mp))` | |
| 1080 | BuffDamageTPResidue | 4 | ✗ | ● `[B/XN] Stat(AddHurt, Cur(p1,Mp))` | |
| 1091 | BuffHurtEPPercent | 6 | ✗ | ● `[EB/XN] Stat(AddHurt, Cur(p1,Ep))` | |
| 1100 | BuffDamageTypeGradation | 11 | △ | ● `[B/N] Stat(AddHurt, TypeCount(pool=p1, types=p[4..], per=p2, cap=p3))` | 数据 p1=2（数目标身上的 Buff）；旧实现只数自身池（已知偏差，§11 Q4） |
| 1101 | BuffHPTypeGradation | 2 | ✗ | ● E,B: `Resource(Hp, 上限 × TypeCount(...))` | |
| 1102 | BuffCritTypeGradation | 5 | ✗ | ● `[EB/XN] Stat(AddCrit, TypeCount(...))` | |
| 1103 | BuffCritDamageTypeGradation | 10 | ✗ | ● `[EB/XN] Stat(AddCritHurt, TypeCount(...))` | |
| 1200 | BuffDefendHPLoss | 5 | ✗ | ● `[B/XN] Stat(AvoidHurt, Lost(p1,Hp))` | |
| 1210 | BuffDamageBySpeed | 7 | ✗ | ● `[EB/XN] Stat(AddHurt, SpeedBonus(p1,p2,p3))` | 只增不减 |
| 1220 | BuffDamageCritByHPLost | 2 | ✗ | ● `[EB/XN] Stat(AddCritHurt, p1)` | 名不副实：参照实现是常量 |
| 1230 | BuffFloorTm | 1 | ✗ | ● `[EB/XN] Stat(AddFloorTime, p1)` | |

#### 5.8.4 生命、能量与 TP（rule 2000–4032）

| id | 类名 | 行 | 旧 | 映射 | 备注 |
|---|---|---|---|---|---|
| 2000 | BuffHP | 304 | ✓ | ● E,T,B: `Resource(Hp, HpAmount(p1, floor=p2))` | 扣血带 floor 时无视护盾 |
| 2002 | BuffModifyHPBasedByLostHp | 1 | ✗ | ✖ 参照工程没有这个类（加载即失败） | 判为死规则或按名实现 `Resource(Hp, Lost(p1,Hp))`（§11 Q4） |
| 2010 | BuffHPMAX | 33 | ✗ | ● `[EB/X] Stat(AddMaxHp, BaseMaxHp(p1))`；激活瞬时 `Resource(Hp, hp×p1)`；X 瞬时 `Resource(Hp, hp/(1+p1) − hp)` | N 不撤销，按参照实现 |
| 2020 | BuffHealingChange | 10 | ✗ | ● `[E/X] Stat(HealingScale, p1)` | 参照为空类，由扣血入口按存在性读取 |
| 3000 | BuffEP | 227 | ✓ | ● E,B: `Resource(Ep, MaxRatio(p1,Ep))` | p1≤1（含负数）为比例 |
| 3010 | BuffEPConsumeScale | 6 | ✓ | ● `[EB/XN] Stat(EpCostScale, p1)` | |
| 3020 | BuffEPRecoveryScale | 51 | ✓ | ● `[EB/XN] Stat(EpRecoveryScale, p1)` | |
| 3030 | BuffCPConsumeScale | 11 | ✗ | ● `[EB/XN] Stat(CpCostScale, p1)` | |
| 3040 | BuffEPPlus | 3 | ✗ | ● `[EB/XN] SkillMod(EpPlus, Demand/Global, p1)` | |
| 4000 | BuffTP | 87 | ✓ | ● E,B: `Resource(Mp, MaxRatio(p1,Mp))` | |
| 4001 | BuffTpAddNewBuff | 2 | ✗ | ● E,B: `Apply(p[], to=Self)` | 由 UseTp 触发 |
| 4010 | BuffTPConsumeScale | 206 | ✓ | ● `[EB/XN] SkillMod(MpCostScale, Bound/All, p1)` | 无绑定时同时登记全局 |
| 4030 | BuffTPRecoveryScale | 169 | ✓ | ● `[EB/XN] Stat(MpRecoveryScale, p1)` | |
| 4031 | BuffTPRecoveryScaleBySpeed | 6 | ✗ | ● `[EB/XN] Stat(MpRecoveryScale, SpeedBonus(p1,p2,p3))` | |
| 4032 | BuffTPRecoveryScaleBySlotCD | 6 | ✗ | ● `[ETB/XN] Stat(MpRecoveryScale, CoolingSlots(p1, cap=p2), live)` | |

#### 5.8.5 技能、冷却、免疫与其他（rule 5000–21004）

| id | 类名 | 行 | 旧 | 映射 | 备注 |
|---|---|---|---|---|---|
| 5000 | BuffCDSkill | 193 | ✓ | ● `[EB/XN] SkillMod(CdScale, Bound/All, p1)` | |
| 5001 | BuffUpdateArtifactCD | 5 | ✗ | ◐ E,T,B: `Cooldown(ArtifactSlots p[], Reduce(pv2[i][1] 秒))`，遇到不在 CD 的槽即停止 | 依赖神器系统，X5 |
| 5020 | BuffHemophagia | 6 | ✗ | ◐ B: `Resource(Hp, p1>1 ? p1 : LastDamage(p1))` | 参照实现由飞行血球到达时回血（表现驱动），需定时机（§11 Q5） |
| 5030 | BuffModifyCDSkill | 3 | ✗ | ● E,B: `Cooldown(Demand/All, SetRatio(Config, p1))` | |
| 5040 | BuffConsumeCrystal | 7 | ✗ | ● E,B: `SetCost(Crystal, p1)` | |
| 5050 | BuffCrit | 276 | ✓ | ● `[EB/XN] Stat(AddCrit, p1)` | |
| 5060 | BuffCritHPLoss | 3 | ✗ | ● `[B/XN] Stat(AddCrit, Lost(p1,Hp))` | |
| 5070 | BuffSign | 17 | ○ | ● 标记 | |
| 5080 | BuffDeductEpReplaceHp | 2 | ✗ | ✖ 拟 `Redirect(Ep, p1, p2)` | 参照工程按另一个名字查询，从未生效（§11 Q4） |
| 5090 | BuffDeductTpReplaceHp | 1 | ✗ | ✖ 拟 `Redirect(Mp, p1)` | 同上 |
| 5100 | BuffImmune | 6 | ✗ | ● `[EB/XN] Immune(Rules p[])` | 参照实现 End 时误调“移除 Buff”；新实现对称撤销 |
| 5110 | BuffComboExp | 5 | ✓ | ● `[EB/XN] Stat(ComboExpScale, ComboTable(p[]) − 1)` | 旧实现直接赋值倍率，单实例时等价 |
| 5120 | BuffExtendDebuff | 2 | ✗ | ● E,B: `Extend(rule=p1, extraTimes=p2)` | |
| 5130 | BuffNotConsumeCrystal | 2 | ✗ | ● `[E/X] Flag(NoCrystalCost)` | |
| 5150 | BuffShield | 35 | ✓ | ● `[ETB/XN] Shield(MaxRatio(p1,Hp), layers=p4) Kids(p2)`；X,N 瞬时 `Apply(p3)` | 无门闩：每次 E/T/B 重设护盾值；ended 多为 ShieldClean |
| 5160 | BuffAvoidCrit | 62 | ✓ | ● `[EB/XN] Stat(AvoidCrit, p1)` | |
| 6000 | BuffImmuneDebuff | 13 | ○ | ● `[E/X] Immune(BuffTypes{3,4})` | 旧实现在准入时按存在性检查，等价 |
| 7000 | BuffLoopCheck | 163 | ✓ | ● T: `Watch(Latched, conditions, kids=p[])` | 条件不满足时挂子 Buff、满足时移除；失败时走能力接缝 |
| 7001 | BuffWithTypeAddBuff | 3 | ✗ | ◐ E,B: `Count(by=p1∈{Type,Id,Rule}, key=p2, stacks≥p3)→r0`、`Apply(p4, n=p5×r0)`；X,N: `Count(stacks<p3)→r0`、`Remove(p4, n=p5×r0)` | X1 |
| 7002 | BuffLoopCheck2 | 187 | ✓ | ● T: `Watch(Edge, conditions, kids=p[])` | |
| 7003 | BuffWithTypeAddBuff2 | 24 | ✗ | ◐ 同 7001，层数比较改为 `=p3` / `≠p3` | X1 |
| 7004 | BuffSpecialAbilityApplicator | 127 | ✓ | ● E: `Ability(bindSpecialAbilityId)` | |
| 8000 | BuffDodge | 54 | ✓ | ● `[EB/XN] Stat(AddDodge, p1)` | |
| 8001 | BuffHit | 58 | ✓ | ● `[EB/XN] Stat(AddHit, p1)` | |
| 9000 | BuffResetSkillCD | 8 | ✗ | ● E,T,B: `Cooldown(基础槽（曝气时取 + 版）, Reset{count=p1, threshold=p2, random})` | 阈值正数表示剩余 > 阈值，负数表示剩余 < 绝对值 |
| 9001 | BuffResetArtifactCD | 3 | ✗ | ◐ E,T,B: `Cooldown(ArtifactSlots{1,2}, Reset{count=p1, threshold=p2, random})` | X5 |
| 9002 | BuffModifyCDSkillNew | 9 | ✗ | ● E,B: `Cooldown(slots=pv2[3], pick=pv2[1]（1 随机/2 顺序）, n=pv2[2], 每槽{threshold=pv2[4][i], base=pv2[5][i]（1 配置/2 剩余）, amount=pv2[6][i]（绝对值≤1 为比例，否则秒）})` | |
| 10003 | BuffCDArtifact | 28 | ✗ | ◐ `[EB/XN] Stat(ArtifactCdRate, p1)`；PVP 中宠物/时装来源即时重算神器 CD | X5 |
| 11000 | BuffAngerAdd | 13 | ✓ | ● E,B: `Resource(Anger, p1)`（变身中改加冻结时间） | |
| 20000 | BuffWhiteFrame | 2 | ✓ | ● `[E/X] Visual(WhiteFrame)` | |
| 20001 | BuffSkyProtect | 4 | ○ | ● 标记 | |
| 20002 | BuffDisableArt | 4 | ✗ | ◐ `[E/X] SkillMod(DisableArtifact, Slots p[])` | X5 |
| 20003 | BuffDisableSlots | 3 | ✗ | ● `[E/X] SkillMod(DisableSlot, Slots p[])` | 槽位编号经“技能序号 → 槽位”表 |
| 21001 | BuffChangeRoleAcitonScaleTime | 1 | ✗ | ● `[EB/XN] Stat(ActionScale, p1)` | 由动作片段进入/退出触发 |
| 21002 | BuffSealSkill | 7 | ✗ | ◐ `[E/X] Choice(均匀三选一){DisableArtifact(随机 1..2) / DisableSlot(随机取 p[] 之一) / DisableSlot{8,9}}` | X3；选择结果存在实例上，撤销对称 |
| 21003 | BuffSummonDummy | 3 | ✗ | ◐ `[E/X] Stat(AvoidHurt,p1) HpLock(上限×p2) Ability(bindSpecialAbilityId)` + `SummonLink` | 被链接的召唤物死亡时提前撤销并广播；X4 |
| 21004 | BuffDummyDeathRattle | 2 | ✗ | ◐ `[E/X] OnSummonDeath(ability=p1) → Apply(p2)` | X4 |
| −1 | （非法 ruleId） | 2 | — | ✖ 转换器报错（buff id 22120、22130） | 修表或删行 |

### 5.9 需要扩展或决策的项

| 编号 | 扩展 | 涉及规则 | 提案 |
|---|---|---|---|
| X1 | 计数寄存器 | 401、7001、7003 | 解释器为每次 op 列表执行提供寄存器 r0；`Remove` 与新增的 `Count{by, key, stackCmp}` 写入 r0；ValueExpr `Reg(p)` 读取。只在同一 op 列表内有效，不入快照 |
| X2 | 状态计时监视器 | 500 | FSM 在状态进入/退出时同步触发 BehaviorStateStart/End（只分派给 StateTimer）；实例上保存三类状态的有效位与累计秒；达到阈值时挂子 Buff 并触发能力；X 时撤销 |
| X3 | 随机分支 | 21002（104 的加权选择已包含在 `Apply.pick`） | `Choice{weights[], branches[Op[]]}`，激活时用 `rng::draw(Choice)` 选定并把分支下标存进实例，撤销时只撤销该分支 |
| X4 | 召唤物链接 | 21003、21004 | `SummonComponent` 记录来源能力 id；DeathSystem 处理召唤物死亡时对主人同步触发 `SummonDied{abilityId}`；`SummonLink` 收到时提前失活；`OnSummonDeath` 是以它为触发点的脉冲 |
| X5 | 神器（武装技能）系统 | 5001、9001、10003、20002、21002 第一分支；另有 BeforeArtifactSkill 脉冲 252 行、ArtifactJudge 条件 282 行、DamageHurt3 79 行 | 旧实现没有神器施法系统，只透传来源字段。op 与谓词先定义好，神器系统缺席时作用为空并在转换报告里列出；是否把神器纳入 PVP 范围见 §11 Q6 |
| — | 伤害转移 `Redirect` | 5080、5090 | 若实现：作为扣血入口在护盾之前的一个阶段（按参照类里的公式扣 EP/TP 代替 HP）。参照工程从未生效，建议先不实现、转换报告标出 |
| — | 吸血时机 | 5020 | 建议命中当帧立即回血（不依赖表现），或固定延迟 N 帧（`ScheduledActions`） |

### 5.10 特殊能力并入

`special_ability`（854 行）与 Buff 共用触发点、谓词与 op 解释器（P-6）。转换器把每行降级为 `AbilityDef`：触发点（event）+ 谓词（condition/condition_value）+ 概率 + CD（含 PVP）+ 周期 + 结束方式（0 不结束 / 1 倒计时）+ op（`SpawnEffect(effect_id, effect_id_type)`、`Apply(bind_buff_id)`）。`pvp_setting` 在准入时处理（0 在 PVP 中禁用，>1 替换为该 id）。实例存放在 `AbilityListComponent`，与 `BuffListComponent` 分开以保持两张表的字段清晰。

实现注记（S2 降级，与上文不一致的地方）：

- `BuffDef` / `AbilityDef` 的查找键是 `m_id`（`KeyByMId`）。Op、ValueExpr、Predicate 是带 kind 的扁平结构；`Choice` 的分支放在 `BuffDef::m_choicePool`，不递归嵌套 op。
- 取值补了四式，都只服务既有规则：`CurMul`（2010 激活时 `hp×p`）、`HpMaxAdjust`（2010 退出时 `hp/(1+p)−hp`）、`RegOfMax`（401 的 `上限×r0×p`）、`MaxTimes`（1101 的 `上限×TypeCount`）。`ComboTable` 另有偏置，5110 为 −1。
- 特殊能力的事件编号与 Buff 触发点不是同一套。降级时映射为 `TriggerPoint`：1–8 对应施法/命中/受击/曝气，9–14 为 `AbilityTime`、`InitAbility`、`GetUp`、`HitRepel`、`HitFly`、`ArtifactButton`（60 起）。能力条件 1/2/3 分别是暴击、连击评价、曝气，不复用同号的 Buff 条件。
- 神器、晶体、可控特效（`effect.control=1`）的 op 保留原 kind，`OpEffect` 标成空作用，整行记为 noop 并写入 `lower_report.txt`。规则 2002、5080、5090 与非法 ruleId（22120、22130）记为 unsupported，不写有效 op，不因此让打包失败。

---

## 6. 技能时间轴、角色状态机、命中、位移、特效与召唤

### 6.1 技能与施法

`SkillDef`（由 `skill.lua` 降级）：

| 字段 | 来源 | 说明 |
|---|---|---|
| `m_sequences[dir][]` | `actionIds[toward][i]` | 每个方向列是一串动作片段 id（2082 行 1 列、111 行 2 列、59 行 5 列） |
| `m_next` | `nextSkill` | 连段下一步 |
| `m_cdMs` / `m_pvpCdMs` / `m_charges` | `cd` / `pvpCd` / `cdCount` | 次数制只有 5 行用 2 |
| `m_cost{mp, ep, crystal, rage}` | 同名字段 | |
| `m_sorder` / `m_sorderControl` | `sorder` / `sorderControlType` | 打断优先级与无视优先级的位掩码 |
| `m_aiGate` | `skillAiId` | SkillAi 闸门配置 |
| 丢弃 | `upActionIds`、`skillType`、`pressTime` | 数据中 `skillType` 全为 0，抬起释放不实现 |

槽位与技能簿：`SkillBookComponent` 由出生装配（玩家技能配置、怪物 AI 技能组、`skill_change_system` 展开）构建，槽位名沿用旧实现（A 普攻、B/C/D、F 闪避、G/H 招式、I/J/K 奥义、L 挣脱、Y 冲刺、`n+` 搓招、`X+` 曝气版、P 怒气变身；搓招失败回退 A），移植自旧实现 `SkillLoadout`/`SkillLoad`。`SkillRuntime` 增加 `m_link`（同槽链序号，1 起）与 `m_mode`（同链模式号）；`B_2` 表示 B 槽第 2 模式，JOYB/JOYC/JOYD 分别映射到 `B_2`/`C_2`/`D_2`。offlineKit 按职业展开：普通职业 kOrdered 10 槽（Attack/Sprint/Dodge/Crazy/A/B/C/EA/EB/SUA），溢出叶子依次填入 J/K/L/P 槽，`leafBuffId` 追加到溢出；`leafRubId` 有 rock 用 `n+`（SA/SB/SC），否则落到被动 M/N/O（SAP/SBP/SCP）。伙伴职业走独立 9 槽 + 2 招式槽装配，不套 kOrdered。

`SkillRequestSystem`（4.1）每个按键意图的处理顺序（移植自旧实现 `SkillInput`/`SkillCastComponent`，对应参照文档“施法通路”）：

1. 按键 → 槽位（`eventSlot`：ATKA→A、ATKF→F、JOY1–12→`n+`、JOYB/C/D→`B_2`/`C_2`/`D_2` 等）；抬起事件忽略（无抬起释放技能）。
2. 槽位修正（`dealButton`）：跑动中或冲刺连段中按 A → 冲刺槽 Y；搓招（JOYn）先取曝气中的 `slot+` 版本（有则改写），缺失或不可施放 → 回退 A（之前触发 BeforePrepareSkill）；曝气键 E 在曝气中、或当前已是 E 槽施法时无效；曝气中其它槽存在 `slot+` 版本则改用。
3. 不在 Attack 时清理残留施法。
4. `getFightSkill(slot, mode)`：同槽按当前链/模式续段；次数（`m_releaseCount`）用尽则 `++m_link` 从 step=1 起（`_2` 模式位保留到新链）；异槽从首段开始。输入带 `_2` 就按 mode=2 查，查不到再退回 mode=1。
5. 可施放检查（`isAllowCast`）：受击（含起身，`isTobeHit`）只允许 L 槽且需 `isCanBreak` 状态；沉默/不可控（`!canControl()`）不可；槽位被禁用（`SkillMod::DisableSlot`）不可；冷却中（`m_releaseCount==0` 且 `m_cd>0`）不可；MP/EP 不足不可；E 槽在曝气中无效；P 槽需怒气满且未变身、非回退中。晶体消耗首批不拦、不扣（no-op）。`m_autoCast` 时还要过 SkillAi 闸门（load_cd → 次数 → check_cd → 组合条件 → 概率）。
6. 挂接：没有当前施法 → 立即 `beginCast`；同槽且 `m_allowInterrupt` 且 `isPriority` → 置 `m_nextSkill` 并 `m_switch = true`；否则缓存为 `m_nextSkill`（同槽 F 直接拒绝，异槽闪避走缓存）。跑步按键只设置 `SkillCastComponent.m_nextRun`。

施法的开始与结束是 Attack 状态的进入/退出动作（4.2），由 `skill::rules` 完成：

| 动作 | 内容 |
|---|---|
| `beginCast` | 新一招首段（step=1）触发 BeforeNextSkill；开始 CD（`cdMs = pvp ? pvpCdMs : cdMs` × 修正）；次数 −1；step=1 时同槽同链异模式同步次数与 CD；link/step>1 时给该槽 link=1 的技能追加 CD；扣费（MP×消耗倍率、EP，曝气中不扣 MP/EP；晶体不扣）；触发 UseTp/UseEp/SkillColdStart；按摇杆选方向列（首批取第 0 列，方向列有意未做）；处理技能组/技能变化（有意未做）；记伙伴怒气首段待领取值（`SkillCastComponent`）；触发 BeforeCastSkill；启动 `ActionTimelineComponent` |
| `endCast` | 触发 AfterCastSkill（末段再触发 AfterNextSkill）；怒气槽 P 收束时若怒气满且未变身则进入变身（`RageStateComponent.m_transformed` + 冻结 ms）；若当前施法仍是本技能，切换到缓存的下一招 |
| `forceInterrupt` | 清空当前与缓存施法，本技能 `autoRelease=0` 的特效立即 `DespawningComponent`（R5）；片段退出动作（移除片段 Buff、AfterRoleSkillAction）在下一次未暂停的帧执行 |

### 6.2 时间轴格式

`ClipDef` 由 `action`（角色片段，5855 行）与 `action_effect`（特效片段，14307 行）降级而来，两者共用一种格式：

```text
ClipDef
  id、motion（动作名/索引）、rate（aciton_scale_time）、loops（1、N、小数、−1=无限）、tailDelayMs（action_delay_time）
  displacementId
  displacementPolicy { wall: Pass | StopAtWall | EndClip, floor: Default | EndClip, endOnBrake }
  control { mode: None | SteerAtEnter(1) | FreeMove(0, velocity) | TurnOnly(2) | MoveNoTurn(3) }
  placement { posType, relativePos, orientation }   进入时瞬移（8 行）
  cancel { normalFrame, extraFrame }                 −1 表示没有窗口
  clipBuffs[]                                        进入时准入，退出时移除（3813 个动作配置了）
  cues[]                                             按 frame 升序，同帧 SpawnEffect 按 effectId 升序（其余保持声明顺序）
```

| Cue | 参数 | 来源字段 |
|---|---|---|
| `SpawnEffect` | effectId、用自定义方向 | `effect_ids` / `effect_frames`、`extend_role_vec` / `custom_vec` |
| `CameraShake` | cameraId | `camera_id` / `camera_frame` |
| `Sound` | ids | `sound_id`（第 0 帧） |
| `Ghost` / `Tips` / `Dialog` / `NameShow` | 开关或 id | `ghost`、`tips_show`、`dialog_show`、`name_show` |
| `DisplaySpine` | 分组、帧数 | `display_spine_ids` / `display_spine_frame`（3 行） |
| `TimeStop` | 目标(0 敌方 / 1 自己+敌方 / 2 同 1 且时长取演出帧数)、时长、复位时间 | `static_*`（112 行，全部为 2）→ 写 `PauseComponent.m_staticMs`（只作用于角色，敌方必中，含自己时自己也中） |
| `Transform` | 目标角色 id、type | `transform_*`（44 行，§6.9）→ `TransformRequest` |
| `Summon` | 角色 id、寿命 ms | 特效片段的 `summon_*`（220 行）→ `SpawnRequest(Role)` + `SummonComponent` |
| `HitSound` | 按受击者 `sound_type` 选音效 | 特效片段的 `sound_id2`（命中时由 Combat 读取） |

时间：片段时间 `clipMs += dtMs × rate × ActionScale`（暂停时不推进；特效使用释放者的 ActionScale）；cue 在 `clipMs ≥ frame × kLogicDtMs` 时触发，每次片段运行只触发一次（`ActionTimelineComponent.m_nextCue` 游标），同帧多个 SpawnEffect 按 effectId 升序发出。这与参照工程“`fTime ≥ fInterval × N`”等价。

片段进入/退出：`enterClip` 把片段的落地/撞墙/刹停策略写进 `DisplacementComponent.m_{floorPolicy, wallPolicy, endOnBrake}`，SteerAtEnter 先按摇杆转向再设位移，随后准入片段 Buff、触发 BeforeRoleSkillAction；`exitClip` 清策略与片段 Buff，并回收主人 `autoRelease = 0` 的特效（对齐旧实现动作结束清特效），空中退出只清速度、开默认重力等落地。

片段结束：读到 `MotionStateComponent.m_completedLoops ≥ loops`（上一帧写入，R10）后开始尾延时，结束后进入下一个片段或结束施法；`loops = −1` 由位移刹停（`endOnBrake`）或落地策略结束；`obstruct = 1` 撞墙结束并记录阻挡。落地/刹停/撞墙的 EndClip 由 `DisplacementComponent` 事件位在 P5 当帧写入、时间轴下一帧消费（R11）。

### 6.3 取消窗口（数据化）

| 窗口 | 条件 | 结果 |
|---|---|---|
| 普通窗口 `normalFrame` | 片段时间到达该帧（−1 永不开启；数据中 5002 个动作为 −1） | 由 TimelineSystem 置 `m_allowInterrupt = true`；有缓存下一招且 `canCancel(next, cur, false)`（同槽，或异槽且 `isPriority`）→ 置 `SkillCastComponent.m_switch`，下一帧 FSM 在 Attack 内 `endCast(deferNext = true)` 结束旧施法、开始新施法；否则若 `m_nextRun` → 结束施法交给 FSM 转入跑动 |
| 至尊窗口 `extraFrame` | 片段时间到达该帧 | `m_allowExtraInterrupt = true`；异槽下一招满足 `isSuperPriority`（`sorderControl` 含 1 且普通窗已开，或含 2 且至尊窗已开），或同槽下一招含控制 2 → 置 `m_switch` 切招，无视 sorder（挣脱、闪避靠这条） |

`isPriority(next, cur)`：任一方 `sorder = −1`，或 `next.sorder ≥ cur.sorder`。判定函数在 `skill::rules::canCancel`（普通窗）与 `isSuperPriority`（至尊窗，同槽还需 `next != cur` 且 `sorderControl` 含 2），窗口数值只来自配置。

### 6.4 角色状态机（P-3）

`RoleStateComponent.m_state` 取值：`Idle, Walk, Run, Patrol, Chase, Alert, Finding, Jostled, Attack, Hit, HitSwitch, HitUp, HitDown, HitFloor, GetUp, Wake, Death, Revive, Puppet`。旧位掩码由 `role::statusBits(state)` 派生（例：Run → `Run|Idle`，Hit → `Idle|Hit`），供 SkillAi 的 `opp_status/self_status` 与 Buff 条件使用，取值与旧实现逐一一致。

`RoleFsmSystem` 是唯一的转移者。每帧对未暂停、未休眠的角色按优先级（R14）取第一条成立的转移：

| 优先级 | 来源 | 转移 |
|---|---|---|
| 1 | `HealthComponent.m_dead` 或死亡请求 | 非受击族 → Death；受击族在自然出口转入（§6.5） |
| 2 | 复活请求（导演置 `m_reviveRequested`） | Death → Revive → Idle |
| 3 | `HitStateComponent` 待处理受击请求 | 非受击族 → Hit（已在 Combat 打断施法）；受击族 → 追击规则（§6.5） |
| 4 | 受击族自然推进 | §6.5 |
| 5 | 挣脱：可挣脱状态下开始了 L 槽施法 | → Attack（清位移与硬直） |
| 6 | `SkillCastComponent` 有当前施法 | Idle/移动 → Attack；Attack 内切招 → 退出旧施法、进入新施法 |
| 7 | 施法结束（TimelineSystem 同帧经 `role::rules::exitAttack`，见 `sim/role/RoleFsmRules.*`） | Attack → Idle（有移动意图且 `nextRun` → Run） |
| 8 | 传送门推开 | → Jostled |
| 9 | 移动意图（仅从 Idle/移动状态） | → Run / Walk / Patrol / Chase / Alert / Finding；停止意图 → Idle |

进入/退出动作：

| 状态 | 进入 | 退出 |
|---|---|---|
| Attack | `skill::rules::beginCast` | `skill::rules::endCast` |
| Hit 族 | 播放动作；启动受击位移；触发 BehaviorStateStart | 触发 BehaviorStateEnd |
| GetUp | 英雄：挂 Buff 156（PVP 起身保护）、154 | — |
| Wake | 英雄：挂 Buff 201（无敌）；恢复浮空值；重置受击保护 | — |
| Death | 死亡位移、清技能、通知变身组；死亡动作结束后 Despawn。位移按进入路径决定：HitSwitch/HitFloor 进死亡不套 `deathDisplacementId` | — |
| Idle / 移动 | 播放 stand / walk / run | — |

实现约定（S1 后补充）：
- 所有转移经 FSM 内唯一的 `transition()`：写 `m_state/m_prev/m_stateMs` 并发 `StateEvent`。每个状态一组 `enterXxx/updateXxx` 函数，不写成内联 if 链。其他系统只写请求位（`HitStateComponent` 待处理受击、`m_deathRequested`、`m_puppetRequested` 等），EntityFactory 只做初始化。
- 进入/退出动作本身是同步规则链（§4.6），受同样约束。
- Combat 在 P8 写受击请求，FSM 在下一帧 P4 才转移，比旧实现晚一帧；在 ±1 帧容差内，接受为设计差异，不列入 `known_deviations`。动画完成位同理：FSM 读上一帧 P6 的完成位（R10），HitSwitch 等“动画结束即转移”的状态因此也可能晚一帧。

S4 补充（以代码为准）：

- 受击族 `Hit/HitSwitch/HitUp/HitDown/HitFloor/GetUp/Wake` 已实现（转移表见 §6.5）。
- 英雄死亡不销毁实体：英雄停在 `Death`，交 RoomDirectorSystem 判复活（`m_reviveRequested` → Death→Revive→Idle）或失败；只有召唤物/怪物死亡动作结束后才 Despawn。
- 施法结束回 Idle/Run 由 TimelineSystem 在同帧检测片段放完并调用 `role::rules::exitAttack`（`sim/role/RoleFsmRules.*`），不是 FSM 自己读完成位。
- `hitType == −1`（无受击）：只计命中、播受击特效，不结算伤害/受击/顿帧/怒气（§6.6 第 6 步门控）。

### 6.5 受击状态机

输入均为上一帧的结果：动画完成位、位移事件位（brake/air/lie/floor）、硬直计时、浮空值是否耗尽、新的受击请求、死亡标志。`hitType`：0 击退、1 击倒、2 击飞、−1 无受击（只计命中/播受击特效，不结算伤害/受击/顿帧/怒气）。

| 当前 | 条件 | 下一状态 / 动作 |
|---|---|---|
| Hit | 追击：地面、type 0 | 重播 Hit |
| Hit | 追击：空中、type 0 | 使用 `air_displacement_id` |
| Hit | 追击：type 1/2 | HitUp |
| Hit | 位移 air（y>0 且下落）且动画未完 | HitDown |
| Hit | type∈{1,2}、动画完、角色 `hit_displacement_id = −1` | HitUp |
| Hit | 动画完、位移 brake、计时 ≥ 硬直 | HitSwitch |
| HitSwitch | 动画完 | Idle（已死 → Death） |
| HitSwitch | 新命中 | Hit |
| HitUp | 位移 air/lie 且动画完 | HitDown |
| HitUp | 追击：type 0 → air 位移；type 1 → HitDown；type≠1 → 扣浮空值 | — |
| HitDown | 位移 lie 且动画完 | HitFloor |
| HitDown | 追击：type 0/2 且浮空值未耗尽 | HitUp |
| HitFloor | 动画完且位移 brake/lie 后开始计时，计时 ≥ 硬直 | GetUp（已死 → Death） |
| HitFloor | 追击：type 1 → HitDown；type 0/2 且浮空值未耗尽 → HitUp（都重设硬直） | — |
| GetUp | 动画完 | Wake（已死 → Death） |
| Wake | 立即 | Idle |
| 任一受击态或 Attack | 英雄受击保护已触发 | HitDown |

数值规则（移植自旧实现 `HitStates`）：硬直 = `stiff_time × (1 + AddFloorTime)`，角色 `hit_stiff_time ≠ −1` 时覆盖；浮空值耗尽后每次读取硬直递减 `fatigue × 受击计数`；扣浮空值后每次受击 `vy −= weight × 受击计数`。浮空值耗尽时落地策略改为“停止不反弹”：HitReactionSystem 在 P4 写 `DisplacementComponent::m_noFloorBounce`，DisplacementSystem 在 P5 当帧读取生效。

### 6.6 命中微管线

CombatSystem 对每个 `ContactEvent`（Began/Continue，特效 → 角色）依次执行；任一步“结束”则处理下一个接触：

| 步 | 内容 | 随机 / 同步触发 |
|---|---|---|
| 1 | 门控：特效与目标未 `DespawningComponent`、主人存在、目标可受击（未死或允许鞭尸、`m_beHitEnabled`）；`hit_target` 与阵营（敌 / 友 / 双方 / 友方也命中）；命中间隔与次数；`hit_interval = −1` 时按目标去重并限制目标数；`hit_condition`（不打倒地 / 不打浮空）；GetUp、Wake 无敌窗口 | — |
| 2 | 更新 `HitTrackerComponent`（次数 +1、记录目标） | — |
| 3 | 目标无敌且非必中且 `hit_extra_control[1] = 0` → 若在闪避技能中则 Evade；结束 | 触发 EvadeSuccess |
| 4 | 目标正在放闪避（F 槽，独立于无敌门）→ Evade；结束 | 触发 EvadeSuccess |
| 5 | 属性闪避（非必中）：`rate = clamp((dodge + AddDodge − hit − AddHit)/100, 0, 0.5)` | 抽样 Dodge；成功触发 AttackMiss，结束 |
| 6 | 受击方 BeforeToBeHit；`hit_type = −1` → 不进入伤害阶段（仍生成受击特效） | 触发 BeforeToBeHit |
| 7 | 攻击方 BeforeHit 与命中类能力 | 触发 BeforeHit |
| 8 | 伤害公式（移植旧实现 `Damage.h`）：攻防类型 → PVP 攻防缩放 → `standHurt × atk/(atk+def) × hurt_rate × (1+addition)` → 暴击 → `(1+AddHurt)(1−AvoidHurt)` → 神器/能力来源 ×`(1+AddArtifactHit)` → 浮动 → 下限 0、取整 | 抽样 Crit、Float |
| 9 | 扣血（`health::rules`）：伤害许可 `hit_restrain` → 护盾（消耗倍率、层数，破裂触发 ShieldClean）→ 锁血 → 写 hp；hp≤0 → BeforeDeath → 仍死亡则 `m_dead` 与 `DeathRequest`（R6） | 触发 ShieldClean / BeforeDeath / HpChange |
| 10 | 受击反应：霸体 → 只做受击保护（落地且已死 → 死亡请求）；否则写顿帧（攻击者、特效覆盖写；受击者经延迟写入，R13）、受击者朝向攻击来源、硬直；已在受击族 → 写追击请求（浮空值耗尽时受击计数 +1）；已死 → 死亡请求；否则 `forceInterrupt` + 受击请求 | — |
| 11 | 怒气与伙伴怒气结算：命中后把施法管线待领取的伙伴怒气 grant 到攻击方 `RageStateComponent`（`m_rage` 封顶）；受击命中数（autoRelease 2/3）回收不在本步，改在 P4 特效遍末尾统一执行 | — |
| 12 | 受击方 AfterToBeHit（伤害数字、受击回 EP、能力）；攻击方 AfterHit（命中回 EP，Boss ×1.5；连击经验 `combo_exp × ComboExpScale`） | 触发 AfterToBeHit / AfterHit |
| 13 | 特效 `debuff_id` 给目标（目标不在闪避时），同步准入 | — |
| 14 | 受击特效 `hit_effect_ids` → `SpawnRequest`（同目标身上已有同 id 则复用） | — |
| 15 | 发出 `HitEvent`、`CueEvent`（按受击者 `sound_type` 选音效） | — |

友方接触（`hit_target` 为友方类）：`buff_id`（打到自己）/ `buff_all_id`（打到队友）同步准入，并对友方触发 BeforeHit/AfterHit 与 `specialability_id`/`specialability_all_id`。障碍物与水晶各走 ObstacleSystem / 水晶一次性命中逻辑。

### 6.7 位移

`DisplacementDef` 直接沿用 `displacement.lua`（已是干净的表）。`DisplacementSystem` 逐字移植旧实现 `DisplacementComponent.cpp` 的积分（x/z 梯形积分、加速段、`velocity_time = −1` 持续、y 轴重力、z 乘 ZRATE 0.55、反弹阈值、撞墙按位移大的轴先检测、追踪位移），差别只有两点：

1. 回调改数据：片段进入时 `enterClip` 把 `DisplacementComponent.m_{floorPolicy, wallPolicy, endOnBrake}` 写成片段的位移策略，系统按策略当帧处理（R11）：

| 事件 | 默认处理 | 策略选项 |
|---|---|---|
| brake（三轴速度归零） | — | `endOnBrake`：结束片段（loops = −1 的动作） |
| air（y>0 且开始下落） | — | 只置事件位 |
| floor（落地且下落速度大于阈值、可反弹） | 反弹 `vy = −vy × bounces` | `floorPolicy ≠ Default`（EndClip/Stop）：清速度不反弹 |
| lie（落地静止） | 清速度、y 置 0 | `EndClip` |
| wall（左/右/上/下边界） | 贴墙 | `Pass`（obstruct 0：跳过地图边界钳制且不置 `m_wall`，弹道可飞出可行走区域）、`StopAtWall`（−1）、`EndClip`（1，并记录阻挡） |

   落地/刹停/撞墙的 EndClip 由事件位当帧写入、时间轴下一帧消费（R11）。片段退出（`exitClip`）清策略；空中退出只清速度、开默认重力，保留活动曲线等落地再发 `disp-`（避免 y>0 时提前结束）。

2. 移速倍率由 `StatsComponent(MoveScaleX/Z)` 与 `canMove()` 派生；不能移动时只影响 x/z，y 照常下落。

位移控制（片段 `control`，由 TimelineSystem 在片段进行时写控制速度/朝向）：FreeMove 用 `control_velocity` 按摇杆写控制速度（此时片段不应配置位移）；TurnOnly 只改朝向；MoveNoTurn 可移动不改朝向；SteerAtEnter 在片段进入时按摇杆设方向与朝向（先于设位移，使曲线按新朝向积分）。Locomotion 在 Idle 态只清水平速度、保留 y 交给重力，避免每帧抹掉落地速度。

### 6.8 特效实体

`EffectDef`（由 `effect.lua` 降级，14495 行）：

| 字段组 | 内容 |
|---|---|
| 片段 | `m_clips[]`（`action_ids`，几乎都是 1 个） |
| 放置 | `position_type`、`relative_position`、`effect_oriebtation_X/Z`、生成时的自定义方向 |
| 跟随 | `follow`：0 不跟随、1 跟随主人、2 跟随目标（`FollowComponent` 组件） |
| 寿命 `LifetimeComponent` | `auto_release`：0 = 动作片段退出时销毁；1 = 自身片段放完销毁，主人销毁时一并销毁；2/3 = 命中目标数达到 `hit_counts` 后销毁（在特效遍末尾回收，命中写进 `hitTargets` 的下一帧；2 关闭后继、3 保留后继） |
| 命中 | `hit_id` → `HitDef`（由 `skill_hit.lua` 降级）、`hit_target`、`hit_extra_control`、`energy`、`combo_exp` |
| 命中附带 | `buff_id` / `buff_all_id` / `debuff_id`、`specialability_id` / `_all_id`、`hit_effect_ids` |
| 后继 | `next_effect_id`（4893 行使用） |
| 其他 | `control = 1` 可控特效（62 行，玩家输入改由特效接收）——首批范围外，按需实现 |

`HitDef`：反应（type、三种位移、硬直、浮空值）、顿帧（时长、作用于攻击者/特效、受击者延迟）、门控（间隔、次数、必中、命中条件）、伤害（攻防类型、PVE/PVP 倍率）。

特效生命周期：`SpawnRequest`（带生成时已解析的位置/朝向/向量）→ SpawnFlushSystem → 角色时间轴出生的特效本帧即跑特效遍/位移/动画（不再 deferAdvance；后继/受击/生命周期特效在特效遍之后才实体化，首次推进自然落在下一帧，见 R3）→ 片段放完 / 命中数达标 / 主人消失 → 打 `DespawningComponent` + `DespawnRequest`；后继特效不改在片段结束生成，由 DespawnSystem（P9）按 `m_next` 且非撞墙 EndClip 时派生 → 帧尾回收。

### 6.9 召唤与攻击变身

**召唤**：特效片段的 `Summon` cue → `SpawnRequest(kind = Role)`，新角色带 `SummonComponent{owner, root, remainingMs}`，阵营/等级与主人相同，出生前按 `attribute_rate` 继承主人属性、继承 `inherit` 含 1 的 Buff，开启 AI。寿命到期由 VitalsTick 置 hp=0 并提请死亡。标准伤害递归取最终主人（`m_root`）。

**攻击变身**（旧实现未实现，按参照工程实现；数据 44 行，41 行 type 0；列为最小实现）：

| 步骤 | 内容 |
|---|---|
| 触发 | 片段时间到达 `transform_frame`、角色未死、本次片段未触发过 → `TransformRequest{entity, transform_id, type}` |
| 生成 | TransformSystem 建 `transform_id` 对应角色（同阵营、同等级、同 AI、继承掉落与房间血量系数），出生阶段 Lifecycle（下一帧行动） |
| 继承 | 位置、起点、朝向、向量；type 0 时生命上限与当前生命继承；`inherit` 含 2 的 Buff 继承（`life::rules::inheritBuffs`） |
| 旧形态 | 加 `InactiveComponent`（不参与任何系统、不可被选为目标），从 `PlayerSlots`/AI 目标中替换为新形态 |
| 链接 | 所有形态共享 `TransformLinkComponent`；活跃形态死亡 → 其余形态同时死亡（死亡同步）；任一形态被销毁 → 其余形态同时销毁（DespawnSystem 对组内成员级联） |
| 表现 | `SpawnEvent` + `CueEvent(TransformSwap)`，表现层据此切换显示节点 |

参照工程在动作进入时预创建变身实体（性能优化），新 sim 在触发时创建，语义相同。最小实现有意不覆盖：动作文件（motionFile）外观切换、技能组/变化、摇杆方向列（见 §10 S3）。

---

## 7. AI（规则表 → IntentComponent）

保留旧实现的规则表方式，唯一变化是输出：动作不再调用 `changeStatus`，而是写 `IntentComponent`，玩家与 AI 走完全相同的施法/移动路径。

| 部分 | 设计 |
|---|---|
| 组件 | `AiBrain`：警戒、上次动作、规则冷却[8]、各槽施放间隔（初值 `skill_priority_level_cd`）、仇恨计时、目标、正在执行的移动规则。伤害记录（按首次命中排序）放在 Combat 拥有的 `HitStateComponent`，AI 只读 |
| 选目标 | 嘲讽优先（`TauntTableComponent` 最高权重，同权取最早）；未警戒取索敌范围内目标；警戒后目标失效从候选中随机重选（RNG） |
| 事实 | `gather()` 生成只读 `AiFacts`（警戒、被激怒、有目标、目标在视野、目标在追击范围外、技能就绪、上次动作、生命百分比、移动中规则、是否攻击中） |
| 规则 | `kMonsterRules`（8 行）不变；`pickRule` 返回第一条命中且不在冷却的行；攻击中不决策；移动中只允许更高优先级抢占；`ChancePercent` 必须是一行的最后一项 |
| 动作 → IntentComponent | `CastWakeSkill`（X 槽）、`CastIdleSkill`（Z 槽）、`CastBestSkill`（按 `skill_priority_level` 从高到低挑间隔已到的槽）→ `IntentComponent.m_buttons += {slot}` 且 `m_autoCast = true`（SkillAi 闸门生效）；`Chase` / `Patrol` / `Wander` → `IntentComponent.m_move = MoveTo{pos, kind}`（巡逻、游走偏移与停留时长按 X、Z 顺序抽样）；`Idle` → `Stop` |
| 施法后 | 同优先级的槽间隔一起重置；已有技能时按同槽续接连段 |
| 开关 | `WorldSetup.m_aiEnabled = false` 时跳过 AiSystem |
| 验收 | 不比较决策；要求开 AI 跑完关卡无断言、无“卡住超过 N 秒”、各槽技能都有施放、怪物会追击与巡逻 |

PVP 机器人、自动战斗、走向传送门不在范围内。

实现偏差（S4，以代码为准）：

- **施法间隔初始化**：`ensureBrain` 在首次决策时就用 `skill_priority_level_cd`（缺省 `skill_interval`）初始化各槽施放间隔并逐帧递减；旧实现 `m_skillIntervalMs` 为空时从空表找不到就绪槽，`skill_priority_level_cd > 0` 时实际永不施法。
- **AI 跳过玩家实体**：AiSystem 遍历时跳过带 `m_playerSlot` 的角色（与旧实现 `isAi=false` 一致），玩家由输入驱动，不参与怪物 AI。
- **掉落概率用原始随机流**：房间死亡掉落概率与抛射速度直接读 `RngState.m_random`（`randomF`），不经过 `rng::draw`，因此不参与 `DrawPolicy`（对齐旧实现 `RoomDirector::drop`）。

---

## 8. 回滚、快照、哈希、多实例、表现接口与服务器

### 8.1 快照与哈希

| 项 | 规则 |
|---|---|
| 快照内容 | `World::serialize` v4：版本号 + `WorldSetup` + 初始化标志 + `ECSManager::serialize()`（实体、按登记顺序的组件池、单例） |
| 不入快照 | `DrawPolicy`、外部追踪输出、进程级只读缓存（Config、avatar 资源）、规则库的临时寄存器 |
| 队列 | 进入快照但帧边界为空（§4.5）；`PresentationEvents` 含上一帧事件，与旧实现一致 |
| 哈希 | xxh64（快照字节），客户端与战斗服每 30 帧对账（沿用） |
| 克隆 | `World` 可拷贝；`RollbackSession` 环形保存每帧快照（沿用旧接口） |
| 版本 | 新 sim 快照版本 `kWorldSnapshotVersion`：2 → 3（新增 `SummonComponent`/`TransformLinkComponent` 与技能链/模式字段）→ 4（新增 `AiBrainComponent`/`ObstacleStateComponent`/`GoodsStateComponent`/`PortalStateComponent`/`DropTableComponent` 与 `IdentityComponent::m_aiDifficulty`、`HitStateComponent::m_hitType/m_enterPending`、`DisplacementComponent::m_noFloorBounce`）；`.mgrp` 升到 v5（头部仍是 `WorldSetup`，帧输入格式不变） |

### 8.2 确定性检查表（在 `mugen/AGENTS.md` 禁令之上）

- 唯一 RNG，只经 `rng::draw(world, DrawKind)`；`DrawKind` = Dodge、Crit、Float、BuffProbability、SkillAiProb、AbilityProb、Choice、AiTarget、AiWander。
- 事件队列、修正栈、实例表、`ScheduledActions` 都是 `std::vector`，顺序 = 写入顺序；`ScheduledActions` 按（到期 ms，登记序号）执行。
- 需要排序时显式给键（例如接触按创建序）；`std::map<Entity,…>` 只用于非序列化的调试映射。
- 所有时间与数值用 `Fixed`；配置里的帧数、毫秒在转换期转成整数或 `Fixed`。
- 系统只在同步点创建/回收实体（R2），保证创建顺序确定。
- `check_rules.ps1` 对 `sim/` 生效；新增规则：`sim/systems` 之间禁止互相 include（脚本检查 `*System.cpp` 的 include 目标）。

### 8.3 多实例

- 每场战斗的全部可变状态都在 `World` 内（组件、单例）；无静态可变量。
- `kSchedule`、op 解释器分派表、谓词表都是 `constexpr` 只读表。
- 进程级共享只允许只读数据：`Config::instance()`、avatar 资源缓存、动作时长缓存（如需要，首次访问时构建且只读）。
- 战斗服同一进程内多个 `World` 互不影响；回滚会话持有自己的 `World` 与快照环。

### 8.4 表现接口（P-7）

| 接口 | 内容 | 使用者 |
|---|---|---|
| `PresentationEvent` 队列 | 载荷沿用旧版（Sound、CameraShake、DisplaySpine、HurtNumber、Ghost、Bubble、Tips、RoomStateChanged、Victory、Failure、RoleDeath、RoleIntensity、WhiteFrame、PartnerRageChanged、BuffSpine、RoomWarning、PvpRoundEnd），新增 EntitySpawned、EntityDespawned、TransformSwap、StealthView、SuperArmorBorder | `WorldPresenter`、`RollbackSession` 去重 |
| `view::WorldView` | 遍历可视实体：`{entity, kind, configId, position, towardX, scale, motion{file, name, entry, timeMs, rate, loop}, flags(隐身/休眠/白框), appearanceKey}` | `WorldPresenter` |
| `view::HudView` | 玩家 HUD：生命/MP/EP/护盾、技能槽（CD、次数、禁用、可施放）、Buff 图标（id、层数、剩余时间）、连击与评价、怒气 | `BattleHudPanel`、`SkillLocalState` |
| `view::DebugView` | 碰撞框、地图范围、状态名 | `BattleDebugPanel` |

规则：表现层只读，不写模拟状态；回滚后的位置平滑（`PoseTrack`）仍在 `render/`，不进 sim。

### 8.5 网络、回滚与战斗服

- `FrameInput`、`PlayerInput`、`PlayerCommand`、`SystemCommand` 字段布局不变（P-8）；新 sim 的 `PlayerSetup` 去掉 `m_class`，职业由 `roleId` 推出。`NetCodec` 字节不变，协议不变。
- `sim/net/LockstepAuthority`、`NetBattleSync`、`sim/rollback/RollbackSession` 对外接口保持旧版不变，内部只依赖新 `World`。
- 联机必须两端同一套 sim：客户端联网模式与战斗服在切片 7 同时切换；此前联网对局两端都用旧实现。
- 战斗服切换 = CMake 改链接 `mugen_sim`，并把 include/命名空间从 `mugen::sim_legacy` 改回 `mugen::sim`；`BattleRoom` 逻辑不变。
- 回归：`net_duel_sync_test`、`NetBattleSyncTest`、`RollbackTest`、`LockstepLoopbackTest` 改为驱动新 sim 后全部通过；客户端/服务器哈希对账一致。

### 8.6 城镇模式

`TownScreen` / `TownSync` 也使用 `sim::World`（Town 模式：本地角色移动 + 远端木偶）。删除旧实现前必须在新 sim 中支持 Town 模式：`WorldMode::Town`、`Puppet` 组件、InputSystem 应用木偶更新、Locomotion、动画循环推进（见切片 5）。

---

## 9. 配置转换

### 9.1 新运行时表（`conf/battle/`，命名空间 `mugen::conf::battle`）

| 新表 | 源表（`AxmolFighter-Config/table`） | 降级要点 |
|---|---|---|
| `BuffDef` | `buff.lua` + `buff_rule.lua` | 规则类 → op 列表（§5.8）；条件编号 → 谓词（§5.6）；began/ended → `TriggerPoint`；参数下标改为具名字段 |
| `AbilityDef` | `special_ability.lua` | 事件/条件 → 触发点/谓词；`effect_id`、`bind_buff_id` → op（§5.10） |
| `SkillDef` | `skill.lua` | §6.1；丢弃数据中未使用的抬起释放字段 |
| `ClipDef` | `action.lua`、`action_effect.lua` | §6.2；`effect_ids/effect_frames`、`camera_id/camera_frame` 等平行数组合并为按帧排序的 cue 列表 |
| `EffectDef` | `effect.lua` | §6.8 |
| `HitDef` | `skill_hit.lua` | 字段原样，单位统一 |
| `RoleDef` | `role.lua`（+ `attribute.lua` 引用） | 物理参数、受击覆盖、阵营、死亡配置、默认 Buff、AI 引用；怪物按等级的属性插值仍在运行时做（等级来自房间） |
| `AiDef` | `ai.lua` + `skill_ai.lua` | 技能组/槽、优先级、施放间隔、索敌/追击/巡逻范围、SkillAi 闸门 |
| `DisplacementDef`、`CameraShakeDef` | `displacement.lua`、`camera.lua` | 原样 |
| 房间、关卡、地图、外观、物品、音效等 | 不变 | 共享表，不降级 |

单位约定：cue 与取消窗口用整数逻辑帧；毫秒字段用整数；Buff 周期用 `Fixed` 秒；比例用 `Fixed`。

### 9.2 转换流水线

```text
读取（严格，沿用现有 LuaReader 与反射结构）
  → 降级 lower/（新增：BuffLower、AbilityLower、SkillLower、ClipLower、EffectLower、HitLower、RoleLower、AiLower）
  → 校验（引用存在、平行数组等长、触发点与条件编号已知、op 参数个数）
  → 打包 config.bin
  → 输出 lower_report.txt
```

`lower_report.txt` 按规则类列出：行数、成功降级行数、◐/✖ 行（含 buff id）、告警（例：触发点 8 已废弃、ruleId −1）。CI 中出现“未知规则类/未知条件编号/未知触发点”即失败。降级代码位于 `AxmolFighter-Tools/config_converter/lower/`，与运行时共享 `conf/battle` 结构体。

### 9.3 共存期

- 仍是单一 `config.bin`、单一登记表 `MG_CONFIG_TABLES`：旧战斗表（供旧实现与导出工具）+ `conf::battle` 新表 + 共享表。`run_config_converter.ps1` 一次产出全部，schema hash 覆盖全部。
- 旧实现与导出工具只读旧表；新 sim 只读新表与共享表。
- 迁移期间每次改动战斗 Lua 表，都要重新导出 golden（建议 CI 在转换后自动导出，见 §11 Q9）。

### 9.4 终态

删除旧实现时，参照形状的战斗表结构迁到 `AxmolFighter-Tools/config_converter/source/`（登记为 `MG_SOURCE_TABLES`，只用于读取与降级，不进 `config.bin`）；运行时登记表只剩 `conf::battle` 与共享表。策划仍编辑参照形状的 Lua 表，转换器负责降级。

---

## 10. 迁移切片与验收标准

每个切片结束都更新 README 对应章节与 `mugen/AGENTS.md`（mugen 架构变化）。

| 切片 | 内容 | 验收标准 |
|---|---|---|
| S0 脚手架 | 机械改名 `sim/` → `sim_legacy/`（目录、命名空间、include、CMake、全部使用方）；`core/ecs` 增补；新 sim 骨架（World、Schedule、Components、Singletons、Events、空系统）；附录 A 格式；导出工具（独立通道）与比对工具；场景清单 | 旧测试全绿且结果不变；空世界 1000 帧哈希稳定、快照往返一致；导出工具对至少 3 个现有场景产出摘要；比对工具自检（与自身比对通过，人为注入偏差时报错） |
| S1 垂直切片 1 | 一个 PVP 英雄（建议职业 1 / role 101）对木桩：出生、走跑、朝向、地图边界；A 普攻连段（技能 → 时间轴 → 特效 → 碰撞 → 伤害）；受击状态机（至少 Hit/HitSwitch；A 段若有击飞则含 HitUp→Wake）；顿帧；一个 Buff（建议动作片段霸体 BuffSuperArmor，或命中挂 BuffBurn） | 场景“职业 1 A 连段打木桩”在暴击 0%/100% 两种 DrawPolicy 下：cast/action/hit/stop/state 帧 ±1；每次伤害相等；Buff 加/移 ±1；位移终点在容差内。另：同输入两次运行哈希一致、快照往返一致、`check_rules` 通过 |
| S2 Buff 引擎 | 准入、生命周期、触发点、谓词、op 解释器、修正栈、特殊能力；转换器 `BuffDef`/`AbilityDef` 降级 | 9335 行全部降级或在报告中显式列出；每种 op 与谓词有单测；旧实现已移植的 79 类：每类至少一个 golden 场景比对通过；64 类新实现规则：按参照语义的单测（无 golden） |
| S3 技能与时间轴完整 ✅ | 首批英雄全部槽位（B/C/D、冲刺、闪避 F、挣脱 L、曝气、搓招、奥义、伙伴怒气）；取消窗口（普通/至尊）、sorder、次数/链/模式、消耗；特效全部寿命语义；召唤；攻击变身 | **已完成**：`class1_swordman`/`class2_ranger`/`class4_mage`/`copy_room_class1` 及全部 buff 场景 min/max 比对通过（34 场景 ×2 = 68 摘要全 PASS）；召唤/变身有专门单测（`SummonTest`/`TransformTest`，另有 `SkillCancelTest`/`SkillLineCTest`/`ClipControlTest`/`FollowTest`/`TimelineFloorEndClipTest`）。有意未做：技能组/变化、摇杆方向列、变身 motionFile 外观；首批 no-op：晶体、神器、可控特效 |
| S4 AI 与 PVE 关卡 ✅ | AiSystem；怪物；一个 PVE 关卡完整流程（房间状态、波次、障碍、物品、传送门、死亡、掉落、KO 慢放、复活） | **已完成**：`mugen_sim_tests` 236 通过、`mugen_tests` 375 通过；`compare-scenario --all` 72 个中 70 PASS（`copy_room_class1` min/max 两项为已知设计差异，见 `known_deviations.txt`）；`mugen_sim_cli run-ai-room 2010101 --frames 3000` 判 victory；`check_rules.ps1` 退出 0。遗留：角色挂起未实现（开场/警告/等复活；RoomDirectorSystem 在 Enter 态把玩家切回 stand 的临时处理待挂起实现后移除）；障碍死亡未走 deathDisplacement；defense_wave 只验收第 1 波；死亡/复活/失败由新 sim 单测覆盖（legacy 无法导出）；2071601 开 AI 因 1 级英雄打不过高级怪判负属预期 |
| S5 渲染接入 | `sim/view`；`WorldPresenter`、HUD、调试面板改用 view；`LocalBattleMode` 与城镇 Town 模式切到新 sim（编译开关 `MG_NEW_SIM`） | 离线战斗可玩；城镇移动与远端木偶正常；视觉冒烟检查清单（命中特效、飘字、Buff 特效、震屏、慢放、胜负界面）通过 |
| S6 网络与回滚 | `RollbackSession`、`LockstepAuthority`、`NetCodec`、`NetBattleSync` 迁入新 sim | `LockstepLoopbackTest`、`NetBattleSyncTest`、`RollbackTest` 用新 sim 通过；回滚重推结果与直跑逐帧哈希一致 |
| S7 战斗服切换 | 战斗服链接新 sim；客户端联网模式同时切换 | `net_duel_sync_test` 通过；真实联网对局 30 帧哈希对账无不一致 |
| S8 删除旧实现 | 打 tag `legacy-sim`；一次提交删除 `sim_legacy/`、旧测试、导出工具、`core/ecs/System.h`；旧战斗表结构迁入转换器 | 客户端、战斗服、工具、测试全部构建通过；README、AGENTS 更新 |

---

## 11. 开放问题（请评审）

> **评审结论（已定）**：Q1 接受 P-1 同步规则链；Q2 首批为 role 101–107 + 副本 101 / 房间 2010101（备选 2071501）；Q4 以参照语义为准，差异进入已知偏差清单，从未生效的两条规则不实现；Q6/Q7 神器、晶体、可控特效首批不做（op/谓词定义为空作用并在转换报告列出），曝气与伙伴怒气在首批；效果一致标准取宽松档（帧数据、Buff 时机均 ±1 帧）。其余问题（Q3、Q5、Q8–Q14）按“建议”列执行。P-1…P-9 全部采纳。

| 编号 | 问题 | 建议 |
|---|---|---|
| Q1 | 是否接受 P-1“同步规则链”例外（命中微管线与触发点同步执行），还是坚持纯队列、接受同帧多命中/叠层场景伤害与旧实现不再逐一相等？ | 接受 P-1 |
| Q2 | 首批内容：PVP 英雄是 role 101–104，还是含伙伴 105–107？PVE 关卡选哪个？ | 101–107 全部；关卡选已有回放与追踪基线的副本 101 / 房间 2010101（备选防守房 2071501） |
| Q3 | 位移容差取多少？DOT、治疗的数值是否也要求逐一相等？ | x/z/y 各 ≤ 2 逻辑单位；DOT/治疗也相等 |
| Q4 | 旧实现与参照工程不一致、或参照本身失效的规则按谁？DamageTypeGradation 数池对象（旧=自身，参照=目标）、AddByApplicator 周期门闩、Chill 禁疗、ModifyHPBasedByLostHp（参照无此类）、DeductEp/TpReplaceHp（参照从未生效） | 以参照语义为准，比对工具用“已知偏差清单”豁免；两个从未生效的规则先不实现、转换报告标出；ModifyHPBasedByLostHp 判为死规则 |
| Q5 | 吸血（Hemophagia）在参照工程由飞行血球到达时回血，新 sim 何时回血？ | 命中当帧立即回血 |
| Q6 | 神器（武装技能）是否进入首批 PVP 范围？它影响 5 类规则、252 行 BeforeArtifactSkill 脉冲、282 行 ArtifactJudge 条件、79 行 DamageHurt3 | 首批不做，相关 op/谓词先定义为空作用并在报告中列出 |
| Q7 | 晶体消耗、可控特效（`effect.control=1`，62 行）、PVP 中的曝气与伙伴怒气是否在首批范围？ | 曝气、怒气在；晶体、可控特效不在 |
| Q8 | golden 导出需要在冻结的旧实现里加一个最小的 `DrawPolicy` 钩子（固定暴击/浮动/闪避/概率抽样）。是否允许这次改动？ | 允许，视为“影响 golden 输出的修复” |
| Q9 | 迁移期间战斗 Lua 表是否冻结？ | 不冻结，CI 在转换后自动重导 golden |
| Q10 | 新表放在同一个 `config.bin`，还是单独 `battle.bin`？ | 同一个（P-9） |
| Q11 | `core/bt` 新 sim 不再使用，是否随旧实现删除？ | 保留到 S8 再定 |
| Q12 | 渲染接入后是否需要运行时切换新旧 sim（需同一二进制链接两套命名空间）？ | 不需要，编译开关即可 |
| Q13 | Buff 消失动画期间是否仍算生效？ | 逻辑立即失效，动画只在表现层 |
| Q14 | 是否接受 R18（特效与角色统一顿帧规则，比旧实现早一帧冻结）？ | 接受 |

本次调研发现的风险：

| 风险 | 影响 | 应对 |
|---|---|---|
| 城镇也在用 `sim::World`（`TownScreen`、`TownSync`，Town 模式 + 木偶） | 删除旧实现前必须覆盖，否则城镇不可用 | 纳入 S5 |
| `ECSManager` 组件上限 64（签名 `bitset<64>`） | 组件拆得过细会触顶 | 计划约 37 个；超限前先合并 |
| 旧实现 `AddByState` 是空函数、`DamageTypeGradation` 与参照不一致等 | “已移植”不等于“正确”，golden 会把旧缺陷固化 | 已知偏差清单（附录 A）+ 新实现按参照语义 |
| 实体创建时机影响帧语义 | 不复刻“组内创建”时 `next_effect` 链每代 +1 帧，超出容差 | R3 出生门控 |
| 追踪标识 `role.<id>.<生成序号>` 依赖全局创建顺序 | 新 sim 内部多建/少建一个实体就全部错位 | 摘要改用“配置 id + 同配置序号”的语义键（附录 A） |
| 联机两端必须同一套 sim | S6 期间无法用新 sim 联机 | S6 只做回环测试，S7 两端同时切换 |
| UI 直接 include sim 内部头（`Skill.h`、`BuffPool.h`、`SkillCaster.h`） | 组件改名即破坏 UI | P-7 `sim/view` |
| 参照工程中有名字不匹配导致从未生效的规则（扣 EP/TP 代替扣血） | 按类实现会引入从未有过的行为 | Q4 |

---

## 附录 A：效果摘要（golden）格式契约

两条通道（旧实现导出、新 sim 比对）都按本格式输出，比对工具只依赖本格式。

### A.1 文件

- 位置：`TestsHeadless/data/golden_summaries/<场景>.<暴击策略>.summary`，UTF-8 文本，按帧升序。
- 头部：

```text
# mugen effect summary v1
# scenario class1_swordman room 2071501 seed 31 ai off
# draw crit=min float=mid dodge=never prob=pass choice=first
```

- 输入：沿用 `TestsHeadless/data/scenarios/*.script`。控制行仍是 `帧 槽位 控制事件 象限 角度 按下类型`。可另加 `frame buff` 注入行（见 A.6），不写入 `FrameInput`。两条通道读同一份脚本，注入语义必须一致。

### A.2 抽样策略 DrawPolicy

| 类别 | 策略 | 返回值（0–100 抽样） |
|---|---|---|
| Crit | `min` / `max` | 101（永不暴击）/ 0（必暴击） |
| Float | `mid` | 50（浮动系数 1.00） |
| Dodge | `never` | 100（永不属性闪避，闪避率上限 50%） |
| BuffProbability、SkillAiProb、AbilityProb | `pass` | 0（总是通过） |
| Choice | `first` | 选第一个候选 |
| AI 类 | 不固定 | golden 场景关闭 AI |

### A.3 实体键

`role:<roleId>#<n>`、`fx:<effectId>#<n>`，n 为本次运行中该配置 id 的第几个生成（从 1 起）。不使用实体句柄或全局生成序号。

### A.4 行类型

| 行 | 字段 | 比对规则 |
|---|---|---|
| `cast` | 帧 施法者 技能 id 槽位 | 帧 ±1 |
| `castend` | 帧 施法者 技能 id 原因（done/cancel/interrupt） | 帧 ±1 |
| `action` / `actionend` | 帧 施法者 动作 id | 帧 ±1 |
| `spawn` | 帧 主人 新实体键 | 帧 ±1 |
| `hit` | 帧 特效键 目标键 hit id 结果（hit/evade/miss/none） | 帧 ±1，结果相同 |
| `dmg` | 帧 攻击者 目标 特效 id 数值 暴击 | 帧 ±1，数值与暴击相同 |
| `hp` | 帧 实体 变化量 原因（dot/heal/cost/shield） | 帧 ±1，数值相同 |
| `stop` | 帧 实体 顿帧帧数 | 开始帧 ±1，帧数 ±1 |
| `state` | 帧 实体 旧状态 新状态 | 帧 ±1 |
| `buff+` / `buff-` | 帧 实体 buff id | 帧 ±1 |
| `disp+` / `disp-` | 帧 实体 位移 id [终点 x y z] | 帧 ±1，终点在容差内 |
| `death` | 帧 实体 | 帧 ±1 |
| `final` | 实体 hp x y z 状态 | hp 相同，位置在容差内 |
| `roomstate` | 帧 房间状态（状态机编号） | 帧 ±1，编号相同 |
| `victory` / `failure` | 帧 房间 id | 帧 ±1，id 相同 |
| `warning` | 帧 载荷 | 帧 ±1，载荷相同 |
| `wave` | 帧 波次序号 | 帧 ±1，序号相同 |
| `revive` | 帧 英雄实体键 | 帧 ±1，键相同 |
| `ko` | 帧 房间 id | 帧 ±1，id 相同 |
| `obstacle` | 帧 障碍键 break | 帧 ±1，键/事件相同 |
| `goods` | 帧 物品键 drop \| pickup | 帧 ±1，键/事件相同 |
| `portal` | 帧 门键 open \| transfer | 帧 ±1，键/事件相同 |

### A.5 比对算法

1. 按（行类型，主键）分组；同组内按出现顺序一一对齐。
2. 对齐后逐行检查上表规则；多出或缺失的行报错。
3. `TestsHeadless/data/golden_summaries/known_deviations.txt` 列出已知偏差（规则 id 或行类型 + 场景模式 + 原因，例如 R18、Chill 禁疗、DamageTypeGradation 数池对象），匹配的差异只告警不失败。
4. 输出：第一个差异的上下文（前后 5 行）+ 汇总计数。

### A.6 脚本 buff 注入（`frame buff`）

效果摘要脚本可以在控制行之外插入 buff 注入行。空行与 `#` 注释仍忽略。旧的六列控制行不变。本行不进 `FrameInput`，也不由回放 / `run-room` 执行（那些命令遇到本行应拒绝）。

```text
<frame> buff <target> <buffId>
```

- `frame`：逻辑帧，从 1 起，且不超过场景帧数。可与同一帧的控制行共存。
- `target`：`hero` | `dummy1` | `dummy2` | `all-dummies`。
- `buffId`：buff 表配置 id，正整数。

两条通道的语义：

1. 时机：在该帧 `tick` 之前施加。同一帧多行按脚本出现顺序依次施加，全部完成后再 `tick`。施加当下产生的追踪（`buff+`、`hp` 等）记在这一帧的摘要里。
2. 目标在施加当下按 ECS 创建顺序解析，不缓存：
   - `hero`：`playerSlot == 0` 的角色。
   - `dummy1` / `dummy2`：没有 `playerSlot`、且没有召唤者（`m_owner` 无效）的角色中，创建顺序的第 1 / 第 2 个。房间 2071501 即怪物 61300、61320。召唤物不占用这两个槽位。
   - `all-dummies`：上述全部角色，按创建顺序各施加一次。
   - 目标不存在：导出失败（英文错误），不静默跳过。
3. 施加：对每个目标调用 `BuffHolder::addBuff(buffId, context, world.setup().m_fightType)`。
   - `context.m_applicator` 始终是 hero（槽位 0），包括目标就是 hero 的自我施加。
   - 不设置 `m_target`、不绑定技能，其余上下文保持默认。与动作给自己挂 buff、以及命中把施加者记成攻击者的路径一致。
   - 准入失败（配置缺失、区域、免疫、冷却、概率）不报错，该帧没有对应 `buff+`。概率仍走 `DrawPolicy`（golden 为 `prob=pass`，抽样 0，概率大于 0 即通过）。

### A.7 场景脚本系统命令与清单 copy 列

效果摘要脚本还接受系统命令行（S4 房间流程需要；两条通道语义一致，写入本帧 `FrameInput.systemCommands`，与 `frame buff` 互不影响）：

```text
<frame> cmd <revive|return|warndone>
```

- `frame`：逻辑帧，从 1 起且不超过场景帧数。
- `revive` → `ReviveCommand`（复活请求，由 RoomDirectorSystem 判 `m_reviving` 且 `usedRevives < maxRevives` 时执行）；`return` → `ReturnToTownCommand`（回城）；`warndone` → `RoomWarningDone`（警告确认，推进 ReadySpine → Tip）。

场景清单 `scenarios.txt` 支持可选第 8 列 `copy`（副本配置 id）：`name script room class level seed frames [copy]`。`copy` 决定复活次数上限（`CopyConfig.maxRevive`）与怪物等级加成（`CopyConfig.addLevel`），缺省为 0。

---

## 附录 B：移植来源索引

| 新模块 | 旧实现参考（`sim_legacy/…`） | 参照文档章节 |
|---|---|---|
| 伤害公式 | `hit/Damage.h`、`hit/DamageCalculator.cpp` | 战斗逻辑 §9 |
| 命中微管线 | `hit/HitResolver.cpp`、`hit/HitSeams.cpp` | 战斗逻辑 §8.2–8.3、§10.1 |
| 碰撞 | `collision/Collision.cpp` | 战斗逻辑 §8.1 |
| 受击状态机 | `role/brain/states/HitStates.cpp` | 战斗逻辑 §10 |
| 施法与取消窗口 | `skill/SkillInput.cpp`、`skill/SkillCastComponent.cpp`、`attack/AttackRole.cpp`、`attack/ActionAttack.cpp` | 战斗逻辑 §5–§7 |
| 技能装配 | `skill/SkillLoadout.cpp`、`skill/SkillLoad.cpp` | 战斗逻辑 §6.1，战斗配置 §8 |
| SkillAi | `skill/SkillAi.cpp` | 战斗逻辑 §6.5 |
| 位移 | `motion/DisplacementComponent.cpp`、`motion/MapRegion.cpp` | 战斗逻辑 §12 |
| Buff 生命周期与准入 | `buff/BuffLifecycle.h`、`buff/BuffPool.cpp`、`buff/BuffInstance.cpp` | 战斗逻辑 §11 |
| Buff 条件 | `buff/BuffCondition.cpp` | 战斗配置 §3.2 |
| 已移植 Buff 规则 | `buff/BuffRules.cpp` | — |
| 特殊能力 | `ability/AbilityPool.cpp` | 战斗配置 §3.4 |
| 特效 | `effect/Effect.cpp`、`spawn/RoleSpawn.cpp` | 战斗逻辑 §7.4–§7.6、§8.4 |
| 连击与评价 | `role/RoleCombo.cpp` | — |
| AI | `ai/AiAgent.cpp`、`ai/AiRules.*` | AI 与行为树文档 §B |
| 房间与 PVP | `room/RoomDirector.cpp`、`room/RoomEntities.cpp` | 战斗逻辑 §2 |
| 输入 | `input/ControlManagerCore.cpp` | 战斗逻辑 §5.1 |
| 网络与回滚 | `net/*`、`rollback/*` | 根 README“战斗同步模型” |
| 追踪 | `trace/TraceSink.cpp` | — |
