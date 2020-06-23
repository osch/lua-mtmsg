#include "serialize.h"

typedef enum {
    BUFFER_NIL,
    BUFFER_INTEGER,
    BUFFER_BYTE,
    BUFFER_NUMBER,
    BUFFER_BOOLEAN,
    BUFFER_STRING,
    BUFFER_SMALLSTRING,
    BUFFER_LIGHTUSERDATA
} SerializeDataType;

size_t mtmsg_serialize_calc_args_size(lua_State* L, int firstArg, int* errorArg)
{
    size_t rslt = 0;
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
                return -1;
            }
        }
    }
    return rslt;
}

void mtmsg_serialize_args_to_buffer(lua_State* L, int firstArg, char* buffer)
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
}

int mtmsg_serialize_get_msg_args(lua_State* L)
{
    GetMsgArgsPar* par = (GetMsgArgsPar*)lua_touserdata(L, 1);

    return mtmsg_serialize_get_msg_args2(L, par);
}
    
int mtmsg_serialize_get_msg_args2(lua_State* L, GetMsgArgsPar* par)
{
    const char*    buffer       = par->inBuffer;
    const size_t   bufferSize   = par->inBufferSize;
    const int      maxArgCount  = par->inMaxArgCount;
    const bool     hasMaxArg    = (maxArgCount >= 0);

    size_t p = 0;
    int    i = 0;
    
    while (true) {
        if (p >= bufferSize || (hasMaxArg && i >= maxArgCount)) {
            par->parsedLength   = p;
            par->parsedArgCount = i; 
            luaL_checkstack(L, LUA_MINSTACK, NULL);
            return i;
        }
        if (i % 10 == 0) {
            luaL_checkstack(L, 10 + LUA_MINSTACK, NULL);
        }
        char type = buffer[p++];
        switch (type) {
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


