
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

#include <sys/time.h>


//#include <linux/stat.h>
#include <signal.h>

#include <stdbool.h> // c99

#include <reactor.h>


static void event_init(Event *e)
{
    memset(e, 0, sizeof(Event));
}


typedef struct Handler Handler;
struct Handler
{
    Handler     *next;
    int         fd;
    int         timeout;    // in millisecs

    struct timeval start_time;
    struct timeval timeout_time;

    bool        cancelled;

    void        *context;
    void        (*read_callback)(void *context, Event *);
    void        (*write_callback)(void *context, Event *);
};


typedef struct Reactor Reactor;
struct Reactor
{
    FILE        *logout;
    Reactor_log_level log_level;
    Handler     *current;
    Handler     *new;

    bool        cancel_all_handlers;

    int         signal_fifo_readfd;
};



// Uggh, must be static as neither sa_handler or sa_sigaction support contexts
// Current limitation that can only have one reactor
static int signal_fifo_writefd;


Reactor *reactor_create_with_log_level(/* FILE *logout, */ Reactor_log_level level)
{
    Reactor *d = malloc(sizeof(Reactor));
    memset(d, 0, sizeof(Reactor));
    d->logout = stdout;
    d->log_level = level;
    reactor_log(d, LOG_INFO, "create");

    // set up signal fifo
    int fd[2];
    if(pipe(fd) < 0) {
        reactor_log(d, LOG_FATAL, "pipe() failed '%s'", strerror(errno));
        exit(EXIT_FAILURE);
    }
    d->signal_fifo_readfd = fd[0];
    signal_fifo_writefd = fd[1];

    return d;
}


Reactor *reactor_create()
{
    return reactor_create_with_log_level(LOG_INFO);
}


void reactor_destroy(Reactor *d)
{
    reactor_log(d, LOG_INFO, "destroy");

    if(d->current) {
        reactor_log(d, LOG_FATAL, "destroy() called with unprocessed handlers");
        exit(EXIT_FAILURE);
    }

    close(d->signal_fifo_readfd);
    close(signal_fifo_writefd);

    memset(d, 0, sizeof(Reactor));
    free(d);
}


void reactor_log(Reactor *d, Reactor_log_level level, const char *format, ...)
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


static Handler *reactor_create_handler(Reactor *d, int fd, int timeout, void *context)
{
    Handler *h = (Handler *)malloc(sizeof(Handler));
    memset(h, 0, sizeof(Handler));
    h->fd = fd;
    // we don't really need to record both, but s
    h->timeout = timeout;

    if(gettimeofday(&h->start_time, NULL) < 0) {
        reactor_log(d, LOG_FATAL, "gettimeofday() failed '%s'", strerror(errno));
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

    // tack onto the new handler list
    h->next = d->new;
    d->new = h;

    return h;
}


void reactor_on_timer(Reactor *d, int timeout, void *context, Reactor_callback callback)
{
    Handler *h = reactor_create_handler( d, -1234, timeout, context);
    // appropriate the read_callback.
    h->read_callback = callback;
}


void reactor_on_read_ready(Reactor *d, int fd, int timeout, void *context, Reactor_callback callback)
{
    Handler *h = reactor_create_handler( d, fd, timeout, context);
    h->read_callback = callback;
}


void reactor_on_write_ready(Reactor *d, int fd, int timeout, void *context, Reactor_callback callback)
{
    Handler *h = reactor_create_handler( d, fd, timeout, context);
    h->write_callback = callback;
}


static int handler_count(Handler *l)
{
    int n = 0;
    for(Handler *h = l; h; h = h->next)
        ++n;

    return n;
}

// might want some magic number checks... to check memory... 

void reactor_cancel_all(Reactor *d)
{
    reactor_log(d, LOG_INFO, "cancel_all() %p\n", d->current);

    d->cancel_all_handlers = true;

    // WHEN WE DO CALLBACKS - there's no chance to access the handler list
    // since we shuffle items (and next pointers) while doing the processing

    // maintain a separate new list is cleaner... and process each time. irrespective
    // of whatever else we do...

    // it's not the processing that's the problem. instead it's the setting of the information 
    // an individual cancelled or something..

}

// THE ONLY PLACE WE TRAVERSE THE LISTS SHOULD BE run_once()


void reactor_run(Reactor *d)
{
    while(reactor_run_once(d));
}


int reactor_run_once(Reactor *d)
{
    reactor_log(d, LOG_DEBUG, "current : %d", handler_count(d->current));
    reactor_log(d, LOG_DEBUG, "new: %d", handler_count(d->current));


    // concatenate new handlers onto the current handler list
    if(d->current) {
        Handler *p = d->current;
        while(p->next)
            p = p->next;
        p->next = d->new;
    } else {
        d->current = d->new;
    }
    d->new = NULL;


    // maybe mark cancelled handlers
    if(d->cancel_all_handlers) {
        for(Handler *h = d->current; h; h = h->next) {
            h->cancelled = true;
        }
        d->cancel_all_handlers = false;
    }
 
    // processed cancelled handlers
    {
        // maintaining separate lists, makes it easier to log, and cleanup, and avoids processing status booleans
        // IMPORTANT Doing cleanup in separate loop is better since
        // callback might manipuate another handler's state - such as setting cancelled flag

        Handler *unchanged = NULL;
        Handler *cancelled = NULL;
        Handler *next = NULL;
        Handler *current = d->current;
        d->current = NULL;

        // we cannot traverse the current list, when calling callbacks inside
        // this traversal of the current list...

        for(Handler *h = current; h; h = next) {

            next = h->next;
            if(h->cancelled) {

                Event e;
                event_init(&e);
                e.reactor = d;
                e.timeout = h->timeout;
                e.fd = h->fd;
                e.type = CANCELLED;

                reactor_log(d, LOG_DEBUG, "fd %d cancelled", h->fd);

                if(h->read_callback)
                  h->read_callback(h->context, &e);
                else if(h->write_callback)
                  h->write_callback(h->context, &e);
                else
                  assert(0);

                h->next = cancelled;
                cancelled = h;
            }
            else {
                h->next = unchanged;
                unchanged = h;
            }
        }

        reactor_log(d, LOG_DEBUG, "cancelled: %d", handler_count(cancelled));
        reactor_log(d, LOG_DEBUG, "unchanged: %d", handler_count(unchanged));

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
                reactor_log(d, LOG_INFO, "no more handlers to process");
            }
            return count;
        }
    }


    // create descriptor watch sets
    fd_set readfds, writefds, exceptfds;
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);
    int max_fd = 0;

    for(Handler *h = d->current; h; h = h->next) {
        // only real fds...
        if(h->fd >= 0) {
            // we always want to know about exceptions
            FD_SET(h->fd, &exceptfds);

            if(h->read_callback) {
                FD_SET(h->fd, &readfds);
            }
            else if(h->write_callback) {
                FD_SET(h->fd, &writefds);
            } else {
                assert(0);
            }

            if(h->fd > max_fd)
                max_fd = h->fd;
        }
    }


    // determine select() timeout
    struct timeval timeout;

    // default of 10 seconds
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    // adjust select() timeout for the next handler that could possibly timeout
    {
        struct timeval now;

        if(gettimeofday(&now, NULL) < 0) {
            reactor_log(d, LOG_FATAL, "gettimeofday() failed '%s'", strerror(errno));
            exit(EXIT_FAILURE);
        }

        for(Handler *h = d->current; h; h = h->next) {
            if(h->timeout > 0) {
                struct timeval remaining;
                timersub(&h->timeout_time, &now, &remaining);
                if(timercmp(&remaining, &timeout, <)) {
                    timeout = remaining; // struct assign or memcpy...
                }
            }
        }
    }

    // wait for i/o
    if(select(max_fd + 1, &readfds, &writefds, &exceptfds, &timeout) < 0) {
        if(errno == EINTR) {
            // signal interupt
            // just return and allow caller to rebind
            // this avoids having to interpret/process exceptions rased on exceptfds
            return handler_count(d->current);
        }
        else {
            reactor_log(d, LOG_FATAL, "select() failed '%s'", strerror(errno));
            // TODO attempt cleanup by calling cancel??
            exit(EXIT_FAILURE);
        }
    }
    else {
        reactor_log(d, LOG_DEBUG, "returned from select()");

        struct timeval now;

        if(gettimeofday(&now, NULL) < 0) {
            reactor_log(d, LOG_FATAL, "gettimeofday() failed '%s'", strerror(errno));
            exit(EXIT_FAILURE);
        }

        // handler lists
        Handler *unchanged = NULL;
        Handler *processed = NULL;
        Handler *current = d->current;
        Handler *next = NULL;

        // clear handler list, to prevent any accidental access to list while processing list
        d->current = NULL;

        // process current list by calling handler callbacks and transfering
        for(Handler *h = current; h; h = next) {

            next = h->next;

            // construct basic event info
            Event e;
            event_init(&e);
            e.timeout = h->timeout;
            e.reactor = d;
            e.fd = h->fd;

            if(h->fd >= 0 && FD_ISSET(h->fd, &exceptfds)) {

                // test exception conditions first
                reactor_log(d, LOG_INFO, "fd %d exception", h->fd);
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
            else if(h->fd >= 0 && h->read_callback && FD_ISSET(h->fd, &readfds)) {

                reactor_log(d, LOG_DEBUG, "fd %d is ready for reading", h->fd);
                e.type = OK;
                h->read_callback(h->context, &e);
                // transfer handler to processed list
                h->next = processed;
                processed = h;
            }
            else if(h->fd >= 0 && h->write_callback && FD_ISSET(h->fd, &writefds)) {

                reactor_log(d, LOG_DEBUG, "fd %d is ready for writing", h->fd);
                e.type = OK;
                h->write_callback(h->context, &e);
                // transfer handler to processed list
                h->next = processed;
                processed = h;
            }
            else if(h->timeout >= 0 && timercmp(&now, &h->timeout_time, >=)) {

                reactor_log(d, LOG_DEBUG, "fd %d timed out", h->fd);
                e.type = TIMEOUT;
                if(h->read_callback)
                  h->read_callback(h->context, &e);
                else if(h->write_callback)
                  h->write_callback(h->context, &e);
                else {
                  assert(0);
                }
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

        // TODO could log the list of fds for each list...
        reactor_log(d, LOG_DEBUG, "processed: %d", handler_count(processed));
        reactor_log(d, LOG_DEBUG, "unchanged: %d", handler_count(unchanged));
        
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
            reactor_log(d, LOG_INFO, "no more handlers to process");
        }

        return count;
    }
    assert(0);
}

/////////////////////////

/*
    Signal/interupt handling
    strategy is to push interupt details onto a unnamed fifo pipe, and then
    read them again in the desired handler/thread context
*/

typedef struct SignalDetail SignalDetail;
struct SignalDetail
{
    int signal;
};


static void reactor_sigaction(int signal, siginfo_t *siginfo, ucontext_t *ucontext)
{
    // fprintf(stdout, "my_sa_sigaction -> got signal %d from pid %d\n", signal, siginfo->si_pid);
    SignalDetail s;
    s.signal = signal;
    write(signal_fifo_writefd, &s, sizeof(SignalDetail));
}


void reactor_register_signal( Reactor *d, int signal )
{
    struct sigaction act;
    memset(&act, 0, sizeof(sigaction));

    // Use sa_sigaction over sa_handler for additional detail
    // act.sa_handler = my_sa_handler;
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = (void (*)(int, siginfo_t *, void *))reactor_sigaction;

    int ret = sigaction(signal, &act, NULL);
    if(ret != 0) {
        reactor_log(d, LOG_FATAL, "sigaction() failed '%s'", strerror(errno));
        exit(EXIT_FAILURE);
    }
}


void reactor_deregister_signal( Reactor *d, int signal )
{
    // TODO
    assert(0);
}


typedef struct SignalContext SignalContext;
struct SignalContext
{
    // int signal;
    void *context;
    Reactor_callback callback;
};


static void reactor_on_signal_fifo_read_ready(SignalContext *sc, Event *e)
{
    switch(e->type) {
        case OK: {
            SignalDetail s;
            if(read(e->fd, &s, sizeof(SignalDetail)) == sizeof(SignalDetail))  {
                // fprintf(stdout, "got signal %d\n", s.signal);
                // call the inner function
                // fill in the signal number for event and call the callback
                e->signal = s.signal;
                (sc->callback)(sc->context, e);
            } else {
                // something went wrong...
                assert(0);
            }
            break;
        }
        case EXCEPTION:
            // handle locally, instead of passing along...
            reactor_log(e->reactor, LOG_FATAL, "exception on signal fifo '%s'", strerror(errno));
            exit(EXIT_FAILURE);
            break;
        case TIMEOUT:
        case CANCELLED:
            (sc->callback)(sc->context, e);
            break;
        default:
            assert(0);
    }

    memset(sc, 0, sizeof(SignalContext));
    free(sc);
}


void reactor_on_signal(Reactor *d, int timeout, void *context, Reactor_callback callback)
{
    SignalContext *sc = malloc(sizeof(SignalContext));
    memset(sc, 0, sizeof(SignalContext));
    // sc->signal = signal;
    sc->context = context;
    sc->callback = callback;
    reactor_on_read_ready(
        d,
        d->signal_fifo_readfd,
        timeout,
        sc,
        (void (*)(void *, Event *))reactor_on_signal_fifo_read_ready
    );
}

