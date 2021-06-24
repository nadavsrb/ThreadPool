#ifndef __THREAD_POOL__
#define __THREAD_POOL__

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include "osqueue.h"

#define SUCCESS 0
#define ERROR -1
#define THREAD_ERROR_VAL ((void *) ERROR)
#define THREAD_SUCCESS_VAL ((void *) SUCCESS)

typedef enum{
    FALSE,
    TRUE
}BOOL;

typedef struct{
    void (*computeFunc) (void *);
    void* param;
}Task;

typedef struct thread_pool
{
    OSQueue* tasksQueue;
    pthread_mutex_t threadPoolVarsMutex;
    pthread_mutex_t condMutex;
    pthread_cond_t cond;
    BOOL isDestroyed;
    BOOL isStopped;
    BOOL isThreadErrorOccurred;
    int numOfThreads;
    pthread_t* threadsIds;
}ThreadPool;

ThreadPool* tpCreate(int numOfThreads);

void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks);

int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param);

#endif
