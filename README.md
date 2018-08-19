# mtmsg 

Low-level multi-threading message buffers for the [Lua] scripting language.

This package provides in-memory message buffers for inter-thread communication. 
The implementation is independent from the underlying threading library
(e.g. [Lanes] or [lua-llthreads2]).

## Example

Simple *Hello world!* example:

```lua
local llthreads = require("llthreads2.ex")
local mtmsg     = require("mtmsg")
local buffer    = mtmsg.newbuffer()
local thread    = llthreads.new(function(bufferId)
                                    local mtmsg = require("mtmsg")
                                    local buffer = mtmsg.buffer(bufferId)
                                    return buffer:nextmsg()
                                end,
                                buffer:id())
thread:start()
buffer:addmsg("Hello", "world!")
print(thread:join())
```

[Lua]:               https://www.lua.org
[Lanes]:             https://luarocks.org/modules/benoitgermain/lanes
[lua-llthreads2]:    https://luarocks.org/modules/moteus/lua-llthreads2

