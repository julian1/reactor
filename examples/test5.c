
// timer callbacks

#include <stdio.h>

#include <reactor.h>

static void on_timeout_1(void *context, Event *e)
{
    fprintf(stdout, "timeout 1\n");
} 


static void on_timeout_2(void *context, Event *e)
{
    fprintf(stdout, "timeout 2\n");
} 


int main()
{
    Reactor *d = reactor_create();
    reactor_on_timer(d, 2, NULL, (void *)on_timeout_1);
    reactor_on_timer(d, 5, NULL, (void *)on_timeout_2);
    reactor_run(d);
    reactor_destroy(d);
    return 0;
}

