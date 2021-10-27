#ifndef MTMSG_CAPI_H
#define MTMSG_CAPI_H

#define MTMSG_CAPI_VERSION_MAJOR 0
#define MTMSG_CAPI_VERSION_MINOR 1
#define MTMSG_CAPI_VERSION_PATCH 0

typedef struct mtmsg_buffer mtmsg_buffer;
typedef struct mtmsg_writer mtmsg_writer;
typedef struct mtmsg_capi   mtmsg_capi;

/**
 *  Mtmsg C API.
 */
struct mtmsg_capi
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
     * index is a valid mtmsg buffer, otherwise must return NULL,
     */
    mtmsg_buffer* (*toBuffer)(lua_State* L, int index);

    /**
     * Increase the reference counter of the buffer object.
     * Must be thread safe.
     */
    void (*retainBuffer)(mtmsg_buffer* b);

    /**
     * Decrease the reference counter of the buffer object and
     * destructs the buffer object if no reference is left.
     * Must be thread safe.
     */
    void (*releaseBuffer)(mtmsg_buffer* b);

    /**
     * Creates new writer object.
     * Does not need to be thread safe.
     */
    mtmsg_writer* (*newWriter)(size_t initialCapacity, float growFactor);

    /**
     * Destructs writer object.
     * Does not need to be thread safe.
     */
    void (*freeWriter)(mtmsg_writer* w);

    /**
     * Does not need to be thread safe.
     */
    void (*clearWriter)(mtmsg_writer* w);

    /**
     * Does not need to be thread safe.
     */
    int  (*addBooleanToWriter)(mtmsg_writer* w, int b);

    /**
     * Does not need to be thread safe.
     */
    int  (*addIntegerToWriter)(mtmsg_writer* w, lua_Integer i);

    /**
     * Does not need to be thread safe.
     */
    int  (*addStringToWriter)(mtmsg_writer* w, const char* s, size_t len);

    /**
     * Must be thread safe.
     */
    int (*addMsgToBuffer)(mtmsg_buffer* b, mtmsg_writer* w);

    /**
     * Must be thread safe.
     */
    int (*setMsgToBuffer)(mtmsg_buffer* b, mtmsg_writer* w);
    
};

/**
 * Gives the associated Mtmsg C API for the object at the given stack index.
 */
static const mtmsg_capi* mtmsg_get_capi(lua_State* L, int index, int* versionError)
{
    if (luaL_getmetafield(L, index, "_capi_mtmsg") != LUA_TNIL) {  /* -> _capi */
        const mtmsg_capi* capi = lua_touserdata(L, -1);
        while (capi) {
            if (   capi->version_major == MTMSG_CAPI_VERSION_MAJOR
                && capi->version_minor >= MTMSG_CAPI_VERSION_MINOR)
            {                                                      /* -> _capi */
                lua_pop(L, 1);                                     /* -> */
                return capi;
            }
            capi = capi->next_capi;
        }
        if (versionError) {
            *versionError = 1;
        }
    }                                                              /* -> _capi */
    lua_pop(L, 1);                                                 /* -> */
    return NULL;
}

#endif /* MTMSG_CAPI_H */
