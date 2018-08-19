#include "async_util.h" /* must be included first, because it might define _XOPEN_SOURCE */

#include <stdlib.h>
#include <stdio.h>

bool async_util_abort(int rc, int line)
{
    fprintf(stderr, 
            "*** unexpected error in thread synchronization (rc=%d, line=%d) ***\n", 
            rc, 
            line);
    abort();
    return false;
}

void mtmsg_async_lock_init(Lock* lock)
{
#if defined(MTMSG_ASYNC_USE_PTHREAD)

    int rc = pthread_mutexattr_init(&lock->attr);
    if (rc != 0) { async_util_abort(rc, __LINE__); }

    rc = pthread_mutexattr_settype(&lock->attr, PTHREAD_MUTEX_RECURSIVE);
    if (rc != 0) { async_util_abort(rc, __LINE__); }

    rc = pthread_mutex_init(&lock->lock, &lock->attr);
    if (rc != 0) { async_util_abort(rc, __LINE__); }

#elif defined(MTMSG_ASYNC_USE_WINTHREAD)
    InitializeCriticalSection(&lock->lock);
#endif
}

void mtmsg_async_lock_destruct(Lock* lock)
{
#if defined(MTMSG_ASYNC_USE_PTHREAD)
    pthread_mutex_destroy(&lock->lock);
    pthread_mutexattr_destroy(&lock->attr);
#elif defined(MTMSG_ASYNC_USE_WINTHREAD)
    DeleteCriticalSection(&lock->lock);
#endif
}


void mtmsg_async_mutex_init(Mutex* mutex)
{
#if defined(MTMSG_ASYNC_USE_PTHREAD)

    int rc = pthread_mutexattr_init(&mutex->attr);
    if (rc != 0) { async_util_abort(rc, __LINE__); }

    rc = pthread_mutexattr_settype(&mutex->attr, PTHREAD_MUTEX_RECURSIVE);
    if (rc != 0) { async_util_abort(rc, __LINE__); }

    rc = pthread_mutex_init(&mutex->mutex, &mutex->attr);
    if (rc != 0) { async_util_abort(rc, __LINE__); }

    rc = pthread_cond_init(&mutex->condition, NULL);
    if (rc != 0) { async_util_abort(rc, __LINE__); }

#elif defined(MTMSG_ASYNC_USE_WINTHREAD)
    InitializeCriticalSection(&mutex->mutex);

    mutex->waitingCounter = 0;

    mutex->event = CreateEvent (NULL,  /*  no security */
                                FALSE, /* auto-reset event */
                                FALSE, /* non-signaled initially */
                                NULL); /* unnamed */

    if (mutex->event == NULL) { async_util_abort((int)mutex->event, __LINE__); }
#endif
}

void mtmsg_async_mutex_destruct(Mutex* mutex)
{
#if defined(MTMSG_ASYNC_USE_PTHREAD)
    pthread_cond_destroy(&mutex->condition);
    pthread_mutex_destroy(&mutex->mutex);
    pthread_mutexattr_destroy(&mutex->attr);
#elif defined(MTMSG_ASYNC_USE_WINTHREAD)
    CloseHandle(mutex->event);
    DeleteCriticalSection(&mutex->mutex);
#endif
}

void mtmsg_async_mutex_wait(Mutex* mutex) 
{
#if defined(MTMSG_ASYNC_USE_PTHREAD)

    int rc = pthread_cond_wait(&mutex->condition, &mutex->mutex);
    if (rc != 0) { async_util_abort(rc, __LINE__); }

#elif defined(MTMSG_ASYNC_USE_WINTHREAD)

    mutex->waitingCounter += 1;
    LeaveCriticalSection(&mutex->mutex);
    
    DWORD rc = WaitForSingleObject(mutex->event, INFINITE);
    
    EnterCriticalSection(&mutex->mutex);
    mutex->waitingCounter -= 1;
    
    if  (rc != WAIT_OBJECT_0) { async_util_abort(rc, __LINE__); }
#endif
}


bool mtmsg_async_mutex_wait_millis(Mutex* mutex, int timeoutMillis)
{
#if defined(MTMSG_ASYNC_USE_PTHREAD)
    struct timespec abstime;
    struct timeval tv;  gettimeofday(&tv, NULL);
    
    abstime.tv_sec = tv.tv_sec + timeoutMillis / 1000;
    abstime.tv_nsec = tv.tv_usec * 1000 + 1000 * 1000 * (timeoutMillis % 1000);
    abstime.tv_sec += abstime.tv_nsec / (1000 * 1000 * 1000);
    abstime.tv_nsec %= (1000 * 1000 * 1000);
    
    int rc = pthread_cond_timedwait(&mutex->condition, &mutex->mutex, &abstime);
    
    if (rc == 0 || rc == ETIMEDOUT) {
        return (rc == 0);
    }
    else {
        return async_util_abort(rc, __LINE__);
    }
#elif defined(MTMSG_ASYNC_USE_WINTHREAD)
    mutex->waitingCounter += 1;
    LeaveCriticalSection(&mutex->mutex);

    DWORD rc = WaitForSingleObject(mutex->event, timeoutMillis);

    EnterCriticalSection(&mutex->mutex);
    mutex->waitingCounter -= 1;

    if (rc == WAIT_OBJECT_0  || rc == WAIT_TIMEOUT) {
        return (rc == WAIT_OBJECT_0);
    } else {
        return async_util_abort(rc, __LINE__);
    }
#endif
}
