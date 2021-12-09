#include "threadpool.c"

#include <stdio.h>
#include <stdlib.h> //????
#include <unistd.h>
#include <string.h> //????
#include <stdbool.h>
#include <pthread.h>

int f(void *a)
{
    int index = *((int *)a);
    for (int i = 0; i < 3; i++)
    {
        printf("%d\n", index);
        sleep(1);
    }
    return index;
}

//int argc, char *argv[]
int main()
{
    threadpool *t = create_threadpool(100);
    int arr[] = {0, 1, 2, 3, 4};
    for (int i = 0; i < 5; i++){
        dispatch(t, f, (void *)&(arr[i]));
    }

    destroy_threadpool(t);

    return 0;
}