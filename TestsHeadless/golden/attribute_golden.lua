local root = arg[1] or "AxmolFighter-Config/table"
local function indexed(name)
    local out = {}
    for _, row in ipairs(dofile(root .. "/" .. name .. ".lua")) do out[row.id] = row end
    return out
end
local roles, attributes, hurts = indexed("role"), indexed("attribute"), indexed("skill_hurt")
local function packedFixed(value) return math.floor(value*1048576+0.5)/1048576 end
for _, role in pairs(roles) do
    for i, rate in ipairs(role.attributeRate) do role.attributeRate[i] = packedFixed(rate) end
end
for _, row in pairs(attributes) do
    for _, name in ipairs({"monsterHitNumber", "bossHpRate", "bossHurtRate", "eliteHpRate", "eliteHurtRate"}) do
        row[name] = packedFixed(row[name])
    end
end
local names = {"sourceForce", "agility", "habitus", "spirit", "hp", "atk", "def", "matk", "mdef", "crit", "critResist", "critDamage", "critDamageResist", "dodge", "hit", "baseDamage"}
local output = assert(io.open(arg[2] or "AxmolFighter-Client/TestsHeadless/golden/attribute_golden.csv", "w"))
for _, id in ipairs({261,101,110}) do
    local role = assert(roles[id])
    local base = ({[2]=140000,[3]=150000,[4]=160000})[role.roleType]
    for _, level in ipairs({100,150}) do
        output:write(id, ",", level)
        local initial = attributes[140000]
        local current = attributes[140000 + math.floor(level/100)]
        local units = level%100
        local nextRow = units ~= 0 and attributes[140001 + math.floor(level/100)] or nil
        local endpoint = nextRow or current
        for i, name in ipairs(names) do
            local source = name == "hp" and "baseDamage" or name == "baseDamage" and "hp" or name
            local value = initial[source]
            if math.floor(level/100) == 0 then value = value + nextRow[source]/100*units
            else
                value = value + current[source]
                if nextRow then value = value + (nextRow[source]-current[source])/100*units end
            end
            if name == "hp" then
                value = (value + hurts[math.ceil(level/10)].hurt)*endpoint.monsterHitNumber
                if base+level>160000 then value=value*endpoint.bossHpRate
                elseif base+level>150000 then value=value*endpoint.eliteHpRate end
                value=math.floor(value)
            elseif name == "baseDamage" then
                value=value/endpoint.playerHitNumber
                if base+level>160000 then value=value*endpoint.bossHurtRate
                elseif base+level>150000 then value=value*endpoint.eliteHurtRate end
            end
            output:write(string.format(",%.12f", value*role.attributeRate[i]))
        end
        output:write("\n")
    end
end
output:close()
print("Attribute golden: 3 roles x 2 levels x 16 attributes written")
