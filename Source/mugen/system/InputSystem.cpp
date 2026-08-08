#include "InputSystem.h"
#include "mugen/Components.h"

NS_MG_BEGIN

InputSystem::InputSystem() {}
InputSystem::~InputSystem() {}

void InputSystem::init(ECSManager* ecs)
{
    Super::init(ecs);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, InputComponent);
}

void InputSystem::onEntityAdded(Entity* entity)
{
    if (getECSManager()->isDeserialized())
    {
        return;
    }

    auto inputComponent         = MG_GET_COMPONENT(entity, InputComponent);
    inputComponent->lastKeyDown = 0;
    inputComponent->keyDown     = 0;
    inputComponent->keyPressedDurationMs.fill(0);
}

void InputSystem::onEntityRemoved(Entity* entity) {}

void InputSystem::update()
{
    int32_t lastUpdateTimeMs = getECSManager()->getLastUpdateTimeMs();
    int64_t runningTimeMs    = getECSManager()->getRunningTimeMs();

    // 保存上一帧输入状态
    for (auto& entity : entities)
    {
        auto inputComp = MG_GET_COMPONENT(entity, InputComponent);

        if (inputComp->keyDown != 0 || inputComp->lastKeyDown != 0)
        {
            for (int32_t i = 1; i < INPUT_SLOT_MAX; ++i)
            {
                // 上一帧按键为按下状态
                if (inputComp->isLastKeyDown(i))
                {
                    // 当前帧按键仍为按下状态,则表示此按键持续按下；否则表示此按键抬起
                    if (inputComp->isKeyDown(i))
                    {
                        inputComp->keyPressedDurationMs[i] += lastUpdateTimeMs;
                    }
                    else
                    {
                        inputComp->keyLastUpTimestampMs[i] = runningTimeMs;
                        inputComp->keyPressedDurationMs[i] = 0;
                    }
                }
                else
                {
                    if (inputComp->isKeyDown(i))
                    {
                        inputComp->keyLastDownTimestampMs[i] = runningTimeMs;
                        inputComp->keyPressedDurationMs[i]   = 0;
                    }
                }
            }
        }
        // 更新上一帧按键状态
        inputComp->lastKeyDown = inputComp->keyDown;
    }
}

NS_MG_END
