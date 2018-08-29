#ifndef MTMSG_ASYNC_DEFINES_H
#define MTMSG_ASYNC_DEFINES_H

/* -------------------------------------------------------------------------------------------- */

#if    defined(MTMSG_ASYNC_USE_WIN32) \
    && (   defined(MTMSG_ASYNC_USE_STDATOMIC) \
        || defined(MTMSG_ASYNC_USE_GNU))
  #error "MTMSG_ASYNC: Invalid compile flag combination"
#endif
#if    defined(MTMSG_ASYNC_USE_STDATOMIC) \
    && (   defined(MTMSG_ASYNC_USE_WIN32) \
        || defined(MTMSG_ASYNC_USE_GNU))
  #error "MTMSG_ASYNC: Invalid compile flag combination"
#endif
#if    defined(MTMSG_ASYNC_USE_GNU) \
    && (   defined(MTMSG_ASYNC_USE_WIN32) \
        || defined(MTMSG_ASYNC_USE_STDATOMIC))
  #error "MTMSG_ASYNC: Invalid compile flag combination"
#endif
 
/* -------------------------------------------------------------------------------------------- */

#if    !defined(MTMSG_ASYNC_USE_WIN32) \
    && !defined(MTMSG_ASYNC_USE_STDATOMIC) \
    && !defined(MTMSG_ASYNC_USE_GNU)

    #if defined(WIN32) || defined(_WIN32)
        #define MTMSG_ASYNC_USE_WIN32
    #elif defined(__GNUC__)
        #define MTMSG_ASYNC_USE_GNU
    #elif __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__)
        #define MTMSG_ASYNC_USE_STDATOMIC
    #else
        #error "MTMSG_ASYNC: unknown platform"
    #endif
#endif

/* -------------------------------------------------------------------------------------------- */

#if defined(__unix__) || defined(__unix) || (defined (__APPLE__) && defined (__MACH__))
    #include <unistd.h>
#endif

#if    !defined(MTMSG_ASYNC_USE_WINTHREAD) \
    && !defined(MTMSG_ASYNC_USE_PTHREAD) \
    && !defined(MTMSG_ASYNC_USE_STDTHREAD)
    
    #ifdef MTMSG_ASYNC_USE_WIN32
        #define MTMSG_ASYNC_USE_WINTHREAD
    #elif _XOPEN_VERSION >= 600
        #define MTMSG_ASYNC_USE_PTHREAD
    #elif __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_THREADS__)
        #define MTMSG_ASYNC_USE_STDTHREAD
    #else
        #define MTMSG_ASYNC_USE_PTHREAD
    #endif
#endif

/* -------------------------------------------------------------------------------------------- */

#if defined(MTMSG_ASYNC_USE_PTHREAD)
    #define _XOPEN_SOURCE 600 /* must be defined before any other include */
    #include <errno.h>
    #include <sys/time.h>
    #include <pthread.h>
#endif
#if defined(MTMSG_ASYNC_USE_WIN32) || defined(MTMSG_ASYNC_USE_WINTHREAD)
    #include <windows.h>
#endif
#if defined(MTMSG_ASYNC_USE_STDATOMIC)
    #include <stdint.h>
    #include <stdatomic.h>
#endif
#if defined(MTMSG_ASYNC_USE_STDTHREAD)
    #include <sys/time.h>
    #include <threads.h>
#endif

/* -------------------------------------------------------------------------------------------- */

#if __STDC_VERSION__ >= 199901L
    #include <stdbool.h>
#else
    #if !defined(__GNUC__) || defined(__STRICT_ANSI__)
        #define inline
    #endif 
    #define bool int
    #define true 1
    #define false 0
#endif

/* -------------------------------------------------------------------------------------------- */


#endif /* MTMSG_ASYNC_DEFINES_H */
