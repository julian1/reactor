
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
    FILE    *logout;
    Dispatcher_log_level log_level;
    Handler *current;
};

/*
void dispatcher_init(Dispatcher *d)
{
    // not sure if should allocate since may want to use stack
    memset(d, 0, sizeof(Dispatcher));
}
*/


Dispatcher *dispatcher_create_with_log_level(Dispatcher_log_level level)
{
    Dispatcher *d = malloc(sizeof(Dispatcher));
    memset(d, 0, sizeof(Dispatcher));
    d->logout = stdout;
    d->log_level = level;

    dispatcher_log(d, LOG_INFO, "create");
    return d;
}


Dispatcher *dispatcher_create()
{
    return dispatcher_create_with_log_level(LOG_INFO);
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
        switch(level) {
          case LOG_DEBUG:   s_level = "DEBUG"; break;
          case LOG_INFO:    s_level = "INFO"; break;
          case LOG_WARN:    s_level = "WARN"; break;
          case LOG_FATAL:   s_level = "FATAL"; break;
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

    // call handlers with cancelled action
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

    // cleanup - shouldn't really need a different loop...
    Handler *next = NULL;
    for(Handler *h = d->current; h; h = next) {
        next = h->next;
        memset(h, 0, sizeof(Handler));
        free(h);
    }

    d->current = NULL;
}


void dispatcher_run(Dispatcher *d)
{
    while(dispatcher_run_once(d));
}


int dispatcher_run_once(Dispatcher *d)
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

        // only add real fds...
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

    // TODO could compute iterate the handler list and compute the smallest expected
    // timeout value, and use it for the next select timeout

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 300 * 1000;  // 300ms

    int ret = select(max_fd + 1, &rs, &ws, &es, &timeout);
    if(ret < 0) {
        dispatcher_log(d, LOG_FATAL, "fatal %s", strerror(errno));
        // TODO attempt cleanup by calling cancel??
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

        // TODO could also log the specific fds
        dispatcher_log(d, LOG_DEBUG, "processed: %d", handler_count(processed));
        dispatcher_log(d, LOG_DEBUG, "unchanged: %d", handler_count(unchanged));
        dispatcher_log(d, LOG_DEBUG, "new: %d", handler_count(d->current));

        // free handler mem
        for(Handler *h = processed; h; h = next) {
            next = h->next;
            memset(h, 0, sizeof(Handler));
            free(h);
        }

        // concatenate the unchanged and new handler lists
        if(d->current) {
            // new handlers were created in callbacks
            // so move to the end of the new list
            Handler *p = d->current;
            while(p && p->next)
                p = p->next;

            // and tack on the unchanged list
            p->next = unchanged;
        } else {
            // no new handlers, so handlers are simply remaining unprocessed handlers
            d->current = unchanged;
        }

        // do we have more handlers to process?
        int handler_still_to_process = handler_count(d->current);
        if(handler_still_to_process == 0) {
            dispatcher_log(d, LOG_INFO, "no more handlers to process");
        }

        return handler_still_to_process;
    }
    assert(0);
}

