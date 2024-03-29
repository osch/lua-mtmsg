# mtmsg 
[![Licence](http://img.shields.io/badge/Licence-MIT-brightgreen.svg)](LICENSE)
[![build status](https://github.com/osch/lua-mtmsg/workflows/test/badge.svg)](https://github.com/osch/lua-mtmsg/actions/workflows/test.yml)
[![Build status](https://ci.appveyor.com/api/projects/status/r4gwhqv2jvv2jirn?svg=true)](https://ci.appveyor.com/project/osch/lua-mtmsg)
[![Install](https://img.shields.io/badge/Install-LuaRocks-brightgreen.svg)](https://luarocks.org/modules/osch/mtmsg)

<!-- ---------------------------------------------------------------------------------------- -->

Low-level multi-threading message buffers for the [Lua] scripting language.

This package provides in-memory message buffers for inter-thread communication. 
The implementation is independent from the underlying threading library
(e.g. [Lanes] or [llthreads2]).

This package is also available via LuaRocks, see https://luarocks.org/modules/osch/mtmsg.

[Lua]:           https://www.lua.org
[Lanes]:         https://luarocks.org/modules/benoitgermain/lanes
[llthreads2]:    https://luarocks.org/modules/moteus/lua-llthreads2
[carray]:        https://github.com/osch/lua-carray

See below for full [reference documentation](#documentation) .

<!-- ---------------------------------------------------------------------------------------- -->

#### Requirements

   * Tested operating systems: Linux, Windows, MacOS
   * Other Unix variants: could work, but untested, required are:
      * gcc atomic builtins or C11 `stdatomic.h`
      * `pthread.h` or C11 `threads.h`
   * Tested Lua versions: 5.1, 5.2, 5.3, 5.4, luajit 2.0 & 2.1

<!-- ---------------------------------------------------------------------------------------- -->

## Examples

Simple example, passes message buffer by integer id to another thread 
([llthreads2] is used as multi-threading implementation in this example):

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
       * mtmsg.type()
       * mtmsg.newwriter()
       * mtmsg.newreader()
   * [Buffer Methods](#buffer-methods)
       * buffer:id()
       * buffer:name()
       * buffer:addmsg()
       * buffer:setmsg()
       * buffer:msgcnt()
       * buffer:clear()
       * buffer:nextmsg()
       * buffer:notifier()
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
       * listener:clear()
       * listener:close()
       * listener:abort()
       * listener:isabort()
   * [Writer Methods](#writer-methods)
       * writer:add()
       * writer:addmsg()
       * writer:setmsg()
       * writer:clear()
   * [Reader Methods](#reader-methods)
       * reader:next()
       * reader:nextmsg()
       * reader:clear()
   * [Errors](#errors)
       * mtmsg.error.ambiguous_name
       * mtmsg.error.message_size
       * mtmsg.error.no_buffers
       * mtmsg.error.object_closed
       * mtmsg.error.operation_aborted
       * mtmsg.error.out_of_memory
       * mtmsg.error.unknown_object
       

<!-- ---------------------------------------------------------------------------------------- -->

### Module Functions

* **`mtmsg.newbuffer([name,][size[,grow]])`**

  Creates a new buffer and returns a lua object for referencing the
  created buffer.

    * *name*  - optional string, the name of the new buffer,
    * *size*  - optional integer, initial size in bytes for the buffer that
                holds all messages for this buffer (defaults to *1024*).
    * *grow*  - optional float, the factor how fast buffer memory grows
                if more data has to be buffered. If *0*, the used
                memory is fixed by the initially given size. If *<=1* the 
                buffer grows only by the needed bytes (default value is *2*,
                i.e. the size doubles if buffer memory needs to grow).
  
  The created buffer is garbage collected if the last object referencing this
  buffer vanishes.
  
  The created buffer objects implement the [Notify C API], the [Receiver C API] 
  and the [Sender C API], i.e. native code can send notifications or send or 
  receive messages from any thread.
  
  [Notify C API]:   https://github.com/lua-capis/lua-notify-capi
  [Receiver C API]: https://github.com/lua-capis/lua-receiver-capi
  [Sender C API]:   https://github.com/lua-capis/lua-sender-capi
  
  Possible errors: *mtmsg.error.operation_aborted*
                   

* **`mtmsg.buffer(id|name)`**

  Creates a lua object for referencing an existing buffer. The buffer must
  be referenced by its *id* or *name*. Referencing the buffer by *id* is
  much faster than referencing by *name* if the number of buffers 
  increases.

    * *id* - integer, the unique buffer id that can be obtained by
             *buffer:id()*.

    * *name* - string, the optional name that was given when the
               buffer was created with *mtmsg.newbuffer()* or with 
               *listener:newbuffer()*. To find a buffer by name 
               the name must be unique for the whole process.
             
  Possible errors: *mtmsg.error.ambiguous_name*,
                   *mtmsg.error.unknown_object*,
                   *mtmsg.error.operation_aborted*


* **`mtmsg.newlistener([name])`**

  Creates a new buffer listener and returns a lua object for referencing the
  created listener.
  
    * *name* - optional string, the name of the new listener.

  The created listener is garbage collected if the last reference object to this
  listener vanishes.

  Possible errors: *mtmsg.error.operation_aborted*,

* **`mtmsg.listener(id|name)`**

  Creates a lua object for referencing an existing listener. The listener must
  be referenced by its *id* or *name*. Referencing the listener by *id* is
  much faster than referencing by *name* if the number of listeners 
  increases.

    * *id* - integer, the unique buffer id that can be obtained by
           *listener:id()*.

    * *name* - string, the optional name that was given when the
               listener was created with *mtmsg.newlistener()*. To
               find a listener by name the name must be unique for
               the whole process.

  Possible errors: *mtmsg.error.ambiguous_name*,
                   *mtmsg.error.unknown_object*,
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
  
* **`mtmsg.type(arg)`**

  Returns the type of *arg* as string. Same as *type(arg)* for builtin types.
  For *userdata* objects it tries to determine the type from the *__name* field in 
  the metatable and checks if the metatable can be found in the lua registry for this key
  as created by [luaL_newmetatable](https://www.lua.org/manual/5.3/manual.html#luaL_newmetatable).

  Returns *"userdata"* for userdata where the *__name* field in the metatable is missing
  or does not have a corresponding entry in the lua registry.
  
  Returns *"mtmsg.buffer"* or *"mtmsg.listener"* if the arg is one of the userdata types 
  provided by the mtmsg package.


* **`mtmsg.newwriter([size[,grow]])`**

  Creates a new writer. A writer can be used to build up messages incrementally by adding
  new message elements using *writer:add()*. The message than can be added to a buffer
  using *writer:addmsg()* in one atomic step.

    * *size*  - optional integer, initial size in bytes for the memory that
                holds all message elements for this writer (defaults to *1024*).
    * *grow*  - optional float, the factor how fast memory grows if more
                message elements have to be buffered. If *0*, the used
                memory is fixed by the initially given size. If *<=1* the 
                memory grows only by the needed bytes (default value is *2*,
                i.e. the size doubles if memory needs to grow).
  
  The created writer canot be accessed from other threads and is garbage collected
  like any normal Lua value.
  

* **`mtmsg.newreader([size[,grow]])`**

  Creates a new reader. A reader can be used to parse messages incrementally by invoking
  *reader:next()* to get the message elements. Invoking *reader:nextmsg()* gets the next
  message of a buffer or listener into the reader in one atomic step.


    * *size*  - optional integer, initial size in bytes for the memory that
                holds all message elements for this reader (defaults to *1024*).
    * *grow*  - optional float, the factor how fast memory grows if more
                message elements have to be buffered. If *0*, the used
                memory is fixed by the initially given size. If *<=1* the 
                memory grows only by the needed bytes (default value is *2*,
                i.e. the size doubles if memory needs to grow).
  
  The created reader canot be accessed from other threads and is garbage collected
  like any normal Lua value.
  


<!-- ---------------------------------------------------------------------------------------- -->

### Buffer Methods

* **`buffer:id()`**
  
  Returns the buffer's id as integer. This id is unique for the whole process.

* **`buffer:name()`**

  Returns the buffer's name that was given to *mtmsg.newbuffer()* or to
  *listener:newbuffer()*.
  
* **`buffer:addmsg(...)`**

  Adds the arguments together as one message to the buffer. Arguments can be
  simple data types (string, number, boolean, nil, light user data, C function)
  or [carray] objects.
  
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
  (string, number, boolean, light user data, C function) or [carray] objects.
  
  Returns *true* if the message could be set into the buffer.
  
  Returns *false* if *buffer:isnonblock() == true* and the buffer is
  concurrently accessed from another thread. 
  
  Possible errors: *mtmsg.error.message_size*,
                   *mtmsg.error.object_closed*,
                   *mtmsg.error.operation_aborted*


* **`buffer:msgcnt()`**

  Returns the number of messages in the buffer.

* **`buffer:clear()`**

  Removes all messages from the buffer.

  Returns *true* if the buffer could be cleared.
  
  Returns *false* if *buffer:isnonblock() == true* and the buffer is
  concurrently accessed from another thread. 
  

  Possible errors: *mtmsg.error.object_closed*


* **`buffer:nextmsg([timeout][, carray]*)`**

  Returns all the arguments that were given as one message by invoking the method
  *buffer:addmsg()* or *buffer:setmsg()*. The returned message is removed
  from the underlying buffer.
  
  * *timeout* optional float, maximal time in seconds for waiting for the next 
    message. The method returns without result if timeout is reached.
  
  * *carray* optional one or more [carray] objects. These objects are reused 
             if the message contains an array of the corresponding data type 
             of the given *carray*. Unused *carray* objects are reset to length 0
             after this call.
  
  If timeout is not given or *nil* and *buffer:isnonblock() == false* then this 
  methods waits without timeout limit forever until a next message becomes available.

  If timeout is not given or *nil* and *buffer:isnonblock() == true* then this method
  returns immediately without result if no next message is available or if the
  buffer is concurrently accessed from another thread.

  Possible errors: *mtmsg.error.object_closed*,
                   *mtmsg.error.operation_aborted*


* **`buffer:notifier(ntf[,type[,threshold]])`**

  Connects a notifier object to the underlying buffer.
  
  * *ntf*       notifier object or *nil*. If *nil* is given, an existing notifier 
                object of the specified type is disconnected from the underlying buffer.
                
  * *type*      optional string, value "<" for a notifier that is notified if a message 
                is removed from the buffer and ">" for a notifier that is notified if
                a message is added to the buffer. Default value is ">".
                
  * *threshold* optional integer. For notifier type "<" the notifier is notified
                if the current number of messages is below the threshold after a
                message is removed from the buffer. For notifier type ">" the notifier
                is notifierd if the current number of message is above the threshold
                after a message is added to the buffer. If nil or not given
                the threshold is not considered. i.e. the corresponding notifier
                is always called if a message is removed or added.
  
  A buffer can only have one connected notifier object per notifier type "<" or ">".
  It is an error to call this method in case the buffer already has a connected 
  notifier object of the same type.
  
  The given notifier object must implement the [Notify C API], 
  see [src/notify_capi.h](./src/notify_capi.h), i.e. the given object must
  have an an associated meta table entry *_capi_notify* delivered by
  the C API function *notify_get_capi()* and the associated 
  C API function *toNotifier()* must return a valid pointer for the given 
  notifier object *ntf*.
  
  [Notify C API]: https://github.com/lua-capis/lua-notify-capi
  
  A buffer itself does also implement the Notify C API, i.e. a buffer can be
  also set as notifier for another buffer, see [lua-mtmsg/example06.lua](./examples/example06.lua).
  
  More Examples:
  * [lua-mtstates/example06.lua](https://github.com/osch/lua-mtstates/blob/master/examples/example06.lua)
  * [lua-lpugl/example10.lua](https://github.com/osch/lua-lpugl/blob/master/example/example10.lua)
  * [lua-nocurses/example03.lua](https://github.com/osch/lua-nocurses/blob/master/examples/example03.lua)

  Possible errors: *mtmsg.error.object_closed*,
                   *mtmsg.error.has_notifier*

* **`buffer:nonblock([flag])`**

  if *flag* is not given or *true* the buffer referencing object will operate
  in *nonblock mode*. This does not affect the underlying buffer, i.e. several
  buffer referencing objects could operate in different modes accessing the same 
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


* **`listener:newbuffer([name,][size[,grow]])`**

  Creates a new buffer that is connected to the listener and returns a lua object 
  for referencing the created buffer.
  
  The arguments are the same as for the module function *mtmsg.newbuffer*.

  The created buffer is garbage collected if the last object referencing this
  buffer vanishes. The buffer is **not** referenced by the connected listener.
  
  If the buffer is garbage collected, the remaining messages in this buffer are 
  still delivered to the listener, i.e. these messages are not discarded.

  Possible errors: *mtmsg.error.operation_aborted*


* **`listener:nextmsg([timeout][, carray]*)`**

  Returns all the arguments that were given as one message by invoking the method
  *buffer:addmsg()* or *buffer:setmsg()* to one of the buffers that are
  connected to this listener.  The returned message is removed
  from the underlying buffer.
  
  * *timeout* optional float, maximal time in seconds for waiting for the next 
    message. The method returns without result if timeout is reached.

  * *carray* optional one or more [carray] objects. These objects are reused 
             if the message contains an array of the corresponding data type 
             of the given *carray*. Unused *carray* objects are reset to length 0
             after this call.
  
  If timeout is not given or *nil* and *listener:isnonblock() == false* then this 
  methods waits without timeout limit forever until a next message becomes available.

  If timeout is not given or *nil* and *listener:isnonblock() == true* then this methods
  returns immediately without result if no next message is available or if the
  listener is concurrently accessed from another thread.

  Possible errors: *mtmsg.error.no_buffers*,
                   *mtmsg.error.object_closed*,
                   *mtmsg.error.operation_aborted*
    

* **`listener:nonblock([flag])`**

  if *flag* is not given or *true* the listener referencing object will operate
  in *nonblock mode*. This does not affect the underlying listener, i.e. several
  listener referencing objects could operate in different modes accessing the same 
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

* **`listener:clear()`**

  Removes all messages from all connected buffers.

  Returns *true* if all buffers could be cleared.
  
  Returns *false* if *listener:isnonblock() == true* and the listener or one of the 
  buffers is concurrently accessed from another thread. 
  
  Possible errors: *mtmsg.error.object_closed*


* **`listener:close()`**

  Closes the listener and all connected buffers and frees the memory. Every 
  operation from any referencing object raises a *mtmsg.error.object_closed*. 
  A closed listener cannot be reactivated.


* **`listener:abort([flag])`**

  Aborts operation on the underlying listener and all connected buffers.
  
    * *flag* - optional boolean. If *true* or not given the listener and connected buffers 
               are aborted, i.e. a *mtmsg.error.operation_aborted* is raised. 
               If *false*, abortion is canceled, i.e. the listener and 
               all connected buffers can be used again.


* **`listener:isabort()`**

  Returns *true* if *listener:abort()* or *listener:abort(true)* was called.


<!-- ---------------------------------------------------------------------------------------- -->

### Writer Methods

A writer can be used to build up messages incrementally by adding
new message elements using *writer:add()*. 

The message than can be added to a buffer using *writer:addmsg()* in one atomic step.

For example, this code
```lua
    writer:add("foo1"); writer:add("foo2", "foo3"); writer:addmsg(buffer)
```
is equivalent to
```lua
    buffer:addmsg("foo1", "foo2", "foo3")
```

* **`writer:add(...)`**

  Adds the arguments as message elements into the writer. Arguments can be
  simple data types (string, number, boolean, nil, light user data, C function)
  or [carray] objects.
  
  Possible errors: *mtmsg.error.message_size*

* **`writer:addmsg(buffer)`**

  Adds the writer's message elements together as one message to the buffer. 
  
  Returns *true* if the message could be added to the buffer. In this case,
  the message elements are removed from the writer, i.e. the writer can
  be re-used to build a new message.
  
  Returns *false* if *buffer:isnonblock() == true* and the buffer is
  concurrently accessed from a parallel thread. 
  
  Returns *false* if the buffer was created with a grow factor *0* and the
  current buffer messages together with the new message would exceed the
  buffer's fixed size.
  
  Possible errors: *mtmsg.error.object_closed*,
                   *mtmsg.error.operation_aborted*

* **`writer:setmsg(buffer)`**

  Sets the writer's message elements together as one message into the buffer. 
  All other messages in the given buffer are discarded.
  
  Returns *true* if the message could be added to the buffer. 
  
  Returns *false* if *buffer:isnonblock() == true* and the buffer is
  concurrently accessed from a parallel thread. 
  
  Returns *false* if the buffer was created with a grow factor *0* and the
  current buffer messages together with the new message would exceed the
  buffer's fixed size.
  
  Possible errors: *mtmsg.error.object_closed*,
                   *mtmsg.error.operation_aborted*

* **`writer:clear()`**
  
  Removes all message elements from the writer.

<!-- ---------------------------------------------------------------------------------------- -->

### Reader Methods

A reader can be used to parse messages incrementally by invoking *reader:next()* to get the message 
elements. 

Invoking *reader:nextmsg()* gets the next message of a buffer or listener into the reader in one 
atomic step.

For example, this code
```lua
    reader:nextmsg(buffer); local a = reader:next(); local b, c = reader:next(2)
```
is equivalent to
```lua
    local a, b, c = buffer:nextmsg()
```

* **`reader:next([count][, carray]*)`**

  Returns next message elements from the reader. Returns nothing if no more message elements 
  are available. The returned elements are removed from the reader's internal list of message
  elements.

  * *count* - optional integer, the maximaum number of message elements to be returned
              (defaults to 1).

  * *carray* optional one or more [carray] objects. These objects are reused 
             if the message contains an array of the corresponding data type 
             of the given *carray*. Unused *carray* objects are reset to length 0
             after this call.

* **`reader:nextmsg(buffer|listener, [timeout])`**

  Gets the next message of a buffer or listener into the reader in one atomic step. The
  message is removed from the given buffer or listener. If the reader contained older
  message elements from a previous message these are discarded before all messages elements 
  from the new message are added to the reader, i.e. after this call the reader contains 
  only message elements from the new message.

  * *timeout* optional float, maximal time in seconds for waiting for the next 
    message. The method returns *false* if timeout is reached and no message
    was reveived.

  Returns *true* if a message could be obtained from the given buffer or listener. In
  this case the message is removed from the given buffer or listener.

  If no timeout is given and *isnonblock() == false* for the given buffer or listener then 
  this methods waits without timeout limit until a next message becomes available.

  If no timeout is given and *isnonblock() == true* for the given buffer or listener then this 
  methods returns immediately *false*  if no next message is available or if the given buffer 
  or listener is concurrently accessed from another thread.

  Possible errors: *mtmsg.error.no_buffers*,
                   *mtmsg.error.object_closed*,
                   *mtmsg.error.operation_aborted*

* **`reader:clear()`**
  
  Removes all message elements from the reader.


<!-- ---------------------------------------------------------------------------------------- -->

### Errors

* All errors raised by this module are string values. Special error strings are
  available in the table `mtmsg.error`, example:

  ```lua
  local mtmsg = require("mtmsg")
  assert(mtmsg.error.object_closed == "mtmsg.error.object_closed")
  ```
  
  These can be used for error evaluation purposes, example:
  
  ```lua
  local mtmsg = require("mtmsg")
  local listener = mtmsg.newlistener()
  local _, err = pcall(function() listener:nextmsg() end)
  assert(err:match(mtmsg.error.no_buffers))
  ```

* **`mtmsg.error.ambiguous_name`**

  More than one buffer or listener was found for the given name to 
  *mtmsg.buffer()* or *mtmsg.listener()*. 
  To find a buffer by name, the buffer name must be unique among all buffers
  of the whole process. To find a listener by name, the listener name must 
  be unique among all listeners of the whole process 


* **`mtmsg.error.message_size`**
  
  The size of one message exceeds the limit that was given in *mtmsg.newbuffer()*, 
  *listener:newbuffer()*, *mtmsg.newwriter()* or *mtmsg.newreader()* and a grow
  factor *0* was specified to prevent the internal memory buffer growing.
  
* **`mtmsg.error.no_buffers`**

  *listener:nextmsg()* was called and no buffer is connected to the listener.
  Buffers are subject to garbage collection and therefore a reference to a created
  buffer is needed to keep it alive. If a listener's buffer becomes garbage
  collected, it is disconnected from the listener.

* **`mtmsg.error.has_notifier`**

  The method *buffer:notifier()* is called with a notifier object but the underlying
  buffer already has notifier object.

* **`mtmsg.error.object_closed`**

  An operation is performed on a closed buffer or listener, i.e. the method
  *buffer:close()* or *listener:close()* has been called.

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
  assert(err:match(mtmsg.error.no_buffers))
  ```

End of document.

<!-- ---------------------------------------------------------------------------------------- -->
