#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "GenericHashTable.h"

//functions decleration
Table *createTable(int size, int dType, int listLength);
void freeTable(Table *table);
int add(Table *table, void *data);
int removeObj(Table *table, void *data);
Object *search(Table *table, void *data);
void printTable(Table *table);
Object *createObject(void *data);
void freeObject(Object *obj, int type);
int isEqual(int type, void *data1, void *data2);
int intHashFun(int *key, int origSize);
int strHashFun(char *key, int origSize);
void MtoErr(char *str);
int findIndex(Table *table, void *data);
void removeNode(Object *prev, Object *toRemove, Table *table, int i);
void printIntTable(Table *table);
void printStrTable(Table *table);

void MtoErr(char *str)
{
    fprintf(stderr, "%s", str);
    fprintf(stderr, "%s", "\n");
}
int findIndex(Table *table, void *data)
{ //calculate and return the index by using hash func considering dType
    int dType = table->dType, D = table->D, M = table->M, index = -1;

    if (dType == INT_TYPE)
        index = D * intHashFun((int *)data, M);
    else if (dType == STR_TYPE)
        index = D * strHashFun((char *)data, M);
    if (index < 0) //check if something went wrong
        return -1;
}

Table *createTable(int size, int dType, int listLength)
{
    //check input validation
    if (size < 0 || (dType != INT_TYPE && dType != STR_TYPE) || listLength < 0)
    {
        MtoErr("invalid input");
        return NULL;
    }

    //alocate heap memory beacose I want to use the table after the func body end
    Table *T = (Table *)malloc(sizeof(Table));
    if (T == NULL)
    {
        MtoErr("Malloc failed");
        return NULL;
    }

    //initialize data members
    T->D = 1;
    T->dType = dType;
    T->t = listLength;
    T->M = size;
    T->N = size;
    T->arr = (Object **)malloc((T->M) * sizeof(Object *));
    if (T->arr == NULL)
    {
        MtoErr("Malloc failed");
        return NULL;
    }

    //initialize lists to null
    for (int i = 0; i < T->N; i++)
        T->arr[i] = NULL;

    return T;
}

void freeTable(Table *table)
{
    if (table == NULL)
    {
        MtoErr("Trying to free null pointer");
        return;
    }
    int dType = table->dType, N = table->N;
    Object **arr = table->arr;

    Object *p1;
    Object *toFree = NULL;

    for (int i = 0; i < N; i++)
    {
        p1 = arr[i];

        while (p1 != NULL)
        {
            toFree = p1;
            p1 = p1->NextObj;
            freeObject(toFree, dType);
        }

        p1 = NULL;
        toFree = NULL;
    }
    free(arr);
    free(table);
}

int add(Table *table, void *data)
{
    //check input validation
    if (table == NULL || data == NULL)
    {
        MtoErr("Invalid input");
        return -1;
    }

    int index = -1, dType = table->dType, M = table->M, D = table->D;

    //creating an element and check for his index
    Object *toAdd;
    if (dType == STR_TYPE)
    { //allocate memory in case we got str case
        int n = strlen((char *)data);
        char *dStr = (char *)malloc(sizeof(char) * (n + 1));
        if (dStr == NULL)
        {
            MtoErr("Malloc failed");
            return -1;
        }
        strncpy(dStr, (char *)data, n + 1);
        toAdd = createObject((void *)dStr);
    }
    else if (dType == INT_TYPE)
    { //allocate memory in case we got int case
        int *dInt = (int *)malloc(sizeof(int));
        if (dInt == NULL)
        {
            MtoErr("Malloc failed");
            return -1;
        }
        *dInt = *(int *)data;
        toAdd = createObject((void *)dInt);
    }

    index = findIndex(table, data);
    if (index == -1)
    { //check if we found the right index
        MtoErr("Finding index failed");
        return -1;
    }

    //trying to find available place on the existing table (without expansion)
    int indexToReturn = index;
    for (; index < indexToReturn + D; index++)
    {
        Object *p = table->arr[index];
        if (p == NULL)
        { //in case of empty list
            table->arr[index] = toAdd;
            indexToReturn = index;
            return indexToReturn;
        }

        //in case list is not empty, count the elements
        int counter = 1; //p is already counted
        while (p->NextObj != NULL)
        {
            p = p->NextObj;
            counter++;
        }

        if (counter < table->t)
        {
            p->NextObj = toAdd;
            indexToReturn = index;
            return indexToReturn;
        }
    }

    //expantion case

    //update variables
    int N = table->N;
    D *= 2;
    N *= 2;
    table->D = D;
    table->N = N;
    indexToReturn *= 2;
    index = indexToReturn;

    //expain table
    Object **expanTable = (Object **)malloc(D * M * sizeof(Object *));
    if (expanTable == NULL)
    {
        MtoErr("Malloc failed");
        return -1;
    }
    for (int i = 0; i < table->N; i++)
    { //initialize table
        expanTable[i] = NULL;
    }

    //update data on the new table on the right place
    for (int i = 0; i < N / 2; i++)
        expanTable[i * 2] = table->arr[i];

    free(table->arr);
    table->arr = expanTable;

    //add the element after expantaion
    for (; index < indexToReturn + D; index++)
    {
        Object *p = table->arr[index];
        if (p == NULL)
        { //in case of empty list
            table->arr[index] = toAdd;
            indexToReturn = index;
            return indexToReturn;
        }

        //in case list not empty, count the elements
        int counter = 1;
        while (p->NextObj != NULL)
        {
            p = p->NextObj;
            counter++;
        }

        if (counter < table->t)
        {
            p->NextObj = toAdd;
            indexToReturn = index;
            return indexToReturn;
        }
    }

    //in case we got here somthing went wrong
    return -1;
}

int removeObj(Table *table, void *data)
{
    //check input validation
    if (table == NULL || data == NULL)
    {
        MtoErr("Invalid input");
        return -1;
    }

    int dType = table->dType, D = table->D, index = findIndex(table, data);
    if (index == -1)
    { //check if we found the right index
        MtoErr("Finding index failed");
        return -1;
    }

    int indexToReturn = index;
    //looking for Object to remove
    for (; index < indexToReturn + D; index++)
    {
        Object *p = table->arr[index];
        Object *prev = NULL;
        while (p != NULL)
        {
            if (isEqual(dType, p->data, data) == 0)
            { //we found first data
                removeNode(prev, p, table, index);
                indexToReturn = index;
                return indexToReturn;
            }

            prev = p;
            p = p->NextObj;
        }
    }

    //in case we got here somthing went wrong
    return -1;
}

void removeNode(Object *prev, Object *toRemove, Table *table, int i)
{ //remove toRemove object
    if (prev == NULL)
    {
        table->arr[i] = toRemove->NextObj;
        freeObject(toRemove, table->dType);

        return;
    }
    prev->NextObj = toRemove->NextObj;
    freeObject(toRemove, table->dType);
}

Object *search(Table *table, void *data)
{
    //check input validation
    if (table == NULL || data == NULL)
    {
        MtoErr("Invalid input");
        return NULL;
    }

    int dType = table->dType, D = table->D, index = findIndex(table, data);

    if (index == -1)
    { //check if we found the right index
        MtoErr("Finding index failed");
        return NULL;
    }

    int initIndex = index;
    for (; index < initIndex + D; index++)
    {
        Object *p = table->arr[index];
        while (p != NULL)
        {
            if (isEqual(dType, data, p->data) == 0)
                return p;

            p = p->NextObj;
        }
    }

    return NULL; //mutch not founded
}

void printTable(Table *table)
{
    //check input validation
    if (table == NULL)
    {
        MtoErr("Invalid input");
        return;
    }

    int type = table->dType;
    if (type == INT_TYPE)
        printIntTable(table);

    if (type == STR_TYPE)
        printStrTable(table);
}

void printIntTable(Table *table)
{
    Object **arr = table->arr;
    for (int i = 0; i < table->N; i++)
    {
        printf("[%d]\t", i);
        Object *p = arr[i];
        while (p != NULL)
        {
            printf("%d\t-->\t", *((int *)p->data));
            p = p->NextObj;
        }
        printf("\n");
    }
}

void printStrTable(Table *table)
{
    Object **arr = table->arr;
    for (int i = 0; i < table->N; i++)
    {
        printf("[%d]\t", i);
        Object *p = arr[i];
        while (p != NULL)
        {
            printf("%s\t-->\t", (char *)p->data);
            p = p->NextObj;
        }
        printf("\n");
    }
}

Object *createObject(void *data)
{ //creating new object and update variables
    if (data == NULL)
    {
        MtoErr("Invalid input");
        return NULL;
    }
    Object *newObj = (Object *)malloc(sizeof(Object));
    if (newObj == NULL)
    {
        MtoErr("malloc failed");
        return NULL;
    }
    newObj->data = data;
    newObj->NextObj = NULL;

    return newObj;
}

void freeObject(Object *obj, int type)
{ //free object
    //in case obj already has been free
    if (obj == NULL)
        return;

    free(obj->data);
    free(obj);
}

int isEqual(int type, void *data1, void *data2)
{ //return true if data are equal

    if (data1 == NULL || data2 == NULL || (type != INT_TYPE && type != STR_TYPE))
    {
        MtoErr("invalid input");
        return -1;
    }
    //int case
    if (type == INT_TYPE && *(int *)data1 == *(int *)data2)
        return 0;

    //string case
    if (type == STR_TYPE && strcmp((char *)data1, (char *)data2) == 0)
        return 0;

    return -1;
}

int intHashFun(int *key, int origSize)
{
    //check input validation
    if (key == NULL || origSize < 0)
    {
        MtoErr("Invalid input");
        return -1;
    }
    int k = *key, toReturn = 0;

    toReturn = k % origSize;

    while (toReturn < 0)
    { //in case k negative number
        toReturn += origSize;
    }

    return toReturn;
}

int strHashFun(char *key, int origSize)
{
    //check input validation
    if (key == NULL || origSize < 0)
    {
        MtoErr("Invalid input");
        return -1;
    }

    int m = 0, n = strlen(key);
    for (int i = 0; i < n; i++)
    {
        m += (int)key[i];
    }
    return m % origSize;
}
