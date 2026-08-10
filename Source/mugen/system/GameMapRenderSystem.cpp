#include "GameMapRenderSystem.h"
#include "mugen/Components.h"
#include "mugen/GameWord.h"
#include "mugen/conf/Config.h"

#ifdef RUNTIME_IN_AXMOL
#    include "mugen/render/LayerRuntimeLoader.h"
#    include "mugen/render/RenderObjectPool.h"

const std::string_view kDistantLayerName        = "distant";
const std::string_view kMiddleLayerName         = "middle";
const std::string_view kNearbyLayerName         = "nearby";
const std::string_view kGroundLayerName         = "ground";
const std::string_view kRegionLayerName         = "region";
const std::string_view kTriggerLayerName        = "trigger";
const std::string_view kEntityLayerName         = "entity";
const std::string_view kCaseLayerName           = "case";
const std::string_view kLightLayerName          = "light";
const std::string_view kGroundDebugDrawNodeName = "__groundDebugDrawNode__";
const std::string_view kActorDebugDrawNodeName  = "__actorDebugDrawNode__";

#endif

NS_MG_BEGIN

GameMapRenderSystem::GameMapRenderSystem() {}
GameMapRenderSystem::~GameMapRenderSystem() {}

void GameMapRenderSystem::init(ECSManager* ecs)
{
    Super::init(ecs);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, GameMapComponent);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, GameMapRenderComponent);
}

#ifdef RUNTIME_IN_AXMOL

void GameMapRenderSystem::update()
{
    auto directorEntity = this->getGameWord()->getDirector();
    auto directorComp   = MG_GET_COMPONENT(directorEntity, DirectorComponent);

    auto lastUpdateTimeMs = getECSManager()->getLastUpdateTimeMs();
    for (auto& entity : entities)
    {
        auto gameMapRenderComp = MG_GET_COMPONENT(entity, GameMapRenderComponent);
        if (gameMapRenderComp->camera)
        {
            auto cameraFollowTarget = getECSManager()->getEntity(directorComp->cameraFollowTarget);
            if (cameraFollowTarget)
            {
                // 优先物理坐标，避免渲染节点延迟
                if (auto* physicsComp = MG_GET_COMPONENT(cameraFollowTarget, PhysicsComponent))
                {
                    gameMapRenderComp->camera->setFocusPosition({physicsComp->position.x, physicsComp->position.y});
                }
                else if (auto* transformComp = MG_GET_COMPONENT(cameraFollowTarget, TransformComponent))
                {
                    gameMapRenderComp->camera->setFocusPosition(
                        {static_cast<float>(transformComp->position.x), static_cast<float>(transformComp->position.y)});
                }
            }

            gameMapRenderComp->camera->doUpdate(lastUpdateTimeMs / 1000.0f);
        }
        if (gameMapRenderComp->groundDebugDrawNode)
            gameMapRenderComp->groundDebugDrawNode->clear();
        if (gameMapRenderComp->actorDebugDrawNode)
            gameMapRenderComp->actorDebugDrawNode->clear();
    }
}

void GameMapRenderSystem::onEntityAdded(Entity* entity)
{
    auto gameMapComp       = MG_GET_COMPONENT(entity, GameMapComponent);
    auto gameMapRenderComp = MG_GET_COMPONENT(entity, GameMapRenderComponent);

    MG_ASSERT(!gameMapComp->layerFile.empty());
    MG_ASSERT(gameMapRenderComp->mapRootNode == nullptr && "Render node already exists!");

    ax::ParallaxNode* mapRootNode         = nullptr;
    std::unique_ptr<VirtualCamera> camera = nullptr;

    if (getECSManager()->isDeserialized())
    {
        const auto rootKey = RenderStashKey(entity->getId()) << "mapRoot" << gameMapComp->mapId;
        const auto camKey  = RenderStashKey(entity->getId());

        auto* pool  = RenderObjectPool::getInstance();
        mapRootNode = pool->acquireNode<ax::ParallaxNode>(rootKey);
        camera      = pool->acquireCamera(camKey);
    }

    if (mapRootNode)
    {
        if (bindToGameMapRenderComponent(entity, mapRootNode, std::move(camera)))
            return;
        MG_LOG_W("Failed to bind mapRootNode from RenderObjectPool to GameMapRenderComponent!");
    }

    // 视觉树：仅客户端（size/scope/soundId 已由 GameMapSystem + LayerLoader 填充；视差在 loadNode 内解析 meta）
    ax::ParallaxNode* root = LayerRuntimeLoader::loadNode(gameMapComp->layerFile);
    MG_ASSERT(root != nullptr && "Failed to load map layer file!");

    if (!bindToGameMapRenderComponent(entity, root, std::move(camera)))
    {
        MG_LOG_E("Failed to bind mapRootNode: need all 9 layers");
    }
}

void GameMapRenderSystem::onEntityRemoved(Entity* entity)
{
    auto gameMapRenderComp = MG_GET_COMPONENT(entity, GameMapRenderComponent);
    if (!gameMapRenderComp->mapRootNode)
        return;

    if (getECSManager()->isDeserialized())
    {
        auto gameMapComp   = MG_GET_COMPONENT(entity, GameMapComponent);
        auto* pool         = RenderObjectPool::getInstance();
        const auto rootKey = RenderStashKey(entity->getId()) << "mapRoot" << (gameMapComp ? gameMapComp->mapId : 0);
        const auto camKey  = RenderStashKey(entity->getId()) << "camera" << (gameMapComp ? gameMapComp->mapId : 0);
        pool->recycleNode(rootKey, gameMapRenderComp->mapRootNode);
        pool->recycleCamera(camKey, std::move(gameMapRenderComp->camera));
        gameMapRenderComp->mapRootNode         = nullptr;
        gameMapRenderComp->distantNode         = nullptr;
        gameMapRenderComp->middleNode          = nullptr;
        gameMapRenderComp->nearbyNode          = nullptr;
        gameMapRenderComp->groundNode          = nullptr;
        gameMapRenderComp->regionNode          = nullptr;
        gameMapRenderComp->triggerNode         = nullptr;
        gameMapRenderComp->entityNode          = nullptr;
        gameMapRenderComp->caseNode            = nullptr;
        gameMapRenderComp->lightNode           = nullptr;
        gameMapRenderComp->groundDebugDrawNode = nullptr;
        gameMapRenderComp->actorDebugDrawNode  = nullptr;
        return;
    }

    if (gameMapRenderComp->groundDebugDrawNode)
        gameMapRenderComp->groundDebugDrawNode->removeFromParent();
    gameMapRenderComp->groundDebugDrawNode = nullptr;

    if (gameMapRenderComp->actorDebugDrawNode)
        gameMapRenderComp->actorDebugDrawNode->removeFromParent();
    gameMapRenderComp->actorDebugDrawNode = nullptr;

    gameMapRenderComp->mapRootNode->removeFromParent();
    gameMapRenderComp->mapRootNode = nullptr;
    gameMapRenderComp->distantNode = nullptr;
    gameMapRenderComp->middleNode  = nullptr;
    gameMapRenderComp->nearbyNode  = nullptr;
    gameMapRenderComp->groundNode  = nullptr;
    gameMapRenderComp->regionNode  = nullptr;
    gameMapRenderComp->triggerNode = nullptr;
    gameMapRenderComp->entityNode  = nullptr;
    gameMapRenderComp->caseNode    = nullptr;
    gameMapRenderComp->lightNode   = nullptr;
    gameMapRenderComp->camera.reset();
}

bool GameMapRenderSystem::bindToGameMapRenderComponent(Entity* entity,
                                                       ax::ParallaxNode* parallaxNode,
                                                       std::unique_ptr<VirtualCamera> camera)
{
    if (!parallaxNode)
        return false;

    auto gameMapComp       = MG_GET_COMPONENT(entity, GameMapComponent);
    auto gameMapRenderComp = MG_GET_COMPONENT(entity, GameMapRenderComponent);

    gameMapRenderComp->mapRootNode         = parallaxNode;
    gameMapRenderComp->distantNode         = parallaxNode->getChildByName(kDistantLayerName);
    gameMapRenderComp->middleNode          = parallaxNode->getChildByName(kMiddleLayerName);
    gameMapRenderComp->nearbyNode          = parallaxNode->getChildByName(kNearbyLayerName);
    gameMapRenderComp->groundNode          = parallaxNode->getChildByName(kGroundLayerName);
    gameMapRenderComp->regionNode          = parallaxNode->getChildByName(kRegionLayerName);
    gameMapRenderComp->triggerNode         = parallaxNode->getChildByName(kTriggerLayerName);
    gameMapRenderComp->entityNode          = parallaxNode->getChildByName(kEntityLayerName);
    gameMapRenderComp->caseNode            = parallaxNode->getChildByName(kCaseLayerName);
    gameMapRenderComp->lightNode           = parallaxNode->getChildByName(kLightLayerName);
    gameMapRenderComp->groundDebugDrawNode = nullptr;
    gameMapRenderComp->actorDebugDrawNode  = nullptr;

    if (!gameMapRenderComp->distantNode || !gameMapRenderComp->middleNode || !gameMapRenderComp->nearbyNode ||
        !gameMapRenderComp->groundNode || !gameMapRenderComp->regionNode || !gameMapRenderComp->triggerNode ||
        !gameMapRenderComp->entityNode || !gameMapRenderComp->caseNode || !gameMapRenderComp->lightNode)
    {
        MG_LOG_E(
            "GameMapRenderSystem: missing layers distant={} middle={} nearby={} ground={} region={} trigger={} "
            "entity={} case={} light={}",
            gameMapRenderComp->distantNode != nullptr, gameMapRenderComp->middleNode != nullptr,
            gameMapRenderComp->nearbyNode != nullptr, gameMapRenderComp->groundNode != nullptr,
            gameMapRenderComp->regionNode != nullptr, gameMapRenderComp->triggerNode != nullptr,
            gameMapRenderComp->entityNode != nullptr, gameMapRenderComp->caseNode != nullptr,
            gameMapRenderComp->lightNode != nullptr);
        return false;
    }

    auto* entityNode       = gameMapRenderComp->entityNode;
    ax::DrawNode* drawNode = nullptr;

    drawNode = dynamic_cast<ax::DrawNode*>(entityNode->getChildByName(kGroundDebugDrawNodeName));
    if (!drawNode)
    {
        drawNode = ax::DrawNode::create();
        drawNode->setName(kGroundDebugDrawNodeName);
        entityNode->addChild(drawNode, INT_MIN);
    }
    gameMapRenderComp->groundDebugDrawNode = drawNode;

    drawNode = dynamic_cast<ax::DrawNode*>(entityNode->getChildByName(kActorDebugDrawNodeName));
    if (!drawNode)
    {
        drawNode = ax::DrawNode::create();
        drawNode->setName(kActorDebugDrawNodeName);
        entityNode->addChild(drawNode, 0);
    }
    gameMapRenderComp->actorDebugDrawNode = drawNode;

    this->getGameWord()->getWordRootNode()->addChild(parallaxNode);

    const float mapW = static_cast<float>(gameMapComp->mapWidth);
    const float mapH = static_cast<float>(gameMapComp->mapHeight);

    if (!camera)
    {
        camera = VirtualCamera::create();
        camera->setViewPortSize(ax::Director::getInstance()->getVisibleSize());
        camera->setRegion({mapW, mapH});
        camera->setEnableCollision(true);

        ax::Vec2 focus(mapW * 0.5f, mapH * 0.5f);
        if (!gameMapComp->spawnPoints.empty())
        {
            focus.x = static_cast<float>(gameMapComp->spawnPoints.front().x);
            focus.y = static_cast<float>(gameMapComp->spawnPoints.front().y);
        }
        camera->setFocusPosition(focus);
    }
    else
    {
        camera->setRegion({mapW, mapH});
    }

    camera->setCall([parallaxNode](float x, float y, float scale) {
        parallaxNode->setPosition(x, y);
        parallaxNode->setScale(scale);
    });

    if (!getECSManager()->isDeserialized())
        camera->snapToFocus();

    gameMapRenderComp->camera = std::move(camera);
    MG_LOG_I("GameMapRenderSystem: bound 9 layers size={}x{}", mapW, mapH);
    return true;
}

#else

void GameMapRenderSystem::update() {}

void GameMapRenderSystem::onEntityAdded(Entity* entity) {}

void GameMapRenderSystem::onEntityRemoved(Entity* entity) {}

#endif

NS_MG_END
