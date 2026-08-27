#include "mugen/skill/Skill.h"

#include "mugen/conf/TableConfig.h"
#include "mugen/core/StdC.h"

#include <algorithm>

NS_MG_BEGIN

void Skill::bindFromConfig(const SkillAttackConfig* cfg)
{
    if (!cfg)
        return;
    if (skillAttackId == cfg->id)
        return;
    skillAttackId     = cfg->id;
    nextSkillAttackId = cfg->nextSkill > 0 ? cfg->nextSkill : -1;
    coolDownMaxMs     = cfg->cd > 0 ? cfg->cd : 0;
    coolDownMs        = 0;
    releaseMax        = cfg->cdCount > 0 ? cfg->cdCount : 1;
    releaseCount      = releaseMax;
    coldTimeScale     = 1.0f;
    mpConsumeScale    = 1.0f;
    epConsumeScale    = 1.0f;
}

void Skill::tick(int32_t dtMs)
{
    if (dtMs <= 0 || coolDownMs <= 0)
        return;
    coolDownMs = (std::max)(0, coolDownMs - dtMs);
    if (coolDownMs != 0)
        return;
    if (releaseCount < releaseMax)
        releaseCount = (std::min)(releaseMax, releaseCount + 1);
    if (releaseCount < releaseMax && coolDownMaxMs > 0)
        coolDownMs = static_cast<int32_t>(static_cast<float>(coolDownMaxMs) * coldTimeScale);
}

NS_MG_END
