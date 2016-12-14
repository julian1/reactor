
// Example showing use of two system timers

#include <stdio.h>

#include <reactor.h>

static void on_timeout_1(Reactor *r, Event *e)
{
    switch(e->type) {
      case TIMEOUT: {
        static int count = 0;
        ++count;
        if(count > 10) {
            reactor_cancel_all(r);
        }

        fprintf(stdout, "timeout 1 - count %d\n", count);
        reactor_on_timer(r, e->timeout, r, (Demux_callback)on_timeout_1);
        break;
      }
      case CANCELLED:
        fprintf(stdout, "timeout 1 - cancelled\n");
        break;
      default:
        ;
    }
}


static void on_timeout_2(Reactor *r, Event *e)
{
    switch(e->type) {
      case TIMEOUT: {
          fprintf(stdout, "timeout 2\n");
          // want a rebind function that just takes the event...
          reactor_on_timer(r, e->timeout, r, (Demux_callback)on_timeout_2);
        break;
      }
      case CANCELLED:
        fprintf(stdout, "timeout 2 - cancelled\n");
        break;
       default:
        ;
    }
}


int main()
{
    Reactor *r = reactor_create();
    reactor_on_timer(r, 200, r, (Demux_callback)on_timeout_1);
    reactor_on_timer(r, 1000, r, (Demux_callback)on_timeout_2);
    reactor_run(r);
    reactor_destroy(r);
    return 0;
}

