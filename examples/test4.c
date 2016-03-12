/*
    handling signals
    - signals are hard. they may lag due to fifo queuing, and this presents
      synchronisation issues with handler invocation, cancelling, and rebinding, as well
      as timeouts. Also if we automatically deregister and re-register non-atomically we 
      could miss a kernel interupt altogether
    - to keep things manageable, we separate registration from callback handling
      in fact it is symetrical with socket/file handling in that dispatcher does not do read/write
    - enables keeping timeouts for signals which is useful
*/

#include <stdio.h>
#include <assert.h>

#include <dispatcher.h>


void signal_callback(void *context, Event *e)
{
    switch(e->type) {
      case OK:
          fprintf(stdout, "got signal %d\n", e->signal);
          dispatcher_on_signal(e->dispatcher, e->timeout, NULL, signal_callback);
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

