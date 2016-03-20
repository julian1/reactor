
/*
    - rather than have lots of freestanding functions...
    should use structs of function pointers?
    - and then a simple wrapper over them?
*/
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <reactor.h>

typedef struct Reactor 
{
    Logger  *logger; 
    Demux   *demux;
    Signal  *signal;

}
Reactor;

Reactor *reactor_create()
{
    Reactor *u = (void *)malloc(sizeof(Reactor));
    memset(u, 0, sizeof(Reactor));
    u->logger = logger_create(stdout, LOG_INFO);
    u->demux = demux_create(u->logger);
    u->signal = signal_create(u->logger, u->demux);
    return u;
}

void reactor_destroy(Reactor *u)
{
    demux_destroy(u->demux);
    logger_destroy(u->logger);
    memset(u, 0, sizeof(Reactor));
    free(u);
}

void reactor_run(Reactor *u)
{
    demux_run(u->demux); 
}



void reactor_cancel_all(Reactor *r)
{
    demux_cancel_all(r->demux);
}

void reactor_on_timer(Reactor *r, int timeout, void *context, Demux_callback callback)
{
    demux_on_timer(r->demux, timeout, context, callback);
}


void reactor_on_read_ready( Reactor *u, int fd, int timeout, void *context, Demux_callback callback)
{
    demux_on_read_ready(u->demux, fd, timeout, context, callback); 
}


///////

void reactor_register_signal(Reactor *u, int signal)
{
    signal_register_signal(u->signal, signal);
}

void reactor_deregister_signal(Reactor *u, int signal)
{
    assert(0);
}

void reactor_on_signal(Reactor *u, int timeout, void *context, Signal_callback callback)
{
    signal_on_signal(u->signal, timeout, context, callback);
}


