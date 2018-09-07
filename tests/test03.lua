print("test03 starting...")
local llthreads = require("llthreads2.ex")
local mtmsg     = require("mtmsg")

local newbuffer = mtmsg.newbuffer


local threadInBuffer   = mtmsg.newbuffer()
local threadOutBuffers = {}
local listener         = mtmsg.newlistener()

local function f(x)
    local rslt = 1.0
    for i = 1, 5000 do
        rslt = rslt + (math.sin(i * 1/x))^2
    end
    return math.floor(rslt * 100)
end

local threadList = {}
for i = 1, 4 do
    threadOutBuffers[#threadOutBuffers + 1] = listener:newbuffer()
    
    local thread_code = function(...)
            local mtmsg = require("mtmsg")
            local f, threadId, id1, id2 = ...
            f = loadstring and loadstring(f) or load(f)
            local threadInBuffer  = mtmsg.buffer(id1)
            local threadOutBuffer = mtmsg.buffer(id2)
            local b               = mtmsg.newbuffer()
            -- print thread's parameter.
            print("CHILD: received params:", select(2, ...))
            threadOutBuffer:addmsg("started")
            local shouldExit = false
            local counter = 0
            local missedCounter = 0
            local maxMissedCounter = 0
            --threadOutBuffer:nonblock(true)
            while not shouldExit do
                local in1, in2 = threadInBuffer:nextmsg()
                if in1 == "exit" then
                    print("shouldExit " .. threadId ..": " .. in2)
                    shouldExit = true
                else
                    local rslt = f(in1)
                    --b:nextmsg(math.random() * 0.00001) -- sleep
                    local c = 0
                    while not threadOutBuffer:addmsg(in1, rslt) do
                        c = c + 1
                    end
                    if c > 0 then
                        missedCounter = missedCounter + 1
                    end
                    if c > maxMissedCounter then
                        maxMissedCounter = c
                    end
                    counter = counter + 1
                end
            end
            return threadId, counter, missedCounter, maxMissedCounter
    end
    
    local thread = llthreads.new(thread_code,
                                 string.dump(f),
                                 i,
                                 threadInBuffer:id(),
                                 threadOutBuffers[#threadOutBuffers]:id())
    assert(thread:start())
    threadList[#threadList + 1] = thread
    print(listener:nextmsg())
end

local COUNT = 10000

local expectedList = {}
for i = 1, COUNT do
    local rslt = f(i)
    expectedList[i] = rslt
end

-----------------------------------------------------------------------
local startTime = mtmsg.time()

for i = 1, COUNT do
    threadInBuffer:addmsg(i)
end
print("waiting...")
for i = 1, COUNT do
    local j, rslt = listener:nextmsg()
    local expected = expectedList[j]
    local diff = expected - rslt
--    print("f("..j..") = " .. rslt)
    assert(diff == 0, diff)
    expectedList[j] = nil
end
print(" --------------------------------- Time:", mtmsg.time() - startTime)

assert(#expectedList == 0)

local pack = table.pack and table.pack or function(...)
    return { n = select('#', ...), ... }
end
local unpack = table.unpack and table.unpack or unpack

while true do
    local r = pack(listener:nextmsg(0))
    print("received", unpack(r))
    if #r == 0 then 
        break
    end
end
for i, thread in ipairs(threadList) do
    threadInBuffer:addmsg("exit", i)
end
for _, thread in ipairs(threadList) do
    print("PARENT: child returned: ", thread:join())
end

print("OK.")
