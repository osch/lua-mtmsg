local mtmsg  = require("mtmsg")

local function PRINT(s)
    print(s.." ("..debug.getinfo(2).currentline..")")
end
local function msgh(err)
    return debug.traceback(err, 2)
end
local function pcall(f, ...)
    return xpcall(f, msgh, ...)
end

PRINT("==================================================================================")
do
    mtmsg.newwriter()
    local ok, err = pcall(function() mtmsg.newwriter("") end)
    assert(not ok)
    print(err)
    mtmsg.newwriter(1, 2)
    mtmsg.newwriter(1, 2.5)
end
PRINT("==================================================================================")
do
    local w = mtmsg.newwriter(4, 0)
    w:add("1")
    local ok, err = pcall(function() w:add("1") end)
    print(err)
    assert(not ok and err:match(mtmsg.error.message_size))
end
PRINT("==================================================================================")
do
    local w = mtmsg.newwriter(4, 1)
    for i = 1, 4000 do
        w:add(i)
    end
    local b = mtmsg.newbuffer()
    w:addmsg(b)
    local r = mtmsg.newreader()
    r:nextmsg(b)
    for i = 1, 4000 do
        assert(i == r:next())
    end
    assert(nil == r:next())
end
PRINT("==================================================================================")
do
    local w = mtmsg.newwriter(4, 1)
    w:add("1") w:add("2")
    local b = mtmsg.newbuffer()
    w:addmsg(b)
    local r = mtmsg.newreader(4, 0)
    local ok, err = pcall(function() r:nextmsg(b) end)
    print(err)
    assert(not ok and err:match(mtmsg.error.message_size))
end
PRINT("==================================================================================")
do
    local N = 100000
    local w = mtmsg.newwriter()
    for i = 1, N do
        w:add(i)
    end
    local b = mtmsg.newbuffer()
    w:addmsg(b)
    local r = mtmsg.newreader()
    r:nextmsg(b)
    for i = 1, N do
        assert(r:next() == i)
    end
    assert(r:next() == nil)
    b:addmsg("abc"); r:nextmsg(b)
    assert(r:next() == "abc")
    print("waiting...")
    assert(r:nextmsg(b, 0.5) == false)
    print("waiting finished.")

    b:addmsg("abc"); r:nextmsg(b)
    r:clear()
    assert(r:next() == nil)
    assert(r:next() == nil)

    b:addmsg("abc"); r:nextmsg(b)
    b:addmsg("xyz"); r:nextmsg(b)
    assert(r:next() == "xyz")
    assert(r:next() == nil)
    
    w:add("xyz"); w:addmsg(b); 
    assert(b:nextmsg() == "xyz")
    assert(b:nextmsg(0) == nil)

    w:add("xyz"); w:clear(); w:add("abc"); w:addmsg(b); 
    assert(b:nextmsg() == "abc")
    assert(b:nextmsg(0) == nil)
    
    w:addmsg(b)
    assert(b:nextmsg() == nil)
end
PRINT("==================================================================================")
print("OK.")
