/** taken from 
 * http://www.qnx.com/developers/docs/6.5.0/index.jsp?topic=%2Fcom.qnx.doc.neutrino_lib_ref%2Fs%2Fsigprocmask.html
 *int sigprocmask (int how, 
                const sigset_t *restrict set, 
                sigset_t *restrict oldset);
 *  */

#include <stdio.h> 
#include <stdlib.h> 
#include <signal.h>
#include <unistd.h>
int main(void){
    sigset_t set, oset, pset;

    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigprocmask(SIG_BLOCK, &set, &oset);
    printf("Old set was %8.8ld.\n", oset);

    sigpending(&pset);
    printf("Pending set is %8.8ld.\n", pset);

    kill(getpid(), SIGINT);

    sigpending(&pset);
    printf("Pending set is %8.8ld.\n", pset);

    sigprocmask(SIG_UNBLOCK, &set, &oset);

    /* The program terminates with a SIGINT */
    return 0;
}