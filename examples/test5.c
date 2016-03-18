
// timer callbacks

#include <stdio.h>

#include <reactor.h>

static void on_timeout_1(void *context, Event *e)
{
    static int count = 0;
    ++count;
    if(count > 10) {
        reactor_cancel_all(e->reactor);
    }

    fprintf(stdout, "timeout 1 - count %d\n", count);
    reactor_on_timer(e->reactor, e->timeout, NULL, (void *)on_timeout_1);
}


static void on_timeout_2(void *context, Event *e)
{
    fprintf(stdout, "timeout 2\n");
    // want a rebind function that just takes the event...
    reactor_on_timer(e->reactor, e->timeout, NULL, (void *)on_timeout_2);
}


int main()
{
    Reactor *d = reactor_create();
    reactor_on_timer(d, 200, NULL, (void *)on_timeout_1);
    reactor_on_timer(d, 1000, NULL, (void *)on_timeout_2);
    reactor_run(d);
    reactor_destroy(d);
    return 0;
}

