#include "error.h"

const char* const MTMSG_ERROR_CLASS_NAME = "mtmsg.error";

static const char* const ERROR_UNKNOWN_OBJECT    = "unknown_object";
static const char* const ERROR_NO_BUFFERS        = "no_buffers";
static const char* const ERROR_OBJECT_EXISTS     = "object_exists";
static const char* const ERROR_OBJECT_CLOSED     = "object_closed";
static const char* const ERROR_OPERATION_ABORTED = "operation_aborted";
static const char* const ERROR_MESSAGE_SIZE      = "message_size";
static const char* const ERROR_OUT_OF_MEMORY     = "out_of_memory";


typedef struct Error {
    const char* name;
    int         message;
    int         fullMessage;
} Error;

// pops message from stack
static void pushErrorClass(lua_State* L, const char* name)
{
    Error* e = lua_newuserdata(L, sizeof(Error));
    e->name        = name;
    e->message     = LUA_NOREF;
    e->fullMessage = LUA_NOREF;
    luaL_setmetatable(L, MTMSG_ERROR_CLASS_NAME);
}

static void pushErrorMessage(lua_State* L, const char* name, int details)
{
    int top = lua_gettop(L);
    
    Error* e = lua_newuserdata(L, sizeof(Error)); ++top;
    e->name  = name;

    const char* detailsString = (details == 0) ? "" : lua_tostring(L, details);

    lua_pushfstring(L, "%s.%s%s", MTMSG_ERROR_CLASS_NAME, name, detailsString);
    int message = ++top;
    
    lua_pushvalue(L, message);
    e->message = luaL_ref(L, LUA_REGISTRYINDEX);

    luaL_traceback(L, L, lua_tostring(L, message), 0);
    e->fullMessage = luaL_ref(L, LUA_REGISTRYINDEX);

    lua_remove(L, message);

    luaL_setmetatable(L, MTMSG_ERROR_CLASS_NAME);
}

// error message details must be on top of stack
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
    lua_pushfstring(L, ": buffer name %s", lua_tostring(L, -1));
    return throwErrorMessage(L, ERROR_UNKNOWN_OBJECT);
}

int mtmsg_ERROR_UNKNOWN_OBJECT_buffer_id(lua_State* L, lua_Integer id)
{
    lua_pushfstring(L, ": buffer id %d", (int)id);
    return throwErrorMessage(L, ERROR_UNKNOWN_OBJECT);
}

int mtmsg_ERROR_UNKNOWN_OBJECT_listener_name(lua_State* L, const char* listenerName, size_t nameLength)
{
    mtmsg_util_quote_lstring(L, listenerName, nameLength);
    lua_pushfstring(L, ": listener name %s", lua_tostring(L, -1));
    return throwErrorMessage(L, ERROR_UNKNOWN_OBJECT);
}

int mtmsg_ERROR_UNKNOWN_OBJECT_listener_id(lua_State* L, lua_Integer id)
{
    lua_pushfstring(L, ": listener id %d", (int)id);
    return throwErrorMessage(L, ERROR_UNKNOWN_OBJECT);
}

int mtmsg_ERROR_NO_BUFFERS(lua_State* L, const char* objectString)
{
    lua_pushfstring(L, ": %s", objectString);
    return throwErrorMessage(L, ERROR_NO_BUFFERS);
}

int mtmsg_ERROR_OBJECT_EXISTS(lua_State* L, const char* objectString)
{
    lua_pushfstring(L, ": %s", objectString);
    return throwErrorMessage(L, ERROR_OBJECT_EXISTS);
}

int mtmsg_ERROR_OBJECT_CLOSED(lua_State* L, const char* objectString)
{
    lua_pushfstring(L, ": %s", objectString);
    return throwErrorMessage(L, ERROR_OBJECT_CLOSED);
}

int mtmsg_ERROR_OPERATION_ABORTED(lua_State* L)
{
    return throwError(L, ERROR_OPERATION_ABORTED);
}

int mtmsg_ERROR_OUT_OF_MEMORY(lua_State* L)
{
    return throwError(L, ERROR_OUT_OF_MEMORY);
}

int mtmsg_ERROR_OUT_OF_MEMORY_bytes(lua_State* L, size_t bytes)
{
#if LUA_VERSION_NUM >= 503
    lua_pushfstring(L, ": failed to allocate %I bytes", (lua_Integer)bytes);
#else
    lua_pushfstring(L, ": failed to allocate %f bytes", (lua_Number)bytes);
#endif
    return throwErrorMessage(L, ERROR_OUT_OF_MEMORY);
}

int mtmsg_ERROR_MESSAGE_SIZE_bytes(lua_State* L, size_t bytes, size_t limit, const char* objectString)
{
#if LUA_VERSION_NUM >= 503
    lua_pushfstring(L, ": message size %I exceeds limit of %I bytes for %s", 
                       (lua_Integer)bytes,
                       (lua_Integer)limit,
                       objectString);
#else
    lua_pushfstring(L, ": message size %f exceeds limit of %f bytes for %s", 
                       (lua_Number)bytes,
                       (lua_Number)limit,
                       objectString);
#endif
    return throwErrorMessage(L, ERROR_MESSAGE_SIZE);
}


static int Error_message(lua_State* L)
{
    int arg = 1;
    Error* e = luaL_checkudata(L, arg++, MTMSG_ERROR_CLASS_NAME);
    
    if (e->message != LUA_NOREF) {
        lua_pushfstring(L, "mtmsg.%s", e->name);
        lua_rawgeti(L, LUA_REGISTRYINDEX, e->message);
        return 1;
    } else {
        return 0;
    }
}

static int Error_name(lua_State* L)
{
    int arg = 1;
    Error* e = luaL_checkudata(L, arg++, MTMSG_ERROR_CLASS_NAME);
    
    lua_pushfstring(L, "%s.%s", MTMSG_ERROR_CLASS_NAME, e->name);
    return 1;
}

static int Error_toString(lua_State* L)
{
    int arg = 1;
    Error* e = luaL_checkudata(L, arg++, MTMSG_ERROR_CLASS_NAME);
    
    if (e->fullMessage != LUA_NOREF) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, e->fullMessage);
    } else if (e->message != LUA_NOREF) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, e->message);
    } else {
        lua_pushfstring(L, "%s.%s", MTMSG_ERROR_CLASS_NAME, e->name);
    }
    return 1;
}

static int Error_equals(lua_State* L)
{
    if (lua_gettop(L) < 2 || (!lua_isuserdata(L, 1) && !lua_isuserdata(L, 2))) {
        return luaL_error(L, "bad arguments");
    }
    Error* e1 = luaL_testudata(L, 1, MTMSG_ERROR_CLASS_NAME);
    Error* e2 = luaL_testudata(L, 2, MTMSG_ERROR_CLASS_NAME);
    if (e1 == NULL && e2 == NULL) {
        return luaL_error(L, "bad arguments");
    }
    if (e1 != NULL && e2 != NULL) {
        lua_pushboolean(L, e1->name == e2->name);
    } else {
        lua_pushboolean(L, false);
    }
    return 1;
}

static int Error_release(lua_State* L)
{
    int arg = 1;
    Error* e = luaL_checkudata(L, arg++, MTMSG_ERROR_CLASS_NAME);
    luaL_unref(L, LUA_REGISTRYINDEX, e->message);
    luaL_unref(L, LUA_REGISTRYINDEX, e->fullMessage);
    return 0;
}

static const luaL_Reg ErrorMethods[] = 
{
    { "name",     Error_name    },
    { "message",  Error_message },

    { NULL,       NULL } /* sentinel */
};

static const luaL_Reg ErrorMetaMethods[] = 
{
    { "__tostring", Error_toString },
    { "__eq",       Error_equals   },
    { "__gc",       Error_release  },

    { NULL,       NULL } /* sentinel */
};

static void publishError(lua_State* L, int module, const char* errorName)
{
    pushErrorClass(L, errorName);
    lua_setfield(L, module, errorName);
}

int mtmsg_error_init_module(lua_State* L, int errorModule, int errorMeta, int errorClass)
{
    publishError(L, errorModule, ERROR_UNKNOWN_OBJECT);
    publishError(L, errorModule, ERROR_NO_BUFFERS);
    publishError(L, errorModule, ERROR_OBJECT_EXISTS);
    publishError(L, errorModule, ERROR_OBJECT_CLOSED);
    publishError(L, errorModule, ERROR_OPERATION_ABORTED);
    publishError(L, errorModule, ERROR_MESSAGE_SIZE);
    publishError(L, errorModule, ERROR_OUT_OF_MEMORY);
    
    lua_pushvalue(L, errorMeta);
        luaL_setfuncs(L, ErrorMetaMethods, 0);

        lua_pushvalue(L, errorClass);
            luaL_setfuncs(L, ErrorMethods, 0);
    
    lua_pop(L, 2);

    return 0;
}

