#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
int exists(char *directory, char *name)
{
    struct dirent *dp;
    struct stat s;
    DIR *dirp = opendir(directory);

    while ((dp = readdir(dirp)) != NULL)
    {
        
        stat(dp->d_name, &s);
        if (dp->d_type == DT_DIR){
                printf("directory: %s\n", dp->d_name);
        }
        else if(S_ISREG(s.st_mode)){
            printf("regular: %s\n", dp->d_name);
        }
        if (!strcmp(dp->d_name, name)){
            closedir(dirp);
            return 1; /* Found */
        }
    }
    closedir(dirp);
    return 0; /* Not Found */
}
int main(int argc, char *argv[]){
    if(argc>1){
        exists(argv[1], argv[2]);
    }
}
