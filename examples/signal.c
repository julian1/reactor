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
#include <signal_.h>   // weird problem here....

// Signal *d;
Reactor *r;

/*
  change name reactor to dumux, dispatch or core
*/

static void signal_callback(void *context, SignalEvent *e)
{
    switch(e->type) {
      case SIGNAL_SIGNAL: {
          static int count = 0;
          fprintf(stdout, "got signal %d\n", e->siginfo.si_signo );
          if(count++ < 5) { 
              ///reactor_rebind_handler(e);
              reactor_on_signal(r, -1, NULL, signal_callback);
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
    r = reactor_create();
    reactor_register_signal(r, 2 );  // SIGINT, ctrl-c
    reactor_register_signal(r, 20 ); // SIGTSTP, ctrl-z
    reactor_on_signal(r, -1, NULL, signal_callback);
    reactor_run(r);
    reactor_destroy(r);

    return 0;
}

