#include "threadpool.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>

threadpool *create_threadpool(int num_threads_in_pool)
{
    if (num_threads_in_pool <= 0 || num_threads_in_pool > MAXT_IN_POOL)
    {
        printf("Usage: threadpool <pool-size> <max-number-of-jobs>\n");//???????????????????????????????????????
        return NULL;
    }
    threadpool *thread_pool = (threadpool *)malloc(sizeof(threadpool));
    if (thread_pool == NULL)
    {
        perror("Malloc Failed\n");
        return NULL;
    }
    //init values:
    thread_pool->num_threads = num_threads_in_pool;
    thread_pool->qsize = 0;
    thread_pool->qhead = NULL;
    thread_pool->qtail = NULL;
    thread_pool->dont_accept = 0;
    thread_pool->shutdown = 0;

    //init condtion vars:
    pthread_cond_init(&(thread_pool->q_not_empty), NULL);
    pthread_cond_init(&(thread_pool->q_empty), NULL);

    //init mutex:
    pthread_mutex_init(&(thread_pool->qlock), NULL);

    //creating the threads and insert into threads array
    //pthread_t *threads = thread_pool->threads;//??????????????????????????????????????????????
    thread_pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * num_threads_in_pool);
    if (thread_pool->threads == NULL)
    {
        free(thread_pool);
        perror("Malloc Failed\n");
        return NULL;
    }
    for (int i = 0; i < num_threads_in_pool; i++)
    {
        int err = pthread_create(&thread_pool->threads[i], NULL, do_work, thread_pool);
        if (err != 0)
        {
            free(thread_pool);
            free(thread_pool->threads);
            perror("Pthread create Failed");
            return NULL;
        }
    }

    return thread_pool;
}

void dispatch(threadpool *from_me, dispatch_fn dispatch_to_here, void *arg)
{
    //check input validation
    if (from_me == NULL)
    {
        printf("Usage: threadpool <pool-size> <max-number-of-jobs>\n");
        return;
    }

    work_t *new_work = (work_t *)malloc(sizeof(work_t));
    if (new_work == NULL)
    {
        perror("Malloc Failed\n");
        exit(EXIT_FAILURE);
    }
    //init new work to add to the queue:
    new_work->routine = dispatch_to_here;
    new_work->arg = arg;
    new_work->next = NULL;

    //locking mutex:
    pthread_mutex_lock(&(from_me->qlock));

    //check if we can accept new job:
    if (from_me->dont_accept == 1)
    {
        pthread_mutex_unlock(&(from_me->qlock));
        free(new_work);
        return;
    }

    //adding the work_t element to the queue:
    work_t *p = from_me->qtail;
    if (from_me->qsize == 0 || p == NULL) //
    {                                     //empty list
        from_me->qhead = new_work;
        from_me->qtail = new_work;
    }
    else
    {
        p->next = new_work;
        from_me->qtail = new_work;
    }
    from_me->qsize += 1;

    //unlocking mutex:
    pthread_mutex_unlock(&(from_me->qlock));

    pthread_cond_signal(&(from_me->q_not_empty));
}

void *do_work(void *p)
{
    //check input validation
    if (p == NULL)
    {
        printf("Usage: threadpool <pool-size> <max-number-of-jobs>\n");
        return NULL;
    }
    threadpool *from_me = (threadpool *)p;

    while (1)
    {
        //locking mutex:
        pthread_mutex_lock(&(from_me->qlock));

        if (from_me->shutdown == 1)
        {
            pthread_mutex_unlock(&(from_me->qlock));
            return NULL;
        }

        if (from_me->qsize == 0) //wainting for job
            pthread_cond_wait(&(from_me->q_not_empty), &(from_me->qlock));

        if (from_me->shutdown == 1)
        { //checking if woke up to commit sucied
            pthread_mutex_unlock(&(from_me->qlock));
            return NULL;
        }

        //queue not empty, takeing the first element:
        work_t *work = from_me->qhead;
        if(work == NULL)
        {//make sure it isn't false wake
            pthread_mutex_unlock(&(from_me->qlock));
            continue;
        }
        from_me->qhead = from_me->qhead->next;
        if (from_me->qhead == NULL)
            from_me->qtail = NULL;

        from_me->qsize -= 1;

        if (from_me->qsize == 0 && from_me->dont_accept == 1)
        { //notifay that the queue empty and ready to be destroyed
            pthread_cond_signal(&(from_me->q_empty));
        }

        //unlocking mutex:
        pthread_mutex_unlock(&(from_me->qlock));
        
        //call the thread routine:
        work->routine((void *)work->arg);
       
        free(work);
    }
}

void destroy_threadpool(threadpool *destroyme)
{
    if (destroyme == NULL)
    {
        printf("Usage: threadpool <pool-size> <max-number-of-jobs>\n");
        return;
    }

    //locking mutex:
    pthread_mutex_lock(&(destroyme->qlock));

    destroyme->dont_accept = 1;
    if (destroyme->qsize > 0) //validate the queue is empty
        pthread_cond_wait(&(destroyme->q_empty), &(destroyme->qlock));

    destroyme->shutdown = 1;

    //unlocking mutex:
    pthread_mutex_unlock(&(destroyme->qlock));

    //wake everybody! time to die
    pthread_cond_broadcast(&(destroyme->q_not_empty));
    for (int i = 0; i < destroyme->num_threads; i++){
        pthread_join(destroyme->threads[i], NULL);
    }
    free(destroyme->threads);
    free(destroyme);
}