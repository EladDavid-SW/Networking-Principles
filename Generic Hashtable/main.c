
// #include "GenericHashTable.c"

// int main(int argc, char const *argv[])
// {
//     int a = 8;
//     int b = 16;
//     int c = 24;
//     int d = 5;
//     int e = 9;
//     int f = 13;
//     int g = 6;
//     int h = 10;
//     int i = 14;
//     int j = 104;
//     Table* t = createTable(4,0,3);
//     add(t,&a);
//     freeTable(t);

//     return 0;
// }

#include "GenericHashTable.c"
int main()
{
    int a = 8;
    int b = 16;
    int c = 24;
    int d = 5;
    int e = 9;
    int f = 13;
    int g = 6;
    int h = 10;
    int i = 14;
    int j = 104;
    char *str1 = "aaaaaaaaaa";
    char *str2 = "b r r r r e ";
    char *str3 = "cwjxcn cwnjcwj";
    char *str4 = "dwcnjwc njcwnjwc";
    char *str5 = "ecwwc wcwc cwcw";
    char *str6 = "f";
    char *str7 = "ew33f";
    Table *temp = createTable(8, 0, 2);
    add(temp, (void *)&a);
    add(temp, (void *)&b);
    add(temp, (void *)&c);
    add(temp, (void *)&d);
    add(temp, (void *)&e);
    add(temp, (void *)&f);
    add(temp, (void *)&g);
    add(temp, (void *)&h);
    add(temp, (void *)&i);
    printTable(temp);
    Object *xyz = search(temp, (void *)&a);
    if(xyz != NULL){
        printf("a = %d\n", *(int*)xyz->data);
    }
    else{printf("NULL\n");}
    xyz = search(temp, (void *)&c);
    if(xyz != NULL){
        printf("c = %d\n", *(int*)xyz->data);
    }
    else{printf("NULL\n");}
    xyz = search(temp, (void *)&j);
    if(xyz != NULL){
        printf("j = %d\n", *(int*)xyz->data);
    }
    else{printf("NULL\n");}
        xyz = search(temp, (void *)&f);
    if(xyz != NULL){
        printf("f = %d\n", *(int*)xyz->data);
    }
    else{printf("NULL\n");}
    int x =removeObj(temp,(void*)&a);
    int xx =removeObj(temp,(void*)&j);
    int xxx =removeObj(temp,(void*)&a);
    int y = removeObj(temp,(void*)&c);
    int z = removeObj(temp,(void*)&i);
    xyz = search(temp, (void *)&a);
    printTable(temp);
    if(xyz != NULL){
        printf("a = %d\n", *(int*)xyz->data);
    }
    else{printf("NULL\n");}
    xyz = search(temp, (void *)&f);
    if(xyz != NULL){
        printf("f = %d\n", *(int*)xyz->data);
    }
    else{printf("NULL\n");}
    xyz = search(temp, (void *)&b);
    if(xyz != NULL){
        printf("b = %d\n", *(int*)xyz->data);
    }
    else{printf("NULL\n");}
    printf("\n%d %d %d %d %d\n\n",x,xx,xxx,y,z);
    Table *tempO = createTable(4, 1, 3);
    add(tempO, (void *)str1);
    add(tempO, (void *)str2);
    add(tempO, (void *)str3);
    add(tempO, (void *)str4);
    add(tempO, (void *)str5);
    add(tempO, (void *)str6);
    printTable(tempO);
    xyz = search(tempO, (void *)str1);
    if(xyz != NULL){
        printf("str1 = %s\n", (char*)xyz->data);
    }
    else{printf("NULL\n");}
    xyz = search(tempO, (void *)str2);
    if(xyz != NULL){
        printf("str2 = %s\n", (char*)xyz->data);
    }
    else{printf("NULL\n");}
    xyz = search(tempO, (void *)str3);
    if(xyz != NULL){
        printf("str3 = %s\n", (char*)xyz->data);
    }
    else{printf("NULL\n");}

    add(tempO, (void *)str1);
    add(tempO, (void *)str2);
    add(tempO, (void *)str3);
    add(tempO, (void *)str4);
    add(tempO, (void *)str5);
    add(tempO, (void *)str6);
    printTable(tempO);
    int x1 =removeObj(tempO,(void*)str6);
    int xx1 =removeObj(tempO,(void*)str3);
    printTable(tempO);
    int xxx1 =removeObj(tempO,(void*)str6);
    int y1 = removeObj(tempO,(void*)str1);
    int z1 = removeObj(tempO,(void*)str7);
    printTable(tempO);
    printf("\n%d %d %d %d %d\n\n",x1,xx1,xxx1,y1,z1);
        xyz = search(tempO, (void *)str1);
    if(xyz != NULL){
        printf("str1 = %s\n", (char*)xyz->data);
    }
    else{printf("NULL\n");}
    xyz = search(tempO, (void *)str2);
    if(xyz != NULL){
        printf("str2 = %s\n", (char*)xyz->data);
    }
    else{printf("NULL\n");}
    xyz = search(tempO, (void *)str3);
    if(xyz != NULL){
        printf("str3 = %s\n", (char*)xyz->data);
    }
    else{printf("NULL\n");}
    freeTable(temp);
    freeTable(tempO);
    return 0;
}