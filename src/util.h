#ifndef MTMSG_UTIL_H
#define MTMSG_UTIL_H

/* async_defines.h must be included first */
#include "async_defines.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef MTMSG_ASYNC_USE_WIN32
    #include <sys/types.h>
    #include <sys/timeb.h>
#else
    #include <sys/time.h>
#endif

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>


/**
 * dllexport
 *
 * see also http://gcc.gnu.org/wiki/Visibility
 */

#define BUILDING_DLL

#if defined _WIN32 || defined __CYGWIN__
  #ifdef BUILDING_DLL
    #ifdef __GNUC__
      #define DLL_PUBLIC __attribute__ ((dllexport))
    #else
      #define DLL_PUBLIC __declspec(dllexport) /* Note: actually gcc seems to also supports this syntax. */
    #endif
  #else
    #ifdef __GNUC__
      #define DLL_PUBLIC __attribute__ ((dllimport))
    #else
      #define DLL_PUBLIC __declspec(dllimport) /* Note: actually gcc seems to also supports this syntax. */
    #endif
  #endif
  #define DLL_LOCAL
#else
  #if __GNUC__ >= 4
    #pragma GCC visibility push (hidden) 
    #define DLL_PUBLIC __attribute__ ((visibility ("default")))
    #define DLL_LOCAL  __attribute__ ((visibility ("hidden")))
  #else
    #define DLL_PUBLIC
    #define DLL_LOCAL
  #endif
#endif

#define COMPAT53_PREFIX mtmsg_compat

#include "async_util.h"
#include "compat-5.3.h"


lua_Number mtmsg_current_time_seconds();

typedef struct MemBuffer {
    lua_Number         growFactor;
    char*              bufferData;
    char*              bufferStart;
    size_t             bufferLength;
    size_t             bufferCapacity;
} MemBuffer;

bool mtmsg_membuf_init(MemBuffer* b, size_t initialCapacity, lua_Number growFactor);

void mtmsg_membuf_free(MemBuffer* b);

int mtmsg_membuf_reserve0(MemBuffer* b, size_t newLength);

/**
 *  0 : ok
 * -1 : buffer should not grow
 * -2 : buffer can   not grow
 */
static inline int mtmsg_membuf_reserve(MemBuffer* b, size_t additionalLength)
{
    size_t newLength = b->bufferLength + additionalLength;
    
    if (b->bufferStart - b->bufferData + newLength > b->bufferCapacity) 
    {
        return mtmsg_membuf_reserve0(b, newLength);
    } else {
        return 0;
    }
}


void mtmsg_util_quote_lstring(lua_State* L, const char* s, size_t len);

void mtmsg_util_quote_string(lua_State* L, const char* s);

#endif /* MTMSG_UTIL_H */
