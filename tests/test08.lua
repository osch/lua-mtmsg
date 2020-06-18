local llthreads = require("llthreads2.ex")
local mtmsg     = require("mtmsg")
local threadIn  = mtmsg.newbuffer()
local threadOut = mtmsg.newbuffer()
local T = 6 -- total time in seconds
local N = 1000
local thread    = llthreads.new(function(threadInId, threadOutId, N)
                                    local mtmsg  = require("mtmsg")
                                    local threadIn  = mtmsg.buffer(threadInId)
                                    local threadOut = mtmsg.buffer(threadOutId)
                                    local stop = false
                                    local objects = {}
                                    for i = 1, N do
                                        objects[i] = {
                                          a = i, b = i + 1, c = i + 3, d = i + 4
                                        }
                                    end
                                    local writeTime = 0
                                    local writeCount = 0
                                    local lastPrint = mtmsg.time()
                                    while not stop do
                                        local msg = threadIn:nextmsg(0.020)
                                        if msg == "stop" then
                                            stop = true
                                        else
                                            threadOut:addmsg("next")
                                            local startT = mtmsg.time()
                                            for i = 1, #objects do
                                                local obj = objects[i]
                                                threadOut:addmsg(i, obj.a, obj.b, obj.c, obj.d)
                                            end
                                            local endT = mtmsg.time()
                                            local dt = endT - startT
                                            writeTime = writeTime + dt
                                            writeCount = writeCount + 1
                                            if  endT > lastPrint + 1 then
                                                io.write(string.format("write: %5.3fms for %d objects\n", 
                                                                       writeTime/writeCount * 1000, N))
                                                lastPrint = endT
                                                writeTime = 0
                                                writeCount = 0
                                            end
                                        end
                                    end
                                end,
                                threadIn:id(), threadOut:id(), N)
thread:start()
local readTime = 0
local readCount = 0
local lastPrint = mtmsg.time()

local startTime = mtmsg.time()

local objects = {}
while mtmsg.time() < startTime + T do
    local startT
    assert("next" == threadOut:nextmsg())
    startT = mtmsg.time()
    for i = 1, N do 
        local i2, a, b, c, d = threadOut:nextmsg()
        assert(i2 == i)
        local obj = objects[i]
        if not obj then
            obj = {}
            objects[i] = obj
        end
        obj.a = a
        obj.b = b
        obj.c = c
        obj.d = d
        assert(obj.a == i)
    end
    local endT = mtmsg.time()
    local dt = endT - startT
    readTime = readTime + dt
    readCount = readCount + 1
    if  endT > lastPrint + 1 then
        io.write(string.format("read : %5.3fms for %d objects\n", 
                               readTime/readCount * 1000, N))
        lastPrint = endT
        readTime = 0
        readCount = 0
    end
end
threadIn:addmsg("stop")
thread:join()
print("OK.")
