#include "error.h"

static const char* const MTMSG_ERROR_CLASS_NAME = "mtmsg.error";

static const char* const MTMSG_ERROR_AMBIGUOUS_NAME    = "ambiguous_name";
static const char* const MTMSG_ERROR_UNKNOWN_OBJECT    = "unknown_object";
static const char* const MTMSG_ERROR_NO_BUFFERS        = "no_buffers";
static const char* const MTMSG_ERROR_OBJECT_CLOSED     = "object_closed";
static const char* const MTMSG_ERROR_OPERATION_ABORTED = "operation_aborted";
static const char* const MTMSG_ERROR_MESSAGE_SIZE      = "message_size";
static const char* const MTMSG_ERROR_OUT_OF_MEMORY     = "out_of_memory";


typedef struct Error {
    const char* name;
    int         details;
    int         traceback;
} Error;

static void pushErrorMessage(lua_State* L, const char* name, int details)
{
    if (details != 0) {
        lua_pushfstring(L, "%s.%s: %s", MTMSG_ERROR_CLASS_NAME, 
                                        name, 
                                        lua_tostring(L, details));
    } else {
        lua_pushfstring(L, "%s.%s", MTMSG_ERROR_CLASS_NAME, 
                                    name);
    }
}

/* error message details must be on top of stack */
static int throwErrorMessage(lua_State* L, const char* errorName)
{
    int messageDetails = lua_gettop(L);
    pushErrorMessage(L, errorName, messageDetails);
    lua_remove(L, messageDetails);
    return lua_error(L);
}

static int throwError(lua_State* L, const char* errorName)
{
    pushErrorMessage(L, errorName, 0);
    return lua_error(L);
}

int mtmsg_ERROR_UNKNOWN_OBJECT_buffer_name(lua_State* L, const char* bufferName, size_t nameLength)
{
    mtmsg_util_quote_lstring(L, bufferName, nameLength);
    lua_pushfstring(L, "buffer name %s", lua_tostring(L, -1));
    return throwErrorMessage(L, MTMSG_ERROR_UNKNOWN_OBJECT);
}

int mtmsg_ERROR_UNKNOWN_OBJECT_buffer_id(lua_State* L, lua_Integer id)
{
    lua_pushfstring(L, "buffer id %d", (int)id);
    return throwErrorMessage(L, MTMSG_ERROR_UNKNOWN_OBJECT);
}

int mtmsg_ERROR_AMBIGUOUS_NAME_buffer_name(lua_State* L, const char* bufferName, size_t nameLength)
{
    mtmsg_util_quote_lstring(L, bufferName, nameLength);
    lua_pushfstring(L, "buffer name %s", lua_tostring(L, -1));
    return throwErrorMessage(L, MTMSG_ERROR_AMBIGUOUS_NAME);
}



int mtmsg_ERROR_UNKNOWN_OBJECT_listener_id(lua_State* L, lua_Integer id)
{
    lua_pushfstring(L, "listener id %d", (int)id);
    return throwErrorMessage(L, MTMSG_ERROR_UNKNOWN_OBJECT);
}

int mtmsg_ERROR_UNKNOWN_OBJECT_listener_name(lua_State* L, const char* listenerName, size_t nameLength)
{
    mtmsg_util_quote_lstring(L, listenerName, nameLength);
    lua_pushfstring(L, "listener name %s", lua_tostring(L, -1));
    return throwErrorMessage(L, MTMSG_ERROR_UNKNOWN_OBJECT);
}

int mtmsg_ERROR_AMBIGUOUS_NAME_listener_name(lua_State* L, const char* listenerName, size_t nameLength)
{
    mtmsg_util_quote_lstring(L, listenerName, nameLength);
    lua_pushfstring(L, "listener name %s", lua_tostring(L, -1));
    return throwErrorMessage(L, MTMSG_ERROR_AMBIGUOUS_NAME);
}

int mtmsg_ERROR_NO_BUFFERS(lua_State* L, const char* objectString)
{
    lua_pushfstring(L, "%s", objectString);
    return throwErrorMessage(L, MTMSG_ERROR_NO_BUFFERS);
}

int mtmsg_ERROR_OBJECT_CLOSED(lua_State* L, const char* objectString)
{
    lua_pushfstring(L, "%s", objectString);
    return throwErrorMessage(L, MTMSG_ERROR_OBJECT_CLOSED);
}

int mtmsg_ERROR_OPERATION_ABORTED(lua_State* L)
{
    return throwError(L, MTMSG_ERROR_OPERATION_ABORTED);
}

int mtmsg_ERROR_OUT_OF_MEMORY(lua_State* L)
{
    return throwError(L, MTMSG_ERROR_OUT_OF_MEMORY);
}

int mtmsg_ERROR_OUT_OF_MEMORY_bytes(lua_State* L, size_t bytes)
{
#if LUA_VERSION_NUM >= 503
    lua_pushfstring(L, "failed to allocate %I bytes", (lua_Integer)bytes);
#else
    lua_pushfstring(L, "failed to allocate %f bytes", (lua_Number)bytes);
#endif
    return throwErrorMessage(L, MTMSG_ERROR_OUT_OF_MEMORY);
}

int mtmsg_ERROR_MESSAGE_SIZE_bytes(lua_State* L, size_t bytes, size_t limit, const char* objectString)
{
#if LUA_VERSION_NUM >= 503
    lua_pushfstring(L, "message size %I exceeds limit of %I bytes for %s", 
                       (lua_Integer)bytes,
                       (lua_Integer)limit,
                       objectString);
#else
    lua_pushfstring(L, "message size %f exceeds limit of %f bytes for %s", 
                       (lua_Number)bytes,
                       (lua_Number)limit,
                       objectString);
#endif
    return throwErrorMessage(L, MTMSG_ERROR_MESSAGE_SIZE);
}



static void publishError(lua_State* L, int module, const char* errorName)
{
    lua_pushfstring(L, "%s.%s", MTMSG_ERROR_CLASS_NAME, errorName);
    lua_setfield(L, module, errorName);
}

int mtmsg_error_init_module(lua_State* L, int errorModule)
{
    publishError(L, errorModule, MTMSG_ERROR_AMBIGUOUS_NAME);
    publishError(L, errorModule, MTMSG_ERROR_UNKNOWN_OBJECT);
    publishError(L, errorModule, MTMSG_ERROR_NO_BUFFERS);
    publishError(L, errorModule, MTMSG_ERROR_OBJECT_CLOSED);
    publishError(L, errorModule, MTMSG_ERROR_OPERATION_ABORTED);
    publishError(L, errorModule, MTMSG_ERROR_MESSAGE_SIZE);
    publishError(L, errorModule, MTMSG_ERROR_OUT_OF_MEMORY);
    
    return 0;
}

