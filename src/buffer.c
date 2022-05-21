#define NOTIFY_CAPI_IMPLEMENT_SET_CAPI   1
#define NOTIFY_CAPI_IMPLEMENT_GET_CAPI   1

#define RECEIVER_CAPI_IMPLEMENT_SET_CAPI 1
#define SENDER_CAPI_IMPLEMENT_SET_CAPI   1

#include "buffer.h"
#include "listener.h"
#include "serialize.h"
#include "main.h"
#include "error.h"
#include "receiver_capi_impl.h"
#include "sender_capi_impl.h"
#include "notify_capi_impl.h"

const char* const MTMSG_BUFFER_CLASS_NAME = "mtmsg.buffer";

typedef lua_Integer BufferId;

typedef struct {
    int        count;
    MsgBuffer* firstBuffer;
} BufferBucket;

static AtomicCounter buffer_counter     = 0;
static lua_Integer   buffer_buckets     = 0;
static int           bucket_usage       = 0;
static BufferBucket* buffer_bucket_list = NULL;

inline static void toBuckets(MsgBuffer* b, lua_Integer n, BufferBucket* list)
{
    BufferBucket* bucket        = &(list[b->id % n]);
    MsgBuffer**   firstBufferPtr = &bucket->firstBuffer;
    if (*firstBufferPtr) {
        (*firstBufferPtr)->prevBufferPtr = &b->nextBuffer;
    }
    b->nextBuffer    = *firstBufferPtr;
    b->prevBufferPtr =  firstBufferPtr;
    *firstBufferPtr  =  b;
    bucket->count += 1;
    if (bucket->count > bucket_usage) {
        bucket_usage = bucket->count;
    }
}

static void newBuckets(lua_Integer n, BufferBucket* newList)
{
    bucket_usage = 0;
    if (buffer_bucket_list) {
        lua_Integer i;
        for (i = 0; i < buffer_buckets; ++i) {
            BufferBucket* bb = &(buffer_bucket_list[i]);
            MsgBuffer*    b  = bb->firstBuffer;
            while (b != NULL) {
                MsgBuffer* b2 = b->nextBuffer;
                toBuckets(b, n, newList);
                b = b2;
            }
        }
        free(buffer_bucket_list);
    }
    buffer_buckets     = n;
    buffer_bucket_list = newList;
}

static void bufferAbort(MsgBuffer* b, bool abortFlag)
{
    async_mutex_lock(b->sharedMutex);

    if (b->aborted != abortFlag) {
        b->aborted = abortFlag;
        if (abortFlag) {
            if (b->listener) {
                mtmsg_buffer_remove_from_ready_list(b->listener, b, false);
            }
        } else {
            if (b->listener && b->mem.bufferLength > 0) {
                mtmsg_buffer_add_to_ready_list(b->listener, b);
            }
        }
        async_mutex_notify(b->sharedMutex);
    }
    async_mutex_unlock(b->sharedMutex);
}

void mtmsg_buffer_abort_all(bool abortFlag) 
{
    lua_Integer i;
    for (i = 0; i < buffer_buckets; ++i) {
        MsgBuffer* b = buffer_bucket_list[i].firstBuffer;
        while (b != NULL) {
            bufferAbort(b, abortFlag);
            b = b->nextBuffer;
        }
    }
}

/*static int internalError(lua_State* L, const char* text, int line) 
{
    return luaL_error(L, "%s (%s:%d)", text, MTMSG_BUFFER_CLASS_NAME, line);
}*/

static MsgBuffer* findBufferWithName(const char* bufferName, size_t bufferNameLength, bool* unique)
{
    MsgBuffer* rslt = NULL;
    if (bufferName && buffer_bucket_list) {
        lua_Integer i;
        for (i = 0; i < buffer_buckets; ++i) {
            MsgBuffer* b = buffer_bucket_list[i].firstBuffer;
            while (b != NULL) {
                if (   b->bufferName 
                    && bufferNameLength == b->bufferNameLength 
                    && memcmp(b->bufferName, bufferName, bufferNameLength) == 0)
                {
                    if (unique) {
                        *unique = (rslt == NULL);
                    }
                    if (rslt) {
                        return rslt;
                    } else {
                        rslt = b;
                    }
                }
                b = b->nextBuffer;
            }
        }
    }
    return rslt;
}

static MsgBuffer* findBufferWithId(BufferId bufferId)
{
    if (buffer_bucket_list) {
        MsgBuffer* b = buffer_bucket_list[bufferId % buffer_buckets].firstBuffer;
        while (b != NULL) {
            if (b->id == bufferId) {
                return b;
            }
            b = b->nextBuffer;
        }
    }
    return NULL;
}

static const char* toLuaString(lua_State* L, BufferUserData* udata, MsgBuffer* b)
{
    if (b) {
        if (b->bufferName) {
            mtmsg_util_quote_lstring(L, b->bufferName, b->bufferNameLength);
            const char* rslt;
            if (udata) {
                rslt = lua_pushfstring(L, "%s: %p (name=%s,id=%d)", MTMSG_BUFFER_CLASS_NAME, 
                                                                        udata,
                                                                        lua_tostring(L, -1),
                                                                        (int)b->id);
            } else {
                rslt = lua_pushfstring(L, "%s(name=%s,id=%d)", MTMSG_BUFFER_CLASS_NAME, 
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
    MsgBuffer* b = calloc(1, sizeof(MsgBuffer));
    if (!b) return NULL;

    b->id      = atomic_inc(&mtmsg_id_counter);
    b->used    = 1;
    if (sharedMutex == NULL) {
        async_mutex_init(&b->ownMutex);
        b->sharedMutex = &b->ownMutex;
    } else {
        b->sharedMutex = sharedMutex;
    }

    return b;
}

static void setupBufferMeta(lua_State* L);

static int pushBufferMeta(lua_State* L)
{
    if (luaL_newmetatable(L, MTMSG_BUFFER_CLASS_NAME)) {
        setupBufferMeta(L);
    }
    return 1;
}

int mtmsg_buffer_new(lua_State* L, ListenerUserData* listenerUdata, int arg)
{
    const char* bufferName       = NULL;
    size_t      bufferNameLength = 0;
    if (lua_gettop(L) >= arg && lua_type(L, arg) == LUA_TSTRING) {
        bufferName = lua_tolstring(L, arg++, &bufferNameLength);
    }
    
   size_t initialCapacity = 1024;
   if (lua_gettop(L) >= arg) {
        lua_Number argValue = luaL_checknumber(L, arg++);
        if (argValue < 0) {
            argValue = 0;
        }
        initialCapacity = argValue;
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
    pushBufferMeta(L);       /* -> udata, meta */
    lua_setmetatable(L, -2); /* -> udata */
    
    async_mutex_lock(mtmsg_global_lock);

    if (mtmsg_abort_flag) {
        async_mutex_unlock(mtmsg_global_lock);
        return mtmsg_ERROR_OPERATION_ABORTED(L);
    }

    /* Examine global BufferList */
    
    Mutex* sharedMutex = NULL;
    if (listenerUdata != NULL && listenerUdata->listener != NULL) {
        sharedMutex = &listenerUdata->listener->listenerMutex;
    }
    MsgBuffer* newBuffer = createNewBuffer(sharedMutex);
    if (!newBuffer) {
        async_mutex_unlock(mtmsg_global_lock);
        return mtmsg_ERROR_OUT_OF_MEMORY(L);
    }
    bufferUdata->buffer = newBuffer;
    
    if (!mtmsg_membuf_init(&newBuffer->mem, initialCapacity, growFactor)) {
        async_mutex_unlock(mtmsg_global_lock);
        return mtmsg_ERROR_OUT_OF_MEMORY_bytes(L, initialCapacity);
    }
    if (bufferName) {
        newBuffer->bufferName = malloc(bufferNameLength + 1);
        if (newBuffer->bufferName == NULL) {
            async_mutex_unlock(mtmsg_global_lock);
            return mtmsg_ERROR_OUT_OF_MEMORY_bytes(L, bufferNameLength + 1);
        }
        memcpy(newBuffer->bufferName, bufferName, bufferNameLength + 1);
        newBuffer->bufferNameLength = bufferNameLength;
    }
    
    if (atomic_get(&buffer_counter) + 1 > buffer_buckets * 4 || bucket_usage > 30) {
        lua_Integer n = buffer_buckets ? (2 * buffer_buckets) : 64;
        BufferBucket* newList = calloc(n, sizeof(BufferBucket));
        if (newList) {
            newBuckets(n, newList);
        } else if (!buffer_buckets) {
            async_mutex_unlock(mtmsg_global_lock);
            return mtmsg_ERROR_OUT_OF_MEMORY(L);
        }
    }
    toBuckets(newBuffer, buffer_buckets, buffer_bucket_list);
    atomic_inc(&buffer_counter);

    if (sharedMutex != NULL) {
        async_mutex_lock(sharedMutex);
        MsgListener* listener = listenerUdata->listener; 
        
        if (listener->aborted) {
            async_mutex_unlock(sharedMutex);
            async_mutex_unlock(mtmsg_global_lock);
            return mtmsg_ERROR_OPERATION_ABORTED(L);
        }
        
        atomic_inc(&listener->used);

        newBuffer->listener           = listener;
        newBuffer->nextListenerBuffer = listener->firstListenerBuffer;
        listener->firstListenerBuffer = newBuffer;

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

static void freeBuffer2(MsgBuffer* b)
{
    if (b->bufferName) {
        free(b->bufferName);
    }
    if (b->decNotifier && atomic_dec(&b->decNotifier->used) <= 0) {
        b->decNotifier->notifyapi->releaseNotifier(b->decNotifier->notifier);
        free(b->decNotifier);
        b->decNotifier = NULL;
    }
    if (b->incNotifier && atomic_dec(&b->incNotifier->used) <= 0) {
        b->incNotifier->notifyapi->releaseNotifier(b->incNotifier->notifier);
        free(b->incNotifier);
        b->incNotifier = NULL;
    }
    mtmsg_membuf_free(&b->mem);
    free(b);
}

void mtmsg_buffer_free_unreachable(MsgListener* listener, MsgBuffer* b)
{
    removeFromListener(listener, b);
    freeBuffer2(b);
}

void mtmsg_free_buffer(MsgBuffer* b)
{
    bool wasInBucket = (b->prevBufferPtr != NULL);

    if (wasInBucket) {
        *b->prevBufferPtr = b->nextBuffer;
    }
    if (b->nextBuffer) {
        b->nextBuffer->prevBufferPtr = b->prevBufferPtr;
    }
    
    bool needsFree2 = true;

    if (b->sharedMutex == &b->ownMutex) {
        async_mutex_destruct(&b->ownMutex);
    }
    else {
        if (b->listener) {
            MsgListener* listener = b->listener;

            async_mutex_lock(b->sharedMutex);

                if (mtmsg_is_on_ready_list(listener, b)) {
                    b->unreachable = true;
                    needsFree2 = false;
                } else {
                    removeFromListener(listener, b);
                }

            async_mutex_unlock(b->sharedMutex);
            
            if (atomic_dec(&listener->used) == 0) {
                mtmsg_listener_free(listener);
            }
        }
    }
    if (needsFree2) {
        freeBuffer2(b);
    }

    if (wasInBucket) {
        int c = atomic_dec(&buffer_counter);
        if (c == 0) {
            if (buffer_bucket_list)  {
                free(buffer_bucket_list);
            }
            buffer_buckets     = 0;
            buffer_bucket_list = NULL;
            bucket_usage      = 0;
        }
        else if (c * 10 < buffer_buckets) {
            lua_Integer n = 2 * c;
            if (n > 64) {
                BufferBucket* newList = calloc(n, sizeof(BufferBucket));
                if (newList) {
                    newBuckets(n, newList);
                }
            }
        }
    }
}

static int MsgBuffer_release(lua_State* L)
{
    BufferUserData* udata = luaL_checkudata(L, 1, MTMSG_BUFFER_CLASS_NAME);
    MsgBuffer*      b = udata->buffer;

    if (b) {
        async_mutex_lock(mtmsg_global_lock);

        if (atomic_dec(&b->used) == 0) {
            mtmsg_free_buffer(b);
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
    if (b->listener) {
        mtmsg_buffer_remove_from_ready_list(b->listener, b, false);
    }
    mtmsg_membuf_free(&b->mem);
    async_mutex_notify(b->sharedMutex);
    async_mutex_unlock(b->sharedMutex);

    return 0;
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
    pushBufferMeta(L);       /* -> udata, meta */
    lua_setmetatable(L, -2); /* -> udata */

    /* Lock */
    
    async_mutex_lock(mtmsg_global_lock);

    if (mtmsg_abort_flag) {
        async_mutex_unlock(mtmsg_global_lock);
        return mtmsg_ERROR_OPERATION_ABORTED(L);
    }

    MsgBuffer* buffer;
    if (bufferName != NULL) {
        bool unique;
        buffer = findBufferWithName(bufferName, bufferNameLength, &unique);
        if (!buffer) {
            async_mutex_unlock(mtmsg_global_lock);
            return mtmsg_ERROR_UNKNOWN_OBJECT_buffer_name(L, bufferName, bufferNameLength);
        } else if (!unique) {
            async_mutex_unlock(mtmsg_global_lock);
            return mtmsg_ERROR_AMBIGUOUS_NAME_buffer_name(L, bufferName, bufferNameLength);
        }
    } else {
        buffer = findBufferWithId(bufferId);
        if (!buffer) {
            async_mutex_unlock(mtmsg_global_lock);
            return mtmsg_ERROR_UNKNOWN_OBJECT_buffer_id(L, bufferId);
        }
    }

    userData->buffer = buffer;
    atomic_inc(&buffer->used);
    
    async_mutex_unlock(mtmsg_global_lock);
    return 1;
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
        return mtmsg_ERROR_OBJECT_CLOSED(L, qstring);
    }
    b->mem.bufferLength = 0;
    b->msgCount = 0;
    
    if (b->listener) {
        mtmsg_buffer_remove_from_ready_list(b->listener, b, false);
    }

    async_mutex_unlock(b->sharedMutex);
    lua_pushboolean(L, true);
    return 1;
}


typedef struct notify_error_handler_data notify_error_handler_data;
struct notify_error_handler_data {
    char*  buffer;
    size_t len;
};

static void notify_error_handler(void* notify_ehdata, const char* msg, size_t msglen)
{
    notify_error_handler_data* e = (notify_error_handler_data*)notify_ehdata;

    static const char prefix[] = "Error while calling notifier: ";
    static const size_t preflen = sizeof(prefix) - 1;

    if (!e->buffer) {
        e->buffer = malloc(preflen + msglen + 1);
        if (e->buffer) {
            memcpy(e->buffer, prefix, preflen);
            memcpy(e->buffer + preflen, msg, msglen);
            e->buffer[preflen + msglen] = '\0';
            e->len = preflen + msglen;
        }
    } else {
        char* newB = realloc(e->buffer, e->len + 1 + msglen + 1);
        if (newB) {
            e->buffer = newB;
            e->buffer[e->len] = '\n';
            memcpy(e->buffer + e->len + 1, msg, msglen);
            e->buffer[e->len + 1 + msglen] = '\0';
            e->len += 1 + msglen;
        }
    }
}

static int pushNotifyErrorMessage(lua_State* L)
{
    notify_error_handler_data* notify_ehdata = lua_touserdata(L, 1);
    lua_pushlstring(L, notify_ehdata->buffer, notify_ehdata->len);
    return 1;
}

int mtmsg_buffer_call_notifier(lua_State* L, MsgBuffer* b, NotifierHolder* ntf, NotifierHolder** targNtf,
                               receiver_error_handler receiver_eh, void* receiver_ehdata)
{
    if (L) {
        luaL_checkstack(L, 10, NULL);
    }
    notify_error_handler_data notify_ehdata = { NULL, 0 };
    int rc = ntf->notifyapi->notify(ntf->notifier, 
                                    notify_error_handler, &notify_ehdata);
    if (rc == 1) {
        /* notifier should not be called again */
        async_mutex_lock(b->sharedMutex);
        if (*targNtf == ntf) {
            atomic_dec(&ntf->used);
            *targNtf = NULL;
        }
        async_mutex_unlock(b->sharedMutex);
    }
    if (atomic_dec(&ntf->used) <= 0) {
        ntf->notifyapi->releaseNotifier(ntf->notifier);
        free(ntf);
    }
    bool isError = (rc != 0 && rc != 1);
    if (notify_ehdata.buffer) {
        if (L && isError) {
            lua_pushcfunction(L, pushNotifyErrorMessage);
            lua_pushlightuserdata(L, &notify_ehdata);
            lua_pcall(L, 1, 1, 0);
            free(notify_ehdata.buffer);
            return lua_error(L);
        } else {
            if (receiver_eh) {
                receiver_eh(receiver_ehdata, notify_ehdata.buffer, notify_ehdata.len);
            }
            free(notify_ehdata.buffer);
            if (isError) {
                return 999;
            }
        }
    } else if (isError) {
        if (L) {
            return luaL_error(L, "Unknown error while calling notifier. (rc=%d)", rc);
        }
        else {
            if (receiver_eh) {
                char errmsg[80];
                int len = 
            #if _XOPEN_SOURCE >= 500 || _ISOC99_SOURCE || __STDC_VERSION__ >= 199901L
                    snprintf(errmsg, sizeof(errmsg),
            #else
                    sprintf(errmsg,
            #endif
                        "Unknown error while calling notifier. (rc=%d)", rc);
                        
                receiver_eh(receiver_ehdata, errmsg, len);
            }
            return 999;
        }
    }
    return 0;
}

int mtmsg_buffer_set_or_add_msg(lua_State* L, MsgBuffer* b, 
                                              bool nonblock, bool clear, int arg, 
                                              const char* args, size_t args_size, 
                                              receiver_error_handler receiver_eh, void* receiver_ehdata)
{
    if (arg) {
        int errorArg = 0;
        args_size = mtmsg_serialize_calc_args_size(L, arg, &errorArg);
        if (args_size < 0) {
            return luaL_argerror(L, errorArg, "parameter type not supported");
        }
    }
    const size_t header_size = mtmsg_serialize_calc_header_size(args_size);
    const size_t msg_size    = header_size + args_size;
    
    if (nonblock) {
        if (!async_mutex_trylock(b->sharedMutex)) {
            return 3; /* buffer not ready */
        }
    } else {
        async_mutex_lock(b->sharedMutex);
    }
    if (b->closed) {
        async_mutex_unlock(b->sharedMutex);
        if (L) {
            const char* bstring = mtmsg_buffer_tostring(L, b);
            return mtmsg_ERROR_OBJECT_CLOSED(L, bstring);
        } else {
            return 1; /* buffer closed */
        }
    }
    if (b->aborted) {
        async_mutex_unlock(b->sharedMutex);
        if (L) {
            return mtmsg_ERROR_OPERATION_ABORTED(L);
        } else {
            return 2; /* buffer aborted */
        }
    }
    if (clear) {
        b->mem.bufferLength = 0;
        b->msgCount = 0;
    }
    {
        int rc = mtmsg_membuf_reserve(&b->mem, msg_size);
        if (rc != 0) {
            async_mutex_unlock(b->sharedMutex);
            if (rc == -1) {
                /* buffer should not grow */
                if (msg_size <= b->mem.bufferCapacity) {
                    /* queue is full */
                    return 4;
                } else {
                    /* message is too large */
                    if (L) {
                        const char* bstring = mtmsg_buffer_tostring(L, b);
                        return mtmsg_ERROR_MESSAGE_SIZE_bytes(L, msg_size, b->mem.bufferCapacity, bstring);
                    } else {
                        return 5;
                    }
                }
            } else {
                if (L) {
                    return mtmsg_ERROR_OUT_OF_MEMORY_bytes(L, b->mem.bufferLength + msg_size);
                } else {
                    return 6;
                }
            }
        }
    }
    char* msgBufferStart = b->mem.bufferStart + b->mem.bufferLength;

    mtmsg_serialize_header_to_buffer(args_size, msgBufferStart);

    if (arg) {
        mtmsg_serialize_args_to_buffer(L, arg, msgBufferStart + header_size);
    }
    else if (args_size > 0) {
        memcpy(msgBufferStart + header_size, args, args_size);
    }
    b->mem.bufferLength += msg_size;
    b->msgCount += 1;

    if (b->listener && !mtmsg_is_on_ready_list(b->listener, b)) {
        mtmsg_buffer_add_to_ready_list(b->listener, b);
    }

    NotifierHolder* ntf = b->incNotifier;
    if (ntf) {
        if (b->msgCount > ntf->threshold) {
            atomic_inc(&ntf->used);
        } else {
            ntf = NULL;
        }
    }
    
    async_mutex_notify(b->sharedMutex);
    async_mutex_unlock(b->sharedMutex);
    
    if (ntf) {
        return mtmsg_buffer_call_notifier(L, b, ntf, &b->incNotifier, receiver_eh, receiver_ehdata);
    } else {
        return 0;
    }
}


static int MsgBuffer_setMsg(lua_State* L)
{
    int arg = 1;
    BufferUserData* udata = luaL_checkudata(L, arg++, MTMSG_BUFFER_CLASS_NAME);
    bool clear = true;
    int rc = mtmsg_buffer_set_or_add_msg(L, udata->buffer, udata->nonblock, clear, arg, NULL, 0, NULL, NULL);
    lua_pushboolean(L, rc == 0);
    return 1;
}
static int MsgBuffer_addMsg(lua_State* L)
{
    int arg = 1;
    BufferUserData* udata = luaL_checkudata(L, arg++, MTMSG_BUFFER_CLASS_NAME);
    bool clear = false;
    int rc = mtmsg_buffer_set_or_add_msg(L, udata->buffer, udata->nonblock, clear, arg, NULL, 0, NULL, NULL);
    lua_pushboolean(L, rc == 0);
    return 1;
}

    
int mtmsg_buffer_next_msg(lua_State* L, BufferUserData* udata,
                          MsgBuffer* b, bool nonblock, int arg, double timeoutSeconds , MemBuffer* resultBuffer, size_t* argsSize,
                          sender_error_handler sender_eh, void* sender_ehdata)
{
    lua_Number endTime   = 0; /* 0 = no timeout */

    if (L) {
        if (arg && !lua_isnoneornil(L, arg)) {
            lua_Number waitSeconds = luaL_checknumber(L, arg);
            endTime = mtmsg_current_time_seconds() + waitSeconds;
        }
    } else {
        if (timeoutSeconds >= 0) {
            endTime = mtmsg_current_time_seconds() + timeoutSeconds;
        }
    }

    if (nonblock) {
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
        if (L) {
            const char* qstring = mtmsg_buffer_tostring(L, b);
            return mtmsg_ERROR_OBJECT_CLOSED(L, qstring);
        } else {
            return -1; /* 1 - if sender is closed. */
        }
    }
    
    if (b->aborted) {
        async_mutex_notify(b->sharedMutex);
        async_mutex_unlock(b->sharedMutex);
        if (L) {
            return mtmsg_ERROR_OPERATION_ABORTED(L);
        } else {
            return -2; /* 2 - if sender was aborted. */
        }
    }
    if (b->mem.bufferLength > 0) {
        SerializedMsgSizes sizes;
        mtmsg_serialize_parse_header(b->mem.bufferStart, &sizes);
        if (argsSize) *argsSize = sizes.args_size;
        
        size_t msg_size;
        int    rslt = 0; /* is parsedArgCount for resultBuffer == NULL */
        if (resultBuffer == NULL) {
            GetMsgArgsPar par; par.inBuffer       = b->mem.bufferStart + sizes.header_size;
                               par.inBufferSize   = sizes.args_size;
                               par.inMaxArgCount  = -1;
                               par.parsedLength   = 0;
                               par.parsedArgCount = 0;
                               par.carrayCapi     = udata->carrayCapi;
            lua_pushcfunction(L, mtmsg_serialize_get_msg_args);
            lua_pushlightuserdata(L, &par);
            int rc = lua_pcall(L, 1, LUA_MULTRET, 0);
            if (rc != LUA_OK) {
                async_mutex_unlock(b->sharedMutex);
                return lua_error(L);
            }
            rslt = par.parsedArgCount;
            udata->carrayCapi = par.carrayCapi;
            msg_size = sizes.header_size + par.parsedLength;
        } else {
            int rc = mtmsg_membuf_reserve(resultBuffer, sizes.args_size);
            if (rc != 0) {
                async_mutex_unlock(b->sharedMutex);
                /* rc = -1 : buffer should not grow */
                /* rc = -2 : buffer can    not grow */
                return (rc == -1) ? -4 : -5;
            }
            memcpy(resultBuffer->bufferStart + resultBuffer->bufferLength, 
                   b->mem.bufferStart + sizes.header_size, 
                   sizes.args_size);
            resultBuffer->bufferLength += sizes.args_size;
            
            msg_size = sizes.header_size + sizes.args_size;
            rslt = 1;
        }

        b->mem.bufferLength -= msg_size;
        if (b->mem.bufferLength == 0) {
            b->mem.bufferStart = b->mem.bufferData;
        } else {
            b->mem.bufferStart += msg_size;
        }
        if (b->listener) {
            mtmsg_buffer_remove_from_ready_list(b->listener, b, false);
        }
        b->msgCount -= 1;
        if (b->mem.bufferLength > 0) {
            if (b->listener) {
                mtmsg_buffer_add_to_ready_list(b->listener, b);
            }
            async_mutex_notify(b->sharedMutex);         
        }

        NotifierHolder* ntf = b->decNotifier;
        if (ntf) {
            if (ntf->threshold <= 0 || b->msgCount < ntf->threshold) {
                atomic_inc(&ntf->used);
            } else {
                ntf = NULL;
            }
        }
        
        async_mutex_unlock(b->sharedMutex);

        if (ntf) {
            int rc2 = mtmsg_buffer_call_notifier(L, b, ntf, &b->decNotifier, sender_eh, sender_ehdata);
            if (rc2 != 0) {
                return -999;
            }
        }
        return rslt;
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
            if (nonblock) {
                async_mutex_unlock(b->sharedMutex);
                return 0;
            } else {
                async_mutex_wait(b->sharedMutex);
                goto again;
            }
        }
    }
}

static int MsgBuffer_nextMsg(lua_State* L)
{
    int arg = 1;
    BufferUserData* udata = luaL_checkudata(L, arg++, MTMSG_BUFFER_CLASS_NAME);
    
    int parsedArgCount = mtmsg_buffer_next_msg(L, udata, udata->buffer, udata->nonblock, arg, 0 /* timeout from arg */, NULL, NULL,
                                                  NULL, NULL);
    return parsedArgCount; /* parsedArgCount because resultBuffer is NULL */
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

static int Mtmsg_notifier(lua_State* L)
{
    int arg = 1;
    BufferUserData* udata = luaL_checkudata(L, arg++, MTMSG_BUFFER_CLASS_NAME);
    MsgBuffer*      b = udata->buffer;

    const notify_capi* notifyapi = NULL;
    notify_notifier*   notifier  = NULL;

    if (lua_gettop(L) >= arg) {
        int errReason = 0;
        notifyapi = notify_get_capi(L, arg, &errReason);
        if (notifyapi) {
            notifier = notifyapi->toNotifier(L, arg);
            if (!notifier) {
                return luaL_argerror(L, arg, "invalid notifier");
            }
            arg += 1;
        }
        else if (errReason == 1) {
            return luaL_argerror(L, arg, "notifier api version mismatch");
        }
        else if (lua_type(L, arg) != LUA_TNIL) {
            return luaL_argerror(L, arg, "notifier expected");
        }
    }
    else {
        return luaL_argerror(L, arg, "notifier or nil expected");
    }
    int threshold = 0;
    int opt = 1;
    
    static const char* const opts[] = { "<", ">", NULL};
    if (!lua_isnoneornil(L, arg)) {
        opt = luaL_checkoption(L, arg++, NULL, opts);
        if (!lua_isnoneornil(L, arg)) {
            threshold = luaL_checkinteger(L, arg);
        }
    }
    async_mutex_lock(b->sharedMutex);
    {
        NotifierHolder** targNtf = (opt == 0) ? &b->decNotifier 
                                              : &b->incNotifier;
        NotifierHolder*  oldNtf = *targNtf;
        
        if (b->closed) {
            async_mutex_unlock(b->sharedMutex);
            const char* qstring = mtmsg_buffer_tostring(L, b);
            return mtmsg_ERROR_OBJECT_CLOSED(L, qstring);
        }
        else if (oldNtf && notifier) {
            async_mutex_unlock(b->sharedMutex);
            return mtmsg_MTMSG_ERROR_HAS_NOTIFIER(L);
        }
        else if (oldNtf) {
            if (atomic_dec(&oldNtf->used) <= 0) {
                oldNtf->notifyapi->releaseNotifier(oldNtf->notifier);
                free(oldNtf);
            }
            *targNtf = NULL;
        }
        else if (notifier) {
            NotifierHolder* notifierHolder = calloc(1, sizeof(NotifierHolder));
            if (!notifierHolder) {
                async_mutex_unlock(b->sharedMutex);
                return mtmsg_ERROR_OUT_OF_MEMORY(L);
            }
            notifierHolder->used           = 1;
            notifierHolder->notifier       = notifier;
            notifierHolder->notifyapi      = notifyapi;
            notifierHolder->threshold = threshold;

            *targNtf = notifierHolder;

            notifyapi->retainNotifier(notifier);
        }
    }
    async_mutex_unlock(b->sharedMutex);

    return 0;
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
        return mtmsg_ERROR_OBJECT_CLOSED(L, qstring);
    }
    if (b->aborted) {
        async_mutex_unlock(b->sharedMutex);
        return mtmsg_ERROR_OPERATION_ABORTED(L);
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
    bufferAbort(b, abortFlag);
    return 0;
}

static int MsgBuffer_isAbort(lua_State* L)
{
    int arg = 1;
    BufferUserData* udata = luaL_checkudata(L, arg++, MTMSG_BUFFER_CLASS_NAME);
    MsgBuffer*      b = udata->buffer;

    async_mutex_lock(b->sharedMutex);
    lua_pushboolean(L, b->aborted);
    async_mutex_unlock(b->sharedMutex);

    return 1;
}

static int MsgBuffer_msgcnt(lua_State* L)
{
    int arg = 1;
    BufferUserData* udata = luaL_checkudata(L, arg++, MTMSG_BUFFER_CLASS_NAME);
    MsgBuffer*      b = udata->buffer;

    async_mutex_lock(b->sharedMutex);
    lua_pushinteger(L, b->msgCount);
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
    { "notifier",   Mtmsg_notifier       },
    { "nonblock",   MsgBuffer_nonblock   },
    { "isnonblock", MsgBuffer_isNonblock },
    { "close",      MsgBuffer_close      },
    { "abort",      MsgBuffer_abort      },
    { "isabort",    MsgBuffer_isAbort    },
    { "msgcnt",     MsgBuffer_msgcnt     },
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

static void setupBufferMeta(lua_State* L)
{                                                        /* -> meta */
    lua_pushstring(L, MTMSG_BUFFER_CLASS_NAME);          /* -> meta, className */
    lua_setfield(L, -2, "__metatable");                  /* -> meta */

    luaL_setfuncs(L, MsgBufferMetaMethods, 0);           /* -> meta */
    
    lua_newtable(L);                                     /* -> meta, BufferClass */
    luaL_setfuncs(L, MsgBufferMethods, 0);               /* -> meta, BufferClass */
    lua_setfield (L, -2, "__index");                     /* -> meta */
    
    receiver_set_capi(L, -1, &mtmsg_receiver_capi_impl); /* -> meta */
    sender_set_capi  (L, -1, &mtmsg_sender_capi_impl); /* -> meta */
    notify_set_capi  (L, -1, &mtmsg_notify_capi_impl);   /* -> meta */
}


int mtmsg_buffer_init_module(lua_State* L, int module)
{
    if (luaL_newmetatable(L, MTMSG_BUFFER_CLASS_NAME)) {
        setupBufferMeta(L);
    }
    lua_pop(L, 1);
    
    lua_pushvalue(L, module);
        luaL_setfuncs(L, ModuleFunctions, 0);
    lua_pop(L, 1);

    return 0;
}

