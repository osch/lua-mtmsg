#include "util.h"

#define CARRAY_CAPI_IMPLEMENT_GET_CAPI 1
#define CARRAY_CAPI_IMPLEMENT_REQUIRE_CAPI 1
#include "carray_capi.h"

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
            } 
            case LUA_TUSERDATA: {
                int errorReason;
                const carray_capi* carrayCapi = carray_get_capi(L, i, &errorReason);
                if (carrayCapi) {
                    carray_info info;
                    const carray* a = carrayCapi->toReadableCarray(L, i, &info);
                    if (a) {
                        rslt += mtmsg_serialize_calc_carray_size(&info);
                        break;
                    } else {
                        /* FALLTHROUGH */
                    }
                } else if (errorReason == 1) {
                    return luaL_argerror(L, i, "carray version mismatch");
                } else {
                    /* FALLTHROUGH */
                }
            }

            /* FALLTHROUGH */
            case LUA_TTABLE:
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
            case LUA_TUSERDATA: {
                int errorReason;
                const carray_capi* carrayCapi = carray_get_capi(L, i, &errorReason);
                if (carrayCapi) {
                    carray_info info;
                    const carray* a = carrayCapi->toReadableCarray(L, i, &info);
                    if (a) {
                        const void* data= carrayCapi->getReadableElementPtr(a, 0, info.elementCount);
                        buffer = mtmsg_serialize_carray_to_buffer(&info, data, buffer);
                        break;
                    } else {
                        /* FALLTHROUGH */
                    }
                } else if (errorReason == 1) {
                    luaL_argerror(L, i, "carray version mismatch");
                    return;
                } else {
                    /* FALLTHROUGH */
                }
            }
            default: {
                break;
            }
        }
    }
}

int raiseCarrayError(lua_State* L, const carray_capi* capi, int reason)
{
    if (!capi && reason == 1) {
        return luaL_error(L, "incompatible carray capi version number");
    } else if (!capi) {
        return luaL_error(L, "carray expected");
    } else {
        return luaL_error(L, "writable carray expected");
    }
}

int mtmsg_serialize_get_msg_args(lua_State* L)
{
    int arg    = 1;
    int argTop = lua_gettop(L);
    
    GetMsgArgsPar* par = (GetMsgArgsPar*)lua_touserdata(L, arg++);
    

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
            while (arg <= argTop) {
                if (!lua_isnil(L, arg)) {
                    int                reason = 0;
                    const carray_capi* capi   = carray_get_capi(L, arg, &reason);
                    carray*            carray = NULL;
                    if (capi) {
                        carray = capi->toWritableCarray(L, arg, NULL);
                    }
                    if (!carray) {
                        par->errorArg = arg;
                        return raiseCarrayError(L, capi, reason);
                    }
                    capi->resizeCarray(carray, 0, 0);
                }
                arg += 1;
            }
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
            case BUFFER_CARRAY: {
                carray_type   elementType  = (unsigned char)buffer[p++];
                unsigned char elementSize  = (unsigned char)buffer[p++];;
                size_t        elementCount;
                memcpy(&elementCount, buffer + p, sizeof(size_t));
                p += sizeof(size_t);
                size_t len = elementSize * elementCount;

                const carray_capi* capi   = NULL;
                carray*            carray = NULL;
                while (arg <= argTop) {
                    if (!lua_isnil(L, arg)) {
                        int reason;
                        capi = carray_get_capi(L, arg, &reason);
                        carray_info info;
                        if (capi) {
                            carray = capi->toWritableCarray(L, arg, &info);
                        }
                        if (!carray) {
                            par->errorArg = arg;
                            return raiseCarrayError(L, capi, reason);
                        }
                        arg += 1;
                        if (info.elementType == elementType && info.elementSize == elementSize) {
                            break;
                        }
                        capi->resizeCarray(carray, 0, 0);
                        capi      = NULL;
                        carray    = NULL;
                    } else {
                        arg += 1;
                    }
                }
                void* data;
                if (carray) {
                    data = capi->resizeCarray(carray, elementCount, 0);
                    if (!data) {
                        par->errorArg = arg - 1;
                        return luaL_error(L, "cannot resize carray");
                    }
                    lua_pushvalue(L, arg - 1);
                } else {
                    if (!par->carrayCapi) {
                        par->carrayCapi = carray_require_capi(L);
                    }
                    if (!par->carrayCapi->newCarray(L, elementType, CARRAY_DEFAULT, elementCount, &data)) {
                        return luaL_error(L, "internal error creating carray for type %d", elementType);
                    }
                }
                memcpy(data, buffer + p, len);
                p += len;
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


