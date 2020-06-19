if jit then
   print("Skipping test under LuaJit (more details: https://github.com/osch/lua-threading-playground)")
   return 
end

local llthreads = require("llthreads2.ex")
local mtmsg     = require("mtmsg")

local THREAD_COUNT = 100

local thread_code = function(...)
    local mtmsg = require("mtmsg")
    local threadId, id1, id2 = ...
    local threadInBuffer  = mtmsg.buffer(id1)
    local threadOutBuffer = mtmsg.buffer(id2)
    -- print thread's parameter.
    --print("CHILD: received params:", select(2, ...))
    local shouldExit = false
    local task1Count = 0
    local task2Count = 0
    while not shouldExit do
        local action, taskId, arg = threadInBuffer:nextmsg()
        if action == "exit" then
            --print("shouldExit " .. threadId)
            shouldExit = true
        elseif action == "task1" then
            mtmsg.sleep(math.random() * 0.001)
            do  local c = 0
                for i = 1, 100 * math.random() do c = c + math.sin(c) end end
            local b = mtmsg.buffer(arg)
            b:addmsg(b:id())
            threadOutBuffer:addmsg("task1", threadId, taskId, b:id())
            task1Count = task1Count + 1
        end
    end
    threadOutBuffer:addmsg("terminated", threadId)
    return task1Count, task2Count
end

local listener = mtmsg.newlistener()

local threadList = {}
for i = 1, THREAD_COUNT do
    local entry = {}
    entry.threadIn  = mtmsg.newbuffer()
    entry.threadOut = listener:newbuffer()
    entry.thread    = llthreads.new(thread_code,
                                    i,
                                    entry.threadIn:id(),
                                    entry.threadOut:id())
    assert(entry.thread:start())
    threadList[#threadList + 1] = entry
    
end

local entryId = 1
local function nextEntry()
    entryId = entryId + 1
    if entryId > THREAD_COUNT then
        entryId = 1
    end
    return threadList[entryId]
end

local tasks = {
    task1 = (function()
        local impl = {}
        local list = {}
        local listener = mtmsg.newlistener()
        do
            local buf = listener:newbuffer()
            assert(listener:nextmsg(0) == nil)
        end
    
        impl.trigger = function()
            local r = math.random()
            while true do
                local entry = nextEntry()
                if not entry.terminating then
                    local b = (r > 0.5) and listener:newbuffer() or mtmsg.newbuffer()
                    local task = { arg = b }
                    list[#list + 1] = task
                    entry.threadIn:addmsg("task1", #list, b:id())
                    break
                end
            end
        end
        
        impl.handleResponse = function(taskId, taskRslt, threadId) 
            local task = list[taskId]
            assert(task) assert(task.rslt == nil) assert(task.threadId == nil)
            task.rslt = mtmsg.buffer(taskRslt)
            assert(task.arg:id() == task.rslt:id())
            task.threadId = threadId
            task.arg = nil
        end
    
        local threadResults = {}
        
        impl.handleJoin = function(threadId, task1Count)
            local rslt = {}
            threadResults[threadId] = rslt
            rslt.expectedTaskCount = task1Count
            rslt.taskCount = 0
        end
        
        impl.checkFinish = function()
            for _, t in ipairs(list) do
                local id = t.rslt:nextmsg()
                assert(id == t.rslt:id())
                threadResults[t.threadId].taskCount = threadResults[t.threadId].taskCount + 1
                t.rslt = nil
            end
            assert(#threadResults == THREAD_COUNT)
            for _, entry in ipairs(threadResults) do
                assert(entry.expectedTaskCount == entry.taskCount)
            end
            collectgarbage()
            local ok, err = pcall(function() listener:nextmsg(0) end)
            assert(not ok and err:match(mtmsg.error.no_buffers))
        end
        
        return impl
    end)(),
    
}

local terminating = 0
local terminated  = 0
local tocalCount = 0
local gcCount = 0

while terminated < THREAD_COUNT do
    tocalCount = tocalCount + 1
    local r = math.random()
    --print(r)
    local taskType, threadId, taskId, taskRslt  = listener:nextmsg(0)
    if taskType then
        --print("received", taskType, threadId, taskId, taskRslt)
        if taskType == "terminated" then
            assert(threadList[threadId].terminated == nil)
            threadList[threadId].terminated = true
            terminated = terminated + 1
        else
            tasks[taskType].handleResponse(taskId, taskRslt, threadId)
        end
    end
    if r < 0.002 then
        gcCount = gcCount + 1
        collectgarbage()
    end
    if terminating < THREAD_COUNT then
        --if r > 0.5 then
            tasks.task1.trigger()
        --end
        if r < 0.001 then
            while true do
                local entry = nextEntry()
                if not entry.terminating then
                    entry.threadIn:addmsg("exit")
                    entry.terminating = true
                    terminating = terminating + 1
                    break
                end
            end
        end
    end
end
print("---")
local rsltOut = mtmsg.newbuffer()
for threadId, entry in ipairs(threadList) do
    local ok, task1Count = entry.thread:join()
    rsltOut:addmsg("PARENT: child returned: " .. (ok and "ok" or "err").. " " .. task1Count)
    tasks.task1.handleJoin(threadId, task1Count)
end
tasks.task1.checkFinish()

local line = rsltOut:nextmsg(0)
while line do
    print(line)
    line = rsltOut:nextmsg(0)
end
print("Total:", tocalCount, gcCount)
print("Done.")
