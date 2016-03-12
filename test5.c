
// test timer callback

#include <stdio.h>

#include <dispatcher.h>

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
    Dispatcher *d = dispatcher_create();
    dispatcher_on_timer(d, 2, NULL, (void *)on_timeout_1);
    dispatcher_on_timer(d, 5, NULL, (void *)on_timeout_2);
    while(dispatcher_dispatch(d));
    dispatcher_destroy(d);
    return 0;
}

