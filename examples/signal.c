/*
    handling signals
    - signals are hard. they may lag due to fifo queuing, and this presents
      synchronisation issues with handler invocation, cancelling, and rebinding, as well
      as timeouts. Also if we automatically deregister and re-register non-atomically we
      could miss a kernel interupt altogether
    - to keep things manageable, we separate registration from callback handling
      in fact it is symetrical with socket/file handling in that reactor does not do read/write
    - enables keeping timeouts for signals which is useful
*/

#include <stdio.h>
#include <assert.h>

#include <reactor.h>


static void signal_callback(void *context, Event *e)
{
    switch(e->type) {
      case OK: {
          static int count = 0;
          fprintf(stdout, "got signal %d\n", e->signal);
          if(count++ < 10) { 
              reactor_on_signal(e->reactor, e->timeout, NULL, signal_callback);
          } else {
            fprintf(stdout, "finishing\n");
          }
          break;
      }
      default:
          assert(0);
    }
}

int main()
{
    Reactor *d = reactor_create();
    reactor_register_signal(d, 2 );  // SIGINT, ctrl-c
    reactor_register_signal(d, 20 ); // SIGTSTP, ctrl-z
    reactor_on_signal(d, -1, NULL, signal_callback);
    reactor_run(d);
    reactor_destroy(d);

    return 0;
}

