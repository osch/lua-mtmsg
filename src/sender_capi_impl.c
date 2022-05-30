#include "util.h"

#include "main.h"
#include "serialize.h"
#include "buffer.h"
#include "sender_capi_impl.h"

struct sender_reader
{
    MemBuffer mem;
};



static sender_object* toSender(lua_State* L, int index)
{
    void* buffer = lua_touserdata(L, index);
    if (buffer) {
        if (lua_getmetatable(L, index))
        {                                                      /* -> meta1 */
            if (luaL_getmetatable(L, MTMSG_BUFFER_CLASS_NAME)
                != LUA_TNIL)                                   /* -> meta1, meta2 */
            {
                if (lua_rawequal(L, -1, -2)) {                 /* -> meta1, meta2 */
                    buffer = ((BufferUserData*)buffer)->buffer;
                } else {
                    buffer = NULL;
                }
            }                                                  /* -> meta1, meta2 */
            lua_pop(L, 2);                                     /* -> */
        }                                                      /* -> */
    }
    return buffer;
}

static void retainSender(sender_object* s)
{
    MsgBuffer* b = (MsgBuffer*)s;
    async_mutex_lock(mtmsg_global_lock);
    atomic_inc(&b->used);
    async_mutex_unlock(mtmsg_global_lock);
}

static void releaseSender(sender_object* s)
{
    MsgBuffer* b = (MsgBuffer*)s;
    async_mutex_lock(mtmsg_global_lock);
    if (atomic_dec(&b->used) == 0) {
        mtmsg_free_buffer(b);
    }
    async_mutex_unlock(mtmsg_global_lock);
}


static sender_reader* newReader(size_t initialCapacity, float growFactor)
{
    sender_reader* reader = malloc(sizeof(sender_reader));
    if (reader) {
        if (!mtmsg_membuf_init(&reader->mem, initialCapacity, growFactor)) {
            free(reader);
            reader = NULL;
        }
    }
    return reader;

}

static void freeReader(sender_reader* reader)
{
    if (reader) {
        mtmsg_membuf_free(&reader->mem);
        free(reader);
    }
}

static void clearReader(sender_reader* reader)
{
    reader->mem.bufferStart  = reader->mem.bufferData;
    reader->mem.bufferLength = 0;
}

static void nextValueFromReader(sender_reader* reader, sender_capi_value* out)
{
    out->type = SENDER_CAPI_TYPE_NONE;
    size_t parsedLength = 0;
    
    const char* buffer     = reader->mem.bufferStart;
    size_t      bufferSize = reader->mem.bufferLength;
    
    if (bufferSize > 0) {
        char type = buffer[parsedLength++];
        switch (type) {
            case BUFFER_NIL: {
                out->type = SENDER_CAPI_TYPE_NIL;
                break;
            }
            case BUFFER_INTEGER: {
                lua_Integer value;
                memcpy(&value, buffer + parsedLength, sizeof(lua_Integer));
                parsedLength += sizeof(lua_Integer);
                out->type = SENDER_CAPI_TYPE_INTEGER;
                out->intVal = value;
                break;
            }
            case BUFFER_BYTE: {
                char byte = buffer[parsedLength++];
                lua_Integer value = ((lua_Integer)byte) & 0xff;
                out->type = SENDER_CAPI_TYPE_INTEGER;
                out->intVal = value;
                break;
            }
            case BUFFER_NUMBER: {
                lua_Number value;
                memcpy(&value, buffer + parsedLength, sizeof(lua_Number));
                parsedLength += sizeof(lua_Number);
                out->type = SENDER_CAPI_TYPE_NUMBER;
                out->numVal = value;
                break;
            }
            case BUFFER_BOOLEAN: {
                char b = buffer[parsedLength++];
                out->type    = SENDER_CAPI_TYPE_BOOLEAN;
                out->boolVal = b ? true : false;
                break;
            }
            case BUFFER_STRING: {
                size_t len;
                memcpy(&len, buffer + parsedLength, sizeof(size_t));
                parsedLength += sizeof(size_t);
                out->type = SENDER_CAPI_TYPE_STRING;
                out->strVal.ptr = buffer + parsedLength;
                out->strVal.len = len;
                parsedLength += len;
                break;
            }
            case BUFFER_SMALLSTRING: {
                size_t len = ((size_t)(buffer[parsedLength++])) & 0xff;
                out->type = SENDER_CAPI_TYPE_STRING;
                out->strVal.ptr = buffer + parsedLength;
                out->strVal.len = len;
                parsedLength += len;
                break;
            }
            case BUFFER_LIGHTUSERDATA: {
                void* value = NULL;
                memcpy(&value, buffer + parsedLength, sizeof(void*));
                parsedLength += sizeof(void*);
                out->type    = SENDER_CAPI_TYPE_LIGHTUSERDATA;
                out->ptrVal  = value;
                break;
            }
            case BUFFER_CFUNCTION: {
                lua_CFunction value = NULL;
                memcpy(&value, buffer + parsedLength, sizeof(lua_CFunction));
                parsedLength += sizeof(lua_CFunction);
                out->type    = SENDER_CAPI_TYPE_CFUNCTION;
                out->funcVal = value;
                break;
            }
            case BUFFER_CARRAY: {
                carray_type   carrayType   = (unsigned char)buffer[parsedLength++];
                unsigned char elementSize  = (unsigned char)buffer[parsedLength++];;
                size_t        elementCount;
                memcpy(&elementCount, buffer + parsedLength, sizeof(size_t));
                parsedLength += sizeof(size_t);

                sender_array_type arrayType;
                switch (carrayType) {
                    case CARRAY_UCHAR:  arrayType = SENDER_UCHAR; break;
                    case CARRAY_SCHAR:  arrayType = SENDER_SCHAR; break;
                    case CARRAY_SHORT:  arrayType = SENDER_SHORT;  break;
                    case CARRAY_USHORT: arrayType = SENDER_USHORT; break;
                    case CARRAY_INT:    arrayType = SENDER_INT;  break;
                    case CARRAY_UINT:   arrayType = SENDER_UINT; break;
                    case CARRAY_LONG:   arrayType = SENDER_LONG;  break;
                    case CARRAY_ULONG:  arrayType = SENDER_ULONG; break;
                    case CARRAY_FLOAT:  arrayType = SENDER_FLOAT;  break;
                    case CARRAY_DOUBLE: arrayType = SENDER_DOUBLE; break;
                #if CARRAY_CAPI_HAVE_LONG_LONG
                    case CARRAY_LLONG:  arrayType = SENDER_LLONG;  break;
                    case CARRAY_ULLONG: arrayType = SENDER_ULLONG; break;
                #endif
                    default: arrayType = 0; break;
                }
                out->type = SENDER_CAPI_TYPE_ARRAY;
                out->arrayVal.type         = arrayType;
                out->arrayVal.elementSize  = elementSize;
                out->arrayVal.elementCount = elementCount;
                out->arrayVal.data         = buffer + parsedLength;
                break;
            }
        }
    }
    reader->mem.bufferLength -= parsedLength;
    if (reader->mem.bufferLength == 0) {
        reader->mem.bufferStart = reader->mem.bufferData;
    } else {
        reader->mem.bufferStart += parsedLength;
    }
}

static int nextMessageFromSender(sender_object* sender, sender_reader* reader, 
                                 int nonblock, double timeoutSeconds,
                                 sender_error_handler eh, void* ehdata)
{
    MsgBuffer* buffer = (MsgBuffer*)sender;
    int rc = mtmsg_buffer_next_msg(NULL /* L */, NULL /* udata */,
                                   buffer, nonblock, 0 /* arg */,
                                   timeoutSeconds, &reader->mem, NULL /* args_size */,
                                   eh, ehdata);
    if (rc >= 0) {
        return (rc > 0) ? 0 : 3; /*  3 - if next message is not available */
    } else {
        return -rc;
    }
}


const sender_capi mtmsg_sender_capi_impl =
{
    SENDER_CAPI_VERSION_MAJOR,
    SENDER_CAPI_VERSION_MINOR,
    SENDER_CAPI_VERSION_PATCH,
    
    NULL, /* next_capi */
    
    toSender,
    retainSender,
    releaseSender,

    newReader,
    freeReader,

    clearReader,
    nextValueFromReader,
    nextMessageFromSender
};
