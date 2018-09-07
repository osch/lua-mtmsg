local mtmsg    = require("mtmsg")

local function printf(...)
    print(string.format(...))
end

local N        = 10000
local withName = true

local function printTime(title, startTime)
    local t = mtmsg.time() - startTime
    printf("%-15s %10.3f kop/sec, %6.3f msecs/op", title, N / t / 1000, t / N * 1000)
end

for run = 1, 2 do
    
    collectgarbage() 

    printf("N = %d, withName = %s", N, tostring(withName))
    
    local idlist = {}
    local bflist = {}
    
    --------------------------------------------------------------
    local startTime = mtmsg.time()
    do
        for i = 1, N do
            local b
            if withName then
                local name = "foo"..i or nil
                b = mtmsg.newbuffer(name)
            else
                b = mtmsg.newbuffer()
            end
            bflist[i] = b
            idlist[i] = b:id()
        end
    end
    printTime("Creation:", startTime)
    --------------------------------------------------------------
    local startTime = mtmsg.time()
    for i = 1, N do
        assert(bflist[i]:id() == idlist[i])
    end
    printTime("Direct call:", startTime)
    --------------------------------------------------------------
    local startTime = mtmsg.time()
    for i = 1, N do
        local id = idlist[i]
        assert(mtmsg.buffer(id):id() == id)
    end
    printTime("Search by id:", startTime)
    --------------------------------------------------------------
    local startTime = mtmsg.time()
    for i = 1, N do
        assert(mtmsg.buffer(idlist[i]):id() == idlist[i])
    end
    printTime("Call by id:", startTime)
    --------------------------------------------------------------
end
print("OK.")
