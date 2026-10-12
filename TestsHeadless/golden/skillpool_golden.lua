local root = arg[1] or "AxmolFighter-Config/table"
local function indexed(name)
    local result = {}
    for _, row in ipairs(dofile(root .. "/" .. name .. ".lua")) do result[row.id] = row end
    return result
end
local roles, ais, skills = indexed("role"), indexed("ai"), indexed("skill")
local ids = {}
for id in pairs(roles) do ids[#ids + 1] = id end
table.sort(ids)
local picked, counts = {}, {0, 0}
for _, id in ipairs(ids) do
    local role = roles[id]
    local kind = role.roleType == 1 and 1 or role.roleType == 2 and 2 or nil
    local ai = ais[role.aiId[1]]
    if kind and counts[kind] < 2 and ai and #ai.skillIds > 0 and skills[ai.skillIds[1][1]] then
        picked[#picked + 1] = id
        counts[kind] = counts[kind] + 1
    end
end
assert(counts[1] == 2 and counts[2] == 2)
local output = assert(io.open(arg[2] or "AxmolFighter-Client/TestsHeadless/golden/skillpool_golden.csv", "w"))
local rows = 0
for _, id in ipairs(picked) do
    local ai = ais[roles[id].aiId[1]]
    local groups = {ai.skillIds, ai.crazySkillIds, ai.joystickSkillIds, ai.joystickSkillIds, ai.otherSkillIds}
    for group, slots in ipairs(groups) do
        for slot, links in ipairs(slots) do
            if #links > 0 and links[1] ~= -1 then
                local name
                if group == 3 or group == 4 then name = slot .. (group == 3 and "+" or "++")
                else name = string.char((group == 5 and 90 - #slots or 64) + slot) .. (group == 2 and "+" or "") end
                local steps = 0
                for _, skillId in ipairs(links) do
                    local seen = {}
                    while skillId ~= -1 and skills[skillId] do
                        assert(not seen[skillId], "cyclic next_skill")
                        seen[skillId] = true
                        steps = steps + 1
                        skillId = skills[skillId].nextSkill
                    end
                end
                if steps > 0 then
                    output:write(id, ",", name, ",", steps, "\n")
                    rows = rows + 1
                end
            end
        end
    end
end
output:close()
print("Skill golden: 2 heroes + 2 monsters; " .. rows .. " ordered slot/step rows")
