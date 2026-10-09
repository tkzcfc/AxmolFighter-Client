local output = assert(io.open(arg[1], "w"))
local elapsed = 0
local interval = 1048576
local dt = 34953
output:write("# frame,tick; BuffBurn id 7 uses one-second interval\n")
for frame = 1, 65 do
    local tick = 0
    if elapsed >= interval then
        elapsed = elapsed - interval
        tick = 1
    else
        elapsed = elapsed + dt
    end
    output:write(string.format("%d,%d\n", frame, tick))
end
output:close()
print("Generated 65 BuffBurn tick timing rows")
