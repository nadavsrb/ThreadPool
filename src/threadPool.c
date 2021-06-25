#include "threadPool.h"

Task* createTask(){
    Task* task = (Task*) malloc(sizeof(Task));

    // if malloc fails it will return null
    return task;
}

void destroyTask(Task* task){
    free(task);
}

void handleError(ThreadPool* threadPool){
    // we can't use mutex because it maybe already failed
    // we wil try to not allow 2 thead errors handle
    // by this if it's not promised it will work
    // but it adds a little defend.
    if(threadPool->isThreadErrorOccurred){
        return; //we are already handeled a thread error.
    }
    threadPool->isThreadErrorOccurred = TRUE;

    perror("Error: ThreadPool: got an internal error inside (mutex or condition)");

    int index;
    int myId = pthread_self();
    for(index = 0; index < threadPool->numOfThreads; ++index){
        if(threadPool->threadsIds[index] == myId){
            continue;
        }

        pthread_cancel(threadPool->threadsIds[index]);
    }
}

void* doTasks(void *threadPoolVoid)
{
    ThreadPool* threadPool = (ThreadPool*) threadPoolVoid;
    for(;;){
        Task* task = NULL;
        if(pthread_mutex_lock(&threadPool->condMutex) != 0) {
                handleError(threadPool);
                pthread_exit(THREAD_ERROR_VAL);
        }

        if(!threadPool->isStopped && osIsQueueEmpty(threadPool->tasksQueue)){
            if(pthread_cond_wait(&threadPool->cond,&threadPool->condMutex) != 0) {
                handleError(threadPool);
                pthread_exit(THREAD_ERROR_VAL);
            }  
        }

        if(threadPool->isStopped){
            break;
        }

        task = osDequeue(threadPool->tasksQueue);

        if(pthread_mutex_unlock(&threadPool->condMutex) != 0) {
           handleError(threadPool);
            pthread_exit(THREAD_ERROR_VAL);
        }

        if(task != NULL){
            task->computeFunc(task->param);
            destroyTask(task);
        }
    }

    if(pthread_mutex_unlock(&threadPool->condMutex) != 0) {
        handleError(threadPool);
        pthread_exit(THREAD_ERROR_VAL);
    }

    return THREAD_SUCCESS_VAL;
}

void stopDoTasks(void* threadPoolVoid){
    ThreadPool* threadPool = (ThreadPool*) threadPoolVoid;
    if(pthread_mutex_lock(&threadPool->condMutex) != 0) {
        handleError(threadPool);
        return ERROR;
    }

    threadPool->isStopped = TRUE;

    if(pthread_mutex_unlock(&threadPool->condMutex) != 0) {
        handleError(threadPool);
        return ERROR;
    }
}

ThreadPool* tpCreate(int numOfThreads){
    ThreadPool* threadPool = (ThreadPool*) malloc(sizeof(ThreadPool));
    if(threadPool == NULL){
        perror("Error: tpCreate: malloc: couldn't allocate memory to threadPool");
        return NULL;
    }

    threadPool->threadsIds = (pthread_t*) malloc(numOfThreads * sizeof(pthread_t));
    if(threadPool->threadsIds == NULL){
        perror("Error: tpCreate: malloc: couldn't allocate memory to threadPool->threadsIds");
        free(threadPool);
        return NULL;
    }

    threadPool->tasksQueue = osCreateQueue();
    if(threadPool->tasksQueue = NULL){
        perror("Error: tpCreate: osCreateQueue: osCreateQueue failed");
        free(threadPool->threadsIds);
        free(threadPool);
        return NULL;
    }

    if(pthread_cond_init(&threadPool->cond, NULL) != 0){
        perror("Error: tpCreate: pthread_cond_init: failed creating cond");
        osDestroyQueue(threadPool->tasksQueue);
        free(threadPool->threadsIds);
        free(threadPool);
        return NULL;
    }

    if(pthread_mutex_init(&threadPool->condMutex, NULL) != 0){
        perror("Error: tpCreate: pthread_mutex_init: failed creating condMutex");
        pthread_cond_destroy(&threadPool->cond);
        osDestroyQueue(threadPool->tasksQueue);
        free(threadPool->threadsIds);
        free(threadPool);
        return NULL;
    }
    
    threadPool->isDestroyed = FALSE;
    threadPool->isStopped = FALSE;
    threadPool->isThreadErrorOccurred = FALSE;
    threadPool->numOfThreads = numOfThreads;
    
    int index;
    for(index = 0; index < threadPool->numOfThreads; ++index){
        if(pthread_create(&threadPool->tasksQueue[index],
         NULL, doTasks, (void *) threadPool) != 0){
             perror("Error: tpCreate: pthread_create: failed creating a thread");
             tpDestroy(threadPool, 0);
             return NULL;
        }
    }

    return threadPool;
}

void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks){
    if(threadPool == NULL){
        return;
    }
    
    threadPool->isDestroyed = TRUE;

    if(pthread_mutex_lock(&threadPool->condMutex) != 0) {
        handleError(threadPool);
        return;
    }

    if(!shouldWaitForTasks){
        while(!osIsQueueEmpty(threadPool->tasksQueue)){
            Task* t = (Task*) osDequeue(threadPool->tasksQueue);
            destroyTask(t);
        }
    }

    if(pthread_mutex_unlock(&threadPool->condMutex) != 0) {
        handleError(threadPool);
        return;
    }

    tpInsertTask(threadPool, stopDoTasks, (void*) threadPool);
}

int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param){
    if(threadPool == NULL || threadPool->isDestroyed){
        return ERROR;
    }

    if(computeFunc == NULL){ //No need to add task
        return SUCCESS;
    }

    Task* t = createTask();

    t->computeFunc =  computeFunc;
    t->param = param;

    if(pthread_mutex_lock(&threadPool->condMutex) != 0) {
        handleError(threadPool);
        return ERROR;
    }

    osEnqueue(threadPool->tasksQueue, (void*) t);

    if(pthread_mutex_unlock(&threadPool->condMutex) != 0) {
        handleError(threadPool);
        return ERROR;
    }

    if(pthread_cond_signal(&threadPool->cond) != 0) {
        handleError(threadPool);
        return ERROR;
    }

    return SUCCESS;
}