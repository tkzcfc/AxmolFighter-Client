#include "ui/battle/LocalRoleUtils.h"

#include "AppContext.h"
#include "mugen/common/TypeConversions.h"
#include "mugen/conf/Config.h"

namespace gameui
{

namespace
{
// 英雄 role 101（resSpineId=88101），选不到角色或角色没配置时的兜底
constexpr int32_t kPilotRoleId = 101;
}  // namespace

int32_t resolveLocalPlayerRoleId()
{
    auto* session = AppContext::get().gameSession();
    if (session && session->selectedCharacter.characterID != 0)
    {
        const int32_t roleId = mugen::type_conversions::toRoleConfigId(
            static_cast<mugen::CharacterClass>(session->selectedCharacter.classID));
        if (mugen::Config::getInstance()->getRoleConfigById(roleId))
            return roleId;

        AXLOGW("resolveLocalPlayerRoleId: role {} missing config, fallback to pilot role {}", roleId, kPilotRoleId);
    }
    return kPilotRoleId;
}

}  // namespace gameui
