#include "main.h"
#include "async_util.h"
#include "buffer.h"
#include "listener.h"
#include "error.h"

const char* const MTMSG_MODULE_NAME = "mtmsg";

static AtomicPtr atomic_lock_holder = 0;
static bool      initialized        = false;
static int       initCounter        = 0;

Mutex*        mtmsg_global_lock = NULL;
AtomicCounter mtmsg_id_counter  = 0;
bool          mtmsg_abort_flag  = false;

static int internalError(lua_State* L, const char* text, int line) 
{
    return luaL_error(L, "Internal error: %s (%s:%d)", text, MTMSG_MODULE_NAME, line);
}


static int Mtmsg_time(lua_State* L)
{
    lua_pushnumber(L, mtmsg_current_time_seconds());
    return 1;
}

static void mtmsg_abort(bool newFlag)
{
    async_mutex_lock(mtmsg_global_lock);

    mtmsg_abort_flag = newFlag;
    mtmsg_buffer_abort_all(newFlag);
    mtmsg_listener_abort_all(newFlag);

    async_mutex_notify(mtmsg_global_lock);
    async_mutex_unlock(mtmsg_global_lock);
}

static int Mtmsg_abort(lua_State* L)
{
    bool newFlag = true;
    int arg = 1;
    if (lua_gettop(L) >= arg)
    {
        luaL_checktype(L, arg, LUA_TBOOLEAN);
        newFlag = lua_toboolean(L, arg++);
    }
    mtmsg_abort(newFlag);
    return 0;
}

static int Mtmsg_isAbort(lua_State* L)
{
    async_mutex_lock(mtmsg_global_lock);
    lua_pushboolean(L, mtmsg_abort_flag);
    async_mutex_unlock(mtmsg_global_lock);
    return 1;
}

static int Mtmsg_sleep(lua_State* L)
{
    lua_Number waitSeconds = luaL_checknumber(L, 1);
    if (waitSeconds <= 0) {
        return 0;
    }

    lua_Number endTime = mtmsg_current_time_seconds() + waitSeconds;

    async_mutex_lock(mtmsg_global_lock);

again:
    if (mtmsg_abort_flag) {
        async_mutex_notify(mtmsg_global_lock);
        async_mutex_unlock(mtmsg_global_lock);
        return mtmsg_ERROR_OPERATION_ABORTED(L);
    }
    lua_Number now = mtmsg_current_time_seconds();
    if (now < endTime) {
        async_mutex_wait_millis(mtmsg_global_lock, (int)((endTime - now) * 1000 + 0.5));
        goto again;
    }
    async_mutex_unlock(mtmsg_global_lock);

    return 0;
}


static const luaL_Reg ModuleFunctions[] = 
{
    { "time",          Mtmsg_time         },
    { "abort",         Mtmsg_abort        },
    { "isabort",       Mtmsg_isAbort      },
    { "sleep",         Mtmsg_sleep        },
    { NULL,            NULL } /* sentinel */
};

static int handleClosingLuaState(lua_State* L)
{
    async_mutex_lock(mtmsg_global_lock);
    initCounter -= 1;
    if (initCounter == 0) {

    }
    async_mutex_unlock(mtmsg_global_lock);
    return 0;
}


DLL_PUBLIC int luaopen_mtmsg(lua_State* L)
{
#if LUA_VERSION_NUM >= 502
    int luaVersion = (int)*lua_version(L);
    if (luaVersion != LUA_VERSION_NUM) {
        return luaL_error(L, "lua version mismatch: mtmsg was compiled for %d, but current version is %d", LUA_VERSION_NUM, luaVersion);
    }
#endif

    /* ---------------------------------------- */

    if (atomic_get_ptr(&atomic_lock_holder) == NULL) {
        Mutex* newLock = malloc(sizeof(Mutex));
        if (!newLock) {
            return internalError(L, "initialize lock failed", __LINE__);
        }
        async_mutex_init(newLock);

        if (!atomic_set_ptr_if_equal(&atomic_lock_holder, NULL, newLock)) {
            async_mutex_destruct(newLock);
            free(newLock);
        }
    }
    /* ---------------------------------------- */

    async_mutex_lock(atomic_get_ptr(&atomic_lock_holder));
    {
        if (!initialized) {
            mtmsg_global_lock = atomic_get_ptr(&atomic_lock_holder);
            /* create initial id that could not accidently be mistaken with "normal" integers */
            const char* ptr = MTMSG_MODULE_NAME;
            AtomicCounter c = 0;
            if (sizeof(AtomicCounter) - 1 >= 1) {
                int i;
                for (i = 0; i < 2 * sizeof(char*); ++i) {
                    c ^= ((int)(((char*)&ptr)[(i + 1) % sizeof(char*)]) & 0xff) << ((i % (sizeof(AtomicCounter) - 1))*8);
                }
                lua_Number t = mtmsg_current_time_seconds();
                for (i = 0; i < 2 * sizeof(lua_Number); ++i) {
                    c ^= ((int)(((char*)&t)[(i + 1) % sizeof(lua_Number)]) & 0xff) << ((i % (sizeof(AtomicCounter) - 1))*8);
                }
            }
            mtmsg_id_counter = c;
            initialized = true;
        }


        /* check if initialization has been done for this lua state */
        lua_pushlightuserdata(L, (void*)&initialized); /* unique void* key */
            lua_rawget(L, LUA_REGISTRYINDEX); 
            bool alreadyInitializedForThisLua = !lua_isnil(L, -1);
        lua_pop(L, 1);
        
        if (!alreadyInitializedForThisLua) 
        {
            initCounter += 1;
            
            lua_pushlightuserdata(L, (void*)&initialized); /* unique void* key */
                lua_newuserdata(L, 1); /* sentinel for closing lua state */
                    lua_newtable(L); /* metatable for sentinel */
                        lua_pushstring(L, "__gc");
                            lua_pushcfunction(L, handleClosingLuaState);
                        lua_rawset(L, -3); /* metatable.__gc = handleClosingLuaState */
                    lua_setmetatable(L, -2); /* sets metatable for sentinal table */
            lua_rawset(L, LUA_REGISTRYINDEX); /* sets sentinel as value for unique void* in registry */
        }
    }
    async_mutex_unlock(mtmsg_global_lock);

    /* ---------------------------------------- */
    
    int n = lua_gettop(L);
    
    int module      = ++n; lua_newtable(L);
    int errorModule = ++n; lua_newtable(L);

    int bufferMeta = ++n; luaL_newmetatable(L, MTMSG_BUFFER_CLASS_NAME);
    int bufferClass= ++n; lua_newtable(L);

    int listenerMeta = ++n; luaL_newmetatable(L, MTMSG_LISTENER_CLASS_NAME);
    int listenerClass= ++n; lua_newtable(L);

    int errorMeta = ++n; luaL_newmetatable(L, MTMSG_ERROR_CLASS_NAME);
    int errorClass= ++n; lua_newtable(L);


    lua_pushvalue(L, module);
        luaL_setfuncs(L, ModuleFunctions, 0);
    lua_pop(L, 1);
        
    lua_pushvalue(L, errorModule);
    lua_setfield(L, module, "error");

    lua_pushvalue(L, bufferClass);
    lua_setfield (L, bufferMeta, "__index");

    lua_pushvalue(L, listenerClass);
    lua_setfield (L, listenerMeta, "__index");

    lua_pushvalue(L, errorClass);
    lua_setfield (L, errorMeta, "__index");


    lua_checkstack(L, LUA_MINSTACK);
    
    mtmsg_buffer_init_module  (L, module,      bufferMeta,   bufferClass);
    mtmsg_listener_init_module(L, module,      listenerMeta, listenerClass);
    mtmsg_error_init_module   (L, errorModule, errorMeta,    errorClass);
    
    lua_settop(L, module);
    return 1;
}
