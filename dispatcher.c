
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>

#include <dispatcher.h>


static void event_init(Event *e)
{
    memset(e, 0, sizeof(Event));
}


typedef struct Handler Handler;
struct Handler
{
    Handler *next;
    int     fd;

    int     timeout;    // secs
    int     start_time; // secs

    void    *context;

    void    (*read_callback)(void *context, Event *);
    void    (*write_callback)(void *context, Event *);
};


typedef struct Dispatcher Dispatcher;
struct Dispatcher
{
    FILE    *logout; // change name

    Dispatcher_log_level log_level;

    Handler *current;
};

/*
void dispatcher_init(Dispatcher *d)
{
    // not sure if should allocate since may want to use stack
    // Dispatcher *d = (Dispatcher *)malloc(sizeof(Dispatcher));
    memset(d, 0, sizeof(Dispatcher));
}
*/


Dispatcher *dispatcher_create()
{
    Dispatcher *d = malloc(sizeof(Dispatcher));
    memset(d, 0, sizeof(Dispatcher));
    d->logout = stdout;
    d->log_level = LOG_INFO; 
    return d;
}


void dispatcher_destroy(Dispatcher *d)
{
    // should traverse the handlers?
    dispatcher_log(d, LOG_INFO, "destroy");

    assert(!d->current);
    memset(d, 0, sizeof(Dispatcher));
    free(d);
}


void dispatcher_log(Dispatcher *d, Dispatcher_log_level level, const char *format, ...)
{
    if(level >= d->log_level) {
        va_list args;
        va_start (args, format);

        // fprintf(d->logout, "%s: ", getTimestamp());
        const char *s_level = NULL;
        switch(d->log_level) { 
          case LOG_WARNING: s_level = "WARNING"; break;
          case LOG_INFO:    s_level = "INFO"; break;
          case LOG_DEBUG:   s_level = "DEBUG"; break;
          default: assert(0);
        }; 
        fprintf(d->logout, "%s: ", s_level);
        vfprintf(d->logout, format, args);
        va_end (args);
        fprintf(d->logout, "\n");
        fflush(d->logout);
    }
}


static Handler * dispatcher_create_handler(Dispatcher *d, int fd, int timeout, void *context)
{
    // change name from do to bind?
    Handler *h = (Handler *)malloc(sizeof(Handler));
    memset(h, 0, sizeof(Handler));

    h->fd = fd;

    h->timeout = timeout;
    h->start_time = time(NULL);

    h->context = context;

    // tack onto the handler list
    h->next = d->current;
    d->current = h;

    return h;
}


void dispatcher_on_timer(Dispatcher *d, int timeout, void *context, Dispatcher_callback callback)
{
    Handler *h = dispatcher_create_handler( d, -1234, timeout, context);
    // appropriate the the read_callback????
    h->read_callback = callback;
}


void dispatcher_on_read_ready(Dispatcher *d, int fd, int timeout, void *context, Dispatcher_callback callback)
{
    Handler *h = dispatcher_create_handler( d, fd, timeout, context);
    h->read_callback = callback;
}


void dispatcher_on_write_ready(Dispatcher *d, int fd, int timeout, void *context, Dispatcher_callback callback)
{
    Handler *h = dispatcher_create_handler( d, fd, timeout, context);
    h->write_callback = callback;
}


static int handler_count(Handler *l)
{
    int n = 0;
    for(Handler *h = l; h; h = h->next)
        ++n;
    return n;
}


void dispatcher_cancel_all(Dispatcher *d)
{
    dispatcher_log(d, LOG_INFO, "cancel all");

    for(Handler *h = d->current; h; h = h->next) {
        Event e;
        event_init(&e);
        e.dispatcher = d;
        e.fd = h->fd;
        e.type = CANCELLED;

        if(h->read_callback)
          h->read_callback(h->context, &e);
        else if(h->write_callback)
          h->write_callback(h->context, &e);
        else
          assert(0);
    }

    // shouldn't actually need a separate loop
    Handler *next = NULL;
    for(Handler *h = d->current; h; h = next) {
        next = h->next;
        memset(h, 0, sizeof(Handler));
        free(h);
    }

    d->current = NULL;
}


int dispatcher_dispatch(Dispatcher *d)
{
    // TODO: maybe change name to run_once()?

    dispatcher_log(d, LOG_DEBUG, "current handlers: %d\n", handler_count(d->current));

    // r, w, e
    fd_set rs, ws, es;
    FD_ZERO(&rs);
    FD_ZERO(&ws);
    FD_ZERO(&es);

    int max_fd = 0;

    // load up sets
    for(Handler *h = d->current; h; h = h->next) {

        // only real fds...
        if(h->fd >= 0) {
            // we always want to know about exceptions
            FD_SET(h->fd, &es);

            if(h->read_callback) {
                FD_SET(h->fd, &rs);
            }
            else if(h->write_callback) {
                FD_SET(h->fd, &ws);
            } else {
                assert(0);
            }

            if(h->fd > max_fd)
                max_fd = h->fd;
        }
    }

    // problem if fd appears in two sets at the same time?...

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 300 * 1000;  // 300ms

    int ret = select(max_fd + 1, &rs, &ws, &es, &timeout);
    if(ret < 0) {
        dispatcher_log(d, LOG_INFO, "fatal %s", strerror(errno));
        exit(0);
    }
    else {
        int now = time(NULL);
        // 0 (timeout) or more resouces affected
        // a resource is ready
        // dispatcher_log(d, "number or resources ready =%d\n", ret);

        Handler *unchanged = NULL;
        Handler *processed = NULL;
        // clear handler list, so that callbacks can bind in new handlers
        Handler *current = d->current;
        d->current = NULL;
        Handler *next = NULL;

        // process the current by calling callbacks and putting on new lists
        for(Handler *h = current; h; h = next) {
            next = h->next;

            Event e;
            event_init(&e);
            e.dispatcher = d;
            e.fd = h->fd;

            if(h->fd >= 0 && FD_ISSET(h->fd, &es)) {

                // test exception conditions first
                dispatcher_log(d, LOG_INFO, "fd %d exception", h->fd);
                e.type = EXCEPTION;
                if(h->read_callback)
                  h->read_callback(h->context, &e);
                else if(h->write_callback)
                  h->write_callback(h->context, &e);
                else
                  assert(0);

                // transfer handler to processed list
                h->next = processed;
                processed = h;
            }
            else if(h->fd >= 0 && h->read_callback && FD_ISSET(h->fd, &rs)) {

                dispatcher_log(d, LOG_DEBUG, "fd %d is ready for reading", h->fd);
                e.type = OK;
                h->read_callback(h->context, &e);
                // transfer handler to processed list
                h->next = processed;
                processed = h;
            }
            else if(h->fd >= 0 && h->write_callback && FD_ISSET(h->fd, &ws)) {

                dispatcher_log(d, LOG_DEBUG, "fd %d is ready for writing", h->fd);
                e.type = OK;
                h->write_callback(h->context, &e);
                // transfer handler to processed list
                h->next = processed;
                processed = h;
            }
            else if(h->timeout > 0 && now >= h->start_time + h->timeout) {

                dispatcher_log(d, LOG_DEBUG, "fd %d timed out", h->fd);
                e.type = TIMEOUT;
                if(h->read_callback)
                  h->read_callback(h->context, &e);
                else if(h->write_callback)
                  h->write_callback(h->context, &e);
                else
                  assert(0);
                // transfer handler to processed list
                h->next = processed;
                processed = h;
            }
            else {
                // nothing changed for handler - transfer handler to un-processed list
                h->next = unchanged;
                unchanged = h;
            }
        }

        //
        dispatcher_log(d, LOG_DEBUG, "processed: %d", handler_count(processed));
        dispatcher_log(d, LOG_DEBUG, "unchanged: %d", handler_count(unchanged));
        dispatcher_log(d, LOG_DEBUG, "new: %d", handler_count(d->current));

        // free resources for the processed list
        for(Handler *h = processed; h; h = next) {
            next = h->next;
            memset(h, 0, sizeof(Handler));
            free(h);
        }

        // combine unchanged and new lists
        if(d->current) {
            // new handlers were created in callbacks
            // so go to end of new
            Handler *p = d->current;
            while(p && p->next)
                p = p->next;

            // and tack on unchanged
            p->next = unchanged;
        } else {
            // no new handlers, so handlers left to process are whatever was unchanged
            d->current = unchanged;
        }

        // more handlers to process?
        // return d->current != NULL; 
        return handler_count(d->current);
    }
}

