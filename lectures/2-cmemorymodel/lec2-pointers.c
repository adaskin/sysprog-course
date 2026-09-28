#include <stdio.h>

int main(){
    int a = 5, b[2] = {0, 1};
    int *p = &a;
    int **q;
    q = &p;
    /////////////////
    int *parray[10];
    parray[0] = &a;
    parray[1] = &b;

    q = &parray[0];
    /*
    q+1 => &parray[1]
    *(q+1) =>q[1]=> parray[1] => &b
    **(q+1) => *q[1] => *parray[1] => b 
    
    *p => p[0]
    */
    printf("a:%d, %d, %d\n", **q, *q[0], q[0][0]);
    printf("a:%d, %d, %d\n", **(q+1), *q[1], q[1][1]);
    printf("bnin bslangic adresi: %p, %p, %p\n", b, &b, &b[0]);

}