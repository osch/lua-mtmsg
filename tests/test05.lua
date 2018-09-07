local llthreads = require("llthreads2.ex")
local mtmsg     = require("mtmsg")

local TIME = 2

print(string.format("Measure time %.2f secs", TIME))

local sizes = { 1, 128, 1024, 64 * 1024, 1024 * 1024, 4 * 1024 * 1024 }

for _, SIZE in ipairs(sizes) do
    local threadIn  = mtmsg.newbuffer()
    local lst       = mtmsg.newlistener()
    local threadOut = lst:newbuffer()
    local thread    = llthreads.new(function(SIZE, inId, outId)
                                        local mtmsg     = require("mtmsg")
                                        local threadIn  = mtmsg.buffer(inId)
                                        local threadOut = mtmsg.buffer(outId)
                                        threadOut:addmsg("started")
                                        local outCount = 0
                                        while true do
                                            local cmd = threadIn:nextmsg(0)
                                            if cmd == "exit" then
                                                break
                                            end
                                            local msg = string.rep("x", SIZE)
                                            threadOut:addmsg("msg", msg)
                                            outCount = outCount + #msg
                                        end
                                        threadOut:addmsg("finished")
                                        return outCount
                                    end,
                                    SIZE, 
                                    threadIn:id(),
                                    threadOut:id())
    thread:start()
    assert(lst:nextmsg() == "started")
    local startTime = mtmsg.time()
    local inBytesCount = 0
    local inCount = 0
    while true do
        local type, msg = lst:nextmsg()
        if type == "finished" then
            break
        elseif type == "msg" then
            inBytesCount = inBytesCount + #msg
            inCount = inCount + 1
        else
            error("unexpected")
        end
        local time = mtmsg.time()
        if time - startTime > TIME then
            threadIn:addmsg("exit")
        end
    end
    local totalTime = mtmsg.time() - startTime
    local ok, threadCount = thread:join()
    --print(inBytesCount, threadCount)
    print("*************", string.format("%10.3f MB/sec, %10.0f op/sec, %10d bytes/op, %10.6f msec/op", inBytesCount/totalTime/1024/1024, inCount/totalTime, SIZE, totalTime/inCount*1000))
    assert(inBytesCount == threadCount)
end
print("OK.")
