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

#include <signal_.h>

Signal *d;

static void signal_callback(void *context, SignalEvent *e)
{
    switch(e->type) {
      case READ_READY: {
          static int count = 0;
          fprintf(stdout, "got signal %d\n", e->siginfo.si_signo );
          if(count++ < 10) { 
              ///reactor_rebind_handler(e);
              signal_on_signal(d, -1, NULL, signal_callback);
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
    Logger *l = logger_create(stdout, LOG_INFO);
    Reactor *r = reactor_create(l);
    d = signal_create(l, r);

    signal_register_signal(d, 2 );  // SIGINT, ctrl-c
    signal_register_signal(d, 20 ); // SIGTSTP, ctrl-z
    signal_on_signal(d, -1, NULL, signal_callback);

    reactor_run(r);
    signal_destroy(d);

    return 0;
}

