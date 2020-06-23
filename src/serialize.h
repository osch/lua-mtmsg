#ifndef MTMSG_SERIALIZE_H
#define MTMSG_SERIALIZE_H

#include "util.h"

typedef struct GetMsgArgsPar {
    const char* inBuffer;
    size_t      inBufferSize;
    int         inMaxArgCount;
    size_t      parsedLength;
    int         parsedArgCount;
} GetMsgArgsPar;

typedef struct SerializedMsgSizes {
    size_t header_size;
    size_t args_size;
} SerializedMsgSizes;

size_t mtmsg_serialize_calc_args_size(lua_State* L, int firstArg, int* errorArg);

void mtmsg_serialize_args_to_buffer(lua_State* L, int firstArg, char* buffer);

int mtmsg_serialize_get_msg_args(lua_State* L);
int mtmsg_serialize_get_msg_args2(lua_State*   L, GetMsgArgsPar* par);

typedef enum {
    BUFFER_MSGSIZE = 0xff
} SerializeSizeType;

static inline size_t mtmsg_serialize_calc_header_size(size_t args_size)
{
    if (args_size < BUFFER_MSGSIZE) {
        return 1;
    }
    else {
        return 1 + sizeof(size_t);
    }
}

static inline void mtmsg_serialize_header_to_buffer(size_t args_size, char* buffer)
{
    if (args_size < BUFFER_MSGSIZE) {
        *buffer     = (char)args_size;
    }
    else {
        *(buffer++) = BUFFER_MSGSIZE;
        memcpy(buffer, &args_size, sizeof(size_t)); 
    }
}


static inline void mtmsg_serialize_parse_header(const char* buffer, SerializedMsgSizes* sizes) 
{
    unsigned char c = *(buffer++);
    if (c != BUFFER_MSGSIZE) {
        sizes->header_size = 1;
        sizes->args_size   = c;
    }
    else {
        size_t args_size;
        memcpy(&args_size, buffer, sizeof(size_t));
        sizes->header_size = 1 + sizeof(size_t);
        sizes->args_size   = args_size;
    }
}

#endif // MTMSG_SERIALIZE_H

