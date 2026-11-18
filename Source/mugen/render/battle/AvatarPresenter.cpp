#include "AvatarPresenter.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/avatar/AvatarPaths.h"
#    include "mugen/conf/ReflectedConfig.h"
#    include "mugen/conf/TypeConversions.h"
#    include "ui/progress/SessionFashionAppearance.h"

#    include <algorithm>
#    include <cmath>

NS_MG_BEGIN

namespace
{
float renderValue(Fixed value)
{
    return value.toFloatForRender();
}

// 实体外观配置：Spine 资源、绘制层级、Spine 相对偏移和挂件职业。
struct AvatarLook
{
    int32_t roleId     = 0;
    int32_t resSpineId = 0;
    int32_t tier       = 0;
    std::vector<int32_t> spineOffset;
    CharacterClass characterClass = CharacterClass::kUnknown;
};

// 从实体配置行读取共有的外观字段。
template <class Row>
AvatarLook lookFromRow(const Row& row)
{
    AvatarLook look;
    look.resSpineId  = row.resSpineId;
    look.tier        = row.tier;
    look.spineOffset = row.spineRelativePosition;
    return look;
}

// 按可视种类与配置 id 找到对应配置行；各组的配置 id 均已在生成时校验。
AvatarLook resolveLook(sim::VisualEntityKind kind, int32_t configId)
{
    const auto& config = conf::Config::instance();
    switch (kind)
    {
    case sim::VisualEntityKind::Role:
    {
        AvatarLook look     = lookFromRow(config.get<conf::battle::RoleDef>(configId));
        look.roleId         = configId;
        look.characterClass = type_conversions::toCharacterClass(configId);
        return look;
    }
    case sim::VisualEntityKind::Effect:
        return lookFromRow(config.get<conf::battle::EffectDef>(configId));
    case sim::VisualEntityKind::Obstacle:
        return lookFromRow(config.get<conf::ObstacleConfig>(configId));
    case sim::VisualEntityKind::Goods:
        return lookFromRow(config.get<conf::GoodsConfig>(configId));
    case sim::VisualEntityKind::Portal:
        return lookFromRow(config.get<conf::PortalConfig>(configId));
    }
    MG_LOG_E("AvatarPresenter: invalid visual entity kind {}", static_cast<int>(kind));
    MG_ASSERT(false && "Invalid visual entity kind for avatar");
    return {};
}

// 按外观配置创建 Avatar，动作文件与逻辑侧绑定的同一份。
// 职业角色按职业默认外观创建：时装图集与皮肤、默认武器挂件（待机时显示，其余动作嫁接到身体插槽）。
// 本地玩家用会话时装/武器外观（可后续 setAppearance 换装）。
Avatar* createAvatar(const AvatarLook& look,
                     const SpineInfo& spine,
                     const std::string& motionFile,
                     bool useSessionFashion,
                     bool transformed)
{
    if (look.characterClass != CharacterClass::kUnknown)
    {
        FashionAppearance appearance;
        if (useSessionFashion)
        {
            appearance = gameui::buildSessionFashionAppearance();
            if (appearance.roleId == 0)
                appearance.roleId = look.roleId;
        }
        else
            appearance.roleId = look.roleId;
        appearance.transformed = transformed;
        return Avatar::create(appearance);
    }
    AvatarDesc desc;
    desc.skeleton       = spine.skeleton;
    desc.atlases        = {spine.atlas};
    desc.scale          = spine.scale;
    desc.motionFile     = motionFile;
    desc.characterClass = look.characterClass;
    return Avatar::create(desc, AvatarLoadMode::kSyncShared);
}

// 读取偏移分量，配置允许省略尾部分量。
float offsetAxis(const std::vector<int32_t>& offset, size_t index)
{
    return index < offset.size() ? static_cast<float>(offset[index]) : 0.0f;
}

// 循环动作时间差折到最短方向，避免过环时被当成跳转。
int wrappedDrift(int shownMs, int targetMs, int durationMs)
{
    int drift = targetMs - shownMs;
    if (durationMs <= 0)
        return drift;
    drift %= durationMs;
    if (drift > durationMs / 2)
        drift -= durationMs;
    else if (drift < -(durationMs / 2))
        drift += durationMs;
    return drift;
}
}  // namespace

void AvatarPresenter::present(sim::World& world, float alpha, float renderDelta)
{
    const uint64_t frame = sim::view::worldFrame(world);
    const Fixed worldDtMs = sim::view::worldDtMs(world);
    const Fixed logicDt = sim::view::logicDtMs();
    std::set<uint32_t> live;
    sim::view::collectVisualEntities(world, m_visuals);
    for (const auto& v : m_visuals)
    {
        live.insert(v.entity.value);
        auto& state = m_avatars[v.entity.value];
        // 回滚恢复快照后同一句柄可能指向另一个实体：身份变化则整体重建显示。
        const LookIdentity identity{v.kind, v.configId, !v.motionFile.empty()};
        if (state.resolved && state.identity != identity)
            resetState(v.entity.value, state);
        if (!state.resolved)
        {
            state.resolved        = true;
            state.identity        = identity;
            const AvatarLook look = resolveLook(v.kind, v.configId);
            const SpineInfo spine = AvatarPaths::resolveSpine(look.resSpineId);
            if (!spine.valid)
                continue;
            const bool roleEntity = v.kind == sim::VisualEntityKind::Role;
            state.fashionAvatar   = roleEntity && look.characterClass != CharacterClass::kUnknown;
            // 只有被镜头跟随的本地玩家使用会话时装；对端玩家按各自职业默认外观创建。
            state.useSessionFashion = roleEntity && v.entity == m_followEntity;
            state.roleId            = look.roleId;
            state.partnerRageTransformed = v.partnerRageTransformed;
            state.partnerRageStateKnown  = state.fashionAvatar;
            // 无显式换装且非职业角色的节点按外观键复用；时装角色实例私有，不入池。
            const bool poolable = v.motionFile.empty() ||
                                  (!state.m_overrideAppearance.has_value() &&
                                   look.characterClass == CharacterClass::kUnknown);
            if (poolable)
                state.poolKey = v.motionFile.empty() ? fmt::format("s:{}", look.resSpineId)
                                                     : fmt::format("a:{}:{}", look.resSpineId, v.motionFile);
            if (!poolable || !acquirePooled(state))
            {
                if (v.motionFile.empty())
                    state.skeleton = MgSkeletonAnimation::createFromCache(look.resSpineId);
                else if (state.m_overrideAppearance.has_value())
                {
                    FashionAppearance appearance = *state.m_overrideAppearance;
                    if (appearance.roleId == 0)
                        appearance.roleId = look.roleId;
                    appearance.transformed = v.partnerRageTransformed;
                    state.avatar          = Avatar::create(appearance);
                }
                else
                    state.avatar = createAvatar(look, spine, v.motionFile, state.useSessionFashion,
                                                v.partnerRageTransformed);
            }
            ax::Node* node = displayNode(state);
            if (!node)
                continue;
            state.spineOffset = look.spineOffset;
            tierLayer(look.tier).addChild(node);
        }
        ax::Node* node = displayNode(state);
        if (!node)
            continue;
        if (state.fashionAvatar &&
            (!state.partnerRageStateKnown || state.partnerRageTransformed != v.partnerRageTransformed))
            syncPartnerRageAppearance(v.entity.value, v, v.partnerRageTransformed, false);
        const bool motionChanged = state.motionName != v.motionName || state.entryId != v.entryId;
        if (motionChanged)
        {
            if (state.avatar)
                state.avatar->setMotion(v.motionName, v.entryId, v.loop);
            else if (!state.skeleton->setAnimation(0, v.motionName, v.loop))
                MG_LOG_W("AvatarPresenter: spine animation '{}' not found for entity {}", v.motionName,
                         v.entity.value);
            state.motionName = v.motionName;
            state.entryId    = v.entryId;
            state.loop       = v.loop;
        }
        if (state.avatar)
            driveAvatarTime(state, v, renderDelta, frame, worldDtMs, logicDt, motionChanged);

        // 仅逻辑帧推进时滚动采样：previous 取上一逻辑帧，current 取最新逻辑帧，
        // 渲染帧按 alpha 在两者间插值，得到平滑运动而非 30Hz 阶梯；回滚纠正偏移随渲染时间衰减。
        // 本地玩家用短时间常数跟手，其余实体（对端英雄等）的回滚纠正用长时间常数与大吸附阈值滑行消化。
        state.track.setProfile(v.entity == m_followEntity ? CorrectionProfile::Local : CorrectionProfile::Remote);
        state.track.sample(frame,
                           {renderValue(v.position.x), renderValue(v.position.y),
                            renderValue(v.position.z)});
        state.track.advance(renderDelta);
        state.towardX = v.towardX < 0 ? -1.0f : 1.0f;
        state.towardZ = v.vectorZ < Fixed{} ? -1.0f : 1.0f;
        const RenderPose pose = interpolatePose(state, alpha);
        // Spine 偏移：x 随水平朝向翻转，z 随纵深朝向翻转；层内按偏移后的纵深排序。
        const float depth = pose.z + pose.towardZ * offsetAxis(state.spineOffset, 2);
        node->setPosition(pose.x + pose.towardX * offsetAxis(state.spineOffset, 0),
                          pose.y + offsetAxis(state.spineOffset, 1) + depth);
        node->setLocalZOrder(-static_cast<int>(std::floor(depth)));
        node->setScaleX(pose.towardX);
    }
    for (auto it = m_avatars.begin(); it != m_avatars.end();)
    {
        if (!live.contains(it->first))
        {
            releaseNode(it->second);
            // 实体消失时同步清理其宠物。
            auto pet = m_petFollowers.find(it->first);
            if (pet != m_petFollowers.end())
            {
                pet->second->clear();
                m_petFollowers.erase(pet);
            }
            it = m_avatars.erase(it);
        }
        else
            ++it;
    }

    for (auto it = m_petFollowers.begin(); it != m_petFollowers.end();)
    {
        RenderPose hero;
        if (samplePose(it->first, alpha, &hero))
        {
            it->second->present({hero.x, hero.y, hero.z, hero.towardX, hero.towardZ}, renderDelta);
            ++it;
        }
        else
        {
            it->second->clear();
            it = m_petFollowers.erase(it);
        }
    }
}

void AvatarPresenter::driveAvatarTime(AvatarState& state, const sim::VisualEntityView& view, float renderDelta,
                                      uint64_t frame, Fixed worldDtMs, Fixed logicDt, bool motionChanged)
{
    const int targetMs = static_cast<int>(view.timeMs.toInt());
    if (!state.hasSimTime || state.lastLogicFrame != frame)
    {
        state.simMoving      = !state.hasSimTime || targetMs != state.lastSimTimeMs;
        state.lastSimTimeMs  = targetMs;
        state.lastLogicFrame = frame;
        state.hasSimTime     = true;
    }

    const int shownMs    = state.avatar->getCurrentTimeMs();
    const int durationMs = state.avatar->durationMs();
    const int driftMs    = view.loop ? wrappedDrift(shownMs, targetMs, durationMs) : targetMs - shownMs;
    // 允许偏差为一逻辑帧的该动作时间（含 actionScale），避免快动作每 tick 被当成跳转。
    const int driftLimit = std::max(
        1, static_cast<int>(std::lround((logicDt * view.actionScale).toFloatForRender())));
    if (motionChanged || std::abs(driftMs) > driftLimit)
    {
        state.avatar->seek(targetMs);
        state.simMoving = false;
    }
    else if (state.simMoving)
    {
        const float worldScale = worldDtMs.toFloatForRender() / logicDt.toFloatForRender();
        const float speed      = view.actionScale.toFloatForRender() * worldScale;
        state.avatar->advance(renderDelta * speed);
    }
    state.avatar->stepAccessories(renderDelta);
}

AvatarPresenter::RenderPose AvatarPresenter::interpolatePose(const AvatarState& state, float alpha)
{
    const VisualPos shown = state.track.display(alpha);
    RenderPose pose;
    pose.x       = shown.x;
    pose.y       = shown.y;
    pose.z       = shown.z;
    pose.towardX = state.towardX;
    pose.towardZ = state.towardZ;
    return pose;
}

bool AvatarPresenter::renderOffset(uint32_t entityValue, float alpha, VisualPos* out) const
{
    const auto it = m_avatars.find(entityValue);
    if (it == m_avatars.end() || !it->second.track.hasSample() || !out)
        return false;
    *out = it->second.track.displayOffset(alpha);
    return true;
}

bool AvatarPresenter::samplePose(uint32_t entityValue, float alpha, RenderPose* out) const
{
    const auto it = m_avatars.find(entityValue);
    if (it == m_avatars.end() || !it->second.track.hasSample() || !out)
        return false;
    *out = interpolatePose(it->second, alpha);
    return true;
}

void AvatarPresenter::applyCorrection(Entity entity, const VisualPos& shift)
{
    const auto it = m_avatars.find(entity.value);
    if (it != m_avatars.end())
        it->second.track.applyCorrection(shift);
}

FashionAppearance AvatarPresenter::buildRoleAppearance(const AvatarState& state, bool transformed)
{
    FashionAppearance appearance;
    if (state.m_overrideAppearance.has_value())
        appearance = *state.m_overrideAppearance;
    else if (state.useSessionFashion)
        appearance = gameui::buildSessionFashionAppearance();
    if (appearance.roleId == 0)
        appearance.roleId = state.roleId;
    appearance.transformed = transformed;
    return appearance;
}

bool AvatarPresenter::syncPartnerRageAppearance(uint32_t entityValue, const sim::VisualEntityView& view,
                                                bool transformed, bool force)
{
    auto it = m_avatars.find(entityValue);
    if (it == m_avatars.end())
        return false;
    auto& state = it->second;
    if (!state.avatar || !state.fashionAvatar)
        return false;
    if (!force && state.partnerRageStateKnown && state.partnerRageTransformed == transformed)
        return true;
    const FashionAppearance appearance = buildRoleAppearance(state, transformed);
    if (!state.avatar->setAppearance(appearance))
        return false;
    state.partnerRageTransformed = transformed;
    state.partnerRageStateKnown  = true;
    // 换骨架后立即重放当前逻辑动作和时间，避免显示回到默认待机片段。
    state.avatar->setMotion(view.motionName, view.entryId, view.loop);
    state.avatar->seek(static_cast<int>(view.timeMs.toInt()));
    state.motionName = view.motionName;
    state.entryId    = view.entryId;
    state.loop       = view.loop;
    state.simMoving  = false;
    return true;
}

void AvatarPresenter::onPartnerRageChanged(sim::World& world, Entity entity, bool active)
{
    const auto info = sim::view::readEntityRoleInfo(world, entity);
    auto it = m_avatars.find(entity.value);
    if (it == m_avatars.end())
        return;
    auto& state = it->second;
    if (info.valid && state.roleId == 0)
        state.roleId = info.roleId;
    if (!state.avatar || !state.fashionAvatar)
        return;
    const FashionAppearance appearance = buildRoleAppearance(state, active);
    if (!state.avatar->setAppearance(appearance))
        return;
    state.partnerRageTransformed = active;
    state.partnerRageStateKnown  = true;
    // 保留当前动作与播放时间，避免变身时跳回默认待机。
    state.avatar->setMotion(state.motionName, state.entryId, state.loop);
    state.avatar->seek(state.lastSimTimeMs);
    state.simMoving = false;
}

void AvatarPresenter::setPet(int32_t petId, int32_t spineId)
{
    setEntityPet(m_followEntity, petId, spineId);
}

void AvatarPresenter::clearPet()
{
    clearEntityPet(m_followEntity);
}

void AvatarPresenter::setEntityPet(Entity entity, int32_t petId, int32_t spineId)
{
    if (entity.value == 0 || petId == 0)
    {
        clearEntityPet(entity);
        return;
    }
    auto& slot = m_petFollowers[entity.value];
    if (!slot)
        slot = std::make_unique<PetFollower>(tierLayer(2));
    slot->setPet(petId, spineId);
}

void AvatarPresenter::clearEntityPet(Entity entity)
{
    const auto it = m_petFollowers.find(entity.value);
    if (it == m_petFollowers.end())
        return;
    it->second->clear();
    m_petFollowers.erase(it);
}

ax::Node* AvatarPresenter::displayNode(const AvatarState& state)
{
    if (state.avatar)
        return state.avatar;
    return state.skeleton;
}


bool AvatarPresenter::acquirePooled(AvatarState& state)
{
    const auto it = m_pool.find(state.poolKey);
    if (it == m_pool.end() || it->second.empty())
        return false;
    const PooledNode pooled = it->second.back();
    it->second.pop_back();
    --m_poolCount;
    state.avatar   = pooled.avatar;
    state.skeleton = pooled.skeleton;
    // 池中节点仅剩入池时的那一份引用：改为 autorelease，保证节点存活到随后的 addChild 接管引用，
    // 直接 release 会让引用计数归零并立即析构。
    if (state.avatar)
        state.avatar->autorelease();
    else
        state.skeleton->autorelease();
    return true;
}

void AvatarPresenter::releaseNode(AvatarState& state)
{
    ax::Node* node = displayNode(state);
    if (!node)
        return;
    if (state.poolKey.empty() || m_poolCount >= kPoolMaxTotal)
    {
        node->removeFromParent();
        return;
    }
    auto& bucket = m_pool[state.poolKey];
    if (bucket.size() >= kPoolMaxPerKey)
    {
        node->removeFromParent();
        return;
    }
    // 复位表现层改过的节点属性；动作与时间在复用时由 setMotion/seek 重新设置。
    node->retain();
    node->removeFromParent();
    node->setColor(ax::Color3B::WHITE);
    node->setOpacity(255);
    node->setScale(1.0f);
    node->setLocalZOrder(0);
    if (state.skeleton)
    {
        state.skeleton->clearTracks();
        state.skeleton->setSlotsToSetupPose();
    }
    bucket.push_back({state.avatar, state.skeleton});
    ++m_poolCount;
}

void AvatarPresenter::resetState(uint32_t entityValue, AvatarState& state)
{
    releaseNode(state);
    const auto pet = m_petFollowers.find(entityValue);
    if (pet != m_petFollowers.end())
    {
        pet->second->clear();
        m_petFollowers.erase(pet);
    }
    state = AvatarState{};
}

void AvatarPresenter::clearPool()
{
    for (auto& [_, bucket] : m_pool)
        for (const PooledNode& pooled : bucket)
        {
            if (pooled.avatar)
                pooled.avatar->release();
            else
                pooled.skeleton->release();
        }
    m_pool.clear();
    m_poolCount = 0;
}

void AvatarPresenter::setEntityLayer(ax::Node& layer)
{
    m_tierLayers.clear();
    m_entityLayer = &layer;
}

ax::Node& AvatarPresenter::tierLayer(int32_t tier)
{
    MG_ASSERT(m_entityLayer && "AvatarPresenter: map entity layer is required");
    auto& layer = m_tierLayers[tier];
    if (!layer)
    {
        layer = ax::Node::create();
        layer->setName(fmt::format("tier_{}", tier));
        m_entityLayer->addChild(layer, tier);
    }
    return *layer;
}

void AvatarPresenter::clear()
{
    for (auto& [_, state] : m_avatars)
        if (ax::Node* node = displayNode(state))
            node->removeFromParent();
    m_avatars.clear();
    for (auto& [_, pet] : m_petFollowers)
        pet->clear();
    m_petFollowers.clear();
    m_tierLayers.clear();
    clearPool();
}

bool AvatarPresenter::setEntityAppearance(uint32_t entityValue, const FashionAppearance& appearance)
{
    auto& state             = m_avatars[entityValue];
    state.m_overrideAppearance = appearance;
    if (!state.avatar)
        return true;
    if (!state.avatar->setAppearance(appearance))
        return false;
    if (state.fashionAvatar)
    {
        state.partnerRageTransformed = appearance.transformed;
        state.partnerRageStateKnown  = true;
    }
    state.motionName.clear();
    state.entryId.clear();
    state.simMoving = false;
    return true;
}

bool AvatarPresenter::setEntityIntensity(uint32_t entityValue, float intensity)
{
    auto it = m_avatars.find(entityValue);
    if (it == m_avatars.end())
        return false;
    ax::Node* node = displayNode(it->second);
    if (!node)
        return false;
    // 引擎 Spine 顶点色为 8 位（0-255），无法超过白色实现 >1 的提亮；
    // 这里把亮度倍率饱和到 [0,1]，intensity>1 时按最亮（白色）处理。
    const float v     = std::clamp(intensity, 0.0f, 1.0f);
    const auto  level = static_cast<uint8_t>(std::lround(v * 255.0f));
    node->setColor(ax::Color3B(level, level, level));
    return true;
}

NS_MG_END

#endif
