
#include <stdio.h>
#include <fcntl.h>
//#include <unistd.h>
#include <sys/select.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include <stdarg.h>
#include <sys/time.h>
//#include <linux/stat.h>
//#include <signal.h>
#include <stdbool.h> // c99


#include <logger.h>
#include <demux.h>

typedef enum {
    INTEREST_READ,   // change name DEMUX_READ
    INTEREST_WRITE,
    // INTEREST_RW,
    INTEREST_TIMEOUT // none...
} Demux_interest_type;




typedef struct Handler Handler;
struct Handler
{
    Handler     *next;

    Demux_interest_type type;

    int         fd;
    int         timeout;    // in millisecs

    struct timeval start_time;
    struct timeval timeout_time;

    bool        cancelled;

  // user data
    void        *context;
    void        (*callback)(void *context, Event *);
};


typedef struct Demux Demux;
struct Demux
{
    // FILE        *logout;
    // Demux_log_level log_level;

    Logger      *logger;

    Handler     *current;
    Handler     *new;

    bool        cancel_all_handlers;

};



// uggh shouldn't go here
static void init_event_from_handler(Demux *d, Demux_event_type type, Handler *h, Event *e)
{
    memset(e, 0, sizeof(Event));
    e->demux = d;
    e->type = type;
    e->handler = h;

    // copy some values out of the handler as convenience to user
    e->fd = h->fd;         
    e->timeout = h->timeout; 
    // int         signal;     // should be in signal user-context?
}



Demux *demux_create(Logger *logger)
{
    Demux *d = malloc(sizeof(Demux));
    memset(d, 0, sizeof(Demux));
    // d->logout = stdout;
    // d->log_level = level;
    d->logger = logger;
    logger_log(d->logger, LOG_INFO, "create");


    return d;
}



void demux_destroy(Demux *d)
{
    // IMPORTANT - does not destroy the logger!!!!

    logger_log(d->logger, LOG_INFO, "destroy");

    if(d->current) {
        logger_log(d->logger, LOG_FATAL, "destroy() called with unprocessed handlers");
        exit(EXIT_FAILURE);
    }

    memset(d, 0, sizeof(Demux));
    free(d);
}

/*
void logger_log(Demux *d, Demux_log_level level, const char *format, ...)
{
    if(level >= d->log_level) {
        va_list args;
        va_start (args, format);

        // fprintf(d->logout, "%s: ", getTimexceptfdstamp());
        const char *s_level = NULL;
        switch(level) {
          case LOG_DEBUG: s_level = "DEBUG"; break;
          case LOG_INFO:  s_level = "INFO"; break;
          case LOG_WARN:  s_level = "WARN"; break;
          case LOG_FATAL: s_level = "FATAL"; break;
          default: assert(0);
        };
        fprintf(d->logout, "%s: ", s_level);
        vfprintf(d->logout, format, args);
        va_end (args);
        fprintf(d->logout, "\n");
        fflush(d->logout);
    }
}
*/


static Handler *demux_create_handler(Demux *d, Demux_interest_type type, int fd, int timeout, void *context, Demux_callback callback)
{
    Handler *h = (Handler *)malloc(sizeof(Handler));
    memset(h, 0, sizeof(Handler));

    h->type = type;

    h->fd = fd;
    // we don't really need to record both, but s
    h->timeout = timeout;

    if(gettimeofday(&h->start_time, NULL) < 0) {
        logger_log(d->logger, LOG_FATAL, "gettimeofday() failed '%s'", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // set the timeout time
    if(h->timeout >= 0) {
        struct timeval timeout;
        timeout.tv_sec =  h->timeout / 1000;
        timeout.tv_usec = (h->timeout % 1000) * 1000;
        timeradd(&h->start_time, &timeout, &h->timeout_time);
    }

    h->context = context;
    h->callback = callback;

    // tack onto the new handler list
    h->next = d->new;
    d->new = h;

    return h;
}


void demux_on_timer(Demux *d, int timeout, void *context, Demux_callback callback)
{
    demux_create_handler(d, INTEREST_TIMEOUT, -1234, timeout, context, callback);
}


void demux_on_read_ready(Demux *d, int fd, int timeout, void *context, Demux_callback callback)
{
    demux_create_handler(d, INTEREST_READ, fd, timeout, context, callback);
}


void demux_on_write_ready(Demux *d, int fd, int timeout, void *context, Demux_callback callback)
{
    demux_create_handler(d, INTEREST_WRITE, fd, timeout, context, callback);
}

/*
void demux_rebind_handler(Event *e)
{
    // doesn't work with signals... because of inner context?
    assert(e->demux);
    Handler *h = e->handler;
    assert(h);
    demux_create_handler(e->demux, h->type, h->fd, h->timeout, h->context, h->callback);
}
*/

static int handler_count(Handler *l)
{
    int n = 0;
    for(Handler *h = l; h; h = h->next)
        ++n;

    return n;
}


void demux_cancel_all(Demux *d)
{
    logger_log(d->logger, LOG_INFO, "cancel_all()");
    d->cancel_all_handlers = true;
}

// IMPORTANT - THE ONLY PLACE WE TRAVERSE THE LISTS SHOULD BE run_once()
// might want some magic number checks... to check memory...


void demux_run(Demux *d)
{
    while(demux_run_once(d));
}


int demux_run_once(Demux *d)
{
    logger_log(d->logger, LOG_DEBUG, "current : %d", handler_count(d->current));
    logger_log(d->logger, LOG_DEBUG, "new: %d", handler_count(d->current));


    // transfer new handlers to current list
    if(d->current) {
        Handler *p = d->current;
        while(p->next)
            p = p->next;
        p->next = d->new;
    } else {
        d->current = d->new;
    }
    d->new = NULL;


    // mark handlers to be cancelled
    if(d->cancel_all_handlers) {
        for(Handler *h = d->current; h; h = h->next) {
            h->cancelled = true;
        }
        d->cancel_all_handlers = false;
    }

    // processed cancelled handlers
    {
        // using process lists makes it easier to log, and cleanup, and avoids needing to
        // keep status booleans
        Handler *unchanged = NULL;
        Handler *cancelled = NULL;
        Handler *next = NULL;
        Handler *current = d->current;
        d->current = NULL;

        // call any cancelled handlers
        for(Handler *h = current; h; h = next) {

            next = h->next;
            if(h->cancelled) {
                Event e;
                init_event_from_handler(d, CANCELLED, h, &e);
                logger_log(d->logger, LOG_DEBUG, "fd %d cancelled", h->fd);
                h->callback(h->context, &e);
                h->next = cancelled;
                cancelled = h;
            }
            else {
                h->next = unchanged;
                unchanged = h;
            }
        }

        logger_log(d->logger, LOG_DEBUG, "cancelled: %d", handler_count(cancelled));
        logger_log(d->logger, LOG_DEBUG, "unchanged: %d", handler_count(unchanged));

        d->current = unchanged;

        if(cancelled) {
            Handler *next = NULL;
            // free cancelled handler mem
            for(Handler *h = cancelled; h; h = next) {
                next = h->next;
                memset(h, 0, sizeof(Handler));
                free(h);
            }

            // return immediately to avoid select() blocking where all handlers were cancelled
            int count = handler_count(d->current) + handler_count(d->new);
            if(count == 0) {
                logger_log(d->logger, LOG_INFO, "no more handlers to process");
            }
            return count;
        }
    }

    // create file descriptor watch sets
    fd_set readfds, writefds, exceptfds;
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);
    int max_fd = 0;

    for(Handler *h = d->current; h; h = h->next) {
        // only real fds
        if(h->fd >= 0) {
            // we always want to know about exceptions or out-of-band events
            FD_SET(h->fd, &exceptfds);

            if(h->type == INTEREST_READ) {
                FD_SET(h->fd, &readfds);
            } else if(h->type == INTEREST_WRITE) {
                FD_SET(h->fd, &writefds);
            } else if(h->type == INTEREST_TIMEOUT) {
                ;
            } else {
                assert(0);
            }

            if(h->fd > max_fd)
                max_fd = h->fd;
        }
    }


    // workout the select() timeout - default of 10 seconds
    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    // adjust select() timeout down for next possible handler that could timeout
    {
        struct timeval now;

        if(gettimeofday(&now, NULL) < 0) {
            logger_log(d->logger, LOG_FATAL, "gettimeofday() failed '%s'", strerror(errno));
            exit(EXIT_FAILURE);
        }

        for(Handler *h = d->current; h; h = h->next) {
            if(h->timeout > 0) {
                struct timeval remaining;
                timersub(&h->timeout_time, &now, &remaining);
                if(timercmp(&remaining, &timeout, <)) {
                    timeout = remaining; // struct assign or memcpy()...
                }
            }
        }
    }

    // wait for i/o
    if(select(max_fd + 1, &readfds, &writefds, &exceptfds, &timeout) < 0) {
        if(errno == EINTR) {
            // signal interupt
            // return to allow caller to re-enter
            // this avoids having to interpret/process exceptions rased on exceptfds
            return handler_count(d->current) + handler_count(d->new);
        }
        else {
            logger_log(d->logger, LOG_FATAL, "select() failed '%s'", strerror(errno));
            // TODO attempt cleanup by calling cancel??
            exit(EXIT_FAILURE);
        }
    }
    else {
        logger_log(d->logger, LOG_DEBUG, "returned from select()");

        struct timeval now;

        if(gettimeofday(&now, NULL) < 0) {
            logger_log(d->logger, LOG_FATAL, "gettimeofday() failed '%s'", strerror(errno));
            exit(EXIT_FAILURE);
        }

        // handler lists
        Handler *unchanged = NULL;
        Handler *processed = NULL;
        Handler *current = d->current;
        Handler *next = NULL;

        // clear handler list, and prevent accidental use of list while processing list
        d->current = NULL;

        // process handler list by calling handler callbacks and transfering to new lists
        for(Handler *h = current; h; h = next) {

            next = h->next;

            // logic is clearer with explicit h->fd >= 0 testing in if/else chain..

            if(h->fd >= 0 && FD_ISSET(h->fd, &exceptfds)) {
                // exceptions get priority
                logger_log(d->logger, LOG_INFO, "fd %d exception", h->fd);
                Event e;
                init_event_from_handler(d, EXCEPTION, h, &e);
                h->callback(h->context, &e);
                h->next = processed;
                processed = h;
            }
            else if(h->fd >= 0 && h->type == INTEREST_READ && FD_ISSET(h->fd, &readfds)) {
                logger_log(d->logger, LOG_DEBUG, "fd %d is ready for reading", h->fd);
                Event e;
                init_event_from_handler(d, READ_READY, h, &e);
                h->callback(h->context, &e);
                h->next = processed;
                processed = h;
            }
            else if(h->fd >= 0 && h->type == INTEREST_WRITE && FD_ISSET(h->fd, &writefds)) {
                logger_log(d->logger, LOG_DEBUG, "fd %d is ready for writing", h->fd);
                Event e;
                init_event_from_handler(d, WRITE_READY, h, &e);
                h->callback(h->context, &e);
                h->next = processed;
                processed = h;
            }
            else if(h->timeout >= 0 && timercmp(&now, &h->timeout_time, >=)) {
                // specify timeout, either fd or non-fd
                logger_log(d->logger, LOG_DEBUG, "fd %d timed out", h->fd);
                Event e;
                init_event_from_handler(d, TIMEOUT, h, &e);
                h->callback(h->context, &e);
                h->next = processed;
                processed = h;
            }
            else {
                // no changes - transfer handler to un-processed list
                h->next = unchanged;
                unchanged = h;
            }
        }

        // TODO could log the list of fds for each list...
        logger_log(d->logger, LOG_DEBUG, "processed: %d", handler_count(processed));
        logger_log(d->logger, LOG_DEBUG, "unchanged: %d", handler_count(unchanged));

        assert(d->current == NULL);

        // free processed handler mem
        for(Handler *h = processed; h; h = next) {
            next = h->next;
            memset(h, 0, sizeof(Handler));
            free(h);
        }

        d->current = unchanged;

        // do we have more handlers to process?
        int count = handler_count(d->current) + handler_count(d->new);
        if(count == 0) {
            logger_log(d->logger, LOG_INFO, "no more handlers to process");
        }

        return count;
    }
    assert(0);
}

