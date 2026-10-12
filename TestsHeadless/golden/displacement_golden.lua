local rows = dofile('AxmolFighter-Config/table/displacement.lua')
local q = function(v) return math.floor(v * 1048576 + 0.5) / 1048576 end
local selected = {rows[1]}
for _, r in ipairs(rows) do
    if #selected == 1 and r.velocity[2] > 0 and r.bounces > 0 and r.gravity > 0 then
        selected[2] = r
        break
    end
end
for _, r in ipairs(rows) do
    if r.velocity[1] > 0 and r.acceleration[1] < 0 then selected[3] = r; break end
end
assert(#selected == 3)
local function axis(time, velocity, acceleration, interval, duration, delta, gravity)
    local dt = 0
    if interval > 0 then
        if time <= interval then dt = delta
        elseif time - interval < delta then dt = delta - (time - interval) end
    elseif interval == -1 then dt = delta end
    local next = velocity + acceleration * dt + gravity * delta
    local offset = 0
    if duration > 0 then
        if time <= duration then offset = 0.5 * (velocity + next) * dt + next * (delta - dt)
        else
            local t = time - duration
            if t < delta then
                if t > dt then offset = 0.5 * (velocity + next) * dt + next * (t - dt)
                else offset = 0.5 * (velocity + next) * t end
            else next = 0 end
        end
    elseif duration == -1 then offset = 0.5 * (velocity + next) * dt + next * (delta - dt)
    else offset = next * delta end
    return offset, next
end
local out = assert(io.open('AxmolFighter-Client/TestsHeadless/golden/displacement_golden.csv', 'w'))
local dt, gravity = q(33.33332), q(-0.0022)
for _, r in ipairs(selected) do
    local p, v = {0,0,0}, {q(r.velocity[1]),q(r.velocity[2]),q(r.velocity[3] or 0)}
    for frame = 1, 90 do
        for a = 1, 3 do
            local g = a == 2 and not (p[2] == 0 and v[2] == 0) and q(r.gravity)*gravity or 0
            local offset
            offset, v[a] = axis(frame*dt, v[a], q(r.acceleration[a]), q(r.accelerationTime[a]), q(r.velocityTime[a]), dt, g)
            p[a] = p[a] + offset * (a == 3 and q(0.55) or 1)
        end
        if p[2] < 0 then
            p[2] = 0
            if v[2] < 0 then
                if v[2] < -q(0.2) and r.bounces > 0 then v[2] = -v[2]*q(r.bounces)
                else v = {0,0,0} end
            end
        end
        out:write(string.format('%d,%d,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f\n',r.id,frame,p[1],p[2],p[3],v[1],v[2],v[3]))
    end
    print('Displacement golden id '..r.id..': 90 frames')
end
out:close()
