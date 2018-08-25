# mtmsg 
[![Licence](http://img.shields.io/badge/Licence-MIT-brightgreen.svg)](LICENSE)
[![Build Status](https://travis-ci.org/osch/lua-mtmsg.png?branch=master)](https://travis-ci.org/osch/lua-mtmsg)
[![Build status](https://ci.appveyor.com/api/projects/status/r4gwhqv2jvv2jirn?svg=true)](https://ci.appveyor.com/project/osch/lua-mtmsg)

<!-- ---------------------------------------------------------------------------------------- -->

Low-level multi-threading message buffers for the [Lua] scripting language.

This package provides in-memory message buffers for inter-thread communication. 
The implementation is independent from the underlying threading library
(e.g. [Lanes] or [lua-llthreads2]).

This package is also available via LuaRocks, see https://luarocks.org/modules/osch/mtmsg.

[Lua]:               https://www.lua.org
[Lanes]:             https://luarocks.org/modules/benoitgermain/lanes
[lua-llthreads2]:    https://luarocks.org/modules/moteus/lua-llthreads2

See below for full [reference documentation](#documentation) .

<!-- ---------------------------------------------------------------------------------------- -->

## Examples

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

Multiple buffers can be connected to one listener:

```lua
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
```

<!-- ---------------------------------------------------------------------------------------- -->

## Documentation

   * [Module Functions](#module-functions)
       * mtmsg.newbuffer()
       * mtmsg.buffer()
       * mtmsg.newlistener()
       * mtmsg.listener()
       * mtmsg.abort()
       * mtmsg.isabort()
       * mtmsg.time()
       * mtmsg.sleep()
   * [Buffer Methods](#buffer-methods)
       * buffer:id()
       * buffer:name()
       * buffer:addmsg()
       * buffer:setmsg()
       * buffer:clear()
       * buffer:nextmsg()
       * buffer:nonblock()
       * buffer:isnonblock()
       * buffer:close()
       * buffer:abort()
       * buffer:isabort()
   * [Listener Methods](#listener-methods)
       * listener:id()
       * listener:name()
       * listener:newbuffer()
       * listener:nextmsg()
       * listener:nonblock()
       * listener:isnonblock()
       * listener:close()
       * listener:abort()
       * listener:isabort()
   * [Error Methods](#error-methods)
       * error:name()
       * error:details()
       * error:traceback()
       * error:message()
       * err1 == err2
   * [Errors](#errors)
       * mtmsg.error.message_size
       * mtmsg.error.no_buffers
       * mtmsg.error.object_closed
       * mtmsg.error.object_exists
       * mtmsg.error.operation_aborted
       * mtmsg.error.out_of_memory
       * mtmsg.error.unknown_object
       

<!-- ---------------------------------------------------------------------------------------- -->

### Module Functions

* **`mtmsg.newbuffer([name,][size1[,size2[,grow]]]`**

  Creates a new buffer and returns a lua object for referencing the
  created buffer.

    * *name*  - optional string, the name of the new buffer,
    * *size1* - optional integer, initial size in bytes for internal memory 
                chunks holding one message (defaults to *128*),
    * *size2* - optional integer, initial size in bytes for the buffer that
                holds all messages for this buffer (defaults to *1024*),
                must be greater or equal than *size1*,
    * *grow*  - optional float, the factor how fast buffer memory grows
                if more data has to be buffered. If *0*, the used
                memory is fixed by the initially given sizes. If *<=1* the 
                buffer grows only by the needed bytes (default value is *2*,
                i.e. the size doubles if buffer memory needs to grow).
  
  The created buffer is garbage collected if the last object referencing this
  buffer vanishes.
  
  Possible errors: *mtmsg.error.object_exists*,
                   *mtmsg.error.operation_aborted*
                   

* **`mtmsg.buffer(id|name)`**

  Creates a lua object for referencing an existing buffer. The buffer must
  be referenced by its *id* or *name*.

    * *id* - integer, the unique buffer id that can be obtained by
             *buffer:id()*.

    * *name* - string, the optional name that was given when the
               buffer was created with *mtmsg.newbuffer()* or with 
               *listener:newbuffer()*.
             
  Possible errors: *mtmsg.error.unknown_object*,
                   *mtmsg.error.operation_aborted*


* **`mtmsg.newlistener([name])`**

  Creates a new buffer listener and returns a lua object for referencing the
  created listener.
  
    * *name* - optional string, the name of the new listener.

  The created listener is garbage collected if the last reference object to this
  listener vanishes.

  Possible errors: *mtmsg.error.object_exists*,
                   *mtmsg.error.operation_aborted*,

* **`mtmsg.listener(id|name)`**

  Creates a lua object for referencing an existing listener. The listener must
  be referenced by its *id* or *name*.

    * *id* - integer, the unique buffer id that can be obtained by
           *listener:id()*.

    * *name* - string, the optional name that was given when the
               listener was created with *mtmsg.newlistener()*

  Possible errors: *mtmsg.error.unknown_object*,
                   *mtmsg.error.operation_aborted*


* **`mtmsg.abort([flag])`**

  Aborts operation on all buffers and listeners.
  
    * *flag* - optional boolean. If *true* or not given all buffers and
               listeners are aborted, i.e. a *mtmsg.error.operation_aborted* is
               raised. If *false*, abortion is canceled, i.e. all buffers and
               listeners can be used again.

* **`mtmsg.isabort()`**

  Returns *true* if *mtmsg.abort()* or *mtmsg.abort(true)* was called.
  
* **`mtmsg.time()`**

  Similiar to *os.time()*: Gives the time in seconds, but as float with higher
  precision (at least milliseconds).
  
* **`mtmsg.sleep(timeout)`**
  
  Suspends the current thread for the specified time.

  * *timeout* float, time in seconds.

  Possible errors: *mtmsg.error.operation_aborted*
  

<!-- ---------------------------------------------------------------------------------------- -->

### Buffer Methods

* **`buffer:id()`**
  
  Returns the buffer's id as integer. This id is unique for the whole process.

* **`buffer:name()`**

  Returns the buffer's name that was given to *mtmsg.newbuffer()* or to
  *listener:newbuffer()*.
  
* **`buffer:addmsg(...)`**

  Adds the arguments together as one message to the buffer. Arguments can be
  simple data types (string, number, boolean, nil, light user data).
  
  Returns *true* if the message could be added to the buffer. 
  
  Returns *false* if *buffer:isnonblock() == true* and the buffer is
  concurrently accessed from a parallel thread. 
  
  Returns *false* if the buffer was created with a grow factor *0* and the
  current buffer messages together with the new message would exceed the
  buffer's fixed size.
  
  Possible errors: *mtmsg.error.message_size*,
                   *mtmsg.error.object_closed*,
                   *mtmsg.error.operation_aborted*

* **`buffer:setmsg(...)`**

  Sets the arguments together as one message into the buffer. All other messages
  in this buffer are discarded. Arguments can be simple data types 
  (string, number, boolean, light user data).
  
  Returns *true* if the message could be set into the buffer.
  
  Returns *false* if *buffer:isnonblock() == true* and the buffer is
  concurrently accessed from another thread. 
  
  Possible errors: *mtmsg.error.message_size*,
                   *mtmsg.error.object_closed*,
                   *mtmsg.error.operation_aborted*


* **`buffer:clear()`**

  Removes all messages from the buffer.

  Possible errors: *mtmsg.error.object_closed*


* **`buffer:nextmsg([timeout])`**

  Returns all the arguments that were given as one message by invoking the method
  *buffer:addmsg()* or *buffer:setmsg()*. The returned message is removed
  from the underlying buffer.
  
  * *timeout* optional float, maximal time in seconds for waiting for the next 
    message. The method returns without result if timeout is reached.
  
  If no timeout is given and *buffer:isnonblock() == false* then this methods waits
  without timeout limit until a next message becomes available.

  If no timeout is given and *buffer:isnonblock() == true* then this methods
  returns immediately without result if no next message is available or if the
  buffer is concurrently accessed from another thread.

  Possible errors: *mtmsg.error.object_closed*,
                   *mtmsg.error.operation_aborted*


* **`buffer:nonblock([flag])`**

  if *flag* is not given or *true* the buffer referencing object will operate
  in *nonblock mode*. This does not affect the underlying buffer, i.e. several
  buffer referencing objects could operate in different modes acessing the same 
  buffer.
  
  *Nonblock mode* affects the methods *buffer:nextmsg()*, *buffer:addmsg()*
  and *buffer:setmsg()*: all these methods return immediately with negative 
  result if the underlying buffer is concurrently accessed from another thread.
  
  *Nonblock mode* can be useful for realtime processing, when it is more importent
  to continue processing and a blocking operation could be postponed or be skipped.

  Possible errors: *mtmsg.error.object_closed*,
                   *mtmsg.error.operation_aborted*

* **`buffer:isnonblock()`**

  Returns *true* if *buffer:nonblock()* or *buffer:nonblock(true)* was invoked.

* **`buffer:close()`**

  Closes the underlying buffer and frees the memory. Every operation from any
  referencing object raises a *mtmsg.error.object_closed*. A closed buffer
  cannot be reactivated.

* **`buffer:abort([flag])`**

  Aborts operation on the underlying buffer.
  
    * *flag* - optional boolean. If *true* or not given the buffer is aborted, 
               i.e. a *mtmsg.error.operation_aborted* is raised. 
               If *false*, abortion is canceled, i.e. the buffer can be used 
               again.

* **`buffer:isabort()`**

  Returns *true* if *buffer:abort()* or *buffer:abort(true)* was called.


<!-- ---------------------------------------------------------------------------------------- -->

### Listener Methods

* **`listener:id()`**
  
  Returns the listener's id as integer. This id is unique for the whole process.

* **`listener:name()`**

  Returns the listener's name that was given to *mtmsg.newlistener()*.


* **`listener:newbuffer([name,][size1[,size2[,grow]]]`**

  Creates a new buffer that is connected to the listener and returns a lua object 
  for referencing the created buffer.
  
  The arguments are the same as for the module function *mtmsg.newbuffer*.

  The created buffer is garbage collected if the last object referencing this
  buffer vanishes. The buffer is **not** referenced by the connected listener.

  Possible errors: *mtmsg.error.object_exists*,
                   *mtmsg.error.operation_aborted*


* **`listener:nextmsg([timeout])`**

  Returns all the arguments that were given as one message by invoking the method
  *buffer:addmsg()* or *buffer:setmsg()* to one of the buffers that are
  connected to this listener.  The returned message is removed
  from the underlying buffer.
  
  * *timeout* optional float, maximal time in seconds for waiting for the next 
    message. The method returns without result if timeout is reached.

  If no timeout is given and *listener:isnonblock() == false* then this methods waits
  without timeout limit until a next message becomes available.

  If no timeout is given and *listener:isnonblock() == true* then this methods
  returns immediately without result if no next message is available or if the
  listener is concurrently accessed from another thread.

  Possible errors: *mtmsg.error.no_buffers*,
                   *mtmsg.error.object_closed*,
                   *mtmsg.error.operation_aborted*
    

* **`listener:nonblock([flag])`**

  if *flag* is not given or *true* the listener referencing object will operate
  in *nonblock mode*. This does not affect the underlying listener, i.e. several
  listener referencing objects could operate in different modes acessing the same 
  listener.
  
  *Nonblock mode* affects the method *listener:nextmsg()*: this method returns
  immediately with negative result if the underlying listener (or one of its
  buffers) is concurrently accessed from another thread.
  
  *Nonblock mode* can be useful for realtime processing, when it is more importent
  to continue processing and a blocking operation could be postponed or be skipped.

  Possible errors: *mtmsg.error.object_closed*,
                   *mtmsg.error.operation_aborted*

* **`listener:isnonblock()`**

  Returns *true* if *listener:nonblock()* or *listener:nonblock(true)* was invoked.

* **`listener:close()`**

  TODO

* **`listener:abort([flag])`**

  Aborts operation on the underlying listener.
  
    * *flag* - optional boolean. If *true* or not given the listener is aborted, 
               i.e. a *mtmsg.error.operation_aborted* is raised. 
               If *false*, abortion is canceled, i.e. the listener can be used 
               again.


* **`listener:isabort()`**

  Returns *true* if *listener:abort()* or *listener:abort(true)* was called.


<!-- ---------------------------------------------------------------------------------------- -->

### Error Methods

* **`error:name()`**

  Name of the error as string, example:
  
  ```lua
  local mtmsg = require("mtmsg")
  assert(mtmsg.error.object_closed:name() == "mtmsg.error.object_closed")
  ```
  
* **`error:details()`**

  Additional details as string regarding this error.

* **`error:traceback()`**

  Stacktrace as string where the error occured.

* **`error:message()`**

  Full message as string containing name, details and traceback.
  Same as *tostring(error)*.
  
* **`err1 == err2`**
  
  Two errors are considered equal if they have the same name. This can be used
  for error evaluation, example:

  ```lua
  local mtmsg = require("mtmsg")
  local listener = mtmsg.newlistener()
  local _, err = pcall(function() listener:nextmsg() end)
  assert(err == mtmsg.error.no_buffers)
  ```

<!-- ---------------------------------------------------------------------------------------- -->

### Errors

* **`mtmsg.error.message_size`**
  
  The size of one message exceeds the limit that was given as parameter *size1*
  in *mtmsg.newbuffer()* or *listener:newbuffer()* and a grow factor *0* was
  specified to prevent buffer growing.
  
* **`mtmsg.error.no_buffers`**

  *listener:nextmsg()* was called and no buffer is connected to the listener.
  Buffers are subject to garbage collection and therefore a reference to a created
  buffer is needed to keep it alive. If a listener's buffer becomes garbage
  collected, it is disconnected from the listener.

* **`mtmsg.error.object_closed`**

  An operation is performed on a closed buffer or listener, i.e. the method
  *buffer:close()* or *listener:close()* has been called.

* **`mtmsg.error.object_exists`**

  A new buffer is to be created via *mtmsg.newbuffer()* or *listener:newbuffer()*
  with a name that refers to an already existing buffer or a new listener is to
  be created via *mtmsg.newlistener()* with a name that refers to an already
  existing listener. 

* **`mtmsg.error.operation_aborted`**

  An operation should be performed on an object with *object:isabort() == true*
  or *mtmsg.abort()* has been called to abort all operations.

* **`mtmsg.error.out_of_memory`**

  Buffer memory cannot be allocated.

* **`mtmsg.error.unknown_object`**

  A reference to an existing buffer (via *mtmsg.buffer()*) or listener (via
  *mtmsg.listener()*) cannot be created because the object cannot be found by
  the given id or name. 
  
  All mtmsg objects are subject to garbage collection and therefore a reference to a 
  created object is needed to keep it alive, i.e. if you want to pass an object
  to another thread via name or id, a reference to this object should be kept in the
  thread that created the object, until the receiving thread signaled that a reference
  to the object has been constructed in the receiving thread, example:
  
  ```lua
  local llthreads = require("llthreads2.ex")
  local mtmsg     = require("mtmsg")
  local threadIn  = mtmsg.newbuffer()
  local lst       = mtmsg.newlistener()
  local threadOut = lst:newbuffer()
  local thread    = llthreads.new(function(inId, outId)
                                      local mtmsg     = require("mtmsg")
                                      local threadIn  = mtmsg.buffer(inId)
                                      local threadOut = mtmsg.buffer(outId)
                                      threadOut:addmsg("started")
                                      assert(threadIn:nextmsg() == "exit")
                                      threadOut:addmsg("finished")
                                  end,
                                  threadIn:id(),
                                  threadOut:id())
  -- threadOut = nil -- not now!
  -- collectgarbage()
  thread:start()
  assert(lst:nextmsg() == "started")
  threadOut = nil -- now it's safe
  collectgarbage()
  threadIn:addmsg("exit")
  assert(lst:nextmsg() == "finished")
  assert(thread:join())
  collectgarbage()
  local _, err = pcall(function() lst:nextmsg() end)
  assert(err == mtmsg.error.no_buffers)
  ```

End of document.

<!-- ---------------------------------------------------------------------------------------- -->
