#include "listener.h"
#include "buffer.h"
#include "serialize.h"
#include "main.h"
#include "error.h"

const char* const MTMSG_LISTENER_CLASS_NAME = "mtmsg.listener";

typedef lua_Integer ListenerId;

typedef struct {
    int          count;
    MsgListener* firstListener;
} ListenerBucket;

static AtomicCounter   listener_counter     = 0;
static lua_Integer     listener_buckets     = 0;
static int             bucket_usage       = 0;
static ListenerBucket* listener_bucket_list = NULL;

inline static void toBuckets(MsgListener* lst, lua_Integer n, ListenerBucket* list)
{
    ListenerBucket* bucket        = &(list[lst->id % n]);
    MsgListener**   firstListenerPtr = &bucket->firstListener;
    if (*firstListenerPtr) {
        (*firstListenerPtr)->prevListenerPtr = &lst->nextListener;
    }
    lst->nextListener    = *firstListenerPtr;
    lst->prevListenerPtr =  firstListenerPtr;
    *firstListenerPtr    =  lst;
    bucket->count += 1;
    if (bucket->count > bucket_usage) {
        bucket_usage = bucket->count;
    }
}

static void newBuckets(lua_Integer n, ListenerBucket* newList)
{
    bucket_usage = 0;
    if (listener_bucket_list) {
        lua_Integer i;
        for (i = 0; i < listener_buckets; ++i) {
            ListenerBucket* bb = &(listener_bucket_list[i]);
            MsgListener*    lst  = bb->firstListener;
            while (lst != NULL) {
                MsgListener* b2 = lst->nextListener;
                toBuckets(lst, n, newList);
                lst = b2;
            }
        }
        free(listener_bucket_list);
    }
    listener_buckets     = n;
    listener_bucket_list = newList;
}

void mtmsg_listener_abort_all(bool abortFlag) 
{
    lua_Integer i;
    for (i = 0; i < listener_buckets; ++i) {
        MsgListener* lst = listener_bucket_list[i].firstListener;
        while (lst != NULL) {
            async_mutex_lock(&lst->listenerMutex);
            lst->aborted = abortFlag;
            async_mutex_notify(&lst->listenerMutex);
            async_mutex_unlock(&lst->listenerMutex);
            lst = lst->nextListener;
        }
    }
}

/*static int internalError(lua_State* L, const char* text, int line) 
{
    return luaL_error(L, "%s (%s:%d)", text, MTMSG_LISTENER_CLASS_NAME, line);
}*/

static MsgListener* findListenerWithName(const char* listenerName, size_t listenerNameLength, bool* unique)
{
    MsgListener* rslt = NULL;
    if (listenerName && listener_bucket_list) {
        lua_Integer i;
        for (i = 0; i < listener_buckets; ++i) {
            MsgListener* lst = listener_bucket_list[i].firstListener;
            while (lst != NULL) {
                if (   lst->listenerName 
                    && listenerNameLength == lst->listenerNameLength 
                    && memcmp(lst->listenerName, listenerName, listenerNameLength) == 0)
                {
                    if (unique) {
                        *unique = (rslt == NULL);
                    }
                    if (rslt) {
                        return rslt;
                    } else {
                        rslt = lst;
                    }
                }
                lst = lst->nextListener;
            }
        }
    }
    return rslt;
}

static MsgListener* findListenerWithId(ListenerId listenerId)
{
    if (listener_bucket_list) {
        MsgListener* lst = listener_bucket_list[listenerId % listener_buckets].firstListener;
        while (lst != NULL) {
            if (lst->id == listenerId) {
                return lst;
            }
            lst = lst->nextListener;
        }
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

static void setupListenerMeta(lua_State* L);

static int pushListenerMeta(lua_State* L)
{
    if (luaL_newmetatable(L, MTMSG_LISTENER_CLASS_NAME)) {
        setupListenerMeta(L);
    }
    return 1;
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
    pushListenerMeta(L); /* -> udata, meta */
    lua_setmetatable(L, -2); /* -> udata */
    
    /* Lock */
    
    async_mutex_lock(mtmsg_global_lock);

    if (mtmsg_abort_flag) {
        async_mutex_notify(mtmsg_global_lock);
        async_mutex_unlock(mtmsg_global_lock);
        return mtmsg_ERROR_OPERATION_ABORTED(L);
    }

    MsgListener* listener;
    if (listenerName != NULL) {
        bool unique;
        listener = findListenerWithName(listenerName, listenerNameLength, &unique);
        if (!listener) {
            async_mutex_unlock(mtmsg_global_lock);
            return mtmsg_ERROR_UNKNOWN_OBJECT_listener_name(L, listenerName, listenerNameLength);
        } else if (!unique) {
            async_mutex_unlock(mtmsg_global_lock);
            return mtmsg_ERROR_AMBIGUOUS_NAME_listener_name(L, listenerName, listenerNameLength);
        }
    } else {
        listener = findListenerWithId(listenerId);
        if (!listener) {
            async_mutex_unlock(mtmsg_global_lock);
            return mtmsg_ERROR_UNKNOWN_OBJECT_listener_id(L, listenerId);
        }
    }

    
    userData->listener = listener;
    atomic_inc(&listener->used);
    
    async_mutex_unlock(mtmsg_global_lock);
    return 1;
}


static MsgListener* createNewListener()
{
    MsgListener* listener = calloc(1, sizeof(MsgListener));
    if (!listener) return NULL;

    listener->id      = atomic_inc(&mtmsg_id_counter);
    listener->used    = 1;
    async_mutex_init(&listener->listenerMutex);

    return listener;
}


static int Mtmsg_newListener(lua_State* L)
{
    /* Evaluate Args */
    
    int arg = 1;
    
    const char* listenerName       = NULL;
    size_t      listenerNameLength = 0;
    if (lua_gettop(L) >= arg) {
        luaL_checktype(L, arg, LUA_TSTRING);
        listenerName = lua_tolstring(L, arg++, &listenerNameLength);
    }
    
    /*  Examine global ListenerList */
    
    ListenerUserData* userData = lua_newuserdata(L, sizeof(ListenerUserData)); /* create before lock */
    memset(userData, 0, sizeof(ListenerUserData));
    pushListenerMeta(L); /* -> udata, meta */
    lua_setmetatable(L, -2); /* -> udata */

    async_mutex_lock(mtmsg_global_lock);

    if (mtmsg_abort_flag) {
        async_mutex_unlock(mtmsg_global_lock);
        return mtmsg_ERROR_OPERATION_ABORTED(L);
    }

    MsgListener* newListener = createNewListener();
    if (!newListener) {
        async_mutex_unlock(mtmsg_global_lock);
        return mtmsg_ERROR_OUT_OF_MEMORY(L);
    }
    userData->listener = newListener;
    
    if (listenerName) {
        newListener->listenerName = malloc(listenerNameLength + 1);
        if (newListener->listenerName == NULL) {
            async_mutex_unlock(mtmsg_global_lock);
            return mtmsg_ERROR_OUT_OF_MEMORY_bytes(L, listenerNameLength + 1);
        }
        memcpy(newListener->listenerName, listenerName, listenerNameLength + 1);
        newListener->listenerNameLength = listenerNameLength;
    }

    if (atomic_get(&listener_counter) + 1 > listener_buckets * 4 || bucket_usage > 30) {
        lua_Integer n = listener_buckets ? (2 * listener_buckets) : 64;
        ListenerBucket* newList = calloc(n, sizeof(ListenerBucket));
        if (newList) {
            newBuckets(n, newList);
        } else if (!listener_buckets) {
            async_mutex_unlock(mtmsg_global_lock);
            return mtmsg_ERROR_OUT_OF_MEMORY(L);
        }
    }
    toBuckets(newListener, listener_buckets, listener_bucket_list);
    atomic_inc(&listener_counter);

    async_mutex_unlock(mtmsg_global_lock);
    return 1;
}

void mtmsg_listener_free(MsgListener* lst) /* mtmsg_global_lock */
{
    bool wasInBucket = (lst->prevListenerPtr != NULL);

    MsgBuffer* b = lst->firstListenerBuffer;
    while (b) {
        MsgBuffer* b2 = b->nextListenerBuffer;
        if (b->unreachable) {
            mtmsg_buffer_free_unreachable(lst, b);
        }
        b = b2;
    }

    if (wasInBucket) {
        *lst->prevListenerPtr = lst->nextListener;
    }
    if (lst->nextListener) {
        lst->nextListener->prevListenerPtr = lst->prevListenerPtr;
    }

    if (lst->listenerName) {
        free(lst->listenerName);
    }
    async_mutex_destruct(&lst->listenerMutex);
    free(lst);

    if (wasInBucket) {
        int c = atomic_dec(&listener_counter);
        if (c == 0) {
            if (listener_bucket_list)  {
                free(listener_bucket_list);
            }
            listener_buckets     = 0;
            listener_bucket_list = NULL;
            bucket_usage      = 0;
        }
        else if (c * 10 < listener_buckets) {
            lua_Integer n = 2 * c;
            if (n > 64) {
                ListenerBucket* newList = calloc(n, sizeof(ListenerBucket));
                if (newList) {
                    newBuckets(n, newList);
                }
            }
        }
    }
}

static int MsgListener_release(lua_State* L)
{
    ListenerUserData* udata    = luaL_checkudata(L, 1, MTMSG_LISTENER_CLASS_NAME);
    MsgListener*      listener = udata->listener;

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


int mtmsg_listener_next_msg(lua_State* L, ListenerUserData* listenerUdata, int arg, 
                            MemBuffer* resultBuffer, size_t* argsSize)
{
    MsgListener* listener = listenerUdata->listener;

    lua_Number endTime   = 0; /* 0 = no timeout */

    if (!lua_isnoneornil(L, arg)) {
        lua_Number waitSeconds = luaL_checknumber(L, arg);
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
        const char* listenerString = listenerToLuaString(L, listener);
        return mtmsg_ERROR_OBJECT_CLOSED(L, listenerString);
    }
    if (listener->aborted) {
        async_mutex_notify(&listener->listenerMutex);
        async_mutex_unlock(&listener->listenerMutex);
        return mtmsg_ERROR_OPERATION_ABORTED(L);
    }
    if (listener->firstListenerBuffer == NULL) {
        async_mutex_notify(&listener->listenerMutex);
        async_mutex_unlock(&listener->listenerMutex);
        const char* listenerString = listenerToLuaString(L, listener);
        return mtmsg_ERROR_NO_BUFFERS(L, listenerString);
    }
    {
        MsgBuffer* b  = listener->firstReadyBuffer;
        while (b != NULL) {
            if (b->mem.bufferLength > 0) {
                SerializedMsgSizes sizes;
                mtmsg_serialize_parse_header(b->mem.bufferStart, &sizes);
                if (argsSize) *argsSize = sizes.args_size;
                
                size_t msg_size;
                int    parsedArgCount = 0;
                if (resultBuffer == NULL) {
                    GetMsgArgsPar par; par.inBuffer       = b->mem.bufferStart + sizes.header_size;
                                       par.inBufferSize   = sizes.args_size;
                                       par.inMaxArgCount  = -1;
                                       par.parsedLength   = 0;
                                       par.parsedArgCount = 0;
                    lua_pushcfunction(L, mtmsg_serialize_get_msg_args);
                    lua_pushlightuserdata(L, &par);
                    int rc = lua_pcall(L, 1, LUA_MULTRET, 0);
                    if (rc != LUA_OK) {
                        async_mutex_unlock(&listener->listenerMutex);
                        return lua_error(L);
                    }
                    parsedArgCount = par.parsedArgCount;
                    msg_size = sizes.header_size + par.parsedLength;
                } else {
                    int rc = mtmsg_membuf_reserve(resultBuffer, sizes.args_size);
                    if (rc != 0) {
                        async_mutex_unlock(&listener->listenerMutex);
                        return rc;
                    }
                    memcpy(resultBuffer->bufferStart + resultBuffer->bufferLength, 
                           b->mem.bufferStart + sizes.header_size, 
                           sizes.args_size);
                    resultBuffer->bufferLength += sizes.args_size;
                    
                    msg_size = sizes.header_size + sizes.args_size;
                    parsedArgCount = 1;
                }
                
                b->mem.bufferLength -= msg_size;
                {
                    mtmsg_buffer_remove_from_ready_list(listener, b, false);
                }
                if (b->mem.bufferLength == 0) {
                    if (b->unreachable) {
                        mtmsg_buffer_free_unreachable(listener, b);
                    }
                    b->mem.bufferStart = b->mem.bufferData;
                } else {
                    b->mem.bufferStart  += msg_size;
                    mtmsg_buffer_add_to_ready_list(listener, b);
                }
                if (listener->firstReadyBuffer) {
                    async_mutex_notify(&listener->listenerMutex);
                }
                async_mutex_unlock(&listener->listenerMutex);
                return parsedArgCount;
            }
            else
            {
                MsgBuffer* b2 = b->nextReadyBuffer;
                mtmsg_buffer_remove_from_ready_list(listener, b, true);
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

static int MsgListener_nextMsg(lua_State* L)
{
    int arg = 1;
    ListenerUserData* udata = luaL_checkudata(L, arg++, MTMSG_LISTENER_CLASS_NAME);
    
    int parsedArgCount = mtmsg_listener_next_msg(L, udata, arg, NULL, NULL);
    return parsedArgCount; /* parsedArgCount because resultBuffer is NULL */
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
        const char* lstring = listenerToLuaString(L, listener);
        return mtmsg_ERROR_OBJECT_CLOSED(L, lstring);
    }
    if (listener->aborted) {
        async_mutex_unlock(&listener->listenerMutex);
        return mtmsg_ERROR_OPERATION_ABORTED(L);
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

static int MsgListener_clear(lua_State* L)
{
    int arg = 1;
    ListenerUserData* udata    = luaL_checkudata(L, arg++, MTMSG_LISTENER_CLASS_NAME);
    MsgListener*      listener = udata->listener;

    if (udata->nonblock) {
        if (!async_mutex_trylock(&listener->listenerMutex)) {
            lua_pushboolean(L, false);
            return 1;
        }
    } else {
        async_mutex_lock(&listener->listenerMutex);
    }

    if (listener->closed) {
        async_mutex_unlock(&listener->listenerMutex);
        const char* listenerString = listenerToLuaString(L, listener);
        return mtmsg_ERROR_OBJECT_CLOSED(L, listenerString);
    }

    MsgBuffer* b = listener->firstListenerBuffer;
    while (b != NULL) {
        b->mem.bufferLength = 0;
        MsgBuffer* b2 = b->nextListenerBuffer;
        mtmsg_buffer_remove_from_ready_list(listener, b, true);
        b = b2;
    }
    async_mutex_unlock(&listener->listenerMutex);

    lua_pushboolean(L, true);
    return 1;
}

static int MsgListener_close(lua_State* L)
{
    int arg = 1;
    ListenerUserData* udata    = luaL_checkudata(L, arg++, MTMSG_LISTENER_CLASS_NAME);
    MsgListener*      listener = udata->listener;

    async_mutex_lock(&listener->listenerMutex);

    MsgBuffer* b = listener->firstListenerBuffer;
    while (b != NULL) {
        b->closed = true;
        MsgBuffer* b2 = b->nextListenerBuffer;
        mtmsg_buffer_remove_from_ready_list(listener, b, true);
        mtmsg_membuf_free(&b->mem);
        b = b2;
    }
    listener->closed = true;
    async_mutex_notify(&listener->listenerMutex);
    async_mutex_unlock(&listener->listenerMutex);

    return 0;
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

    MsgBuffer* b = listener->firstListenerBuffer;
    while (b != NULL) {
        MsgBuffer* b2 = b->nextListenerBuffer;
        if (b->aborted != abortFlag) {
            b->aborted = abortFlag;
            if (abortFlag) {
                mtmsg_buffer_remove_from_ready_list(listener, b, false);
            } else if (b->mem.bufferLength > 0) {
                mtmsg_buffer_add_to_ready_list(listener, b);
            }
        }
        b = b2;
    }
    if (abortFlag) {
        async_mutex_notify(&listener->listenerMutex);
    }
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
    { "clear",        MsgListener_clear        },
    { "nonblock",     MsgListener_nonblock     },
    { "isnonblock",   MsgListener_isNonblock   },
    { "close",        MsgListener_close        },
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

static void setupListenerMeta(lua_State* L)
{
    lua_pushstring(L, MTMSG_LISTENER_CLASS_NAME);
    lua_setfield(L, -2, "__metatable");

    luaL_setfuncs(L, MsgListenerMetaMethods, 0);
    
    lua_newtable(L);  /* ListenerClass */
        luaL_setfuncs(L, MsgListenerMethods, 0);
    lua_setfield (L, -2, "__index");
}


int mtmsg_listener_init_module(lua_State* L, int module)
{
    if (luaL_newmetatable(L, MTMSG_LISTENER_CLASS_NAME)) {
        setupListenerMeta(L);
    }
    lua_pop(L, 1);

    lua_pushvalue(L, module);
        luaL_setfuncs(L, ModuleFunctions, 0);
    lua_pop(L, 1);

    return 0;
}
