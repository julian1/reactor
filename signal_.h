#pragma once
// does 

#include <logger.h>
#include <reactor.h>

#include <signal.h>

typedef struct Signal Signal;

Signal *signal_create(Logger *, Reactor *);

void signal_destroy(Signal *);

void signal_register_signal(Signal *d, int signal);

void signal_deregister_signal(Signal *d, int signal);

// use regular callback?
// we have to deliver the data which will be 

// we don't want to extend, the enum.... because we don't want read or write...
// it's getting messy

typedef enum {
    SIGNAL_SIGNAL,
    SIGNAL_EXCEPTION,
    SIGNAL_TIMEOUT,
    SIGNAL_CANCELLED,
    // SIGNAL...
} Signal_event_type;     // change name to just Event_type?


typedef struct SignalEvent
{
    Signal_event_type type;
    // there's no handler to keep around
    // Reactor     *reactor;   // may remove this...
    // int         timeout; 
    // int         signal;       // for signals
    siginfo_t       siginfo;
} SignalEvent;

typedef void (*Signal_callback)(void *context, SignalEvent *);

void signal_on_signal(Signal *d, int timeout, void *context, Signal_callback callback);

