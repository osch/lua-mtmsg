#include "capi_impl.h"
#include "buffer.h"
#include "main.h"
#include "serialize.h"

struct mtmsg_writer
{
    MemBuffer mem;
};

static mtmsg_buffer* toBuffer(lua_State* L, int index)
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

static void retainBuffer(mtmsg_buffer* buffer)
{
    MsgBuffer* b = (MsgBuffer*)buffer;
    async_mutex_lock(mtmsg_global_lock);
    atomic_inc(&b->used);
    async_mutex_unlock(mtmsg_global_lock);
}


static void releaseBuffer(mtmsg_buffer* buffer)
{
    MsgBuffer* b = (MsgBuffer*)buffer;
    async_mutex_lock(mtmsg_global_lock);
    if (atomic_dec(&b->used) == 0) {
        mtmsg_free_buffer(b);
    }
    async_mutex_unlock(mtmsg_global_lock);
}

static mtmsg_writer* newWriter(size_t initialCapacity, float growFactor)
{
    mtmsg_writer* writer = malloc(sizeof(mtmsg_writer));
    if (writer) {
        if (!mtmsg_membuf_init(&writer->mem, initialCapacity, growFactor)) {
            free(writer);
            writer = NULL;
        }
    }
    return writer;
}
static void freeWriter(mtmsg_writer* writer)
{
    if (writer) {
        mtmsg_membuf_free(&writer->mem);
    }
}

static void clearWriter(mtmsg_writer* writer)
{
    writer->mem.bufferStart  = writer->mem.bufferData;
    writer->mem.bufferLength = 0;
}

static int addBooleanToWriter(mtmsg_writer* writer, int value)
{
    size_t args_size = MTMSG_ARG_SIZE_BOOLEAN;
    int rc = mtmsg_membuf_reserve(&writer->mem, args_size);
    if (rc == 0) {
        mtmsg_serialize_boolean_to_buffer(value, writer->mem.bufferStart + writer->mem.bufferLength);
        writer->mem.bufferLength += args_size;
    }
    return rc;
}

static int addIntegerToWriter(mtmsg_writer* writer, lua_Integer value)
{
    size_t args_size = mtmsg_serialize_calc_integer_size(value);
    int rc = mtmsg_membuf_reserve(&writer->mem, args_size);
    if (rc == 0) {
        mtmsg_serialize_integer_to_buffer(value, writer->mem.bufferStart + writer->mem.bufferLength);
        writer->mem.bufferLength += args_size;
    }
    return rc;
}

static int addNumberToWriter(mtmsg_writer* writer, lua_Number value)
{
    size_t args_size = MTMSG_ARG_SIZE_NUMBER;
    int rc = mtmsg_membuf_reserve(&writer->mem, args_size);
    if (rc == 0) {
        mtmsg_serialize_number_to_buffer(value, writer->mem.bufferStart + writer->mem.bufferLength);
        writer->mem.bufferLength += args_size;
    }
    return rc;
}

static int addStringToWriter(mtmsg_writer* writer, const char* value, size_t len)
{
    size_t args_size = mtmsg_serialize_calc_string_size(len);
    int rc = mtmsg_membuf_reserve(&writer->mem, args_size);
    if (rc == 0) {
        mtmsg_serialize_string_to_buffer(value, len, writer->mem.bufferStart + writer->mem.bufferLength);
        writer->mem.bufferLength += args_size;
    }
    return rc;
}

static int addMsgToBuffer(mtmsg_buffer* buffer, mtmsg_writer* writer)
{
    MsgBuffer* b  = (MsgBuffer*)buffer;
    bool nonblock = false;
    bool clear    = false;
    int rc = mtmsg_buffer_set_or_add_msg(NULL, b, nonblock, clear, 0, writer->mem.bufferStart, writer->mem.bufferLength);
    return rc;
}

static int setMsgToBuffer(mtmsg_buffer* buffer, mtmsg_writer* writer)
{
    MsgBuffer* b  = (MsgBuffer*)buffer;
    bool nonblock = false;
    bool clear    = true;
    int rc = mtmsg_buffer_set_or_add_msg(NULL, b, nonblock, clear, 0, writer->mem.bufferStart, writer->mem.bufferLength);
    return rc;
}

const mtmsg_capi mtmsg_capi_impl =
{
    MTMSG_CAPI_VERSION_MAJOR,
    MTMSG_CAPI_VERSION_MINOR,
    MTMSG_CAPI_VERSION_PATCH,
    
    NULL, // next_capi
    
    toBuffer,

    retainBuffer,
    releaseBuffer,

    newWriter,
    freeWriter,

    clearWriter,
    addBooleanToWriter,
    addIntegerToWriter,
    addStringToWriter,

    addMsgToBuffer,
    setMsgToBuffer
};
