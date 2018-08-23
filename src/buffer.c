#include "util.h"
#include "async_util.h"
#include "buffer.h"
#include "listener.h"
#include "main.h"

const char* const MTMSG_BUFFER_CLASS_NAME = "mtmsg.buffer";

typedef lua_Integer BufferId;

static MsgBuffer* firstBuffer = NULL;

void mtmsg_buffer_abort_all(bool abortFlag) 
{
    MsgBuffer* b = firstBuffer;
    while (b != NULL) {
        async_mutex_lock(b->sharedMutex);
        if (!b->listener) {
            b->aborted = abortFlag;
            async_mutex_notify(b->sharedMutex);
        }
        async_mutex_unlock(b->sharedMutex);
        b = b->nextBuffer;
    }
}

/*static int internalError(lua_State* L, const char* text, int line) 
{
    return luaL_error(L, "%s (%s:%d)", text, MTMSG_BUFFER_CLASS_NAME, line);
}*/

static MsgBuffer* findBufferWithName(const char* bufferName, size_t bufferNameLength)
{
    if (bufferName) {
        MsgBuffer* b = firstBuffer;
        while (b != NULL) {
            if (   b->bufferName 
                && bufferNameLength == b->bufferNameLength 
                && memcmp(b->bufferName, bufferName, bufferNameLength) == 0)
            {
                return b;
            }
            b = b->nextBuffer;
        }
    }
    return NULL;
}

static MsgBuffer* findBufferWithId(BufferId bufferId)
{
    MsgBuffer* b = firstBuffer;
    while (b != NULL) {
        if (b->id == bufferId) {
            return b;
        }
        b = b->nextBuffer;
    }
    return NULL;
}

static const char* toLuaString(lua_State* L, BufferUserData* udata, MsgBuffer* b)
{
    if (b) {
        if (b->bufferName) {
            luaL_Buffer tmp;
            luaL_buffinit(L, &tmp);
            int i;
            for (i = 0; i < b->bufferNameLength; ++i) {
                char c = b->bufferName[i];
                if (c == 0) {
                    luaL_addstring(&tmp, "\\0");
                } else if (c == '"') {
                    luaL_addstring(&tmp, "\\\"");
                } else if (c == '\\') {
                    luaL_addstring(&tmp, "\\\\");
                } else {
                    luaL_addchar(&tmp, c);
                }
            }
            luaL_pushresult(&tmp);
            const char* rslt;
            if (udata) {
                rslt = lua_pushfstring(L, "%s: %p (name=\"%s\",id=%d)", MTMSG_BUFFER_CLASS_NAME, 
                                                                        udata,
                                                                        lua_tostring(L, -1),
                                                                        (int)b->id);
            } else {
                rslt = lua_pushfstring(L, "%s(name=\"%s\",id=%d)", MTMSG_BUFFER_CLASS_NAME, 
                                                                   lua_tostring(L, -1),
                                                                   (int)b->id);
            }
            lua_remove(L, -2);
            return rslt;
        } else {
            if (udata) {
                return lua_pushfstring(L, "%s: %p (id=%d)", MTMSG_BUFFER_CLASS_NAME, udata, (int)b->id);
            } else {
                return lua_pushfstring(L, "%s(id=%d)", MTMSG_BUFFER_CLASS_NAME, (int)b->id);
            }
        }
    } else {
        return lua_pushfstring(L, "%s: invalid", MTMSG_BUFFER_CLASS_NAME);
    }
}

const char* mtmsg_buffer_tostring(lua_State* L, MsgBuffer* b)
{
    return toLuaString(L, NULL, b);
}

static const char* udataToLuaString(lua_State* L, BufferUserData* udata)
{
    if (udata) {
        return toLuaString(L, udata, udata->buffer);
    } else {
        return lua_pushfstring(L, "%s: invalid", MTMSG_BUFFER_CLASS_NAME);
    }
}

static MsgBuffer* createNewBuffer(Mutex* sharedMutex)
{
    MsgBuffer* b = malloc(sizeof(MsgBuffer));
    if (!b) return NULL;

    memset(b, 0, sizeof(MsgBuffer));
    b->id      = atomic_inc(&mtmsg_id_counter);
    b->used    = 1;
    if (sharedMutex == NULL) {
        async_mutex_init(&b->ownMutex);
        b->sharedMutex = &b->ownMutex;
    } else {
        b->sharedMutex = sharedMutex;
    }

    if (firstBuffer) {
        firstBuffer->prevBufferPtr = &b->nextBuffer;
    }
    b->nextBuffer    = firstBuffer;
    b->prevBufferPtr = &firstBuffer;
    firstBuffer      = b;
    return b;
}

int mtmsg_buffer_new(lua_State* L, ListenerUserData* listenerUdata, int arg)
{
    const char* bufferName       = NULL;
    size_t      bufferNameLength = 0;
    if (lua_gettop(L) >= arg && lua_type(L, arg) == LUA_TSTRING) {
        bufferName = lua_tolstring(L, arg++, &bufferNameLength);
    }
    
    size_t initialTmpBufferSize = 128;
    if (lua_gettop(L) >= arg) {
        lua_Number argValue = luaL_checknumber(L, arg++);
        if (argValue < 0) {
            argValue = 0;
        }
        initialTmpBufferSize = argValue;
    }

    size_t initialCapacity = 1024;
    if (lua_gettop(L) >= arg) {
        initialCapacity = luaL_checknumber(L, arg++);
        if (initialCapacity < initialTmpBufferSize) {
            initialCapacity = initialTmpBufferSize;
        }
    }

    lua_Number growFactor = 2;
    if (lua_gettop(L) >= arg) {
        growFactor = luaL_checknumber(L, arg++);
        if (growFactor < 0) {
            growFactor = 0;
        }
    }

    BufferUserData* bufferUdata = lua_newuserdata(L, sizeof(BufferUserData)); /* create before lock */
    memset(bufferUdata, 0, sizeof(BufferUserData));
    luaL_setmetatable(L, MTMSG_BUFFER_CLASS_NAME);

    async_mutex_lock(mtmsg_global_lock);

    if (mtmsg_abort_flag) {
        async_mutex_notify(mtmsg_global_lock);
        async_mutex_unlock(mtmsg_global_lock);
        return luaL_error(L, "operation was aborted");
    }

    /* Examine global BufferList */
    
    MsgBuffer* otherBuffer = findBufferWithName(bufferName, bufferNameLength);
    if (otherBuffer) {
        const char* otherString = mtmsg_buffer_tostring(L, otherBuffer);
        async_mutex_unlock(mtmsg_global_lock);
        return luaL_error(L, "Error: buffer with same name already exists: %s", otherString);
    }
    Mutex* sharedMutex = NULL;
    if (listenerUdata != NULL && listenerUdata->listener != NULL) {
        sharedMutex = &listenerUdata->listener->listenerMutex;
    }
    MsgBuffer* newBuffer = createNewBuffer(sharedMutex);
    if (!newBuffer) {
        async_mutex_unlock(mtmsg_global_lock);
        return luaL_error(L, "Error allocating new %s", MTMSG_BUFFER_CLASS_NAME);
    }
    bufferUdata->buffer = newBuffer;
    newBuffer->initialTmpBufferSize = initialTmpBufferSize;
    
    if (!mtmsg_membuf_init(&bufferUdata->tmp, initialTmpBufferSize, growFactor)) {
        async_mutex_unlock(mtmsg_global_lock);
        return luaL_error(L, "Error allocating %d bytes", (int)initialTmpBufferSize);
    }

    if (!mtmsg_membuf_init(&newBuffer->mem, initialCapacity, growFactor)) {
        async_mutex_unlock(mtmsg_global_lock);
        return luaL_error(L, "Error allocating %d bytes", (int)initialCapacity);
    }
    if (bufferName) {
        newBuffer->bufferName = malloc(bufferNameLength + 1);
        if (newBuffer->bufferName == NULL) {
            async_mutex_unlock(mtmsg_global_lock);
            return luaL_error(L, "Error allocating %d bytes", bufferNameLength + 1);
        }
        memcpy(newBuffer->bufferName, bufferName, bufferNameLength + 1);
        newBuffer->bufferNameLength = bufferNameLength;
    }
    if (sharedMutex != NULL) {
        async_mutex_lock(sharedMutex);
        MsgListener* listener = listenerUdata->listener; 
        
        if (listener->aborted) {
            async_mutex_notify(sharedMutex);
            async_mutex_unlock(sharedMutex);
            async_mutex_unlock(mtmsg_global_lock);
            return luaL_error(L, "operation was aborted");
        }
        
        atomic_inc(&listener->used);

        newBuffer->listener           = listener;
        newBuffer->nextListenerBuffer = listener->firstListenerBuffer;
        listener->firstListenerBuffer = newBuffer;

        if (growFactor > listenerUdata->tmp.growFactor) {
            listenerUdata->tmp.growFactor = growFactor;
        }
        if (initialTmpBufferSize > listenerUdata->tmp.bufferCapacity) {
            if (mtmsg_membuf_reserve(&listenerUdata->tmp, initialTmpBufferSize) != 0) {
                async_mutex_unlock(sharedMutex);
                async_mutex_unlock(mtmsg_global_lock);
                return luaL_error(L, "Error allocating %d bytes", (int)initialTmpBufferSize);
            }
        }
        async_mutex_notify(sharedMutex);
        async_mutex_unlock(sharedMutex);
    }
    
    async_mutex_unlock(mtmsg_global_lock);
    return 1;
}

static int Mtmsg_newBuffer(lua_State* L)
{
    /* Evaluate Args */
    
    int arg = 1;
    
    return mtmsg_buffer_new(L, NULL, arg);
}

static void removeFromListener(MsgListener* listener, MsgBuffer* b)
{
    {
        MsgBuffer** b1 = &listener->firstListenerBuffer;
        MsgBuffer*  b2 =  listener->firstListenerBuffer;
        while (b2 != NULL) {
            if (b2 == b) {
                *b1 = b2->nextListenerBuffer;
                b2->listener           = NULL;
                b2->nextListenerBuffer = NULL;
                break;
            }
            b1 = &b2->nextListenerBuffer;
            b2 =  b2->nextListenerBuffer;
        }
    }
    {
        mtmsg_buffer_remove_from_ready_list(listener, b);
    }
}

static void MsgBuffer_free(MsgBuffer* b)
{
    if (b->bufferName) {
        free(b->bufferName);
    }

    *b->prevBufferPtr = b->nextBuffer;
    if (b->nextBuffer) {
        b->nextBuffer->prevBufferPtr = b->prevBufferPtr;
    }
    if (b->sharedMutex == &b->ownMutex) {
        async_mutex_destruct(&b->ownMutex);
    }
    else {
        if (b->listener) {
            MsgListener* listener = b->listener;

            async_mutex_lock(b->sharedMutex);
            removeFromListener(listener, b);
            async_mutex_notify(b->sharedMutex);
            async_mutex_unlock(b->sharedMutex);
            
            if (atomic_dec(&listener->used) == 0) {
                mtmsg_listener_free(listener);
            }
        }
    }
    mtmsg_membuf_free(&b->mem);
    free(b);
}

static int MsgBuffer_release(lua_State* L)
{
    BufferUserData* udata = luaL_checkudata(L, 1, MTMSG_BUFFER_CLASS_NAME);
    MsgBuffer*      b = udata->buffer;

    mtmsg_membuf_free(&udata->tmp);
    
    if (b) {
        async_mutex_lock(mtmsg_global_lock);

        if (atomic_dec(&b->used) == 0) {
            MsgBuffer_free(b);
        }
        udata->buffer = NULL;
        
        async_mutex_unlock(mtmsg_global_lock);
    }
    return 0;
}

static int MsgBuffer_close(lua_State* L)
{
    int arg = 1;
    BufferUserData* udata = luaL_checkudata(L, arg++, MTMSG_BUFFER_CLASS_NAME);
    MsgBuffer*      b     = udata->buffer;

    async_mutex_lock(b->sharedMutex);

    b->closed = true;
    mtmsg_membuf_free(&udata->tmp);
    if (b->listener) {
        removeFromListener(b->listener, b);
    }
    mtmsg_membuf_free(&b->mem);
    async_mutex_notify(b->sharedMutex);
    async_mutex_unlock(b->sharedMutex);
    lua_pushboolean(L, true);
    return 1;
}


static int Mtmsg_buffer(lua_State* L)
{
    int arg = 1;

    bool hasArg = false;
    const char* bufferName       = NULL;
    size_t      bufferNameLength = 0;
    BufferId    bufferId         = 0;

    if (lua_gettop(L) >= arg) {
        if (lua_type(L, arg) == LUA_TSTRING) {
            bufferName = lua_tolstring(L, arg++, &bufferNameLength);
            hasArg = true;
        }
        else if (lua_type(L, arg) == LUA_TNUMBER && lua_isinteger(L, arg)) {
            bufferId = lua_tointeger(L, arg++);
            hasArg = true;
        }
    }
    if (!hasArg) {
        return luaL_argerror(L, arg, "buffer name or id expected");
    }

    BufferUserData* userData = lua_newuserdata(L, sizeof(BufferUserData)); /* create before lock */
    memset(userData, 0, sizeof(BufferUserData));
    luaL_setmetatable(L, MTMSG_BUFFER_CLASS_NAME);

    /* Lock */
    
    async_mutex_lock(mtmsg_global_lock);

    if (mtmsg_abort_flag) {
        async_mutex_notify(mtmsg_global_lock);
        async_mutex_unlock(mtmsg_global_lock);
        return luaL_error(L, "operation was aborted");
    }

    MsgBuffer* buffer;
    if (bufferName != NULL) {
        buffer = findBufferWithName(bufferName, bufferNameLength);
    } else {
        buffer = findBufferWithId(bufferId);
    }

    if (!buffer) {
        async_mutex_unlock(mtmsg_global_lock);
        return luaL_error(L, "Error: buffer does not exist");
    }
    
    if (!mtmsg_membuf_init(&userData->tmp, buffer->initialTmpBufferSize, buffer->mem.growFactor)) {
        async_mutex_unlock(mtmsg_global_lock);
        return luaL_error(L, "Error allocating %d bytes", (int)buffer->initialTmpBufferSize);
    }

    userData->buffer = buffer;
    atomic_inc(&buffer->used);
    
    async_mutex_unlock(mtmsg_global_lock);
    return 1;
}

typedef enum {
    BUFFER_END,
    BUFFER_NIL,
    BUFFER_INTEGER,
    BUFFER_BYTE,
    BUFFER_NUMBER,
    BUFFER_BOOLEAN,
    BUFFER_STRING, 
    BUFFER_SMALLSTRING,
    BUFFER_LIGHTUSERDATA
    
} BufferDataType;

static size_t calcArgsSize(lua_State* L, int firstArg, int* errorArg)
{
    size_t rslt = 1; /* BUFFER_END */
    int n = lua_gettop(L);
    int i;
    for (i = firstArg; i <= n; ++i)
    {
        int type = lua_type(L, i);
        switch (type) {
            case LUA_TNIL: {
                rslt += 1; 
                break;
            }
            case LUA_TNUMBER:  {
                if (lua_isinteger(L, i)) {
                    lua_Integer value = lua_tointeger(L, i);
                    if (0 <= value && value <= 0xff) {
                        rslt += 1 + 1;
                    } else {
                        rslt += 1 + sizeof(lua_Integer);
                    }
                } else {
                    rslt += 1 + sizeof(lua_Number);
                }
                break;
            }
            case LUA_TBOOLEAN: {
                rslt += 2; 
                break;
            }
            case LUA_TSTRING: {
                size_t len = 0;
                lua_tolstring(L, i, &len);
                if (len <= 0xff) {
                    rslt += 1 + 1 + len;
                } else {
                    rslt += 1 + sizeof(size_t) + len;
                }
                break;
            }
            case LUA_TLIGHTUSERDATA: {
                rslt += 1 + sizeof(void*);
                break;
            }
            case LUA_TTABLE:
            case LUA_TFUNCTION:
            case LUA_TUSERDATA:
            case LUA_TTHREAD:
            default: {
                *errorArg = i;
                return 0;
            }
        }
    }
    return rslt;
}

static void argsToBuffer(lua_State* L, int firstArg, char* buffer)
{
    size_t p = 0;
    int    n = lua_gettop(L);
    int    i;

    for (i = firstArg; i <= n; ++i)
    {
        int type = lua_type(L, i);
        switch (type) {
            case LUA_TNIL: {
                buffer[p++] = BUFFER_NIL;
                break;
            }
            case LUA_TNUMBER:  {
                if (lua_isinteger(L, i)) {
                    lua_Integer value = lua_tointeger(L, i);
                    if (0 <= value && value <= 0xff) {
                        buffer[p++] = BUFFER_BYTE;
                        buffer[p++] = ((char)value);
                    } else {
                        buffer[p++] = BUFFER_INTEGER;
                        memcpy(buffer + p, &value, sizeof(lua_Integer));
                        p += sizeof(lua_Integer);
                    }
                } else {
                    buffer[p++] = BUFFER_NUMBER;
                    lua_Number value = lua_tonumber(L, i);
                    memcpy(buffer + p, &value, sizeof(lua_Number));
                    p += sizeof(lua_Number);
                }
                break;
            }
            case LUA_TBOOLEAN: {
                buffer[p++] = BUFFER_BOOLEAN;
                buffer[p++] = lua_toboolean(L, i);
                break;
            }
            case LUA_TSTRING: {
                size_t      len     = 0;
                const char* content = lua_tolstring(L, i, &len);
                if (len <= 0xff) {
                    buffer[p++] = BUFFER_SMALLSTRING;
                    buffer[p++] = ((char)len);
                } else {
                    buffer[p++] = BUFFER_STRING;
                    memcpy(buffer + p, &len, sizeof(size_t));
                    p += sizeof(size_t);
                }
                memcpy(buffer + p, content, len);
                p += len;
                break;
            }
            case LUA_TLIGHTUSERDATA: {
                void* value = lua_touserdata(L, i);
                buffer[p++] = BUFFER_LIGHTUSERDATA;
                memcpy(buffer + p, &value, sizeof(void*));
                p += sizeof(void*);
                break;
            }
            default: {
                break;
            }
        }
    }
    
    buffer[p++] = BUFFER_END;
}

size_t mtmsg_buffer_getsize(const char* buffer)
{
    size_t p = 0;
    while (true) {
        char type = buffer[p++];
        switch (type) {
            case BUFFER_END: {
                return p;
            }
            case BUFFER_NIL: {
                break;
            }
            case BUFFER_INTEGER: {
                p += sizeof(lua_Integer);
                break;
            }
            case BUFFER_BYTE: {
                p += 1;
                break;
            }
            case BUFFER_NUMBER: {
                p += sizeof(lua_Number);
                break;
            }
            case BUFFER_BOOLEAN: {
                p += 1;
                break;
            }
            case BUFFER_STRING: {
                size_t len;
                memcpy(&len, buffer + p, sizeof(size_t));
                p += sizeof(size_t);
                p += len;
                break;
            }
            case BUFFER_SMALLSTRING: {
                size_t len = ((size_t)(buffer[p++])) & 0xff;
                p += len;
                break;
            }
            case BUFFER_LIGHTUSERDATA: {
                p += sizeof(void*);
                break;
            }
            default: {
                break;
            }
        }
    }
}

int mtmsg_buffer_push_msg(lua_State* L, const char* buffer, size_t* bufferSize)
{
    size_t p = 0;
    int    i = 0;
    while (true) {
        char type = buffer[p++];
        switch (type) {
            case BUFFER_END: {
                *bufferSize = p;
                return i;
            }
            case BUFFER_NIL: {
                lua_pushnil(L);
                break;
            }
            case BUFFER_INTEGER: {
                lua_Integer value;
                memcpy(&value, buffer + p, sizeof(lua_Integer));
                p += sizeof(lua_Integer);
                lua_pushinteger(L, value);
                break;
            }
            case BUFFER_BYTE: {
                char byte = buffer[p++];
                lua_Integer value = ((lua_Integer)byte) & 0xff;
                lua_pushinteger(L, value);
                break;
            }
            case BUFFER_NUMBER: {
                lua_Number value;
                memcpy(&value, buffer + p, sizeof(lua_Number));
                p += sizeof(lua_Number);
                lua_pushnumber(L, value);
                break;
            }
            case BUFFER_BOOLEAN: {
                lua_pushboolean(L, buffer[p++]);
                break;
            }
            case BUFFER_STRING: {
                size_t len;
                memcpy(&len, buffer + p, sizeof(size_t));
                p += sizeof(size_t);
                lua_pushlstring(L, buffer + p, len);
                p += len;
                break;
            }
            case BUFFER_SMALLSTRING: {
                size_t len = ((size_t)(buffer[p++])) & 0xff;
                lua_pushlstring(L, buffer + p, len);
                p += len;
                break;
            }
            case BUFFER_LIGHTUSERDATA: {
                void* value = NULL;
                memcpy(&value, buffer + p, sizeof(void*));
                p += sizeof(void*);
                lua_pushlightuserdata(L, value);
                break;
            }
            default: {
                i -= 1;
                break;
            }
        }
        i += 1;
    }
}

static int MsgBuffer_clear(lua_State* L)
{
    int arg = 1;
    BufferUserData* udata = luaL_checkudata(L, arg++, MTMSG_BUFFER_CLASS_NAME);
    MsgBuffer*  b = udata->buffer;

    if (udata->nonblock) {
        if (!async_mutex_trylock(b->sharedMutex)) {
            lua_pushboolean(L, false);
            return 1;
        }
    } else {
        async_mutex_lock(b->sharedMutex);
    }

    if (b->closed) {
        async_mutex_unlock(b->sharedMutex);
        const char* qstring = mtmsg_buffer_tostring(L, b);
        return luaL_error(L, "Error: buffer is closed: %s", qstring);
    }
    if (b->aborted || (b->listener && b->listener->aborted)) {
        async_mutex_unlock(b->sharedMutex);
        return luaL_error(L, "operation was aborted");
    }

    b->mem.bufferLength = 0;

    async_mutex_unlock(b->sharedMutex);
    lua_pushboolean(L, true);
    return 1;
}


static int MsgBuffer_setOrAddMsg(lua_State* L, bool clear)
{
    int arg = 1;
    BufferUserData* udata = luaL_checkudata(L, arg++, MTMSG_BUFFER_CLASS_NAME);
    MsgBuffer*  b = udata->buffer;

    int errorArg = 0;
    const size_t msgLength = calcArgsSize(L, arg, &errorArg);

    {
        int rc = mtmsg_membuf_reserve(&udata->tmp, msgLength);
        if (rc != 0) {
            const char* qstring = udataToLuaString(L, udata);
            if (rc == 1) {
                return luaL_error(L, "Message length %d bytes exceeds limit %d for %s", (int)(msgLength), (int)(udata->tmp.bufferCapacity), qstring);
            } else {
                return luaL_error(L, "Error allocating %d bytes for %s", (int)(msgLength), qstring);
            }
        }
    }
    argsToBuffer(L, arg, udata->tmp.bufferStart);
    
    if (msgLength == 0) {
        return luaL_argerror(L, errorArg, "parameter type not supported");
    }
    
    if (udata->nonblock) {
        if (!async_mutex_trylock(b->sharedMutex)) {
            lua_pushboolean(L, false);
            return 1;
        }
    } else {
        async_mutex_lock(b->sharedMutex);
    }
    if (b->closed) {
        async_mutex_unlock(b->sharedMutex);
        const char* qstring = mtmsg_buffer_tostring(L, b);
        return luaL_error(L, "Error: buffer is closed: %s", qstring);
    }
    if (b->aborted || (b->listener && b->listener->aborted)) {
        async_mutex_unlock(b->sharedMutex);
        return luaL_error(L, "operation was aborted");
    }
    if (clear) {
        b->mem.bufferLength = 0;
    }
    {
        int rc = mtmsg_membuf_reserve(&b->mem, msgLength);
        if (rc != 0) {
            async_mutex_unlock(b->sharedMutex);
            if (rc == 1) {
                /* buffer should not grow */
                lua_pushboolean(L, false);
                return 1;
            } else {
                const char* qstring = udataToLuaString(L, udata);
                return luaL_error(L, "Error allocating %d bytes for %s", (int)(b->mem.bufferLength + msgLength), qstring);
            }
        }
    }
    memcpy(b->mem.bufferStart + b->mem.bufferLength, udata->tmp.bufferStart, msgLength);
    
    b->mem.bufferLength += msgLength;
    
    if (b->listener && !b->prevReadyBuffer && b->listener->firstReadyBuffer != b) {
        MsgListener* listener = b->listener;
        if (listener->lastReadyBuffer) {
            listener->lastReadyBuffer->nextReadyBuffer = b;
            b->prevReadyBuffer = listener->lastReadyBuffer;
            listener->lastReadyBuffer = b;
        } else {
            listener->firstReadyBuffer = b;
            listener->lastReadyBuffer  = b;
        }
    }

    async_mutex_notify(b->sharedMutex);
    async_mutex_unlock(b->sharedMutex);
    lua_pushboolean(L, true);
    return 1;
}

static int MsgBuffer_setMsg(lua_State* L)
{
    bool clear = true;
    return MsgBuffer_setOrAddMsg(L, clear);
}
static int MsgBuffer_addMsg(lua_State* L)
{
    bool clear = false;
    return MsgBuffer_setOrAddMsg(L, clear);
}

static int MsgBuffer_nextMsg(lua_State* L)
{
    int arg = 1;
    BufferUserData* udata = luaL_checkudata(L, arg++, MTMSG_BUFFER_CLASS_NAME);
    MsgBuffer*  b = udata->buffer;

    lua_Number endTime   = 0; /* 0 = no timeout */

    if (lua_gettop(L) >= arg) {
        lua_Number waitSeconds = luaL_checknumber(L, arg++);
        endTime = mtmsg_current_time_seconds() + waitSeconds;
    }

    if (udata->nonblock) {
        if (!async_mutex_trylock(b->sharedMutex)) {
            return 0;
        }
    } else {
        async_mutex_lock(b->sharedMutex);
    }

again:
    if (b->closed) {
        async_mutex_notify(b->sharedMutex);
        async_mutex_unlock(b->sharedMutex);
        const char* qstring = mtmsg_buffer_tostring(L, b);
        return luaL_error(L, "Error: buffer is closed: %s", qstring);
    }
    
    if (b->aborted || (b->listener && b->listener->aborted)) {
        async_mutex_notify(b->sharedMutex);
        async_mutex_unlock(b->sharedMutex);
        return luaL_error(L, "operation was aborted");
    }
    size_t msgLength = 0;
    if (b->mem.bufferLength > 0) {
        msgLength = mtmsg_buffer_getsize(b->mem.bufferStart);
        int rc = mtmsg_membuf_reserve(&udata->tmp, msgLength);
        if (rc != 0) {
            async_mutex_unlock(b->sharedMutex);
            const char* qstring = udataToLuaString(L, udata);
            if (rc == 1) {
                return luaL_error(L, "Message length %d bytes exceeds limit %d for %s", (int)(msgLength), (int)(udata->tmp.bufferCapacity), qstring);
            } else {
                return luaL_error(L, "Error allocating %d bytes for %s", (int)(msgLength), qstring);
            }
        }
        memcpy(udata->tmp.bufferStart, b->mem.bufferStart, msgLength);
        b->mem.bufferLength -= msgLength;
        if (b->mem.bufferLength == 0) {
            b->mem.bufferStart = b->mem.bufferData;
        } else {
            b->mem.bufferStart  += msgLength;
        }
        if (b->mem.bufferLength > 0) {
            async_mutex_notify(b->sharedMutex);         
        }
    } else {
        if (endTime > 0) {
            lua_Number now = mtmsg_current_time_seconds();
            if (now < endTime) {
                async_mutex_wait_millis(b->sharedMutex, (int)((endTime - now) * 1000 + 0.5));
                goto again;
            } else {
                async_mutex_unlock(b->sharedMutex);
                return 0;
            }
        } else {
            if (udata->nonblock) {
                async_mutex_unlock(b->sharedMutex);
                return 0;
            } else {
                async_mutex_wait(b->sharedMutex);
                goto again;
            }
        }
    }
    async_mutex_unlock(b->sharedMutex);

    int rsltCount = 0;
    if (msgLength > 0) {
        size_t msgLength = 0;
        rsltCount = mtmsg_buffer_push_msg(L, udata->tmp.bufferStart, &msgLength);
    }
    return rsltCount;
}

static int MsgBuffer_toString(lua_State* L)
{
    BufferUserData* udata = luaL_checkudata(L, 1, MTMSG_BUFFER_CLASS_NAME);
    
    udataToLuaString(L, udata);
    return 1;
}


static int MsgBuffer_id(lua_State* L)
{
    int arg = 1;
    BufferUserData* udata = luaL_checkudata(L, arg++, MTMSG_BUFFER_CLASS_NAME);
    MsgBuffer*      b = udata->buffer;
    lua_pushinteger(L, b->id);
    return 1;
}

static int MsgBuffer_name(lua_State* L)
{
    int arg = 1;
    BufferUserData* udata = luaL_checkudata(L, arg++, MTMSG_BUFFER_CLASS_NAME);
    MsgBuffer*      b = udata->buffer;
    if (b->bufferName) {
        lua_pushlstring(L, b->bufferName, b->bufferNameLength);
        return 1;
    } else {
        return 0;
    }
}

static int MsgBuffer_nonblock(lua_State* L)
{
    int arg = 1;
    BufferUserData* udata = luaL_checkudata(L, arg++, MTMSG_BUFFER_CLASS_NAME);
    MsgBuffer*      b = udata->buffer;
    
    bool newNonblock = true;

    if (lua_gettop(L) >= arg) 
    {
        luaL_checktype(L, arg, LUA_TBOOLEAN);
        newNonblock = lua_toboolean(L, arg++);
    } 
    async_mutex_lock(b->sharedMutex);

    if (b->closed) {
        async_mutex_unlock(b->sharedMutex);
        const char* qstring = mtmsg_buffer_tostring(L, b);
        return luaL_error(L, "Error: buffer is closed: %s", qstring);
    }
    if (b->aborted || (b->listener && b->listener->aborted)) {
        async_mutex_unlock(b->sharedMutex);
        return luaL_error(L, "operation was aborted");
    }
    
    udata->nonblock = newNonblock;

    async_mutex_unlock(b->sharedMutex);
    return 0;
}

static int MsgBuffer_isNonblock(lua_State* L)
{
    int arg = 1;
    BufferUserData* udata = luaL_checkudata(L, arg++, MTMSG_BUFFER_CLASS_NAME);

    lua_pushboolean(L, udata->nonblock);

    return 1;
}

static int MsgBuffer_abort(lua_State* L)
{
    int arg = 1;
    BufferUserData* udata = luaL_checkudata(L, arg++, MTMSG_BUFFER_CLASS_NAME);
    MsgBuffer*      b = udata->buffer;

    bool abortFlag = true;

    if (lua_gettop(L) >= arg) 
    {
        luaL_checktype(L, arg, LUA_TBOOLEAN);
        abortFlag = lua_toboolean(L, arg++);
    } 

    async_mutex_lock(b->sharedMutex);
    if (b->listener) {
        b->listener->aborted = abortFlag;
    } else {
        b->aborted = abortFlag;
    }
    async_mutex_unlock(b->sharedMutex);

    return 0;
}

static int MsgBuffer_isAbort(lua_State* L)
{
    int arg = 1;
    BufferUserData* udata = luaL_checkudata(L, arg++, MTMSG_BUFFER_CLASS_NAME);
    MsgBuffer*      b = udata->buffer;

    async_mutex_lock(b->sharedMutex);
    if (b->listener) {
        lua_pushboolean(L, b->listener->aborted);
    } else {
        lua_pushboolean(L, b->aborted);
    }
    async_mutex_unlock(b->sharedMutex);

    return 1;
}

static const luaL_Reg MsgBufferMethods[] = 
{
    { "addmsg",     MsgBuffer_addMsg     },
    { "setmsg",     MsgBuffer_setMsg     },
    { "clear",      MsgBuffer_clear      },
    { "nextmsg",    MsgBuffer_nextMsg    },
    { "id",         MsgBuffer_id         },
    { "name",       MsgBuffer_name       },
    { "nonblock",   MsgBuffer_nonblock   },
    { "isnonblock", MsgBuffer_isNonblock },
    { "close",      MsgBuffer_close      },
    { "abort",      MsgBuffer_abort      },
    { "isabort",    MsgBuffer_isAbort    },
    { NULL,         NULL } /* sentinel */
};

static const luaL_Reg MsgBufferMetaMethods[] = 
{
    { "__tostring", MsgBuffer_toString },
    { "__gc",       MsgBuffer_release  },

    { NULL,       NULL } /* sentinel */
};

static const luaL_Reg ModuleFunctions[] = 
{
    { "newbuffer", Mtmsg_newBuffer  },
    { "buffer",    Mtmsg_buffer     },
    { NULL,        NULL } /* sentinel */
};


int mtmsg_buffer_init_module(lua_State* L, int module, int bufferMeta, int bufferClass)
{
    lua_pushvalue(L, module);
        luaL_setfuncs(L, ModuleFunctions, 0);

        lua_pushvalue(L, bufferMeta);
            luaL_setfuncs(L, MsgBufferMetaMethods, 0);
    
            lua_pushvalue(L, bufferClass);
                luaL_setfuncs(L, MsgBufferMethods, 0);
    
    lua_pop(L, 3);

    return 0;
}

