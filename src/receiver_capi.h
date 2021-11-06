#ifndef RECEIVER_CAPI_H
#define RECEIVER_CAPI_H

#define RECEIVER_CAPI_VERSION_MAJOR 0
#define RECEIVER_CAPI_VERSION_MINOR 1
#define RECEIVER_CAPI_VERSION_PATCH 0

typedef struct receiver_object receiver_object;
typedef struct receiver_writer receiver_writer;
typedef struct receiver_capi   receiver_capi;

#ifndef RECEIVER_CAPI_IMPLEMENT_GET_CAPI
#  define RECEIVER_CAPI_IMPLEMENT_GET_CAPI 0
#endif

/**
 * Type for pointer to function that may be called if an error occurs.
 * ehdata: void pointer that is given in add/setMsgToReceiver method (see below)
 * msg:    detailed error message
 * msglen: length of error message
 */
typedef void (*receiver_error_handler)(void* ehdata, const char* msg, size_t msglen);


/**
 *  Receiver C API.
 */
struct receiver_capi
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
     * index is a valid receiver object, otherwise must return NULL,
     */
    receiver_object* (*toReceiver)(lua_State* L, int index);

    /**
     * Increase the reference counter of the receiver object.
     * Must be thread safe.
     */
    void (*retainReceiver)(receiver_object* b);

    /**
     * Decrease the reference counter of the receiver object and
     * destructs the receiver object if no reference is left.
     * Must be thread safe.
     */
    void (*releaseReceiver)(receiver_object* b);

    /**
     * Creates new writer object.
     * Does not need to be thread safe.
     */
    receiver_writer* (*newWriter)(size_t initialCapacity, float growFactor);

    /**
     * Destructs writer object.
     * Does not need to be thread safe.
     */
    void (*freeWriter)(receiver_writer* w);

    /**
     * Does not need to be thread safe.
     */
    void (*clearWriter)(receiver_writer* w);

    /**
     * Does not need to be thread safe.
     */
    int  (*addBooleanToWriter)(receiver_writer* w, int b);

    /**
     * Does not need to be thread safe.
     */
    int  (*addIntegerToWriter)(receiver_writer* w, lua_Integer i);

    /**
     * Does not need to be thread safe.
     */
    int  (*addStringToWriter)(receiver_writer* w, const char* s, size_t len);

    /**
     * Must be thread safe.
     *
     * eh:     error handling function, may be NULL
     * ehdata: additional data that is given to error handling function.
     */
    int (*addMsgToReceiver)(receiver_object* b, receiver_writer* w, receiver_error_handler eh, void* ehdata);

    /**
     * Must be thread safe.
     *
     * eh:     error handling function, may be NULL
     * ehdata: additional data that is given to error handling function.
     */
    int (*setMsgToReceiver)(receiver_object* b, receiver_writer* w, receiver_error_handler eh, void* ehdata);
    
};


#if RECEIVER_CAPI_IMPLEMENT_GET_CAPI
/**
 * Gives the associated Receiver C API for the object at the given stack index.
 */
static const receiver_capi* receiver_get_capi(lua_State* L, int index, int* versionError)
{
    if (luaL_getmetafield(L, index, "_capi_receiver") != LUA_TNIL) { /* -> _capi */
        const receiver_capi* capi = lua_touserdata(L, -1);           /* -> _capi */
        while (capi) {
            if (   capi->version_major == RECEIVER_CAPI_VERSION_MAJOR
                && capi->version_minor >= RECEIVER_CAPI_VERSION_MINOR)
            {                                                        /* -> _capi */
                lua_pop(L, 1);                                       /* -> */
                return capi;
            }
            capi = capi->next_capi;
        }
        if (versionError) {
            *versionError = 1;
        }                                                            /* -> _capi */
        lua_pop(L, 1);                                               /* -> */
    }                                                                /* -> */
    return NULL;
}
#endif /* RECEIVER_CAPI_IMPLEMENT_GET_CAPI */

#endif /* RECEIVER_CAPI_H */
