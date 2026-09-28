#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

int exists(char *dir, char *fname)
{
    DIR *dirp;
    struct dirent *dp;
    struct stat s;
    int len = strlen(fname);
    dirp = opendir(dir);
    if (!dirp)
    {
        perror("directory error: ");
        return -1;
    }
    
    while ((dp = readdir(dirp)) != NULL)
    {
        stat(dp->d_name, &s);
        if(dp->d_type == 4 )//S_ISDIR(s.st_mode))
            printf("directory: %s\n", dp->d_name);
        else if(S_ISREG(s.st_mode)){
            printf("regular: %s\n", dp->d_name);
                
        }
        
        if (len == dp->d_namlen && !strcmp(dp->d_name, fname))
        {
            closedir(dirp);
            return 1;
        }
    }
    closedir(dirp);
    return 0;
}
int main(int argc, char *argv[]){
    if(argc >= 3){
        if(exists(argv[1], argv[2])){
            puts("bulundu");
        }else puts("bulunamadi");
    }
}