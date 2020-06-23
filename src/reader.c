#include "reader.h"
#include "buffer.h"
#include "listener.h"
#include "error.h"
#include "serialize.h"

static const char* const MTMSG_READER_CLASS_NAME = "mtmsg.reader";

typedef struct ReaderUserData {
    MemBuffer mem;
} ReaderUserData;

static void setupReaderMeta(lua_State* L);

static int pushReaderMeta(lua_State* L)
{
    if (luaL_newmetatable(L, MTMSG_READER_CLASS_NAME)) {
        setupReaderMeta(L);
    }
    return 1;
}

static int Mtmsg_newReader(lua_State* L)
{
    int arg = 1;
    
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

    ReaderUserData* readerUdata = lua_newuserdata(L, sizeof(ReaderUserData)); /* -> udata */
    memset(readerUdata, 0, sizeof(ReaderUserData));
    pushReaderMeta(L);       /* -> udata, meta */
    lua_setmetatable(L, -2); /* -> udata */

    if (!mtmsg_membuf_init(&readerUdata->mem, initialCapacity, growFactor)) {
        return mtmsg_ERROR_OUT_OF_MEMORY_bytes(L, initialCapacity);
    }
    
    return 1;
}


static int Reader_release(lua_State* L)
{
    ReaderUserData* udata = luaL_checkudata(L, 1, MTMSG_READER_CLASS_NAME);
    
    mtmsg_membuf_free(&udata->mem);

    return 0;    
}

static int Reader_clear(lua_State* L)
{
    ReaderUserData* udata   = luaL_checkudata(L, 1, MTMSG_READER_CLASS_NAME);
    udata->mem.bufferStart  = udata->mem.bufferData;
    udata->mem.bufferLength = 0;
    return 0;
}

static int Reader_next(lua_State* L)
{
    int arg = 1;
    ReaderUserData* udata = luaL_checkudata(L, arg++, MTMSG_READER_CLASS_NAME);

    lua_Integer count = 1;
    if (lua_gettop(L) >= arg) {
        count = luaL_checkinteger(L, arg++);
    }
    if (count <= 0 || udata->mem.bufferLength == 0) {
        return 0;
    }
    GetMsgArgsPar par; par.inBuffer       = udata->mem.bufferStart;
                       par.inBufferSize   = udata->mem.bufferLength;
                       par.inMaxArgCount  = count;
                       par.parsedLength   = 0;
                       par.parsedArgCount = 0;

    mtmsg_serialize_get_msg_args2(L, &par);

    udata->mem.bufferLength -= par.parsedLength;
    if (udata->mem.bufferLength == 0) {
        udata->mem.bufferStart = udata->mem.bufferData;
    } else {
        udata->mem.bufferStart += par.parsedLength;
    }
    return par.parsedArgCount;
}

static int Reader_nextMsg(lua_State* L)
{
    int arg = 1;
    ReaderUserData* rudata = luaL_checkudata(L, arg++, MTMSG_READER_CLASS_NAME);
    rudata->mem.bufferStart  = rudata->mem.bufferData;
    rudata->mem.bufferLength = 0;
    
    BufferUserData*   budata = NULL;
    ListenerUserData* ludata = NULL;
    budata = luaL_testudata(L, arg, MTMSG_BUFFER_CLASS_NAME);
    if (!budata) {
        ludata = luaL_testudata(L, arg, MTMSG_LISTENER_CLASS_NAME);
        if (!ludata) {
            lua_pushfstring(L, "%s or %s expected", MTMSG_BUFFER_CLASS_NAME, MTMSG_LISTENER_CLASS_NAME);
            return luaL_argerror(L, arg, lua_tostring(L, -1));
        }
    }
    arg += 1;
    
    size_t args_size = 0;
    int rc = 0;
    if (budata) {    
        rc = mtmsg_buffer_next_msg  (L, budata, arg, &rudata->mem, &args_size);
    } else {
        rc = mtmsg_listener_next_msg(L, ludata, arg, &rudata->mem, &args_size);
    }
    if (rc < 0) {
        if (rc == -1) {
            /* buffer should not grow and message is too large */
            const char* wstring = luaL_tolstring(L, 1, NULL);
            return mtmsg_ERROR_MESSAGE_SIZE_bytes(L, rudata->mem.bufferLength + args_size, rudata->mem.bufferCapacity, wstring);
        } else {
            return mtmsg_ERROR_OUT_OF_MEMORY_bytes(L, rudata->mem.bufferLength + args_size);
        }
    }
    lua_pushboolean(L, rc > 0);
    return 1;
}


static const luaL_Reg ReaderMethods[] = 
{
    { "clear",      Reader_clear      },
    { "next",       Reader_next       },
    { "nextmsg",    Reader_nextMsg    },
    { NULL,         NULL } /* sentinel */
};

static const luaL_Reg ReaderMetaMethods[] = 
{
    { "__gc",       Reader_release  },
    { NULL,         NULL } /* sentinel */
};

static const luaL_Reg ModuleFunctions[] = 
{
    { "newreader",  Mtmsg_newReader  },
    { NULL,         NULL } /* sentinel */
};


static void setupReaderMeta(lua_State* L)
{
    lua_pushstring(L, MTMSG_READER_CLASS_NAME);
    lua_setfield(L, -2, "__metatable");

    luaL_setfuncs(L, ReaderMetaMethods, 0);
    
    lua_newtable(L);  /* ReaderClass */
        luaL_setfuncs(L, ReaderMethods, 0);
    lua_setfield (L, -2, "__index");
}

int mtmsg_reader_init_module(lua_State* L, int module)
{
    if (luaL_newmetatable(L, MTMSG_READER_CLASS_NAME)) {
        setupReaderMeta(L);
    }
    lua_pop(L, 1);
    
    lua_pushvalue(L, module);
        luaL_setfuncs(L, ModuleFunctions, 0);
    lua_pop(L, 1);
    return 0;
}
