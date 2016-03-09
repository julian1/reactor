/*
    overide signals, 
        - we'll set up a single set
        - it would be nice if we could keep all this out of the core dispatcher functionality...
            - inherit by just extending the first structure

    - on creation - make fifo queue. 
    - when create signal handler - we will just use the same writer, 

    - problem - the sa_handler doesn't have a context - which we will need to write
        the pipe file descriptor. 
*/
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>


void myhandler(int signal)
{
    fprintf(stdout, "got signal %d\n", signal);
}

int main()
{
    struct sigaction act;
    memset(&act, 0, sizeof(sigaction));
    act.sa_handler = myhandler;

    int ret = sigaction(2, &act, NULL);
    if(ret != 0) {
        fprintf(stdout, "sigaction failed %d\n", ret);
        assert(0);
    }

    while(1)
        sleep(1);

    return 0;
}

