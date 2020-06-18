local llthreads = require("llthreads2.ex")
local mtmsg     = require("mtmsg")
print(mtmsg._VERSION)
local threadIn  = mtmsg.newbuffer("threadIn")
local listener  = mtmsg.newlistener()
local threadOut1 = listener:newbuffer("threadOut1")
local threadOut2 = listener:newbuffer("threadOut2")

local threadOut1Id = threadOut1:id()
local threadOut2Id = threadOut2:id()

print("=============================")
print(threadOut1)
print(threadOut2)
print("=============================")

local thread    = llthreads.new(function(inId, outId1, outId2)
                                    local mtmsg     = require("mtmsg")
                                    local threadIn  = mtmsg.buffer(inId)
                                    local threadOut1 = mtmsg.buffer(outId1)
                                    local threadOut2 = mtmsg.buffer(outId2)
                                    print(threadOut1)
                                    print(threadOut2)
                                    print("=============================")
                                    threadOut1:addmsg("started")
                                    assert(threadIn:nextmsg() == "continue1")
                                    threadOut2:addmsg("response1")
                                    assert(threadIn:nextmsg() == "continue2")
                                    threadOut2 = nil
                                    collectgarbage()
                                    threadOut1:addmsg("response2")
                                    assert(threadIn:nextmsg() == "exit")
                                    threadOut1:addmsg("finished")
                                    threadOut1 = nil
                                end,
                                threadIn:id(),
                                threadOut1Id,
                                threadOut2Id)
-- threadOut1 = nil -- not now!
-- collectgarbage()

thread:start()

assert(listener:nextmsg() == "started")

threadOut1 = nil -- now it's safe
collectgarbage()

threadIn:addmsg("continue1")

print(mtmsg.buffer(threadOut2Id))

threadOut2 = nil collectgarbage()

print(mtmsg.buffer(threadOut2Id)) collectgarbage()

assert(listener:nextmsg() == "response1")
threadIn:addmsg("continue2")

assert(listener:nextmsg() == "response2")
local _, err = pcall(function() mtmsg.buffer(threadOut2Id) end)
assert(err:match(mtmsg.error.unknown_object))

print(mtmsg.buffer(threadOut1Id)) collectgarbage()

print("=============================")

threadIn:addmsg("exit")
assert(thread:join())

print("=============================")

local _, err = pcall(function() mtmsg.buffer(threadOut1Id) end)
assert(err:match(mtmsg.error.unknown_object))

assert(listener:nextmsg() == "finished")

print("=============================")

local _, err = pcall(function() listener:nextmsg() end)
assert(err:match(mtmsg.error.no_buffers))

print("OK.")
