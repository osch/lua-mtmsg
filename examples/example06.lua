local llthreads  = require("llthreads2.ex")
local mtmsg      = require("mtmsg")
local threadIn   = mtmsg.newbuffer()
local lst        = mtmsg.newlistener()
local threadOut1 = lst:newbuffer()
local threadOut2 = lst:newbuffer()

local notifier = mtmsg.newbuffer()  -- buffer used as common notifier

threadOut1:notifier(notifier)
threadOut2:notifier(notifier)

local thread    = llthreads.new(function(inId, outId1, outId2)
                                    local mtmsg      = require("mtmsg")
                                    local threadIn   = mtmsg.buffer(inId)
                                    local threadOut1 = mtmsg.buffer(outId1)
                                    local threadOut2 = mtmsg.buffer(outId2)

                                    threadOut1:addmsg("started")

                                    assert(threadIn:nextmsg() == "next1") -- blocks for next message
                                    threadOut2:addmsg(nil)

                                    assert(threadIn:nextmsg() == "next2") -- blocks for next message
                                    threadOut1:addmsg() -- empty message

                                    assert(threadIn:nextmsg() == "exit")  -- blocks for next message
                                    threadOut1:addmsg("finished")
                                end,
                                threadIn:id(),
                                threadOut1:id(), threadOut2:id())
thread:start()

local function assertNone(...)
    assert(select("#", ...) == 0)
end

local function assertIs(expected, ...)
    assert(select("#", ...) == 1)
    local value = ...
    assert(value == expected)
end

do
    assertNone(         notifier:nextmsg())     -- blocks for next message
    assertIs("started", threadOut1:nextmsg(0))  -- nonblocking read from buffer1
    assertNone(         threadOut2:nextmsg(0))  -- nonblocking read from buffer2
    threadIn:addmsg("next1")

    assertNone(   notifier:nextmsg())           -- blocks for next message
    assertNone(   threadOut1:nextmsg(0))        -- nonblocking read from buffer1
    assertIs(nil, threadOut2:nextmsg(0))        -- nonblocking read from buffer2
    threadIn:addmsg("next2")

    assertNone(   notifier:nextmsg())           -- blocks for next message
    assertNone(   threadOut1:nextmsg(0))        -- nonblocking read from buffer1
    assertNone(   threadOut2:nextmsg(0))        -- nonblocking read from buffer2
    threadIn:addmsg("exit")

    assertNone(          notifier:nextmsg())    -- blocks for next message
    assertIs("finished", threadOut1:nextmsg(0)) -- nonblocking read from buffer1
    assertNone(          threadOut2:nextmsg(0)) -- nonblocking read from buffer2
end

print("Example06 OK.")
