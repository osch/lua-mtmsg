#include "util.h"
#include "listener.h"
#include "async_util.h"
#include "buffer.h"
#include "main.h"

const char* const MTMSG_LISTENER_CLASS_NAME = "mtmsg.listener";

typedef lua_Integer ListenerId;

static MsgListener* firstListener = NULL;


void mtmsg_listener_abort_all(bool abortFlag) 
{
    MsgListener* lst = firstListener;
    while (lst != NULL) {
        async_mutex_lock(&lst->listenerMutex);
        lst->aborted = abortFlag;
        async_mutex_notify(&lst->listenerMutex);
        async_mutex_unlock(&lst->listenerMutex);
        lst = lst->nextListener;
    }
}

/*static int internalError(lua_State* L, const char* text, int line) 
{
    return luaL_error(L, "%s (%s:%d)", text, MTMSG_LISTENER_CLASS_NAME, line);
}*/

static MsgListener* createNewListener()
{
    MsgListener* listener = malloc(sizeof(MsgListener));
    if (!listener) return NULL;

    memset(listener, 0, sizeof(MsgListener));
    listener->id      = atomic_inc(&mtmsg_id_counter);
    listener->used    = 1;
    async_mutex_init(&listener->listenerMutex);

    if (firstListener) {
        firstListener->prevListenerPtr = &listener->nextListener;
    }
    listener->nextListener    = firstListener;
    listener->prevListenerPtr = &firstListener;
    firstListener             = listener;
    return listener;
}

static MsgListener* findListenerWithName(const char* listenerName, size_t listenerNameLength)
{
    if (listenerName) {
        MsgListener* lst = firstListener;
        while (lst != NULL) {
            if (   lst->listenerName 
                && listenerNameLength == lst->listenerNameLength 
                && memcmp(lst->listenerName, listenerName, listenerNameLength) == 0)
            {
                return lst;
            }
            lst = lst->nextListener;
        }
    }
    return NULL;
}

static MsgListener* findListenerWithId(ListenerId listenerId)
{
    MsgListener* b = firstListener;
    while (b != NULL) {
        if (b->id == listenerId) {
            return b;
        }
        b = b->nextListener;
    }
    return NULL;
}

static const char* toLuaString(lua_State* L, ListenerUserData* udata, MsgListener* lst)
{
    if (lst) {
        if (lst->listenerName) {
            luaL_Buffer b;
            luaL_buffinit(L, &b);
            int i;
            for (i = 0; i < lst->listenerNameLength; ++i) {
                char c = lst->listenerName[i];
                if (c == 0) {
                    luaL_addstring(&b, "\\0");
                } else if (c == '"') {
                    luaL_addstring(&b, "\\\"");
                } else if (c == '\\') {
                    luaL_addstring(&b, "\\\\");
                } else {
                    luaL_addchar(&b, c);
                }
            }
            luaL_pushresult(&b);
            const char* rslt;
            if (udata) {
                rslt = lua_pushfstring(L, "%s: %p (name=\"%s\",id=%d)", MTMSG_LISTENER_CLASS_NAME, 
                                                                        udata,
                                                                        lua_tostring(L, -1),
                                                                        (int)lst->id);
            } else {
                rslt = lua_pushfstring(L, "%s(name=\"%s\",id=%d)", MTMSG_LISTENER_CLASS_NAME, 
                                                                   lua_tostring(L, -1),
                                                                   (int)lst->id);
            }
            lua_remove(L, -2);
            return rslt;
        } else {
            if (udata) {
                return lua_pushfstring(L, "%s: %p (id=%d)", MTMSG_LISTENER_CLASS_NAME, udata, (int)lst->id);
            } else {
                return lua_pushfstring(L, "%s(id=%d)", MTMSG_LISTENER_CLASS_NAME, (int)lst->id);
            }
        }
    } else {
        return lua_pushfstring(L, "%s: invalid", MTMSG_LISTENER_CLASS_NAME);
    }
}

static const char* listenerToLuaString(lua_State* L, MsgListener* lst)
{
    return toLuaString(L, NULL, lst);
}

static const char* udataToLuaString(lua_State* L, ListenerUserData* udata)
{
    if (udata) {
        return toLuaString(L, udata, udata->listener);
    } else {
        return lua_pushfstring(L, "%s: invalid", MTMSG_LISTENER_CLASS_NAME);
    }
}


static int Mtmsg_listener(lua_State* L)
{
    int arg = 1;

    bool hasArg = false;
    const char* listenerName       = NULL;
    size_t      listenerNameLength = 0;
    ListenerId  listenerId         = 0;

    if (lua_gettop(L) >= arg) {
        if (lua_type(L, arg) == LUA_TSTRING) {
            listenerName = lua_tolstring(L, arg++, &listenerNameLength);
            hasArg = true;
        }
        else if (lua_type(L, arg) == LUA_TNUMBER && lua_isinteger(L, arg)) {
            listenerId = lua_tointeger(L, arg++);
            hasArg = true;
        }
    }
    if (!hasArg) {
        return luaL_argerror(L, arg, "listener name or id expected");
    }

    ListenerUserData* userData = lua_newuserdata(L, sizeof(ListenerUserData)); /* create before lock */
    memset(userData, 0, sizeof(ListenerUserData));
    luaL_setmetatable(L, MTMSG_LISTENER_CLASS_NAME);
    mtmsg_membuf_init(&userData->tmp, 0, 1);
    
    /* Lock */
    
    async_mutex_lock(mtmsg_global_lock);

    if (mtmsg_abort_flag) {
        async_mutex_notify(mtmsg_global_lock);
        async_mutex_unlock(mtmsg_global_lock);
        return luaL_error(L, "operation was aborted");
    }

    MsgListener* listener;
    if (listenerName != NULL) {
        listener = findListenerWithName(listenerName, listenerNameLength);
    } else {
        listener = findListenerWithId(listenerId);
    }

    if (!listener) {
        async_mutex_unlock(mtmsg_global_lock);
        return luaL_error(L, "Error: listener does not exist");
    }
    
    userData->listener = listener;
    atomic_inc(&listener->used);
    
    async_mutex_unlock(mtmsg_global_lock);
    return 1;
}

static int Mtmsg_newListener(lua_State* L)
{
    /* Evaluate Args */
    
    int arg = 1;
    
    const char* listenerName       = NULL;
    size_t      listenerNameLength = 0;
    if (lua_gettop(L) >= arg && lua_type(L, arg) == LUA_TSTRING) {
        listenerName = lua_tolstring(L, arg++, &listenerNameLength);
    }
    
    /*  Examine global ListenerList */
    
    ListenerUserData* userData = lua_newuserdata(L, sizeof(ListenerUserData)); /* create before lock */
    memset(userData, 0, sizeof(ListenerUserData));
    luaL_setmetatable(L, MTMSG_LISTENER_CLASS_NAME);
    mtmsg_membuf_init(&userData->tmp, 0, 1);

    async_mutex_lock(mtmsg_global_lock);

    if (mtmsg_abort_flag) {
        async_mutex_notify(mtmsg_global_lock);
        async_mutex_unlock(mtmsg_global_lock);
        return luaL_error(L, "operation was aborted");
    }

    MsgListener* otherListener = findListenerWithName(listenerName, listenerNameLength);
    if (otherListener) {
        const char* otherString = listenerToLuaString(L, otherListener);
        async_mutex_unlock(mtmsg_global_lock);
        return luaL_error(L, "Error: listener with same name already exists: %s", otherString);
    }

    MsgListener* newListener = createNewListener();
    if (!newListener) {
        async_mutex_unlock(mtmsg_global_lock);
        return luaL_error(L, "Error allocating new %s", MTMSG_LISTENER_CLASS_NAME);
    }
    userData->listener = newListener;
    if (listenerName) {
        newListener->listenerName = malloc(listenerNameLength + 1);
        if (newListener->listenerName == NULL) {
            async_mutex_unlock(mtmsg_global_lock);
            return luaL_error(L, "Error allocating %d bytes", listenerNameLength + 1);
        }
        memcpy(newListener->listenerName, listenerName, listenerNameLength + 1);
        newListener->listenerNameLength = listenerNameLength;
    }
    async_mutex_unlock(mtmsg_global_lock);
    return 1;
}

void mtmsg_listener_free(MsgListener* listener) /* mtmsg_global_lock */
{
    if (listener->listenerName) {
        free(listener->listenerName);
    }
    *listener->prevListenerPtr = listener->nextListener;
    if (listener->nextListener) {
        listener->nextListener->prevListenerPtr = listener->prevListenerPtr;
    }
    async_mutex_destruct(&listener->listenerMutex);
    free(listener);
}

static int MsgListener_release(lua_State* L)
{
    ListenerUserData* udata    = luaL_checkudata(L, 1, MTMSG_LISTENER_CLASS_NAME);
    MsgListener*      listener = udata->listener;

    mtmsg_membuf_free(&udata->tmp);

    if (listener) {
        async_mutex_lock(mtmsg_global_lock);
        
        if (atomic_dec(&listener->used) == 0) 
        {
            mtmsg_listener_free(listener);
        }
        async_mutex_unlock(mtmsg_global_lock);
    }
    return 0;
}


static int MsgListener_newBuffer(lua_State* L)
{
    int arg = 1;
    
    ListenerUserData* listenerUdata = luaL_checkudata(L, arg++, MTMSG_LISTENER_CLASS_NAME);
    
    return mtmsg_buffer_new(L, listenerUdata, arg);
}


static int MsgListener_nextMsg(lua_State* L)
{
    int arg = 1;
    
    ListenerUserData* listenerUdata = luaL_checkudata(L, arg++, MTMSG_LISTENER_CLASS_NAME);
    MsgListener*      listener      = listenerUdata->listener;

    lua_Number endTime   = 0; /* 0 = no timeout */

    if (lua_gettop(L) >= arg) {
        lua_Number waitSeconds = luaL_checknumber(L, arg++);
        endTime = mtmsg_current_time_seconds() + waitSeconds;
    }

    if (listenerUdata->nonblock) {
        if (!async_mutex_trylock(&listener->listenerMutex)) {
            return 0;
        }
    } else {
        async_mutex_lock(&listener->listenerMutex);
    }
    
again:
    if (listener->closed) {
        async_mutex_notify(&listener->listenerMutex);
        async_mutex_unlock(&listener->listenerMutex);
        return luaL_error(L, "listener was closed");
    }
    if (listener->aborted) {
        async_mutex_notify(&listener->listenerMutex);
        async_mutex_unlock(&listener->listenerMutex);
        return luaL_error(L, "operation was aborted");
    }
    if (listener->firstListenerBuffer == NULL) {
        async_mutex_notify(&listener->listenerMutex);
        async_mutex_unlock(&listener->listenerMutex);
        return luaL_error(L, "listener has no buffers");
    }
    {
        MsgBuffer* b  = listener->firstReadyBuffer;
        while (b != NULL) {
            if (b->mem.bufferLength > 0) 
            {
                size_t msgLength = mtmsg_buffer_getsize(b->mem.bufferStart);
                if (msgLength > listenerUdata->tmp.bufferCapacity) {
                    int rc = mtmsg_membuf_reserve(&listenerUdata->tmp, msgLength);
                    if (rc != 0) {
                        async_mutex_unlock(&listener->listenerMutex);
                        const char* qstring = udataToLuaString(L, listenerUdata);
                        return luaL_error(L, "Error allocating %d bytes for %s", (int)(msgLength), qstring);
                    }
                }
                memcpy(listenerUdata->tmp.bufferStart, b->mem.bufferStart, msgLength);
                b->mem.bufferLength -= msgLength;
                {
                    mtmsg_buffer_remove_from_ready_list(listener, b);
                }
                if (b->mem.bufferLength == 0) {
                    b->mem.bufferStart = b->mem.bufferData;
                } else {
                    if (listener->lastReadyBuffer) {
                        listener->lastReadyBuffer->nextReadyBuffer = b;
                        b->prevReadyBuffer = listener->lastReadyBuffer;
                        listener->lastReadyBuffer = b;
                    } else {
                        listener->firstReadyBuffer = b;
                        listener->lastReadyBuffer  = b;
                    }
                    b->mem.bufferStart  += msgLength;
                }
                if (listener->firstReadyBuffer) {
                    async_mutex_notify(&listener->listenerMutex);
                }
                async_mutex_unlock(&listener->listenerMutex);
                int rsltCount = 0;
                if (msgLength > 0) {
                    size_t msgLength2 = 0;
                    rsltCount = mtmsg_buffer_push_msg(L, listenerUdata->tmp.bufferStart, &msgLength2);
                }
                return rsltCount;
            }
            else
            {
                MsgBuffer* b2 = b->nextReadyBuffer;
                mtmsg_buffer_remove_from_ready_list(listener, b);
                b = b2;
            }
        }
    }
    if (endTime > 0) {
        lua_Number now = mtmsg_current_time_seconds();
        if (now < endTime) {
            async_mutex_wait_millis(&listener->listenerMutex, (int)((endTime - now) * 1000 + 0.5));
            goto again;
        }
    } else if (!listenerUdata->nonblock) {
        async_mutex_wait(&listener->listenerMutex);
        goto again;
    }

    async_mutex_unlock(&listener->listenerMutex);
    return 0;
}

static int MsgListener_toString(lua_State* L)
{
    ListenerUserData* udata = luaL_checkudata(L, 1, MTMSG_LISTENER_CLASS_NAME);
    
    udataToLuaString(L, udata);
    return 1;
}


static int MsgListener_id(lua_State* L)
{
    int arg = 1;
    ListenerUserData* udata = luaL_checkudata(L, arg++, MTMSG_LISTENER_CLASS_NAME);
    MsgListener*      lst = udata->listener;
    lua_pushinteger(L, lst->id);
    return 1;
}

static int MsgListener_name(lua_State* L)
{
    int arg = 1;
    ListenerUserData* udata = luaL_checkudata(L, arg++, MTMSG_LISTENER_CLASS_NAME);
    MsgListener*      lst = udata->listener;
    if (lst->listenerName) {
        lua_pushlstring(L, lst->listenerName, lst->listenerNameLength);
        return 1;
    } else {
        return 0;
    }
}

static int MsgListener_nonblock(lua_State* L)
{
    int arg = 1;
    ListenerUserData* udata    = luaL_checkudata(L, arg++, MTMSG_LISTENER_CLASS_NAME);
    MsgListener*      listener = udata->listener;

    bool newNonblock = true;
    
    if (lua_gettop(L) >= arg) 
    {
        luaL_checktype(L, arg, LUA_TBOOLEAN);
        newNonblock = lua_toboolean(L, arg++);
    } 

    async_mutex_lock(&listener->listenerMutex);

    if (listener->closed) {
        async_mutex_unlock(&listener->listenerMutex);
        const char* bstring = listenerToLuaString(L, listener);
        return luaL_error(L, "Error: listener is closed: %s", bstring);
    }
    if (listener->aborted) {
        async_mutex_unlock(&listener->listenerMutex);
        return luaL_error(L, "operation was aborted");
    }
    
    udata->nonblock = newNonblock;

    async_mutex_unlock(&listener->listenerMutex);
    return 0;
}

static int MsgListener_isNonblock(lua_State* L)
{
    int arg = 1;
    ListenerUserData* udata    = luaL_checkudata(L, arg++, MTMSG_LISTENER_CLASS_NAME);

    lua_pushboolean(L, udata->nonblock);

    return 1;
}

static int MsgListener_abort(lua_State* L)
{
    int arg = 1;
    ListenerUserData* udata    = luaL_checkudata(L, arg++, MTMSG_LISTENER_CLASS_NAME);
    MsgListener*      listener = udata->listener;

    bool abortFlag = true;

    if (lua_gettop(L) >= arg) 
    {
        luaL_checktype(L, arg, LUA_TBOOLEAN);
        abortFlag = lua_toboolean(L, arg++);
    } 

    async_mutex_lock(&listener->listenerMutex);
    listener->aborted = abortFlag;
    async_mutex_notify(&listener->listenerMutex);
    async_mutex_unlock(&listener->listenerMutex);

    return 0;
}
static int MsgListener_isAbort(lua_State* L)
{
    int arg = 1;
    ListenerUserData* udata    = luaL_checkudata(L, arg++, MTMSG_LISTENER_CLASS_NAME);
    MsgListener*      listener = udata->listener;

    async_mutex_lock(&listener->listenerMutex);
    lua_pushboolean(L, listener->aborted);
    async_mutex_unlock(&listener->listenerMutex);

    return 1;
}

static const luaL_Reg MsgListenerMethods[] = 
{
    { "id",           MsgListener_id           },
    { "name",         MsgListener_name         },
    { "newbuffer",    MsgListener_newBuffer    },
    { "nextmsg",      MsgListener_nextMsg      },
    { "nonblock",     MsgListener_nonblock     },
    { "isnonblock",   MsgListener_isNonblock   },
    { "abort",        MsgListener_abort        },
    { "isabort",      MsgListener_isAbort      },
    { NULL,           NULL } /* sentinel */
};

static const luaL_Reg MsgListenerMetaMethods[] = 
{
    { "__tostring",   MsgListener_toString },
    { "__gc",         MsgListener_release  },
    { NULL,           NULL } /* sentinel */
};

static const luaL_Reg ModuleFunctions[] = 
{
    { "newlistener",  Mtmsg_newListener },
    { "listener",     Mtmsg_listener    },
    { NULL,           NULL } /* sentinel */
};



int mtmsg_listener_init_module(lua_State* L, int module, int listenerMeta, int listenerClass)
{
    lua_pushvalue(L, module);
        luaL_setfuncs(L, ModuleFunctions, 0);

        lua_pushvalue(L, listenerMeta);
            luaL_setfuncs(L, MsgListenerMetaMethods, 0);
    
            lua_pushvalue(L, listenerClass);
                luaL_setfuncs(L, MsgListenerMethods, 0);
    
    lua_pop(L, 3);

    return 0;
}

