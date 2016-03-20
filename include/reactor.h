#pragma once

#include <logger.h>
#include <demux.h>
#include <signal_.h>

typedef struct Reactor Reactor;

Reactor *reactor_create(); 

void reactor_destroy(Reactor *);

int reactor_run_once(Reactor *d);

void reactor_run(Reactor *d);

void reactor_cancel_all(Reactor *d);

//void reactor_cancel_handler(Handler *h);

void reactor_on_timer(Reactor *d, int timeout, void *context, Demux_callback callback);

void reactor_on_read_ready( Reactor *d, int fd, int timeout, void *context, Demux_callback callback);

void reactor_on_write_ready(Reactor *d, int fd, int timeout, void *context, Demux_callback callback);

void reactor_register_signal(Reactor *, int signal);

void reactor_deregister_signal(Reactor *, int signal);

void reactor_on_signal(Reactor *, int timeout, void *context, Signal_callback callback);

