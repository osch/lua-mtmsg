local mtmsg     = require("mtmsg")
local b         = mtmsg.newbuffer()

local COUNT = 10000

local function f(x)
    local rslt = 1.0
    for i = 1, 5000 do
        rslt = rslt + (math.sin(i * 1/x))^2
    end
    b:nextmsg(math.random() * 0.00001)
    return rslt
end

local expectedList = {}
for i = 1, COUNT do
    local rslt = f(i)
--    print("f("..i..") = " .. rslt)
    expectedList[i] = rslt
end

-----------------------------------------------------------------------
local startTime = mtmsg.time()

for i = 1, COUNT do
    local rslt = f(i)
    local expected = expectedList[i]
    local diff = expected - rslt
--    print("f("..i..") = " .. rslt)
    assert(diff == 0)
    expectedList[i] = nil
end

print(" --------------------------------- Time:", mtmsg.time() - startTime)

assert(#expectedList == 0)
