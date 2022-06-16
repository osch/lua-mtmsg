#ifndef SENDER_CAPI_H
#define SENDER_CAPI_H

#define SENDER_CAPI_ID_STRING     "_capi_sender"
#define SENDER_CAPI_VERSION_MAJOR  1
#define SENDER_CAPI_VERSION_MINOR  0
#define SENDER_CAPI_VERSION_PATCH  0

#ifndef SENDER_CAPI_HAVE_LONG_LONG
#  include <limits.h>
#  if defined(LLONG_MAX)
#    define SENDER_CAPI_HAVE_LONG_LONG 1
#  else
#    define SENDER_CAPI_HAVE_LONG_LONG 0
#  endif
#endif

#ifndef SENDER_CAPI_IMPLEMENT_SET_CAPI
#  define SENDER_CAPI_IMPLEMENT_SET_CAPI 0
#endif

#ifndef SENDER_CAPI_IMPLEMENT_GET_CAPI
#  define SENDER_CAPI_IMPLEMENT_GET_CAPI 0
#endif

#ifdef __cplusplus

extern "C" {

struct sender_object;
struct sender_reader;
struct sender_capi;
struct sender_capi_value;

#else /* __cplusplus */

typedef struct sender_object          sender_object;
typedef struct sender_reader          sender_reader;
typedef struct sender_capi            sender_capi;
typedef struct sender_capi_value      sender_capi_value;

typedef enum   sender_capi_value_type sender_capi_value_type;
typedef enum   sender_array_type      sender_array_type;

#endif /* ! __cplusplus */

enum sender_capi_value_type
{
    SENDER_CAPI_TYPE_NONE = 0,
    SENDER_CAPI_TYPE_NIL,
    SENDER_CAPI_TYPE_BOOLEAN,
    SENDER_CAPI_TYPE_INTEGER,
    SENDER_CAPI_TYPE_NUMBER,
    SENDER_CAPI_TYPE_LIGHTUSERDATA,
    SENDER_CAPI_TYPE_CFUNCTION,
    SENDER_CAPI_TYPE_STRING,
    SENDER_CAPI_TYPE_ARRAY
};

enum sender_array_type
{
    SENDER_UCHAR  =  1,
    SENDER_SCHAR  =  2,
    
    SENDER_SHORT  =  3,
    SENDER_USHORT =  4,
    
    SENDER_INT    =  5,
    SENDER_UINT   =  6,
    
    SENDER_LONG   =  7,
    SENDER_ULONG  =  8,
    
    SENDER_FLOAT  =  9,
    SENDER_DOUBLE = 10,

#if SENDER_CAPI_HAVE_LONG_LONG
    SENDER_LLONG  = 11,
    SENDER_ULLONG = 12,
#endif
};

struct sender_capi_value
{
    sender_capi_value_type type;
    union {
        int           boolVal;
        lua_Integer   intVal;
        lua_Number    numVal;
        void*         ptrVal;
        lua_CFunction funcVal;
        struct {
            const char*       ptr;
            size_t            len;
         }            strVal;
        struct {
            sender_array_type type;
            size_t            elementSize;
            size_t            elementCount;
            const void*       data;
        }             arrayVal;
    };
};

/**
 * Type for pointer to function that may be called if an error occurs.
 * ehdata: void pointer that is given in add/setMsgToSender method (see below)
 * msg:    detailed error message
 * msglen: length of error message
 */
typedef void (*sender_error_handler)(void* ehdata, const char* msg, size_t msglen);


/**
 *  Sender C API.
 */
struct sender_capi
{
    int version_major;
    int version_minor;
    int version_patch;
    
    /**
     * May point to another (incompatible) version of this API implementation.
     * NULL if no such implementation exists.
     *
     * The usage of next_capi makes it possible to implement two or more
     * incompatible versions of the C API.
     *
     * An API is compatible to another API if both have the same major 
     * version number and if the minor version number of the first API is
     * greater or equal than the second one's.
     */
    void* next_capi;
    
    /**
     * Must return a valid pointer if the object at the given stack
     * index is a valid sender object, otherwise must return NULL,
     */
    sender_object* (*toSender)(lua_State* L, int index);

    /**
     * Increase the reference counter of the sender object.
     * Must be thread safe.
     */
    void (*retainSender)(sender_object* s);

    /**
     * Decrease the reference counter of the sender object and
     * destructs the sender object if no reference is left.
     * Must be thread safe.
     */
    void (*releaseSender)(sender_object* s);

    /**
     * Creates new reader object.
     * Does not need to be thread safe.
     */
    sender_reader* (*newReader)(size_t initialCapacity, float growFactor);

    /**
     * Destructs reader object.
     * Does not need to be thread safe.
     */
    void (*freeReader)(sender_reader* r);

    /**
     * Does not need to be thread safe.
     */
    void (*clearReader)(sender_reader* r);

    /**
     * Does not need to be thread safe.
     * Sets out->type to SENDER_CAPI_TYPE_NONE if there is
     * no value left.
     */
    void (*nextValueFromReader)(sender_reader* r, sender_capi_value* out);

    /**
     * Must be thread safe.
     *
     * Gets the next message of a sender into the reader in one atomic step. The
     * message is removed from the given sender. If the reader contained older
     * message elements from a previous message these are discarded before all messages elements 
     * from the new message are added to the reader, i.e. after this call the reader contains 
     * only message elements from the new message.
     *
     * nonblock: not 0 if function should do nothing if sender cannot send immediately,
     *           0 if function should wait for sender to becomde ready.
     * timeout   time in seconds to wait until a message is available. If negative, waits
     *           without timeout until next message is available.
     * eh:       error handling function, may be NULL
     * ehdata:   additional data that is given to error handling function.
     *
     * returns:   0 - if next message could be read from the sender
     *            1 - if sender is closed. The caller is expected to release the sender.
     *                Subsequent calls will always result this return code again.
     *            2 - if sender was aborted. Subsequent calls may function again. Exact
     *                context depends on implementation and use case. Normally the caller is
     *                expected to abort its operation.
     *            3 - if next message is not available
     *            4 - if reader could not handle the message because the new message
     *                is larger than the reader's memory limit.
     *            5 - if reader has growable memory without limit, but cannot allocate more 
     *                memory because overall memory is exhausted.
     *
     * All other error codes are implementation specific.
     */
    int (*nextMessageFromSender)(sender_object* s, sender_reader* r, 
                                 int nonblock, double timeout,
                                 sender_error_handler eh, void* ehdata);

};


#if SENDER_CAPI_IMPLEMENT_SET_CAPI
/**
 * Sets the Sender C API into the metatable at the given index.
 * 
 * index: index of the table that is be used as metatable for objects 
 *        that are associated to the given capi.
 */
static int sender_set_capi(lua_State* L, int index, const sender_capi* capi)
{
    lua_pushlstring(L, SENDER_CAPI_ID_STRING, strlen(SENDER_CAPI_ID_STRING));           /* -> key */
    void** udata = (void**) lua_newuserdata(L, sizeof(void*) + strlen(SENDER_CAPI_ID_STRING) + 1); /* -> key, value */
    *udata = (void*)capi;
    strcpy((char*)(udata + 1), SENDER_CAPI_ID_STRING);  /* -> key, value */
    lua_rawset(L, (index < 0) ? (index - 2) : index);     /* -> */
    return 0;
}
#endif /* SENDER_CAPI_IMPLEMENT_SET_CAPI */

#if SENDER_CAPI_IMPLEMENT_GET_CAPI
/**
 * Gives the associated Sender C API for the object at the given stack index.
 * Returns NULL, if the object at the given stack index does not have an 
 * associated Sender C API or only has a Sender C API with incompatible version
 * number. If errorReason is not NULL it receives the error reason in this case:
 * 1 for incompatible version nummber and 2 for no associated C API at all.
 */
static const sender_capi* sender_get_capi(lua_State* L, int index, int* errorReason)
{
    if (luaL_getmetafield(L, index, SENDER_CAPI_ID_STRING) != LUA_TNIL)      /* -> _capi */
    {
        const void** udata = (const void**) lua_touserdata(L, -1);           /* -> _capi */

        if (   udata
            && (lua_rawlen(L, -1) >= sizeof(void*) + strlen(SENDER_CAPI_ID_STRING) + 1)
            && (memcmp((char*)(udata + 1), SENDER_CAPI_ID_STRING, 
                       strlen(SENDER_CAPI_ID_STRING) + 1) == 0))
        {
            const sender_capi* capi = (const sender_capi*) *udata;             /* -> _capi */
            while (capi) {
                if (   capi->version_major == SENDER_CAPI_VERSION_MAJOR
                    && capi->version_minor >= SENDER_CAPI_VERSION_MINOR)
                {                                                              /* -> _capi */
                    lua_pop(L, 1);                                             /* -> */
                    return capi;
                }
                capi = (const sender_capi*) capi->next_capi;
            }
            if (errorReason) {
                *errorReason = 1;
            }
        } else {                                                               /* -> _capi */
            if (errorReason) {
                *errorReason = 2;
            }
        }                                                                      /* -> _capi */
        lua_pop(L, 1);                                                         /* -> */
    } else {                                                                   /* -> */
        if (errorReason) {
            *errorReason = 2;
        }
    }                                                                          /* -> */
    return NULL;
}
#endif /* SENDER_CAPI_IMPLEMENT_GET_CAPI */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SENDER_CAPI_H */
