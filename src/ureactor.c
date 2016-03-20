
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
    Demux   *demux;
    Signal  *signal;

}
UReactor;

UReactor *ureactor_create()
{
    UReactor *u = (void *)malloc(sizeof(UReactor));
    memset(u, 0, sizeof(UReactor));
    u->logger = logger_create(stdout, LOG_INFO);
    u->demux = demux_create(u->logger);
    u->signal = signal_create(u->logger, u->demux);
    return u;
}

void ureactor_destroy(UReactor *u)
{
    demux_destroy(u->demux);
    logger_destroy(u->logger);
    memset(u, 0, sizeof(UReactor));
    free(u);
}

void ureactor_run(UReactor *u)
{
    demux_run(u->demux); 
}

void ureactor_on_read_ready( UReactor *u, int fd, int timeout, void *context, Demux_callback callback)
{
    demux_on_read_ready(u->demux, fd, timeout, context, callback); 
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


