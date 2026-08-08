#include "AvatarComponent.h"

NS_MG_BEGIN

void AvatarComponent::serializeCustomImpl(ByteBuffer& byteBuffer) const
{
    byteBuffer.writeInt32(roleConfig ? roleConfig->id : roleId);
}

bool AvatarComponent::deserializeCustomImpl(ByteBuffer& byteBuffer)
{
    roleId           = byteBuffer.readInt32();
    roleConfig       = nullptr;
    resSpine         = nullptr;
    behaviorTemplate = nullptr;

    if (roleId > 0)
    {
        roleConfig = Config::getInstance()->getRoleConfigById(roleId);
        if (!roleConfig)
        {
            MG_LOG_E("Failed to load RoleConfig id={}", roleId);
            return false;
        }
        if (roleConfig->resSpineId > 0)
            resSpine = Config::getInstance()->getResSpineConfigById(roleConfig->resSpineId);
        if (roleConfig->behaviorTemplateId > 0)
            behaviorTemplate = Config::getInstance()->getBehaviorTemplateConfigById(roleConfig->behaviorTemplateId);
    }
    return true;
}

NS_MG_END
