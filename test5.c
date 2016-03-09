
// test timer callback

#include <stdio.h>

#include <dispatcher.h>

static void on_timeout_1(void *context, Event *e)
{
    fprintf(stdout, "timeout_1");
} 

static void on_timeout_2(void *context, Event *e)
{
    fprintf(stdout, "timeout_2");
} 



int main()
{
    Dispatcher *d = dispatcher_create();
    dispatcher_on_timer(d, 3, NULL, (void *)on_timeout_1);
    dispatcher_on_timer(d, 5, NULL, (void *)on_timeout_2);
    while(dispatcher_dispatch(d));
    dispatcher_destroy(d);
    return 0;
}

