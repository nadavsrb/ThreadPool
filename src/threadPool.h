#ifndef __THREAD_POOL__
#define __THREAD_POOL__

#include "osqueue.h"

typedef struct thread_pool
{
 //The field x is here because a struct without fields
 //doesn't compile. Remove it once you add fields of your own
 int x;
 //TODO - FILL THIS WITH YOUR FIELDS
}ThreadPool;

ThreadPool* tpCreate(int numOfThreads);

void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks);

int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param);

#endif
