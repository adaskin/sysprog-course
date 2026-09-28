#include <stdlib.h>
#include <stdio.h>
/*char* getenv (const char *name)
int setenv (const char *name, const char *value, 
int overwrite)
int putenv (const char *string)
void unsetenv (const char *name)
*/
int main(){
    /*example use*/
    char* home = getenv("HOME");
    //setenv("HOME", "/home/adaskin", 1);
    printf("home: %s\n", home);
    return 0;
}