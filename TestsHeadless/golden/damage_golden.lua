-- 独立双精度公式；输入和抽样写入 CSV，C++ 不重建生成器。
local output = assert(io.open(arg[1], "w"))
local seed = 2222
local function integer(max)
    seed = (seed * 48271) % 2147483647
    return seed % max
end
local function clamp(x, low, high) return math.max(low, math.min(high, x)) end
output:write("id,type,must,artifact,standard,addition,atk,matk,def,mdef,crit,critdef,crithurt,crithurtdef,dodge,hit,addhurt,avoidhurt,addcrit,avoidcrit,adddodge,addhit,addcrithurt,addcrithurtdef,artifacthurt,rate,r0,r1,r2,damage,critical,miss,draws\n")
for id = 1, 200 do
    local t, must, artifact = (id-1)%4, id%3 == 0 and 1 or 0, id%2
    local standard, addition = integer(6000), integer(5)/8
    local atk, matk, def, mdef = integer(2000), integer(2000), integer(2000), integer(2000)
    local crit, critdef, ch, chdef = integer(150), integer(100), integer(200), integer(100)
    local dodge, hit = integer(100), integer(100)
    local ah, sh = integer(8)/8, integer(12)/8
    local ac, sc, ad, ai, ach, sch, artifacthurt = integer(20), integer(20), integer(20), integer(20), integer(20), integer(20), integer(8)/8
    local rate = integer(24)/8
    local random = {integer(100*1024)/1024, integer(100*1024)/1024, integer(100*1024)/1024}
    local cursor = 0
    local function draw() cursor = cursor+1; return random[cursor] end
    local miss = must == 0 and clamp((dodge+ad-hit-ai)/100,0,0.5)*100 >= draw()
    local hurt, critical = 0, false
    if not miss then
        local a,d
        if t == 0 or (t == 2 and atk >= matk) then a,d = atk,def else a,d = matk,mdef end
        local fraction = t == 3 and 1 or (a+d == 0 and 0 or a/(a+d))
        hurt = standard * fraction
        hurt = hurt * (rate * (1+addition))
        critical = clamp((crit+ac-critdef-sc)/100,0.01,1)*100 >= draw()
        if critical then hurt = clamp(1.5+(ch+ach-chdef-sch)/100,1.5,2.5)*hurt end
        hurt = hurt * (1+ah)
        hurt = hurt * (1-sh)
        if artifact == 1 then hurt = hurt * (1+artifacthurt) end
        hurt = math.floor(math.max(hurt*(0.02*draw()+99)/100,0))
    end
    local row = {id,t,must,artifact,standard,addition,atk,matk,def,mdef,crit,critdef,ch,chdef,dodge,hit,ah,sh,ac,sc,ad,ai,ach,sch,artifacthurt,rate,random[1],random[2],random[3],hurt,critical and 1 or 0,miss and 1 or 0,cursor}
    for i,v in ipairs(row) do output:write(i == 1 and "" or ",", string.format("%.17g",v)) end
    output:write("\n")
end
output:close()
print("Generated 200 damage golden groups")
