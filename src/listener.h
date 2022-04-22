#ifndef MTMSG_LISTENER_H
#define MTMSG_LISTENER_H

#include "util.h"

extern const char* const MTMSG_LISTENER_CLASS_NAME;

typedef struct MsgListener {
    lua_Integer          id;
    AtomicCounter        used;
    char*                listenerName;
    size_t               listenerNameLength;
    bool                 aborted;
    bool                 closed;
    Mutex                listenerMutex;

    struct MsgBuffer*    firstListenerBuffer;

    struct MsgListener** prevListenerPtr;
    struct MsgListener*  nextListener;
    
    struct MsgBuffer*    firstReadyBuffer;
    struct MsgBuffer*    lastReadyBuffer;
} MsgListener;

typedef struct ListenerUserData {
    MsgListener*       listener;
    bool               nonblock;
} ListenerUserData;


void mtmsg_listener_free(MsgListener* q);

void mtmsg_listener_abort_all(bool abortFlag);

int mtmsg_listener_next_msg(lua_State* L, MsgListener* lst, bool nonblock, int arg, 
                            MemBuffer* resultBuffer, size_t* argsSize);

int mtmsg_listener_init_module(lua_State* L, int module);


#endif /* MTMSG_LISTENER_H */
