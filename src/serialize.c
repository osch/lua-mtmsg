#include "serialize.h"

size_t mtmsg_serialize_calc_args_size(lua_State* L, int firstArg, int* errorArg)
{
    size_t rslt = MTMSG_ARG_SIZE_INITIAL;
    int n = lua_gettop(L);
    int i;
    for (i = firstArg; i <= n; ++i)
    {
        int type = lua_type(L, i);
        switch (type) {
            case LUA_TNIL: {
                rslt += MTMSG_ARG_SIZE_NIL; 
                break;
            }
            case LUA_TNUMBER:  {
                if (lua_isinteger(L, i)) {
                    lua_Integer value = lua_tointeger(L, i);
                    rslt += mtmsg_serialize_calc_integer_size(value);
                } else {
                    rslt += MTMSG_ARG_SIZE_NUMBER;
                }
                break;
            }
            case LUA_TBOOLEAN: {
                rslt += MTMSG_ARG_SIZE_BOOLEAN; 
                break;
            }
            case LUA_TSTRING: {
                size_t len = 0;
                lua_tolstring(L, i, &len);
                rslt += mtmsg_serialize_calc_string_size(len);
                break;
            }
            case LUA_TLIGHTUSERDATA: {
                rslt += MTMSG_ARG_SIZE_LIGHTUSERDATA;
                break;
            }
            case LUA_TFUNCTION: {
                if (lua_iscfunction(L, i)) {
                    rslt += MTMSG_ARG_SIZE_CFUNCTION;
                    break;
                }
            } /* FALLTHROUGH */
            case LUA_TTABLE:
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
    int    n = lua_gettop(L);
    int    i;

    for (i = firstArg; i <= n; ++i)
    {
        int type = lua_type(L, i);
        switch (type) {
            case LUA_TNIL: {
                *buffer++ = BUFFER_NIL;
                break;
            }
            case LUA_TNUMBER:  {
                if (lua_isinteger(L, i)) {
                    lua_Integer value = lua_tointeger(L, i);
                    buffer = mtmsg_serialize_integer_to_buffer(value, buffer);
                } else {
                    lua_Number value = lua_tonumber(L, i);
                    buffer = mtmsg_serialize_number_to_buffer(value, buffer);
                }
                break;
            }
            case LUA_TBOOLEAN: {
                buffer = mtmsg_serialize_boolean_to_buffer(lua_toboolean(L, i), buffer);
                break;
            }
            case LUA_TSTRING: {
                size_t      len     = 0;
                const char* content = lua_tolstring(L, i, &len);
                buffer = mtmsg_serialize_string_to_buffer(content, len, buffer);
                break;
            }
            case LUA_TLIGHTUSERDATA: {
                void* value = lua_touserdata(L, i);
                buffer = mtmsg_serialize_lightuserdata_to_buffer(value, buffer);
                break;
            }
            case LUA_TFUNCTION: {
                lua_CFunction func = lua_tocfunction(L, i);
                if (func) {
                    buffer = mtmsg_serialize_cfunction_to_buffer(func, buffer);
                    break;
                }
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
            case BUFFER_CFUNCTION: {
                lua_CFunction value = NULL;
                memcpy(&value, buffer + p, sizeof(lua_CFunction));
                p += sizeof(lua_CFunction);
                lua_pushcfunction(L, value);
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


