#pragma once

#include <logger.h>
#include <reactor.h>
#include <signal_.h>

// Fuck. The event type is a problem...

typedef struct UReactor UReactor;

UReactor *ureactor_create(); 
void ureactor_destroy(UReactor *);

int ureactor_run_once(UReactor *d);
void ureactor_run(UReactor *d);
void ureactor_cancel_all(UReactor *d);
// typedef struct Handler Handler;
//void ureactor_cancel_handler(Handler *h);

void ureactor_on_timer(UReactor *d, int timeout, void *context, Reactor_callback callback);
void ureactor_on_read_ready( UReactor *d, int fd, int timeout, void *context, Reactor_callback callback);
void ureactor_on_write_ready(UReactor *d, int fd, int timeout, void *context, Reactor_callback callback);

void ureactor_register_signal(UReactor *, int signal);
void ureactor_deregister_signal(UReactor *, int signal);
void ureactor_on_signal(UReactor *, int timeout, void *context, Signal_callback callback);

