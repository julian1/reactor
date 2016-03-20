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

#include <ureactor.h>
#include <signal_.h>   // weird problem here....

// Signal *d;
UReactor *r;

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
              ureactor_on_signal(r, -1, NULL, signal_callback);
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
    r = ureactor_create();

    ureactor_register_signal(r, 2 );  // SIGINT, ctrl-c
    ureactor_register_signal(r, 20 ); // SIGTSTP, ctrl-z
    ureactor_on_signal(r, -1, NULL, signal_callback);
    // ureactor_on_signal(UReactor *, int timeout, void *context, Signal_callback callback);

    ureactor_run(r);
    ureactor_destroy(r);

    return 0;
}

