
/*
    - rather than have lots of freestanding functions...
    should use structs of function pointers?
    - and then a simple wrapper over them?
*/
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <ureactor.h>

typedef struct UReactor 
{
    Logger  *logger; 
    Reactor *reactor;  // change name core...
    Signal  *signal;

}
UReactor;

UReactor *ureactor_create()
{
    UReactor *u = (void *)malloc(sizeof(UReactor));
    memset(u, 0, sizeof(UReactor));
    u->logger = logger_create(stdout, LOG_INFO);
    u->reactor = reactor_create(u->logger);
    u->signal = signal_create(u->logger, u->reactor);
    return u;
}

void ureactor_destroy(UReactor *u)
{
    reactor_destroy(u->reactor);
    logger_destroy(u->logger);
    memset(u, 0, sizeof(UReactor));
    free(u);
}

void ureactor_run(UReactor *u)
{
    reactor_run(u->reactor); 
}

void ureactor_on_read_ready( UReactor *u, int fd, int timeout, void *context, Reactor_callback callback)
{
    reactor_on_read_ready(u->reactor, fd, timeout, context, callback); 
}


///////

void ureactor_register_signal(UReactor *u, int signal)
{
    signal_register_signal(u->signal, signal);
}

void ureactor_deregister_signal(UReactor *u, int signal)
{
    assert(0);
}

void ureactor_on_signal(UReactor *u, int timeout, void *context, Signal_callback callback)
{
    signal_on_signal(u->signal, timeout, context, callback);
}


