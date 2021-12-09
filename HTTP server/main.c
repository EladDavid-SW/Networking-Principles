#include "threadpool.c"
#include <stdio.h>
#include <stdlib.h>


/*TEST  FUNCTIONS*/
int f1(void* f)
{
    int* p = (int*)f;
    printf("Number you chose is:\t%d\n",*p);
    //printf("\n1\n");
    return 0;
}

int f2(void* f)
{
    int* p = (int*)f;
    printf("Number you chose:\t%d\n Squared value is:\t%d\n",(*p),(*p)*(*p));
    //printf("\n2\n");
    return 0;
}

int f3(void* f)
{
    //printf("\n3\n");

    int* p = (int*)f;
    int q = (*p - 1)+(*p + 1);
    q/=2;
    printf("Number you chose (as average of 2 numbers) is:\t%d\n",(q));
    return 0;
}

/*    MAIN      */
int main()
{

    //create threadpool
    threadpool *toCheck = create_threadpool(100);
    //create argument for f1
    int* intCheck1 = (int*)calloc(1,sizeof(int));
    *intCheck1=5;
    
    //add to job pool
    dispatch(toCheck,f1,(void*)intCheck1);
    dispatch(toCheck,f2,(void*)intCheck1);
    dispatch(toCheck,f3,(void*)intCheck1);


    //destroy
    destroy_threadpool(toCheck);
    free(intCheck1);
    return 0;
}