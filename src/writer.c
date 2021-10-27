#include "writer.h"
#include "buffer.h"
#include "serialize.h"
#include "error.h"

static const char* const MTMSG_WRITER_CLASS_NAME = "mtmsg.writer";

typedef struct WriterUserData {
    MemBuffer mem;
} WriterUserData;


static void setupWriterMeta(lua_State* L);

static int pushWriterMeta(lua_State* L)
{
    if (luaL_newmetatable(L, MTMSG_WRITER_CLASS_NAME)) {
        setupWriterMeta(L);
    }
    return 1;
}

static int Mtmsg_newWriter(lua_State* L)
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

    WriterUserData* writerUdata = lua_newuserdata(L, sizeof(WriterUserData)); /* -> udata */
    memset(writerUdata, 0, sizeof(WriterUserData));
    pushWriterMeta(L);       /* -> udata, meta */
    lua_setmetatable(L, -2); /* -> udata */

    if (!mtmsg_membuf_init(&writerUdata->mem, initialCapacity, growFactor)) {
        return mtmsg_ERROR_OUT_OF_MEMORY_bytes(L, initialCapacity);
    }
    
    return 1;
}

static int Writer_release(lua_State* L)
{
    WriterUserData* udata = luaL_checkudata(L, 1, MTMSG_WRITER_CLASS_NAME);
    
    mtmsg_membuf_free(&udata->mem);

    return 0;    
}


static int Writer_clear(lua_State* L)
{
    WriterUserData* udata = luaL_checkudata(L, 1, MTMSG_WRITER_CLASS_NAME);
    udata->mem.bufferStart  = udata->mem.bufferData;
    udata->mem.bufferLength = 0;
    return 0;
}


static int Writer_add(lua_State* L)
{
    int arg = 1;
    WriterUserData* udata = luaL_checkudata(L, arg++, MTMSG_WRITER_CLASS_NAME);

    int errorArg = 0;
    const size_t args_size = mtmsg_serialize_calc_args_size(L, arg, &errorArg);

    if (args_size < 0) {
        return luaL_argerror(L, errorArg, "parameter type not supported");
    }

    int rc = mtmsg_membuf_reserve(&udata->mem, args_size);

    if (rc != 0) {
        if (rc == -1) {
            /* buffer should not grow and message is too large */
            const char* wstring = luaL_tolstring(L, 1, NULL);
            return mtmsg_ERROR_MESSAGE_SIZE_bytes(L, udata->mem.bufferLength + args_size, udata->mem.bufferCapacity, wstring);
        } else {
            return mtmsg_ERROR_OUT_OF_MEMORY_bytes(L, udata->mem.bufferLength + args_size);
        }
    }
    mtmsg_serialize_args_to_buffer(L, arg, udata->mem.bufferStart + udata->mem.bufferLength);
    udata->mem.bufferLength += args_size;
    
    return 0;
}

static int Writer_addMsg(lua_State* L)
{
    int arg = 1;
    WriterUserData* wudata = luaL_checkudata(L, arg++, MTMSG_WRITER_CLASS_NAME);
    BufferUserData* budata = luaL_checkudata(L, arg++, MTMSG_BUFFER_CLASS_NAME);
    bool clear = false;
    int rc = mtmsg_buffer_set_or_add_msg(L, budata->buffer, budata->nonblock, clear, 0, wudata->mem.bufferStart, wudata->mem.bufferLength);
    if (rc == 0) {
        wudata->mem.bufferLength = 0;
    }
    lua_pushboolean(L, rc == 0);
    return 1;
}

static int Writer_setMsg(lua_State* L)
{
    int arg = 1;
    WriterUserData* wudata = luaL_checkudata(L, arg++, MTMSG_WRITER_CLASS_NAME);
    BufferUserData* budata = luaL_checkudata(L, arg++, MTMSG_BUFFER_CLASS_NAME);
    bool clear = true;
    int rc = mtmsg_buffer_set_or_add_msg(L, budata->buffer, budata->nonblock, clear, 0, wudata->mem.bufferStart, wudata->mem.bufferLength);
    if (rc == 0) {
        wudata->mem.bufferLength = 0;
    }
    lua_pushboolean(L, rc == 0);
    return 1;
}

static const luaL_Reg WriterMethods[] = 
{
    { "clear",      Writer_clear      },
    { "add",        Writer_add        },
    { "addmsg",     Writer_addMsg     },
    { "setmsg",     Writer_setMsg     },
    { NULL,         NULL } /* sentinel */
};

static const luaL_Reg WriterMetaMethods[] = 
{
    { "__gc",       Writer_release  },

    { NULL,       NULL } /* sentinel */
};

static const luaL_Reg ModuleFunctions[] = 
{
    { "newwriter", Mtmsg_newWriter  },
    { NULL,        NULL } /* sentinel */
};



static void setupWriterMeta(lua_State* L)
{
    lua_pushstring(L, MTMSG_WRITER_CLASS_NAME);
    lua_setfield(L, -2, "__metatable");

    luaL_setfuncs(L, WriterMetaMethods, 0);
    
    lua_newtable(L);  /* WriterClass */
        luaL_setfuncs(L, WriterMethods, 0);
    lua_setfield (L, -2, "__index");
}

int mtmsg_writer_init_module(lua_State* L, int module)
{
    if (luaL_newmetatable(L, MTMSG_WRITER_CLASS_NAME)) {
        setupWriterMeta(L);
    }
    lua_pop(L, 1);
    
    lua_pushvalue(L, module);
        luaL_setfuncs(L, ModuleFunctions, 0);
    lua_pop(L, 1);

    return 0;
}

