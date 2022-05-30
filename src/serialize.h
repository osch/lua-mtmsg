#ifndef MTMSG_SERIALIZE_H
#define MTMSG_SERIALIZE_H

#include "util.h"
#include "carray_capi.h"

typedef struct carray_capi carray_capi;

typedef enum {
    BUFFER_NIL,
    BUFFER_INTEGER,
    BUFFER_BYTE,
    BUFFER_NUMBER,
    BUFFER_BOOLEAN,
    BUFFER_STRING,
    BUFFER_SMALLSTRING,
    BUFFER_LIGHTUSERDATA,
    BUFFER_CFUNCTION,
    BUFFER_CARRAY
} SerializeDataType;

#define MTMSG_ARG_SIZE_INITIAL       0
#define MTMSG_ARG_SIZE_NIL           1
#define MTMSG_ARG_SIZE_NUMBER        (1 + sizeof(lua_Number))
#define MTMSG_ARG_SIZE_BOOLEAN       (1 + 1)
#define MTMSG_ARG_SIZE_LIGHTUSERDATA (1 + sizeof(void*))
#define MTMSG_ARG_SIZE_CFUNCTION     (1 + sizeof(lua_CFunction))
                                    
typedef struct GetMsgArgsPar {
    const char*        inBuffer;
    size_t             inBufferSize;
    int                inMaxArgCount;
    size_t             parsedLength;
    int                parsedArgCount;
    const carray_capi* carrayCapi;
    int                errorArg;
} GetMsgArgsPar;

typedef struct SerializedMsgSizes {
    size_t header_size;
    size_t args_size;
} SerializedMsgSizes;

size_t mtmsg_serialize_calc_args_size(lua_State* L, int firstArg, int* errorArg);

static inline size_t mtmsg_serialize_calc_integer_size(lua_Integer value) 
{
    if (0 <= value && value <= 0xff) {
        return 1 + 1;
    } else {
        return 1 + sizeof(lua_Integer);
    }
}

static inline size_t mtmsg_serialize_calc_string_size(size_t len) 
{
    if (len <= 0xff) {
        return 1 + 1 + len;
    } else {
        return 1 + sizeof(size_t) + len;
    }
}

static inline size_t mtmsg_serialize_calc_carray_size(carray_info* info) 
{
    return 1 + 1 + 1 + sizeof(size_t) + info->elementSize * info->elementCount;
}


static inline char* mtmsg_serialize_boolean_to_buffer(int value, char* buffer)
{
    *buffer++ = BUFFER_BOOLEAN;
    *buffer++ = value;
    return buffer;
}

static inline char* mtmsg_serialize_integer_to_buffer(lua_Integer value, char* buffer)
{
    if (0 <= value && value <= 0xff) {
        *buffer++ = BUFFER_BYTE;
        *buffer++ = ((char)value);
    } else {
        *buffer++ = BUFFER_INTEGER;
        memcpy(buffer, &value, sizeof(lua_Integer));
        buffer += sizeof(lua_Integer);
    }
    return buffer;
}

static inline char* mtmsg_serialize_number_to_buffer(lua_Number value, char* buffer)
{
    *buffer++ = BUFFER_NUMBER;
    memcpy(buffer, &value, sizeof(lua_Number));
    buffer += sizeof(lua_Number);
    return buffer;
}

static inline char* mtmsg_serialize_string_to_buffer(const char* content, size_t len, char* buffer)
{
    if (len <= 0xff) {
        *buffer++ = BUFFER_SMALLSTRING;
        *buffer++ = ((char)len);
    } else {
        *buffer++ = BUFFER_STRING;
        memcpy(buffer, &len, sizeof(size_t));
        buffer += sizeof(size_t);
    }
    memcpy(buffer, content, len);
    buffer += len;
    return buffer;
}

static inline char* mtmsg_serialize_lightuserdata_to_buffer(void* value, char* buffer)
{
    *buffer++ = BUFFER_LIGHTUSERDATA;
    memcpy(buffer, &value, sizeof(void*));
    buffer += sizeof(void*);
    return buffer;
}

static inline char* mtmsg_serialize_cfunction_to_buffer(lua_CFunction value, char* buffer)
{
    *buffer++ = BUFFER_CFUNCTION;
    memcpy(buffer, &value, sizeof(lua_CFunction));
    buffer += sizeof(lua_CFunction);
    return buffer;
}

static inline char* mtmsg_serialize_carray_header_to_buffer(carray_info* info, char* buffer)
{
    *buffer++ = BUFFER_CARRAY;
    *buffer++ = info->type;
    *buffer++ = (unsigned char)info->elementSize;
    memcpy(buffer, &info->elementCount, sizeof(size_t));
    buffer += sizeof(size_t);
    return buffer;
}

static inline char* mtmsg_serialize_carray_to_buffer(carray_info* info, const void* content, char* buffer)
{
    buffer = mtmsg_serialize_carray_header_to_buffer(info, buffer);
    size_t len = info->elementSize * info->elementCount;
    memcpy(buffer, content, len);
    buffer += len;
    return buffer;
}


void mtmsg_serialize_args_to_buffer(lua_State* L, int firstArg, char* buffer);

int mtmsg_serialize_get_msg_args(lua_State* L);

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
        *(buffer++) = (char)BUFFER_MSGSIZE;
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

#endif /* MTMSG_SERIALIZE_H */

