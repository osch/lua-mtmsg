#ifndef MTMSG_BUFFER_H
#define MTMSG_BUFFER_H

#include "util.h"
#include "listener.h"

extern const char* const MTMSG_BUFFER_CLASS_NAME;

typedef struct MsgBuffer {
    lua_Integer        id;
    AtomicCounter      used;
    char*              bufferName;
    size_t             bufferNameLength;
    bool               aborted;
    bool               closed;
    Mutex*             sharedMutex;
    Mutex              ownMutex;
    MemBuffer          mem;
    
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
} BufferUserData;

struct ListenerUserData;

int mtmsg_buffer_new(lua_State* L, struct ListenerUserData* listenerUdata, int arg);

const char* mtmsg_buffer_tostring(lua_State* L, MsgBuffer* q);

typedef struct PushMsgPar {
    const char* buffer;
    size_t      msgLength;
    int         argCount;
} PushMsgPar;

/*size_t mtmsg_buffer_getsize(const char* buffer);*/
int mtmsg_buffer_push_msg(lua_State* L);

int mtmsg_buffer_init_module(lua_State* L, int module, int bufferMeta, int bufferClass);

void mtmsg_buffer_abort_all(bool abortFlag);

static inline void mtmsg_buffer_remove_from_ready_list(MsgListener* listener, MsgBuffer* b)
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
