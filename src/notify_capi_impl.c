#include "notify_capi_impl.h"
#include "buffer.h"
#include "main.h"
#include "serialize.h"

struct notify_writer
{
    MemBuffer mem;
};

static notify_notifier* toNotifier(lua_State* L, int index)
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

static void retainNotifier(notify_notifier* n)
{
    MsgBuffer* b = (MsgBuffer*)n;
    async_mutex_lock(mtmsg_global_lock);
    atomic_inc(&b->used);
    async_mutex_unlock(mtmsg_global_lock);
}


static void releaseNotifier(notify_notifier* n)
{
    MsgBuffer* b = (MsgBuffer*)n;
    async_mutex_lock(mtmsg_global_lock);
    if (atomic_dec(&b->used) == 0) {
        mtmsg_free_buffer(b);
    }
    async_mutex_unlock(mtmsg_global_lock);
}


static int notify(notify_notifier* n, notifier_error_handler eh, void* ehdata)
{
    MsgBuffer* b  = (MsgBuffer*)n;
    int rc = mtmsg_buffer_set_or_add_msg(NULL, b, false, false, 0, NULL, 0, eh, ehdata);
    return rc;
}

const notify_capi mtmsg_notify_capi_impl =
{
    NOTIFY_CAPI_VERSION_MAJOR,
    NOTIFY_CAPI_VERSION_MINOR,
    NOTIFY_CAPI_VERSION_PATCH,
    
    NULL, /* next_capi */
    
    toNotifier,

    retainNotifier,
    releaseNotifier,

    notify,
};
