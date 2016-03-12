/*
    test signals 

    overide signals, 
        - we'll set up a single set
        - it would be nice if we could keep all this out of the core dispatcher functionality...
            - inherit by just extending the first structure

    - on creation - make fifo queue. 
    - when create signal handler - we will just use the same writer, 

    - problem - the sa_handler doesn't have a context - which we will need to write
        the pipe file descriptor. 
*/

/*
  IMPORTANT - use a struct  to represent the signal information that
    we write to the fifo buffer
  eg. 
    write(fifo, sig_data, sizeof(MyStruct) ); 
  uggh there's no callback context - uggh. so may need to use static ref to fifo .  
*/

/*
    - issue with lag, if we unset the signal handler, between when the handler
    is called and the interrupt...

    - might also get an interupt after saying not interested...
        ok, maybe ok, if no handler registered 
 
    - might get :w
 
    probably easier, to not allow deallocation 
*/

/*
  http://www.tldp.org/LDP/lpg/node18.html
  Ok, see if we can make it work, and then change to unnamed pipe.
*/


#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/stat.h>
#include <stdlib.h>
#include <errno.h>


#include <dispatcher.h>

/*
  so we cannot differentiate a context for signal handler 
  that means single pipe, and 

*/



///////////////////////////////

void signal_callback( void *context, Event *e)
{
    // timeouts... cancelling is more complicated with a signal...
    fprintf(stdout, "WHOOT - got signal %d\n", e->signal); 

    // rebind
    dispatcher_on_signal(e->dispatcher, -1, NULL, signal_callback);
} 

int main()
{
    Dispatcher *d = dispatcher_create();

    dispatcher_register_signal(d, 2 );  // SIGINT, ctrl-c
    dispatcher_register_signal(d, 20 ); // SIGTSTP, ctrl-z

    dispatcher_on_signal(d, -1, NULL, signal_callback);

    dispatcher_run(d);
    dispatcher_destroy(d);

    return 0;
}



    // setting up the sigaction should probably be last...
    // now the handler will run, and do we don't really want to unbind...
    // so have an excplcit o

    // actually i think it's ok to rebind... because if an interupt happens it will always get 
    // written to the fifo...
    // no the issue is an interupt created between clearing the sigaction and resetting it in the event handler


    // Dispatcher *d = dispatcher_create_with_log_level(LOG_DEBUG);

    // so set up the pipe and signal_fifo_readfd in dispatcher create,
    // it only needs to go in dispatch so we have somewhere to record the signal_fifo_readfd

    // dispatcher with signals...

/*
    int fd[2];
    if(pipe(fd) < 0) {
        dispatcher_log(d, LOG_FATAL, "could not create fifo pipe %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    signal_fifo_readfd = fd[0];
    signal_fifo_writefd = fd[1];
*/
