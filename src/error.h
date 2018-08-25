#ifndef MTMSG_ERROR_H
#define MTMSG_ERROR_H

#include "util.h"

extern const char* const MTMSG_ERROR_CLASS_NAME;

int mtmsg_ERROR_UNKNOWN_OBJECT_buffer_name(lua_State* L, const char* bufferName, size_t nameLength);
int mtmsg_ERROR_UNKNOWN_OBJECT_buffer_id(lua_State* L, lua_Integer id);

int mtmsg_ERROR_UNKNOWN_OBJECT_listener_name(lua_State* L, const char* listenerName, size_t nameLength);
int mtmsg_ERROR_UNKNOWN_OBJECT_listener_id(lua_State* L, lua_Integer id);

int mtmsg_ERROR_OBJECT_EXISTS(lua_State* L, const char* objectString);
int mtmsg_ERROR_OBJECT_CLOSED(lua_State* L, const char* objectString);

int mtmsg_ERROR_NO_BUFFERS(lua_State* L, const char* objectString);

int mtmsg_ERROR_OPERATION_ABORTED(lua_State* L);
int mtmsg_ERROR_OUT_OF_MEMORY(lua_State* L);
int mtmsg_ERROR_OUT_OF_MEMORY_bytes(lua_State* L, size_t bytes);
int mtmsg_ERROR_MESSAGE_SIZE_bytes(lua_State* L, size_t bytes, size_t limit, const char* objectString);

int mtmsg_error_init_module(lua_State* L, int module, int errorMeta, int errorClass);


#endif /* MTMSG_ERROR_H */
