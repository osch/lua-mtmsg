local mtmsg = require("mtmsg")
local lst   = mtmsg.newlistener()
local b1    = lst:newbuffer()
local b2    = lst:newbuffer()
b1:addmsg("m1")
b2:addmsg("m2")
b1:addmsg("m3")
b2:setmsg("m4") -- overwrites m2 in b2
b2:addmsg("m5")
assert(lst:nextmsg() == "m1")
assert(lst:nextmsg() == "m4")
assert(lst:nextmsg() == "m3")
assert(lst:nextmsg() == "m5")
assert(lst:nextmsg(0) == nil)