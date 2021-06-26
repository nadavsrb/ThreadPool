#include "threadPool.h"

/**
 * @brief Creates a Task object.
 * 
 * @return Task* - pointer to the created task, if failed NULL.
 */
Task* createTask(){
    Task* task = (Task*) malloc(sizeof(Task));

    if(task == NULL){
        perror("Error: ThreedPool: createTask: malloc: failed create new task");
    }

    // if malloc fails it will return null
    return task;
}

/**
 * @brief Destroys a Task object. 
 * 
 * @param task task to destroy.
 */
void destroyTask(Task* task){
    if(task == NULL){
        return;
    }

    free(task);
}
/**
 * @brief This func frees the Thread Pool,
 * should be called only if error occurred.
 * 
 * @param threadPool the thread pool to free.
 */
void freeThreadPoolOnError(ThreadPool* threadPool){
    if(threadPool == NULL){
        return;
    }

    // we are trying to lock the mutex
    pthread_mutex_lock(&threadPool->condMutex);

    if(threadPool->isDestroyed){
        pthread_mutex_unlock(&threadPool->condMutex);
        return;
    }

    // kills all the threads in thread pool accept this one.
    int index;
    int myId = pthread_self();
    for(index = 0; index < threadPool->numOfThreads; ++index){
        if(threadPool->threadsIds[index] == myId){
            continue;
        }

        pthread_cancel(threadPool->threadsIds[index]);
    }
    
    //clears the queue.
    while(!osIsQueueEmpty(threadPool->tasksQueue)){
        Task* t = (Task*) osDequeue(threadPool->tasksQueue);
        destroyTask(t);
    }

    // tells the other funcs that the thread pool was destroyed.
    threadPool->isDestroyed = TRUE;

    // we are trying to unlock the mutex
    pthread_mutex_unlock(&threadPool->condMutex);

    //frees allocated memory:
    pthread_mutex_destroy(&threadPool->condMutex);
    pthread_cond_destroy(&threadPool->cond);
    osDestroyQueue(threadPool->tasksQueue);
    free(threadPool->threadsIds);
    free(threadPool);
}

/**
 * @brief handels a mutex or cond error.
 * 
 * @param threadPool the thread pool to handle.
 */
void handleError(ThreadPool* threadPool){
    // we are trying to lock the mutex
    pthread_mutex_lock(&threadPool->condMutex);

    //if allready destroyed do nothing.
    if(threadPool->isDestroyed){
        pthread_mutex_unlock(&threadPool->condMutex);
        return;
    }
    pthread_mutex_unlock(&threadPool->condMutex);

    perror("Error: ThreadPool: got an internal error inside (mutex or condition)");

    // frees the Thread Pool memory. 
    freeThreadPoolOnError(threadPool);
}

/**
 * @brief This is the work of single thread
 * in the thread pool.
 * 
 * @param threadPoolVoid this is a void pointer to the threadPool.
 * @return void* 0 on success, -1 if failed.
 */
void* doTasks(void *threadPoolVoid)
{
    // getting the thread pool
    ThreadPool* threadPool = (ThreadPool*) threadPoolVoid;

    // the loop of the thread.
    for(;;){
        // creates a pointer to a task element.
        Task* task = NULL;

        // we will use threadPools' args so we need to lock the mutex.
        if(pthread_mutex_lock(&threadPool->condMutex) != 0) {
                handleError(threadPool);
                pthread_exit(THREAD_ERROR_VAL);
        }

        // if not stopped and no tasks
        if((!threadPool->isStopped) && osIsQueueEmpty(threadPool->tasksQueue)){

            // waits to the next task 
            if(pthread_cond_wait(&threadPool->cond,&threadPool->condMutex) != 0) {
                handleError(threadPool);
                pthread_exit(THREAD_ERROR_VAL);
            }  
        }

        // if stopped break the loop.
        if(threadPool->isStopped){
            // unlocks the mutex.
            if(pthread_mutex_unlock(&threadPool->condMutex) != 0) {
                handleError(threadPool);
                pthread_exit(THREAD_ERROR_VAL);
            }

            break;
        }

        // else, gets the next task.
        task = osDequeue(threadPool->tasksQueue);

        if(pthread_mutex_unlock(&threadPool->condMutex) != 0) {
           handleError(threadPool);
            pthread_exit(THREAD_ERROR_VAL);
        }

        // if can handle task, will handle the task.
        if(task != NULL && task->computeFunc != NULL){
            task->computeFunc(task->param);
        }
        
        //destroys the task.
        destroyTask(task);
    }

    return THREAD_SUCCESS_VAL;
}

/**
 * @brief this func is for passing it to the tasks queue
 * for destronig the queue.
 * 
 * @param threadPoolVoid this is a void pointer to the threadPool.
 */
void stopDoTasks(void* threadPoolVoid){
    // gets the thread pool
    ThreadPool* threadPool = (ThreadPool*) threadPoolVoid;

    // we will use threadPools' args so we need to lock the mutex.
    if(pthread_mutex_lock(&threadPool->condMutex) != 0) {
        handleError(threadPool);
        pthread_exit(THREAD_ERROR_VAL);
        return;
    }

    // tells the threads to stop.
    threadPool->isStopped = TRUE;
    
    // wakes all the waiting threads in the condition.
    if(pthread_cond_broadcast(&threadPool->cond) != 0) {
        pthread_mutex_unlock(&threadPool->condMutex);

        handleError(threadPool);
        pthread_exit(THREAD_ERROR_VAL);
    }

    if(pthread_mutex_unlock(&threadPool->condMutex) != 0) {
        handleError(threadPool);
        pthread_exit(THREAD_ERROR_VAL);
        return;
    }    
}

/**
 * @brief Creates a Thread Pool object.
 * 
 * @param numOfThreads 
 * @return ThreadPool* returns a pointer to the ThreadPool,
 * if failed returns NULL.
 */
ThreadPool* tpCreate(int numOfThreads){
    // creating the ThreadPool struct.
    ThreadPool* threadPool = (ThreadPool*) malloc(sizeof(ThreadPool));
    if(threadPool == NULL){
        perror("Error: tpCreate: malloc: couldn't allocate memory to threadPool");
        return NULL;
    }

    // creating the array of the threads ids.
    threadPool->threadsIds = (pthread_t*) malloc(numOfThreads * sizeof(pthread_t));
    if(threadPool->threadsIds == NULL){
        perror("Error: tpCreate: malloc: couldn't allocate memory to threadPool->threadsIds");
        
        // frees allocated data.
        free(threadPool);

        return NULL;
    }

    // creating the tasks queue.
    threadPool->tasksQueue = osCreateQueue();
    if(threadPool->tasksQueue == NULL){
        perror("Error: tpCreate: osCreateQueue: osCreateQueue failed");

        // frees allocated data.
        free(threadPool->threadsIds);
        free(threadPool);

        return NULL;
    }

    // creating the cond for the thread pool.
    if(pthread_cond_init(&threadPool->cond, NULL) != 0){
        perror("Error: tpCreate: pthread_cond_init: failed creating cond");

        // frees allocated data.
        osDestroyQueue(threadPool->tasksQueue);
        free(threadPool->threadsIds);
        free(threadPool);

        return NULL;
    }

    // creating the mutex for the cond.
    if(pthread_mutex_init(&threadPool->condMutex, NULL) != 0){
        perror("Error: tpCreate: pthread_mutex_init: failed creating condMutex");

        // frees allocated data.
        pthread_cond_destroy(&threadPool->cond);
        osDestroyQueue(threadPool->tasksQueue);
        free(threadPool->threadsIds);
        free(threadPool);

        return NULL;
    }
    
    // initiallizing another fields of the thread pool.
    threadPool->isDestroyed = FALSE;
    threadPool->isStopped = FALSE;
    threadPool->numOfThreads = numOfThreads;

    // we will use threadPools' args (and threads) so we need to lock the mutex.
    if(pthread_mutex_lock(&threadPool->condMutex) != 0) {
         handleError(threadPool);
         return NULL;
    }

    // creates the threads in the thread pool.
    int index;
    for(index = 0; index < threadPool->numOfThreads; ++index){
        if(pthread_create(&threadPool->threadsIds[index],
         NULL, doTasks, (void *) threadPool) != 0){
             perror("Error: tpCreate: pthread_create: failed creating a thread");

             // frees allocated data.
             --index;
             while(index >= 0){
                pthread_cancel(threadPool->threadsIds[index]);
                 --index;
             }
             
             // now no threads we unlocks the mutex.
             pthread_mutex_unlock(&threadPool->condMutex);

             pthread_mutex_destroy(&threadPool->condMutex);
             pthread_cond_destroy(&threadPool->cond);
             osDestroyQueue(threadPool->tasksQueue);
             free(threadPool->threadsIds);
             free(threadPool);

             return NULL;
        }
    }

    if(pthread_mutex_unlock(&threadPool->condMutex) != 0) {
        handleError(threadPool);
        return NULL;
    }  

    return threadPool;
}

/**
 * @brief Destroys the Thread Pool object.
 * 
 * @param threadPool pointer to threadpool to destroy.
 * @param shouldWaitForTasks if 0 not waiting for tasks and as soon
 * as the tasks that are running would stop will destroy the thread pool,
 * if no zero, would wait for the tasks in the queue to finish first.
 */
void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks){
    // check if threadPool is valid or accessible.
    if(threadPool == NULL || threadPool->isDestroyed){
        return;
    }

    if(pthread_mutex_lock(&threadPool->condMutex) != 0) {
        handleError(threadPool);
        return;
    }

    // we checks that the isDestroyed didn't change.
    if(threadPool->isDestroyed){
        // try to unlock lock
        pthread_mutex_unlock(&threadPool->condMutex);

        return;
    }

    if(shouldWaitForTasks == 0){
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

    if(pthread_mutex_lock(&threadPool->condMutex) != 0) {
        handleError(threadPool);
        return;
    }

    // we checks that the isDestroyed didn't change.
    if(threadPool->isDestroyed){
        // try to unlock lock
        pthread_mutex_unlock(&threadPool->condMutex);

        return;
    }

    threadPool->isDestroyed = TRUE;

    if(pthread_mutex_unlock(&threadPool->condMutex) != 0) {
        handleError(threadPool);
        return;
    }

    // no need mutex because threadPool->numOfThreads, threadPool->threadsIds
    // doesn't change from their creation and threadPool->isDestroyed = TRUE
    // so no one would destroy them.

    int index;
    BOOL isErrorOccurred = FALSE;
    int myId = pthread_self();
    for(index = 0; index < threadPool->numOfThreads; ++index){
        if(threadPool->threadsIds[index] == myId){
            continue;
        }

        if(isErrorOccurred){
            pthread_cancel(threadPool->threadsIds[index]);
        }

        if(pthread_join(threadPool->threadsIds[index], NULL) != 0) {
            perror("Error: tpDestroy: failed to join a thread");
            isErrorOccurred = TRUE;
        }
    }

    // all threads in Thread Pool are dead
    // and no one else will touch it because 
    // threadPool->isDestroyed = TRUE
    // so no mutex is needed

    //clears the queue.
    while(!osIsQueueEmpty(threadPool->tasksQueue)){
        Task* t = (Task*) osDequeue(threadPool->tasksQueue);
        destroyTask(t);
    }

    //frees allocated memory:
    pthread_mutex_destroy(&threadPool->condMutex);
    pthread_cond_destroy(&threadPool->cond);
    osDestroyQueue(threadPool->tasksQueue);
    free(threadPool->threadsIds);
    free(threadPool);
}

/**
 * @brief Inserts a Task to the ThreadPool.
 * 
 * @param threadPool the thread pool to insert to.
 * @param computeFunc the func to do.
 * @param param the func param.
 * @return int 0 on seccess, else -1 is returnned.
 */
int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param){
    // check if threadPool is valid or accessible.
    if(threadPool == NULL || threadPool->isDestroyed){
        return ERROR;
    }

    // we will use threadPools' args so we need to lock the mutex.
    if(pthread_mutex_lock(&threadPool->condMutex) != 0) {
        handleError(threadPool);
        return ERROR;
    }

    // we checks that the isDestroyed didn't change.
    if(threadPool->isDestroyed){
        // try to unlock lock
        pthread_mutex_unlock(&threadPool->condMutex);

        return ERROR;
    }

    if(computeFunc != NULL){ // if false no need to add task.
        //creates the task to add.
        Task* t = createTask();
        if(t == NULL){
            perror("Error: ThreedPool: createTask: malloc: failed create new task");

            pthread_mutex_unlock(&threadPool->condMutex); //try to unlock the lock.
            
            freeThreadPoolOnError(threadPool);
            return ERROR;
        }

        // initializing the task.
        t->computeFunc =  computeFunc;
        t->param = param;
        
        // adding the task to the queue
        osEnqueue(threadPool->tasksQueue, (void*) t);
        
        // semding a signal to tell that the queue got a new task.
        if(pthread_cond_signal(&threadPool->cond) != 0) {
            pthread_mutex_unlock(&threadPool->condMutex); //try to unlock the lock.

            handleError(threadPool);
            return ERROR;
        }
    }

    if(pthread_mutex_unlock(&threadPool->condMutex) != 0) {
        handleError(threadPool);
        return ERROR;
    }

    return SUCCESS;
}