#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int intcmp(int *a, int *b, int n){
	for (int i = 0; i <n ; i++){
		if(a[i] != b[i]){
			return -1;
		}
	}
	return 0;
}
int stringcmp(char a[], char *b, int n){
	for (int i = 0; i <n ; i++){
		if(a[i] != b[i]){
			return -1;
		}
	}
	return 0;
}

int main1() {
	int a[] = {1, 2, 3}, b[] = {1, 2,4};
	typedef int (*fptr) (void*, void*, int);


	fptr fp = (int(*) (void *, void *, int)) intcmp;
	
	int result = (*fp)(a, b, sizeof(a)/sizeof(int));
	printf("%d\n", result);
	return 0;

}

int main2(){
    struct sa{
        char s1[1];
		char s2[2];
        int i1;
        int i;
    };
    struct sa a;
    //printf("size: %d, %d, %d, %d, %d\n", sizeof(struct sa), &a.s1, &a.i1,  &a.s2, &a.i);
    return 0;

}

int main(){
	char *pstr = "ders1";
	pstr[4] = '4';
	char str[] = "ders2";
	char str2[] = {'d', 'e', 'r', 's', '\0'};
	str[4] = '3';
	printf("%s-%s\n", str, pstr); 




}