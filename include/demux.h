#pragma once

#include <logger.h>

/*
    who is responsible for destroying...
    really needs to be another structure... 
*/

typedef struct Demux Demux;

typedef enum {
    READ_READY,
    WRITE_READY,
    EXCEPTION,
    TIMEOUT,
    CANCELLED,
    // SIGNAL...
} Demux_event_type;   // change name to just Event_type?


typedef struct Event Event;   // change name core_event ? 

typedef void (*Demux_callback)(void *context, Event *);

typedef struct Handler Handler;

struct Event
{
    Demux_event_type type;
    Demux     *demux;    // may remove this...
    Handler     *handler;   // opaque to caller space, but enables rebinding...

    // this mostly duplicates  handler, but makes it easier for end user ..
    int         fd;         // for socket,stdin,file,device etc
    int         timeout;    // associated with handler, not just timer, can make rebinding easier
    // int         signal;     // for signals
/*
    // support for rebinding...
    void        *context;
    Demux_callback callback; 
*/
};


// TODO better prefixes for enum values...

// ALL < DEBUG < INFO < WARN < ERROR < FATAL < OFF.
/*
typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_FATAL,
    LOG_NONE
} Demux_log_level;
*/


// setup and run
//Demux * demux_create();

Demux *demux_create(Logger *);

void demux_destroy(Demux *);

int demux_run_once(Demux *d);

void demux_run(Demux *d);

void demux_cancel_all(Demux *d);


typedef struct Handler Handler;

void demux_cancel_handler(Handler *h);

void demux_on_timer(Demux *d, int timeout, void *context, Demux_callback callback);

void demux_on_read_ready( Demux *d, int fd, int timeout, void *context, Demux_callback callback);

void demux_on_write_ready(Demux *d, int fd, int timeout, void *context, Demux_callback callback);

// void demux_rebind_handler(Event *e);

/*
// signal stuff
void demux_register_signal(Demux *d, int signal);

void demux_deregister_signal(Demux *d, int signal);

void demux_on_signal(Demux *d, int timeout, void *context, Demux_callback callback);
*/

