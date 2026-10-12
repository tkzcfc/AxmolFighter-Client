#include "BuffPresentation.h"
#include "BuffInstance.h"
#include "mugen/sim/event/LogicTimers.h"
#include "mugen/sim/World.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/avatar/AvatarPaths.h"
#include "mugen/avatar/data/AvatarAssetCache.h"

namespace mugen::sim::buff
{
// 定时器载荷使用稳定序号匹配，不依赖组件数组地址。
void cancelAppearance(World& world, Entity entity, uint64_t serial)
{
    auto& timers = world.ecs().singleton<LogicTimers>().entries;
    std::erase_if(timers, [entity, serial](const LogicTimer& timer) {
        const auto* payload = std::get_if<BuffContinue>(&timer.payload);
        return timer.owner == entity && payload && payload->m_serial == serial;
    });
}

// 只在实例仍处于活动生命周期时发出持续阶段。
void continuePresentation(World& world, Entity entity, uint64_t serial)
{
    const auto* instance = findInstance(world, entity, serial);
    if (!instance || instance->m_lifecycle.m_exit || instance->m_lifecycle.m_destroy) return;
    const auto* row = conf::Config::instance().find<conf::BuffConfig>(instance->m_id);
    if (row && row->spineId != -1 && row->spineStep.size() > 1 && row->spineStep[1] == 1)
        world.emit(entity, BuffSpine{serial, row->spineId, 2, row->id});
}

// 出现时长来自逻辑动作；无法解析时表现层自行播放，不猜测时长。
void beginPresentation(World& world, Entity entity, uint64_t serial)
{
    cancelAppearance(world, entity, serial);
    const auto* instance = findInstance(world, entity, serial);
    if (!instance) return;
    const auto* row = conf::Config::instance().find<conf::BuffConfig>(instance->m_id);
    if (!row || row->spineId == -1) return;
    const bool appear = !row->spineStep.empty() && row->spineStep[0] == 1;
    const bool continuing = row->spineStep.size() > 1 && row->spineStep[1] == 1;
    world.emit(entity, BuffSpine{serial, row->spineId, appear ? 1 : continuing ? 2 : -1, row->id});
    if (!appear || !continuing) return;
    const auto* spine = conf::Config::instance().find<conf::ResSpineConfig>(row->spineId);
    const auto* map = spine ? AvatarAssetCache::getInstance()->getMotionMap(
        AvatarPaths::motionFileFromSpine(spine->fileName)) : nullptr;
    const auto* motion = map ? map->findMotion("appear") : nullptr;
    if (!motion || motion->clipCount() == 0) return;
    if (motion->durationMs() <= 0) continuePresentation(world, entity, serial);
    else world.ecs().singleton<LogicTimers>().add(entity, Fixed::fromInt(motion->durationMs()), false, BuffContinue{serial});
}

// 只读已加载的逻辑动作资源，不访问 Spine 渲染后端。
std::optional<Fixed> disappearDuration(int32_t spineId)
{
    const auto* row = conf::Config::instance().find<conf::ResSpineConfig>(spineId);
    if (!row) return std::nullopt;
    const auto path = AvatarPaths::motionFileFromSpine(row->fileName);
    const auto* map = AvatarAssetCache::getInstance()->getMotionMap(path);
    const auto* motion = map ? map->findMotion("disappear") : nullptr;
    if (!motion || motion->clipCount() == 0) return std::nullopt;
    return Fixed::fromInt(motion->durationMs());
}

// 在最后一刻才运行退出钩子，定时器不直接删除实体。
void finishExit(World& world, Entity entity, uint64_t serial)
{
    auto* instance = findInstance(world, entity, serial);
    if (!instance || !instance->m_lifecycle.m_exit || instance->m_lifecycle.m_destroy) return;
    const auto* config = conf::Config::instance().find<conf::BuffConfig>(instance->m_id);
    instance->m_lifecycle.m_destroy = true;
    invokeRule(world, entity, serial, RuleHook::Exit);
    if (config && config->spineId != -1) world.emit(entity, BuffSpine{serial, config->spineId, 0, config->id});
}

// 消失动画只有可解析的逻辑时长才延迟退出；缺失资源按场次去重记录。
void exitInstance(World& world, Entity entity, uint64_t serial)
{
    auto* instance = findInstance(world, entity, serial);
    if (!instance || instance->m_lifecycle.m_exit) return;
    cancelAppearance(world, entity, serial);
    instance->m_lifecycle.m_exit = true;
    const auto* config = conf::Config::instance().find<conf::BuffConfig>(instance->m_id);
    if (config && config->spineId != -1 && config->spineStep.size() >= 3 && config->spineStep[2] == 1)
    {
        world.emit(entity, BuffSpine{serial, config->spineId, 3, config->id});
        const auto duration = disappearDuration(config->spineId);
        if (duration && *duration > Fixed{})
        {
            world.ecs().singleton<LogicTimers>().add(entity, *duration, false, BuffExit{serial});
            return;
        }
        if (!duration)
        {
            auto& logged = world.ecs().singleton<RoomState>().m_missingBuffMotionIds;
            if (std::find(logged.begin(), logged.end(), config->id) == logged.end())
            {
                logged.push_back(config->id);
                MG_LOG_W("Buff disappearance motion missing: buff={} spine={}", config->id, config->spineId);
            }
        }
    }
    finishExit(world, entity, serial);
}
}
