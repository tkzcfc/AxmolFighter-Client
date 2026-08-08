#include "TownView.h"

#include "AppContext.h"
#include "DungeonSelectView.h"
#include "GameView.h"
#include "mugen/ActorSpawner.h"
#include "mugen/Components.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"
#include "mugen/conf/TableConfig.h"
#include "mugen/render/SpineSkeletonLoader.h"
#include "ui/battle/BattleBootParams.h"
#include "ui/widgets/common/MessagePopup.h"

#include "2d/DrawNode.h"
#include "imgui.h"
#include "spine/SkeletonAnimation.h"

#include <net/client_game.pb.h>
#include <net/client_town.pb.h>

#include <algorithm>
#include <cmath>

using namespace mugen;

namespace gameui
{

namespace
{
// 城镇试点：town 41 → city_newcity_xueyuan
constexpr int32_t TOWN_ID = 41;
// 状态上报间隔（秒）
constexpr float REPORT_INTERVAL = 0.2f;
// 位置变化上报阈值（像素）
constexpr float REPORT_POS_EPSILON = 4.0f;
// 远程玩家位置偏差超过该值直接瞬移（像素）
constexpr float REMOTE_SNAP_DISTANCE = 300.0f;
// 视为"已到达目标"的距离阈值（像素），到达后才应用暂缓的动作切换
constexpr float REMOTE_ARRIVE_EPSILON = 3.0f;
// 追赶速度上浮系数：略快于本体移动速度，保证滞后距离逐渐收敛而不会越拉越远
constexpr float REMOTE_CATCHUP_FACTOR = 1.15f;
// 远程玩家默认动作
const char* REMOTE_DEFAULT_MOTION         = "idle motion";
const char* REMOTE_DEFAULT_WALK_MOTION    = "walk motion";
constexpr float REMOTE_DEFAULT_MOVE_SPEED = 850.0f;
// 速度变化上报阈值（像素/秒）
constexpr float REPORT_VEL_EPSILON = 20.0f;
// 远端用速度更新方向的最小模长（像素/秒）
constexpr float REMOTE_VEL_DIR_EPSILON = 1.0f;
// 相对权威点允许外推的最长时间（秒），略小于上报间隔
constexpr float REMOTE_EXTRAP_MAX_TIME = 0.15f;
// 朝向与实际追赶方向相反时，距离小于该值只切朝向，更大则瞬移纠正
constexpr float REMOTE_FACING_SNAP_DISTANCE = 60.0f;

constexpr uint32_t LOCAL_MOVE_KEY_MASK = (1u << INPUT_SLOT_MOVE_LEFT) | (1u << INPUT_SLOT_MOVE_RIGHT) |
                                         (1u << INPUT_SLOT_MOVE_UP) | (1u << INPUT_SLOT_MOVE_DOWN);
}  // namespace

void TownView::onEnter()
{
    Super::onEnter();

    if (!initGameWord())
    {
        m_gameWord = nullptr;
        MessagePopup::show("城镇初始化失败");
        return;
    }

    if (!createLocalPlayer())
    {
        MessagePopup::show("创建角色失败");
        return;
    }

    addMessagePushReceiver();

    // 城镇只响应移动输入
    m_slotMap[ax::EventKeyboard::KeyCode::KEY_LEFT_ARROW]  = INPUT_SLOT_MOVE_LEFT;
    m_slotMap[ax::EventKeyboard::KeyCode::KEY_RIGHT_ARROW] = INPUT_SLOT_MOVE_RIGHT;
    m_slotMap[ax::EventKeyboard::KeyCode::KEY_UP_ARROW]    = INPUT_SLOT_MOVE_UP;
    m_slotMap[ax::EventKeyboard::KeyCode::KEY_DOWN_ARROW]  = INPUT_SLOT_MOVE_DOWN;

    sendEnterScene();
}

void TownView::onExit()
{
    if (m_keyboardListener)
    {
        ax::Director::getInstance()->getEventDispatcher()->removeEventListener(m_keyboardListener);
        m_keyboardListener = nullptr;
    }

    Super::onExit();
}

void TownView::onUpdate(float delta)
{
    if (!m_gameWord)
    {
        return;
    }

    m_gameWord->update(delta);

    updateRemoteInterpolation(delta);
    updatePortals();
    reportLocalState(delta);
}

void TownView::onKeyPressed(ax::EventKeyboard::KeyCode code, ax::Event* event)
{
    auto it = m_slotMap.find(code);
    if (it == m_slotMap.end())
    {
        return;
    }

    auto player = getLocalPlayer();
    if (player)
    {
        auto inputComp = MG_GET_COMPONENT(player, InputComponent);
        if (inputComp)
        {
            MG_BIT_SET(inputComp->keyDown, 1 << it->second);
        }
    }
}

void TownView::onKeyReleased(ax::EventKeyboard::KeyCode code, ax::Event* event)
{
    auto it = m_slotMap.find(code);
    if (it == m_slotMap.end())
    {
        return;
    }

    auto player = getLocalPlayer();
    if (player)
    {
        auto inputComp = MG_GET_COMPONENT(player, InputComponent);
        if (inputComp)
        {
            MG_BIT_REMOVE(inputComp->keyDown, 1 << it->second);
        }
    }
}

bool TownView::initGameWord()
{
    auto currentScene = ax::Director::getInstance()->getRunningScene();

    m_keyboardListener                = ax::EventListenerKeyboard::create();
    m_keyboardListener->onKeyPressed  = AX_CALLBACK_2(TownView::onKeyPressed, this);
    m_keyboardListener->onKeyReleased = AX_CALLBACK_2(TownView::onKeyReleased, this);
    ax::Director::getInstance()->getEventDispatcher()->addEventListenerWithSceneGraphPriority(m_keyboardListener,
                                                                                              currentScene);

    m_gameWord = std::make_unique<mugen::GameWord>();
    if (!m_gameWord->init(currentScene, 0xe53c2))
    {
        return false;
    }

    m_gameWord->setMode(GameWordMode::kTown);

    if (!m_gameWord->loadMap(TOWN_ID))
    {
        AXLOGE("TownView: loadMap({}) failed", TOWN_ID);
        return false;
    }

    initPortals();
    return true;
}

bool TownView::createLocalPlayer()
{
    auto* session = AppContext::get().gameSession();
    if (!session || session->selectedCharacter.characterID == 0)
    {
        AXLOGE("TownView: no selected character");
        return false;
    }

    // 出生点：优先 TownConfig.actorPos，否则 map scope 中心
    int32_t spawnX = 0;
    int32_t spawnY = 0;
    if (const auto* town = Config::getInstance()->getTownConfigById(TOWN_ID))
    {
        spawnX = town->actorPosX;
        spawnY = town->actorPosZ;
    }
    else if (auto* mapCompEntity = m_gameWord->ecsManager.getEntity(
                 MG_GET_COMPONENT(m_gameWord->getDirector(), DirectorComponent)->mapEntityId))
    {
        auto* mapComp = MG_GET_COMPONENT(mapCompEntity, GameMapComponent);
        if (mapComp && mapComp->mapConfig)
        {
            spawnX = mapComp->mapConfig->scope.x + mapComp->mapConfig->scope.width / 2;
            spawnY = mapComp->mapConfig->scope.y + mapComp->mapConfig->scope.height / 2;
        }
    }

    // classID 是 JobType，映射到可玩英雄 RoleConfig
    const int32_t roleId = actor_spawner::resolvePlayableRoleId(session->selectedCharacter.classID);
    auto player          = actor_spawner::spawnRolePlayerActor(
        &m_gameWord->ecsManager, roleId, spawnX, spawnY,
        actor_spawner::PlayerSpawnParams{static_cast<int32_t>(session->account.playerID),
                                         std::string(session->selectedCharacter.name)}
            .toActorParams());
    if (!player)
    {
        return false;
    }

    m_gameWord->bindLocalPlayer(player->getId());
    player->notifyEntityReady();

    return true;
}

void TownView::sendEnterScene()
{
    PB::Game::EnterSceneReq req;
    req.set_type(PB::Types::SceneEnterType::Map);
    req.set_map_id(static_cast<uint32_t>(TOWN_ID));
    fillLocalState(req.mutable_state());

    this->call(req, [this](const PB::Game::EnterSceneResp* resp, std::string_view error) {
        if (!resp)
        {
            MessagePopup::showNetErr(error, [this]() { sendEnterScene(); });
            return;
        }

        if (resp->code() != 0)
        {
            MessagePopup::show(resp->message().empty() ? "进入城镇失败" : resp->message());
            return;
        }

        m_enteredScene = true;

        const int64_t selfId = AppContext::get().gameSession()->account.playerID;
        for (const auto& state : resp->players())
        {
            if (state.player_id() == selfId)
            {
                continue;
            }
            addOrUpdateRemotePlayer(state, true);
        }

        AXLOGI("TownView: entered scene, players={}", resp->players_size());
    });
}

void TownView::fillLocalState(PB::Types::PlayerState* state) const
{
    auto* session = AppContext::get().gameSession();
    if (session)
    {
        state->set_player_id(session->account.playerID);
        state->set_class_id(session->selectedCharacter.classID);
        state->set_name(session->selectedCharacter.name);
    }

    auto player = getLocalPlayer();
    if (!player)
    {
        return;
    }

    auto physicsComp   = MG_GET_COMPONENT(player, PhysicsComponent);
    auto transformComp = MG_GET_COMPONENT(player, TransformComponent);
    auto avatarComp    = MG_GET_COMPONENT(player, AvatarComponent);
    auto attributeComp = MG_GET_COMPONENT(player, AttributeComponent);
    auto inputComp     = MG_GET_COMPONENT(player, InputComponent);

    if (physicsComp)
    {
        state->set_pos_x(physicsComp->position.x);
        state->set_pos_y(physicsComp->position.y);
        state->set_pos_z(physicsComp->position.z);
        state->set_vel_x(physicsComp->velocity.x);
        state->set_vel_y(physicsComp->velocity.y);
    }
    if (transformComp)
    {
        state->set_facing(transformComp->facingDirection == FacingDirection::kFacingRight ? 1 : 0);
    }
    if (avatarComp)
    {
        state->set_state(avatarComp->playback.getCurrentMotionName());
    }
    if (attributeComp)
    {
        state->set_hp(attributeComp->currentAttribute.hpMax);
    }

    // 仅以移动输入为准，不用速度兜底，避免按键与 FSM 时序造成抖动
    const bool moving = inputComp && MG_BIT_HAS_ANY(inputComp->keyDown, LOCAL_MOVE_KEY_MASK);
    state->set_moving(moving);
}

void TownView::reportLocalState(float delta)
{
    if (!m_enteredScene)
    {
        return;
    }

    m_reportTimer += delta;
    if (m_reportTimer < REPORT_INTERVAL)
    {
        return;
    }
    m_reportTimer = 0.0f;

    auto player = getLocalPlayer();
    if (!player)
    {
        return;
    }

    PB::Types::PlayerState state;
    fillLocalState(&state);

    const bool posChanged = std::fabs(state.pos_x() - m_lastSentX) > REPORT_POS_EPSILON ||
                            std::fabs(state.pos_y() - m_lastSentY) > REPORT_POS_EPSILON ||
                            std::fabs(state.pos_z() - m_lastSentZ) > REPORT_POS_EPSILON;
    const bool motionChanged = state.state() != m_lastSentMotion;
    const bool facingChanged = state.facing() != m_lastSentFacing;
    const bool movingChanged = state.moving() != m_lastSentMoving;
    const bool velChanged    = std::fabs(state.vel_x() - m_lastSentVelX) > REPORT_VEL_EPSILON ||
                            std::fabs(state.vel_y() - m_lastSentVelY) > REPORT_VEL_EPSILON;

    if (!posChanged && !motionChanged && !facingChanged && !movingChanged && !velChanged)
    {
        return;
    }

    m_lastSentX      = state.pos_x();
    m_lastSentY      = state.pos_y();
    m_lastSentZ      = state.pos_z();
    m_lastSentMotion = state.state();
    m_lastSentFacing = state.facing();
    m_lastSentMoving = state.moving();
    m_lastSentVelX   = state.vel_x();
    m_lastSentVelY   = state.vel_y();

    PB::Town::TownPlayerStatePush push;
    *push.mutable_state() = state;
    this->notify(push);
}

void TownView::addMessagePushReceiver()
{
    // 玩家进入消息推送
    this->listenPush([this](PB::Town::ScenePlayerEnterPush& push) {
        if (!push.has_state())
        {
            return;
        }
        addOrUpdateRemotePlayer(push.state(), true);
        AXLOGI("TownView: player {} entered scene", push.state().player_id());
    });

    // 玩家离开消息推送
    this->listenPush([this](PB::Town::ScenePlayerLeavePush& push) {
        removeRemotePlayer(push.player_id());
        AXLOGI("TownView: player {} left scene", push.player_id());
    });

    // 玩家状态更新消息推送
    this->listenPush([this](PB::Town::ScenePlayerStatePush& push) {
        if (!push.has_state())
        {
            return;
        }
        addOrUpdateRemotePlayer(push.state(), false);
    });

    // 决斗邀请
    this->listenPush([this](PB::Game::DuelInvitePush& push) {
        m_duelInviterId   = push.from_player_id();
        m_duelInviterName = push.from_name();
        m_showDuelInvite  = true;
    });

    // 决斗开始
    this->listenPush([this](PB::Game::DuelStartPush& push) {
        BattleBootParams boot;
        boot.battleId      = push.battle_id();
        boot.serverFrame   = push.server_frame();
        boot.actorEntityId = push.actor_entity_id();
        boot.mapId         = push.map_id();
        boot.randomSeed    = push.random_seed();
        boot.worldDump     = push.world_dump();
        getViewManager()->switchView<GameView>(std::move(boot));
    });

    // 决斗结束/取消
    this->listenPush([this](PB::Game::DuelEndPush& push) {
        m_showDuelInvite = false;
        MessagePopup::show(push.message().empty() ? "决斗已取消" : push.message());
    });
}

void TownView::addOrUpdateRemotePlayer(const PB::Types::PlayerState& state, bool snap)
{
    if (!m_gameWord)
    {
        return;
    }

    const int64_t playerId = state.player_id();
    auto it                = m_remotePlayers.find(playerId);
    if (it != m_remotePlayers.end())
    {
        applyRemoteState(it->second, state, snap);
        return;
    }

    // 新的远程玩家：创建展示实体（class_id 同样是 JobType）
    const int32_t roleId = actor_spawner::resolvePlayableRoleId(state.class_id());
    auto entity          = actor_spawner::spawnRemoteRoleActor(
        &m_gameWord->ecsManager, roleId, static_cast<int32_t>(state.pos_x()), static_cast<int32_t>(state.pos_y()),
        actor_spawner::PlayerSpawnParams{static_cast<int32_t>(playerId), state.name()}.toActorParams());
    if (!entity)
    {
        return;
    }
    entity->notifyEntityReady();

    RemotePlayer remote;
    remote.entityId = entity->getId();
    remote.name     = state.name();
    remote.classId  = state.class_id();

    if (const auto* role = Config::getInstance()->getRoleConfigById(roleId))
    {
        remote.moveSpeed  = role->attribute.moveSpeed > 0 ? role->attribute.moveSpeed : 300.0f;
        remote.idleMotion = "stand";
        remote.walkMotion = "running";
    }
    if (remote.moveSpeed <= 0.0f)
    {
        remote.moveSpeed = REMOTE_DEFAULT_MOVE_SPEED;
    }
    if (remote.idleMotion.empty())
    {
        remote.idleMotion = REMOTE_DEFAULT_MOTION;
    }
    if (remote.walkMotion.empty())
    {
        remote.walkMotion = REMOTE_DEFAULT_WALK_MOTION;
    }

    applyRemoteState(remote, state, true);

    // 初始动作
    if (remote.motion.empty())
    {
        remote.pendingMotion = REMOTE_DEFAULT_MOTION;
        applyPendingMotion(remote, entity);
    }

    m_remotePlayers.emplace(playerId, remote);
}

void TownView::removeRemotePlayer(int64_t playerId)
{
    auto it = m_remotePlayers.find(playerId);
    if (it == m_remotePlayers.end())
    {
        return;
    }

    if (m_gameWord)
    {
        auto entity = m_gameWord->ecsManager.getEntity(it->second.entityId);
        if (entity)
        {
            entity->destroy();
        }
    }

    if (m_selectedPlayerId == playerId)
    {
        m_selectedPlayerId = 0;
    }

    m_remotePlayers.erase(it);
}

void TownView::applyRemoteState(RemotePlayer& remote, const PB::Types::PlayerState& state, bool snap)
{
    const float newX = state.pos_x();
    const float newY = state.pos_y();
    const float newZ = state.pos_z();

    const float velX   = state.vel_x();
    const float velY   = state.vel_y();
    const float velLen = std::sqrt(velX * velX + velY * velY);

    remote.authX  = newX;
    remote.authY  = newY;
    remote.hp     = state.hp();
    remote.moving = state.moving();
    if (!state.name().empty())
    {
        remote.name = state.name();
    }

    const float toAuthX  = newX - remote.curX;
    const float toAuthY  = newY - remote.curY;
    const float authDist = std::sqrt(toAuthX * toAuthX + toAuthY * toAuthY);

    if (snap || authDist > REMOTE_SNAP_DISTANCE)
    {
        // 大偏差直接瞬移（含停步包），避免长距离倒走
        remote.targetX = newX;
        remote.targetY = newY;
        remote.targetZ = newZ;
        remote.curX    = newX;
        remote.curY    = newY;
        remote.curZ    = newZ;
        remote.dirX    = 0.0f;
        remote.dirY    = 0.0f;
    }
    else if (!remote.moving)
    {
        // 无移动输入：不改目标、不外推、清方向，就地停
        remote.dirX    = 0.0f;
        remote.dirY    = 0.0f;
        remote.targetX = remote.curX;
        remote.targetY = remote.curY;
        remote.targetZ = remote.curZ;
    }
    else
    {
        // 仍在移动：更新方向并以外推填补上报间隙
        if (velLen > REMOTE_VEL_DIR_EPSILON)
        {
            remote.dirX = velX / velLen;
            remote.dirY = velY / velLen;
        }
        else
        {
            const float ddx = newX - remote.targetX;
            const float ddy = newY - remote.targetY;
            const float len = std::sqrt(ddx * ddx + ddy * ddy);
            if (len > REMOTE_ARRIVE_EPSILON)
            {
                remote.dirX = ddx / len;
                remote.dirY = ddy / len;
            }
        }
        remote.targetX = newX;
        remote.targetY = newY;
        remote.targetZ = newZ;
    }

    if (!m_gameWord)
    {
        return;
    }
    auto entity = m_gameWord->ecsManager.getEntity(remote.entityId);
    if (!entity)
    {
        return;
    }

    const int32_t facing = state.facing();
    remote.facing        = facing;
    auto transformComp   = MG_GET_COMPONENT(entity, TransformComponent);
    if (transformComp)
    {
        transformComp->facingDirection = facing == 1 ? FacingDirection::kFacingRight : FacingDirection::kFacingLeft;
    }

    const float remaining     = std::fabs(remote.targetX - remote.curX) + std::fabs(remote.targetY - remote.curY);
    const std::string& motion = state.state();
    if (!motion.empty())
    {
        remote.pendingMotion = motion;
    }

    if (remote.moving)
    {
        // 仍在移动：不切 idle，保持/切到 walk（或网络下发的非 idle 动作）
        if (remote.pendingMotion.empty() || remote.pendingMotion == remote.idleMotion)
        {
            remote.pendingMotion = remote.walkMotion;
        }
        applyPendingMotion(remote, entity);
    }
    else
    {
        // 已停步：未到点则先走过去，到点再切 idle
        const bool deferToArrival = !snap && remaining > REMOTE_ARRIVE_EPSILON;
        if (deferToArrival)
        {
            if (!remote.pendingMotion.empty() && remote.pendingMotion != remote.idleMotion)
            {
                applyPendingMotion(remote, entity);
            }
        }
        else
        {
            if (remote.pendingMotion.empty())
            {
                remote.pendingMotion = remote.idleMotion;
            }
            applyPendingMotion(remote, entity);
        }
    }
}

void TownView::applyPendingMotion(RemotePlayer& remote, mugen::Entity* entity)
{
    if (remote.pendingMotion.empty() || remote.pendingMotion == remote.motion)
    {
        return;
    }

    remote.motion   = remote.pendingMotion;
    auto avatarComp = MG_GET_COMPONENT(entity, AvatarComponent);
    if (avatarComp)
    {
        avatarComp->play(remote.motion, -1, false);
    }
}

void TownView::updateRemoteInterpolation(float delta)
{
    if (!m_gameWord)
    {
        return;
    }

    for (auto& [playerId, remote] : m_remotePlayers)
    {
        auto entity = m_gameWord->ecsManager.getEntity(remote.entityId);
        if (!entity)
        {
            continue;
        }

        const float speed = remote.moveSpeed > 0.0f ? remote.moveSpeed : REMOTE_DEFAULT_MOVE_SPEED;

        // moving 时沿最近方向外推 target，填补上报间隙；相对权威点钳制超前量
        if (remote.moving)
        {
            const float dirLen = std::sqrt(remote.dirX * remote.dirX + remote.dirY * remote.dirY);
            if (dirLen > 0.001f)
            {
                const float inv      = 1.0f / dirLen;
                const float ndx      = remote.dirX * inv;
                const float ndy      = remote.dirY * inv;
                const float maxAhead = speed * REMOTE_EXTRAP_MAX_TIME;
                remote.targetX += ndx * speed * delta;
                remote.targetY += ndy * speed * delta;

                const float ax         = remote.targetX - remote.authX;
                const float ay         = remote.targetY - remote.authY;
                const float aheadAlong = ax * ndx + ay * ndy;
                if (aheadAlong > maxAhead)
                {
                    remote.targetX = remote.authX + ndx * maxAhead;
                    remote.targetY = remote.authY + ndy * maxAhead;
                }
            }
        }

        const float dx       = remote.targetX - remote.curX;
        const float dy       = remote.targetY - remote.curY;
        const float dz       = remote.targetZ - remote.curZ;
        const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

        if (std::fabs(dx) > REMOTE_SNAP_DISTANCE || std::fabs(dy) > REMOTE_SNAP_DISTANCE)
        {
            // 偏差过大直接瞬移
            remote.curX = remote.targetX;
            remote.curY = remote.targetY;
            remote.curZ = remote.targetZ;
        }
        else if (distance > REMOTE_ARRIVE_EPSILON)
        {
            // 恒速追赶：速度与本体一致（略上浮保证收敛），避免指数插值拖尾滑步。
            const float step = speed * REMOTE_CATCHUP_FACTOR * delta;
            if (step >= distance)
            {
                remote.curX = remote.targetX;
                remote.curY = remote.targetY;
                remote.curZ = remote.targetZ;
            }
            else
            {
                const float ratio = step / distance;
                remote.curX += dx * ratio;
                remote.curY += dy * ratio;
                remote.curZ += dz * ratio;
            }
        }

        // 朝向必须与实际追赶方向一致；反向且偏差大时瞬移纠正
        const float moveDeltaX = remote.targetX - remote.curX;
        const float moveDeltaY = remote.targetY - remote.curY;
        const float moveDist   = std::sqrt(moveDeltaX * moveDeltaX + moveDeltaY * moveDeltaY);
        if (moveDist > REMOTE_ARRIVE_EPSILON)
        {
            const int32_t displayFacing = (moveDeltaX >= 0.0f) ? 1 : -1;
            if (displayFacing != remote.facing)
            {
                remote.facing = displayFacing;
                if (moveDist >= REMOTE_FACING_SNAP_DISTANCE)
                {
                    // 反向且已走远：瞬移到目标，避免长距离倒走
                    remote.curX = remote.targetX;
                    remote.curY = remote.targetY;
                    remote.curZ = remote.targetZ;
                }
                auto transformComp = MG_GET_COMPONENT(entity, TransformComponent);
                if (transformComp)
                {
                    transformComp->facingDirection =
                        displayFacing == 1 ? FacingDirection::kFacingRight : FacingDirection::kFacingLeft;
                }
            }
        }

        const float remaining = std::fabs(remote.targetX - remote.curX) + std::fabs(remote.targetY - remote.curY);
        if (remaining > REMOTE_ARRIVE_EPSILON)
        {
            // 有剩余位移：播放 walk
            if (remote.motion != remote.walkMotion && !remote.walkMotion.empty())
            {
                remote.pendingMotion = remote.walkMotion;
                applyPendingMotion(remote, entity);
            }
        }
        else if (remote.moving)
        {
            // 已到点但仍在移动：保持 walk，等下一包（禁止误切 idle）
            if (remote.motion != remote.walkMotion && !remote.walkMotion.empty())
            {
                remote.pendingMotion = remote.walkMotion;
                applyPendingMotion(remote, entity);
            }
        }
        else
        {
            // 无位移且已停步：切 idle
            if (remote.pendingMotion.empty() || remote.pendingMotion == remote.walkMotion)
            {
                remote.pendingMotion = remote.idleMotion;
            }
            applyPendingMotion(remote, entity);
        }

        auto transformComp = MG_GET_COMPONENT(entity, TransformComponent);
        if (transformComp)
        {
            transformComp->position.x = static_cast<int32_t>(remote.curX);
            transformComp->position.y = static_cast<int32_t>(remote.curY);
            transformComp->position.z = static_cast<int32_t>(remote.curZ);
        }
    }
}

void TownView::requestSoloBattle()
{
    auto* session = AppContext::get().gameSession();
    if (!session || !session->account.isLoggedIn())
    {
        MessagePopup::show("请先登录");
        return;
    }

    PB::Game::BattleJoinReq req;
    req.set_player_id(session->account.playerID);
    req.set_map_id(1);

    this->call(req, [this](const PB::Game::BattleJoinResp* resp, std::string_view err) {
        if (!resp)
        {
            MessagePopup::showNetErr(err);
            return;
        }
        if (resp->code() != 0)
        {
            MessagePopup::show(resp->message().empty() ? "进入战斗失败" : resp->message());
            return;
        }

        BattleBootParams boot;
        boot.battleId      = resp->battle_id();
        boot.serverFrame   = resp->server_frame();
        boot.actorEntityId = resp->actor_entity_id();
        boot.mapId         = 1;
        boot.randomSeed    = resp->random_seed();
        boot.worldDump     = resp->world_dump();
        getViewManager()->switchView<GameView>(std::move(boot));
    });
}

void TownView::sendDuelInvite(int64_t targetPlayerId)
{
    if (targetPlayerId == 0)
        return;

    auto* session = AppContext::get().gameSession();
    if (session && targetPlayerId == session->account.playerID)
    {
        MessagePopup::show("不能和自己决斗");
        return;
    }

    PB::Game::DuelInviteReq req;
    req.set_target_player_id(targetPlayerId);
    this->call(req, [this](const PB::Game::DuelInviteResp* resp, std::string_view err) {
        if (!resp)
        {
            MessagePopup::showNetErr(err);
            return;
        }
        if (resp->code() != 0)
        {
            MessagePopup::show(resp->message().empty() ? "决斗邀请失败" : resp->message());
        }
    });
}

void TownView::respondDuelInvite(int32_t accept)
{
    PB::Game::DuelRespondReq req;
    req.set_accept(accept);
    this->call(req, [this](const PB::Game::DuelRespondResp* resp, std::string_view err) {
        if (!resp)
        {
            MessagePopup::showNetErr(err);
            return;
        }
        if (resp->code() != 0)
        {
            MessagePopup::show(resp->message().empty() ? "决斗回应失败" : resp->message());
        }
    });
}

ax::Node* TownView::findChildByNameRecursive(ax::Node* root, const std::string& name)
{
    if (!root)
        return nullptr;
    if (root->getName() == name)
        return root;

    const auto& children = root->getChildren();
    for (auto* child : children)
    {
        if (auto* found = findChildByNameRecursive(child, name))
            return found;
    }
    return nullptr;
}

void TownView::initPortals()
{
    m_portals.clear();
    if (!m_gameWord)
        return;

    const auto* town = Config::getInstance()->getTownConfigById(TOWN_ID);
    if (!town || town->portals.empty())
    {
        AXLOGI("TownView: town {} has no portals", TOWN_ID);
        return;
    }

    auto director     = m_gameWord->getDirector();
    auto directorComp = director ? MG_GET_COMPONENT(director, DirectorComponent) : nullptr;
    auto* mapEntity   = directorComp ? m_gameWord->ecsManager.getEntity(directorComp->mapEntityId) : nullptr;
    auto* mapRender   = mapEntity ? MG_GET_COMPONENT(mapEntity, GameMapRenderComponent) : nullptr;
    if (!mapRender || !mapRender->entityNode)
    {
        AXLOGW("TownView: entityNode missing, cannot spawn portals");
        return;
    }

    ax::Node* entityNode = mapRender->entityNode;

    for (const auto& entry : town->portals)
    {
        if (entry.portalId <= 0 || entry.slot <= 0)
            continue;

        const std::string markerName = fmt::format("POR_{}", entry.slot);
        ax::Node* marker             = findChildByNameRecursive(entityNode, markerName);
        if (!marker)
        {
            AXLOGW("TownView: portal marker '{}' not found in entity layer", markerName);
            continue;
        }

        // 取相对 entityNode 的坐标（entity 层无视差，即地图逻辑坐标）
        const ax::Vec2 worldPos = marker->convertToWorldSpaceAR(ax::Vec2::ZERO);
        const ax::Vec2 localPos = entityNode->convertToNodeSpace(worldPos);

        TownPortal portal;
        portal.portalId = entry.portalId;
        portal.slot     = entry.slot;
        portal.destType = entry.destType;
        portal.posX     = localPos.x;
        portal.posY     = localPos.y;

        const auto* portalCfg = Config::getInstance()->getPortalConfigById(entry.portalId);
        portal.radius         = portalCfg && portalCfg->radius > 0 ? static_cast<float>(portalCfg->radius) : 80.0f;

        // 外观：优先 Spine，失败则画调试圆
        ax::Node* visual = nullptr;
        if (portalCfg && portalCfg->resSpineId > 0)
        {
            const auto* spineCfg = Config::getInstance()->getResSpineConfigById(portalCfg->resSpineId);
            if (spineCfg && !spineCfg->spine.empty() && !spineCfg->atlas.empty())
            {
                const float scale = spineCfg->scale > 0.0f ? spineCfg->scale : 1.0f;
                auto* skeleton =
                    SpineSkeletonLoader::createSkeletonAnimation(spineCfg->spine, spineCfg->atlas, scale);
                if (skeleton)
                {
                    if (!spineCfg->defaultSkin.empty())
                        skeleton->setSkin(spineCfg->defaultSkin);

                    // 播放第一个可用动画作占位
                    if (skeleton->getSkeleton() && skeleton->getSkeleton()->getData())
                    {
                        auto& anims = skeleton->getSkeleton()->getData()->getAnimations();
                        if (anims.size() > 0)
                        {
                            const char* animName = anims[0]->getName().buffer();
                            if (animName && animName[0] != '\0')
                                skeleton->setAnimation(0, animName, true);
                        }
                    }
                    visual = skeleton;
                }
            }
        }

        if (!visual)
        {
            auto* draw = ax::DrawNode::create();
            draw->drawCircle(ax::Vec2::ZERO, portal.radius, 0.0f, 32, false, ax::Color4F(0.2f, 0.8f, 1.0f, 0.8f));
            draw->drawSolidCircle(ax::Vec2::ZERO, 12.0f, 0.0f, 16, ax::Color4F(0.2f, 0.8f, 1.0f, 0.5f));
            visual = draw;
        }

        visual->setPosition(portal.posX, portal.posY);
        visual->setName(fmt::format("portal_runtime_{}", entry.slot));
        entityNode->addChild(visual, 10);
        portal.visual = visual;

        AXLOGI("TownView: portal id={} slot={} destType={} at ({:.1f},{:.1f}) radius={}", portal.portalId, portal.slot,
               portal.destType, portal.posX, portal.posY, portal.radius);
        m_portals.push_back(portal);
    }
}

void TownView::updatePortals()
{
    if (m_portals.empty())
        return;

    auto player = getLocalPlayer();
    if (!player)
        return;

    auto* physics = MG_GET_COMPONENT(player, PhysicsComponent);
    if (!physics)
        return;

    const float px = physics->position.x;
    const float py = physics->position.y;

    for (auto& portal : m_portals)
    {
        const float dx   = px - portal.posX;
        const float dy   = py - portal.posY;
        const float dist = std::sqrt(dx * dx + dy * dy);

        if (!portal.playerInside)
        {
            if (dist <= portal.radius)
            {
                portal.playerInside = true;
                onPortalTriggered(portal);
            }
        }
        else
        {
            // 滞回：离开 1.2 倍半径后才重置，避免原地反复触发
            if (dist > portal.radius * 1.2f)
                portal.playerInside = false;
        }
    }
}

void TownView::onPortalTriggered(const TownPortal& portal)
{
    // 对齐黑月 PortalTransferType：1=Function（功能 UI），5=City
    constexpr int32_t kDestTypeFunction = 1;
    constexpr int32_t kDestTypeCity     = 5;

    AXLOGI("TownView: portal triggered id={} slot={} destType={}", portal.portalId, portal.slot, portal.destType);

    if (portal.destType == kDestTypeFunction)
    {
        getViewManager()->pushView<DungeonSelectView>();
        return;
    }

    if (portal.destType == kDestTypeCity)
    {
        AXLOGI("TownView: city portal not implemented yet (portalId={})", portal.portalId);
        return;
    }

    AXLOGI("TownView: unsupported portal destType={}", portal.destType);
}

mugen::Entity* TownView::getLocalPlayer() const
{
    if (!m_gameWord)
    {
        return nullptr;
    }

    auto director = m_gameWord->getDirector();
    if (!director)
    {
        return nullptr;
    }

    auto directorComp = MG_GET_COMPONENT(director, DirectorComponent);
    if (!directorComp || directorComp->localPlayerEntityId == INVALID_ENTITY_ID)
    {
        return nullptr;
    }

    return m_gameWord->ecsManager.getEntity(directorComp->localPlayerEntityId);
}

void TownView::onImGUIRender()
{
    ImGui::SetNextWindowPos(ImVec2(20.0f, 60.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(260.0f, 320.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("城镇玩家");

    if (!m_enteredScene)
    {
        ImGui::TextUnformatted("正在进入城镇...");
    }
    else if (m_remotePlayers.empty())
    {
        ImGui::TextUnformatted("附近没有其他玩家");
    }
    else
    {
        for (auto& [playerId, remote] : m_remotePlayers)
        {
            std::string label =
                fmt::format("{} (ID:{})##player_{}", remote.name.empty() ? "???" : remote.name, playerId, playerId);
            if (ImGui::Selectable(label.c_str(), m_selectedPlayerId == playerId))
            {
                m_selectedPlayerId = playerId;
                ImGui::OpenPopup("player_actions");
            }
        }
    }

    // 点击玩家后的交互弹窗
    if (ImGui::BeginPopup("player_actions"))
    {
        auto it = m_remotePlayers.find(m_selectedPlayerId);
        if (it != m_remotePlayers.end())
        {
            ImGui::Text("%s (ID:%lld)", it->second.name.empty() ? "???" : it->second.name.c_str(),
                        static_cast<long long>(m_selectedPlayerId));
            ImGui::Separator();

            if (ImGui::Button("决斗", ImVec2(100.0f, 0.0f)))
            {
                sendDuelInvite(m_selectedPlayerId);
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::Button("组队", ImVec2(100.0f, 0.0f)))
            {
                // TODO: 组队功能在后续计划中实现
                AXLOGI("TownView: TODO team up with player {}", m_selectedPlayerId);
                ImGui::CloseCurrentPopup();
            }
        }
        else
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();
    if (ImGui::Button("开始战斗", ImVec2(-1, 0)))
    {
        getViewManager()->switchView<GameView>();
    }
    if (ImGui::Button("单人战斗", ImVec2(-1, 0)))
    {
        requestSoloBattle();
    }

    // 决斗邀请弹窗
    if (m_showDuelInvite)
    {
        ImGui::OpenPopup("duel_invite");
        m_showDuelInvite = false;
    }
    if (ImGui::BeginPopupModal("duel_invite", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("%s 邀请你决斗", m_duelInviterName.empty() ? "???" : m_duelInviterName.c_str());
        ImGui::Separator();
        if (ImGui::Button("接受", ImVec2(120, 0)))
        {
            respondDuelInvite(1);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("拒绝", ImVec2(120, 0)))
        {
            respondDuelInvite(0);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

}  // namespace gameui
