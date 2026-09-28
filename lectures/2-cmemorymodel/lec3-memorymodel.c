#include <stdio.h>

int main1 (int argc, char *argv[]){
    int a = 1; int b = 2;
    int arr[5] = {1, 2, 3, 4, 5};
    int *parray[10];
    parray[0] = &a;
    parray[1] = &b;
    parray[2] = &arr[0]; 

    while(parray[2]<= &arr[4]){
        printf("%d\n",*parray[2]);
        parray[2]++;
    }

    for (int i = 0; i < argc; i++){
        printf("%s\n", argv[i]);
    }

}
int *f(int *ptr){
    return NULL;
}
int main3(){
    struct st {

        int a; /**/
        int b;
    };


    typedef struct {int a; int b} pair;

    struct st x, y;
    pair p1, p2 = {5, 4};
    p1.a = 5;
    int *p = &p1.a;
    
    pair *ptr;


    ptr = &p2;
    
    printf("%d-%d, %d\n\n %d\n%d\n %d\n", p2.a, ptr->b, (*ptr).a, ptr, ptr+1, sizeof(pair));



}

int main(){

    struct st {
        /*3 byte padding char slot[3];*/
        char c;
       char pad[2];
        int b;
        int ic;
        int d;
        char c2; /*4 byte*/
    } st1;
    printf("%lu", sizeof(struct st));
   
}