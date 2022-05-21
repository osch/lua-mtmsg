#ifndef MTMSG_BUFFER_H
#define MTMSG_BUFFER_H

#include "util.h"
#include "listener.h"
#include "notify_capi.h"
#include "receiver_capi.h"
#include "sender_capi.h"

extern const char* const MTMSG_BUFFER_CLASS_NAME;;

typedef struct carray_capi carray_capi;

typedef struct NotifierHolder {
    AtomicCounter      used;
    const notify_capi* notifyapi;
    notify_notifier*   notifier;
    int                threshold;
} NotifierHolder;

typedef struct MsgBuffer {
    lua_Integer        id;
    AtomicCounter      used;
    bool               unreachable;
    char*              bufferName;
    size_t             bufferNameLength;
    bool               aborted;
    bool               closed;
    Mutex*             sharedMutex;
    Mutex              ownMutex;
    MemBuffer          mem;
    NotifierHolder*    decNotifier;
    NotifierHolder*    incNotifier;
    int                msgCount;
    
    struct MsgListener* listener;          
    struct MsgBuffer*   nextListenerBuffer;

    struct MsgBuffer**  prevBufferPtr;
    struct MsgBuffer*   nextBuffer;
    
    struct MsgBuffer*   prevReadyBuffer;
    struct MsgBuffer*   nextReadyBuffer;
    
} MsgBuffer;

typedef struct BufferUserData {
    MsgBuffer*         buffer;
    bool               nonblock;
    const carray_capi* carrayCapi;
} BufferUserData;

struct ListenerUserData;

int mtmsg_buffer_new(lua_State* L, struct ListenerUserData* listenerUdata, int arg);

void mtmsg_free_buffer(MsgBuffer* b);

const char* mtmsg_buffer_tostring(lua_State* L, MsgBuffer* q);

int mtmsg_buffer_init_module(lua_State* L, int module);

void mtmsg_buffer_abort_all(bool abortFlag);

void mtmsg_buffer_free_unreachable(MsgListener* listener, MsgBuffer* b);

int mtmsg_buffer_set_or_add_msg(lua_State* L, MsgBuffer* b, bool nonblock, bool clear, int arg, const char* args, size_t args_size,
                                receiver_error_handler eh, void* ehdata);

int mtmsg_buffer_next_msg(lua_State* L, BufferUserData* u, 
                          MsgBuffer* b, bool nonblock, int arg, double timeoutSeconds, MemBuffer* resultBuffer, size_t* argsSize,
                          sender_error_handler eh, void* ehdata);

int mtmsg_buffer_call_notifier(lua_State* L, MsgBuffer* b, NotifierHolder* ntf, NotifierHolder** targNtf,
                               receiver_error_handler receiver_eh, void* receiver_ehdata);

static inline void mtmsg_buffer_remove_from_ready_list(MsgListener* listener, MsgBuffer* b, bool freeIfUnreachable)
{
    if (b->prevReadyBuffer) {
        b->prevReadyBuffer->nextReadyBuffer = b->nextReadyBuffer;
    }
    else if (listener->firstReadyBuffer == b) {
        listener->firstReadyBuffer = b->nextReadyBuffer;
    }
    if (b->nextReadyBuffer) {
        b->nextReadyBuffer->prevReadyBuffer = b->prevReadyBuffer;
    }
    else if (listener->lastReadyBuffer == b) {
        listener->lastReadyBuffer = b->prevReadyBuffer;
    }
    b->prevReadyBuffer = NULL;
    b->nextReadyBuffer = NULL;
    if (freeIfUnreachable && b->unreachable) {
        mtmsg_buffer_free_unreachable(listener, b);
    }
}
static inline bool mtmsg_is_on_ready_list(MsgListener* listener, MsgBuffer* b)
{
    return (b->prevReadyBuffer) || (listener->firstReadyBuffer == b);
}
static inline void mtmsg_buffer_add_to_ready_list(MsgListener* listener, MsgBuffer* b)
{
    if (listener->lastReadyBuffer) {
        listener->lastReadyBuffer->nextReadyBuffer = b;
        b->prevReadyBuffer = listener->lastReadyBuffer;
        listener->lastReadyBuffer = b;
    } else {
        listener->firstReadyBuffer = b;
        listener->lastReadyBuffer  = b;
    }
}


#endif /* MTMSG_BUFFER_H */
