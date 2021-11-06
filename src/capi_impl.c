#include "capi_impl.h"
#include "buffer.h"
#include "main.h"
#include "serialize.h"

struct receiver_writer
{
    MemBuffer mem;
};

static receiver_object* toReceiver(lua_State* L, int index)
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

static void retainReceiver(receiver_object* buffer)
{
    MsgBuffer* b = (MsgBuffer*)buffer;
    async_mutex_lock(mtmsg_global_lock);
    atomic_inc(&b->used);
    async_mutex_unlock(mtmsg_global_lock);
}


static void releaseReceiver(receiver_object* buffer)
{
    MsgBuffer* b = (MsgBuffer*)buffer;
    async_mutex_lock(mtmsg_global_lock);
    if (atomic_dec(&b->used) == 0) {
        mtmsg_free_buffer(b);
    }
    async_mutex_unlock(mtmsg_global_lock);
}

static receiver_writer* newWriter(size_t initialCapacity, float growFactor)
{
    receiver_writer* writer = malloc(sizeof(receiver_writer));
    if (writer) {
        if (!mtmsg_membuf_init(&writer->mem, initialCapacity, growFactor)) {
            free(writer);
            writer = NULL;
        }
    }
    return writer;
}
static void freeWriter(receiver_writer* writer)
{
    if (writer) {
        mtmsg_membuf_free(&writer->mem);
    }
}

static void clearWriter(receiver_writer* writer)
{
    writer->mem.bufferStart  = writer->mem.bufferData;
    writer->mem.bufferLength = 0;
}

static int addBooleanToWriter(receiver_writer* writer, int value)
{
    size_t args_size = MTMSG_ARG_SIZE_BOOLEAN;
    int rc = mtmsg_membuf_reserve(&writer->mem, args_size);
    if (rc == 0) {
        mtmsg_serialize_boolean_to_buffer(value, writer->mem.bufferStart + writer->mem.bufferLength);
        writer->mem.bufferLength += args_size;
    }
    return rc;
}

static int addIntegerToWriter(receiver_writer* writer, lua_Integer value)
{
    size_t args_size = mtmsg_serialize_calc_integer_size(value);
    int rc = mtmsg_membuf_reserve(&writer->mem, args_size);
    if (rc == 0) {
        mtmsg_serialize_integer_to_buffer(value, writer->mem.bufferStart + writer->mem.bufferLength);
        writer->mem.bufferLength += args_size;
    }
    return rc;
}

static int addNumberToWriter(receiver_writer* writer, lua_Number value)
{
    size_t args_size = MTMSG_ARG_SIZE_NUMBER;
    int rc = mtmsg_membuf_reserve(&writer->mem, args_size);
    if (rc == 0) {
        mtmsg_serialize_number_to_buffer(value, writer->mem.bufferStart + writer->mem.bufferLength);
        writer->mem.bufferLength += args_size;
    }
    return rc;
}

static int addStringToWriter(receiver_writer* writer, const char* value, size_t len)
{
    size_t args_size = mtmsg_serialize_calc_string_size(len);
    int rc = mtmsg_membuf_reserve(&writer->mem, args_size);
    if (rc == 0) {
        mtmsg_serialize_string_to_buffer(value, len, writer->mem.bufferStart + writer->mem.bufferLength);
        writer->mem.bufferLength += args_size;
    }
    return rc;
}

static int addMsgToReceiver(receiver_object* buffer, receiver_writer* writer, receiver_error_handler eh, void* ehdata)
{
    MsgBuffer* b  = (MsgBuffer*)buffer;
    bool nonblock = false;
    bool clear    = false;
    int rc = mtmsg_buffer_set_or_add_msg(NULL, b, nonblock, clear, 0, writer->mem.bufferStart, writer->mem.bufferLength, eh, ehdata);
    if (rc == 0) {
        writer->mem.bufferLength = 0;
    }
    return rc;
}

static int setMsgToReceiver(receiver_object* buffer, receiver_writer* writer, receiver_error_handler eh, void* ehdata)
{
    MsgBuffer* b  = (MsgBuffer*)buffer;
    bool nonblock = false;
    bool clear    = true;
    int rc = mtmsg_buffer_set_or_add_msg(NULL, b, nonblock, clear, 0, writer->mem.bufferStart, writer->mem.bufferLength, eh, ehdata);
    if (rc == 0) {
        writer->mem.bufferLength = 0;
    }
    return rc;
}

const receiver_capi mtmsg_receiver_capi_impl =
{
    RECEIVER_CAPI_VERSION_MAJOR,
    RECEIVER_CAPI_VERSION_MINOR,
    RECEIVER_CAPI_VERSION_PATCH,
    
    NULL, /* next_capi */
    
    toReceiver,

    retainReceiver,
    releaseReceiver,

    newWriter,
    freeWriter,

    clearWriter,
    addBooleanToWriter,
    addIntegerToWriter,
    addStringToWriter,

    addMsgToReceiver,
    setMsgToReceiver
};
