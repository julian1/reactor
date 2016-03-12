/*
    handling signals
    - signals are hard. they may lag due to fifo queuing, and this presents
      synchronisation issues with handler invocation, cancelling, and rebinding, as well
      as timeouts. Also if deregister and re-register then we may miss an interupt altogether
    - to keep things manageable, we separate registration from callback handling
      actually it's symetrical with sockets in that dispatcher doens't do read/write
    - which enables us to keep timeouts for signals
*/


#include <stdio.h>
#include <assert.h>

#include <dispatcher.h>


void signal_callback(void *context, Event *e)
{
    switch(e->type) {
      case OK:
          fprintf(stdout, "got signal %d\n", e->signal);
          dispatcher_on_signal(e->dispatcher, -1, NULL, signal_callback);
          break;
      default:
          assert(0);
    }
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

