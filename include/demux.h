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
} Demux_event_type;   

typedef struct Event Event;   // change name demux_event ? 

typedef void (*Demux_callback)(void *context, Event *);

typedef struct Handler Handler;

struct Event
{
    Demux_event_type type;
    //Demux       *demux;     // may remove this...
    Handler     *handler;   // opaque to caller space, but enables cancelling...
                            // perhaps should remove...
                            // this mostly duplicates  handler, but makes it easier for end user ..
    int         fd;         // for socket,stdin,file,device etc
    int         timeout;    // associated with handler, not just timer, can make rebinding easier
};

Demux *demux_create(Logger *);

void demux_destroy(Demux *);

int demux_run_once(Demux *d);

void demux_run(Demux *d);

void demux_cancel_all(Demux *d);

void demux_cancel_handler(Handler *h);

void demux_on_timer(Demux *d, int timeout, void *context, Demux_callback callback);

void demux_on_read_ready( Demux *d, int fd, int timeout, void *context, Demux_callback callback);

void demux_on_write_ready(Demux *d, int fd, int timeout, void *context, Demux_callback callback);

