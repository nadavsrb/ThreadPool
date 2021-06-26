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

/**
 * @brief Boolean var.
 */
typedef enum{
    FALSE,
    TRUE
}BOOL;

/**
 * @brief Task for saving the funcs and their param
 * inside the queue
 */
typedef struct{
    void (*computeFunc) (void *);
    void* param;
}Task;

/**
 * @brief this is the Thread Pool struct.
 */
typedef struct thread_pool
{
    OSQueue* tasksQueue;
    pthread_t* threadsIds;
    pthread_mutex_t condMutex;
    pthread_cond_t cond;
    int numOfThreads;
    BOOL isDestroyed;
    BOOL isStopped;
}ThreadPool;

/**
 * @brief Creates a Thread Pool object.
 * 
 * @param numOfThreads 
 * @return ThreadPool* returns a pointer to the ThreadPool,
 * if failed returns NULL.
 */
ThreadPool* tpCreate(int numOfThreads);

/**
 * @brief Destroys the Thread Pool object.
 * 
 * @param threadPool pointer to threadpool to destroy.
 * @param shouldWaitForTasks if 0 not waiting for tasks and as soon
 * as the tasks that are running would stop will destroy the thread pool,
 * if no zero, would wait for the tasks in the queue to finish first.
 */
void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks);

/**
 * @brief Inserts a Task to the ThreadPool.
 * 
 * @param threadPool the thread pool to insert to.
 * @param computeFunc the func to do.
 * @param param the func param.
 * @return int 0 on seccess, else -1 is returnned.
 */
int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param);

#endif
