local llthreads = require("llthreads2.ex")
local mtmsg     = require("mtmsg")
local buffer    = mtmsg.newbuffer()

local thread    = llthreads.new(
    function(bufferId)
        local mtmsg = require("mtmsg")
        local buffer = mtmsg.buffer(bufferId)
        return buffer:nextmsg()
    end,
    buffer:id()
)
thread:start()
buffer:addmsg("Hello", "world!")
print(thread:join())
