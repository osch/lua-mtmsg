# mtmsg 
[![Licence](http://img.shields.io/badge/Licence-MIT-brightgreen.svg)](LICENSE)
[![Build Status](https://travis-ci.org/osch/lua-mtmsg.png?branch=master)](https://travis-ci.org/osch/lua-mtmsg)
[![Build status](https://ci.appveyor.com/api/projects/status/r4gwhqv2jvv2jirn?svg=true)](https://ci.appveyor.com/project/osch/lua-mtmsg)

Low-level multi-threading message buffers for the [Lua] scripting language.

This package provides in-memory message buffers for inter-thread communication. 
The implementation is independent from the underlying threading library
(e.g. [Lanes] or [lua-llthreads2]).

This package is also available via LuaRocks, see https://luarocks.org/modules/osch/mtmsg.

[Lua]:               https://www.lua.org
[Lanes]:             https://luarocks.org/modules/benoitgermain/lanes
[lua-llthreads2]:    https://luarocks.org/modules/moteus/lua-llthreads2


## Example

Simple example, passes message buffer by integer id to another
thread:

```lua
local llthreads = require("llthreads2.ex")
local mtmsg     = require("mtmsg")
local buffer    = mtmsg.newbuffer()
local thread    = llthreads.new(function(bufferId)
                                    local mtmsg  = require("mtmsg")
                                    local buffer = mtmsg.buffer(bufferId)
                                    return buffer:nextmsg()
                                end,
                                buffer:id())
thread:start()
buffer:addmsg("Hello", "world!")
print(thread:join())
```

## Documentation

   * [Module Functions](#module-functions)
       * mtmsg.newbuffer()
       * mtmsg.buffer()
       * mtmsg.newlistener()
       * mtmsg.listener()
   * [Buffer Methods](#buffer-methods)
       * buffer:id()
       * buffer:name()
       * buffer:addmsg()
       * buffer:setmsg()
       * buffer:nextmsg()
   * [Listener Methods](#listener-methods)
       * listener:id()
       * listener:name()
       * listener:newbuffer()
       * listener:nextmsg()

### Module Functions

* **`mtmsg.newbuffer([name,][size1[,size2[,grow]]]`**

  Creates a new buffer and returns a lua object for referencing the
  created buffer.

    * `name`  optional string, the name of the new buffer,
    * `size1` optional integer, initial size in bytes for internal memory 
              chunks holding one message (defaults to `128`),
    * `size2` optional integer, initial size in bytes for the buffer that
              holds all messages for this buffer (defaults to `1024`),
    * `grow`  optional float, the factor how fast buffer memory grows
              if more data has to be buffered. If `0`, the used
              memory is fixed by the initially given sizes. If `<=1` the 
              buffer grows only by the needed bytes (defaults to `2`).

  The created buffer is garbage collected if the last object referencing this
  buffer vanishes.

* **`mtmsg.buffer(id|name)`**

  Creates a lua object for referencing an existing buffer. The buffer must
  be referenced by its `id` or `name`.

    * `id` integer, the unique buffer id that can be obtained by
           `buffer:id()`.

    * `name` string, the optional name that was given when the
             buffer was created with `mtmsg.newbuffer()` or with 
             `listener:newbuffer()`.
             
* **`mtmsg.newlistener([name])`**

  Creates a new buffer listener and returns a lua object for referencing the
  created listener.
  
    * `name` optional string, the name of the new listener.

  The created listener is garbage collected if the last reference object to this
  listener vanishes.

* **`mtmsg.listener(id|name)`**

  Creates a lua object for referencing an existing listener. The listener must
  be referenced by its `id` or `name`.

    * `id` integer, the unique buffer id that can be obtained by
           `listener:id()`.

    * `name` string, the optional name that was given when the
             listener was created with `mtmsg.newlistener()`
             
### Buffer Methods

* **`buffer:id()`**
  
  Returns the buffer id as integer. The id is generated when `mtmsg.newbuffer()`
  or `listener:newbuffer()` is invoked and is unique for the whole process.

* **`buffer:name()`**

  Returns the buffer's name that was given to `mtmsg.newbuffer()` or to
  `listener:newbuffer()`.
  
* **`buffer:addmsg(...)`**

  Adds the arguments together as one message to the buffer. Arguments can be
  simple data types (string, number, boolean, light user data).
  
* **`buffer:setmsg(...)`**

  Sets the arguments together as one message into the buffer. All other messages
  in this buffer are discarded. Arguments can be simple data types 
  (string, number, boolean, light user data).
  
* **`buffer:nextmsg([timeout])`**

  Returns all the arguments that were given as one message by invoking the method
  `buffer:addmsg()` or `buffer:setmsg()`. The returned message is removed
  from the underlying buffer.
  
  * `timeout` optional float, maximal time in seconds for waiting for the next 
    message. The method returns without result, when timeout is reached.
    

### Listener Methods


* **`listener:id()`**
  
  Returns the listener id as integer. The id is generated when `mtmsg.newlistener()`
  is invoked and is unique for the whole process.

* **`listener:name()`**

  Returns the listener's name that was given to `mtmsg.newlistener()`.


* **`listener:newbuffer([name,][size1[,size2[,grow]]]`**

  Creates a new buffer that is connected to the listener and returns a lua object 
  for referencing the created buffer.
  
  The arguments are the same as for the module function `mtmsg.newbuffer`.

  The created buffer is garbage collected if the last object referencing this
  buffer vanishes. The buffer is not referenced by the connected listener.

* **`listener:nextmsg([timeout])`**

  Returns all the arguments that were given as one message by invoking the method
  `buffer:addmsg()` or `buffer:setmsg()` to one of the buffers that are
  connected to this listener.  The returned message is removed
  from the underlying buffer.
  
  * `timeout` optional float, maximal time in seconds for waiting for the next 
    message. The method returns without result, when timeout is reached.
    
