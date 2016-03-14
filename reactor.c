
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

//#include <linux/stat.h>
#include <signal.h>

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
    int         timeout;    // secs
    int         start_time; // secs
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
        reactor_log(d, LOG_FATAL, "could not create fifo pipe %s", strerror(errno));
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
        reactor_log(
            d, 
            LOG_FATAL, 
            "destroy() called with unprocessed handlers - use cancel_all() instead"
        );
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


static Handler * reactor_create_handler(Reactor *d, int fd, int timeout, void *context)
{
    Handler *h = (Handler *)malloc(sizeof(Handler));
    memset(h, 0, sizeof(Handler));
    h->fd = fd;
    // we don't really need to record both, but s
    h->timeout = timeout;
    h->start_time = time(NULL);
    h->context = context;
    // tack onto the handler list
    h->next = d->current;
    d->current = h;

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


void reactor_cancel_all(Reactor *d)
{
    reactor_log(d, LOG_INFO, "cancel all");

    // call handlers with cancelled action
    for(Handler *h = d->current; h; h = h->next) {
        Event e;
        event_init(&e);
        e.reactor = d;
        e.timeout = h->timeout;
        e.fd = h->fd;
        e.type = CANCELLED;

        if(h->read_callback)
          h->read_callback(h->context, &e);
        else if(h->write_callback)
          h->write_callback(h->context, &e);
        else
          assert(0);
    }

    // cleanup - doesn't really need separate different loop...
    Handler *next = NULL;
    for(Handler *h = d->current; h; h = next) {
        next = h->next;
        memset(h, 0, sizeof(Handler));
        free(h);
    }

    d->current = NULL;
}


void reactor_run(Reactor *d)
{
    while(reactor_run_once(d));
}


int reactor_run_once(Reactor *d)
{
    reactor_log(d, LOG_DEBUG, "current handlers: %d\n", handler_count(d->current));

    // r, w, e
    fd_set readfds, writefds, exceptfds;
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);

    int max_fd = 0;

    // load select() watch sets
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

    // issue if fd appeareadfds in two sets at the same time?...

    // TODO could compute iterate the handler list and compute the smallexceptfdst expected
    // timeout value, and use it for the next select timeout

    struct timeval timeout;
    timeout.tv_sec = 0; // 60
    timeout.tv_usec = 300 * 1000;  // 300ms

    if(select(max_fd + 1, &readfds, &writefds, &exceptfds, &timeout) < 0) {
        if(errno == EINTR) {
            // signal interupt 
            // simply return to allow caller to rebind
            // avoids needing to interpret/process exceptions rased in exceptfds
            return handler_count(d->current);
        } 
        else {
            reactor_log(d, LOG_FATAL, "fatal select() error %s", strerror(errno));
            // TODO attempt cleanup by calling cancel??
            exit(EXIT_FAILURE);
        }
    }
    else {

        int now = time(NULL);
        // 0 (timeout) or more resource affected
        // a resource is ready
        // reactor_log(d, "number or rexceptfdsourcexceptfds ready =%d\n", ret);

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
            e.timeout = h->timeout;
            e.reactor = d;
            e.fd = h->fd;

            if(h->fd >= 0 && FD_ISSET(h->fd, &exceptfds)) {

                // texceptfdst exception conditions fireadfdst
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
            else if(h->timeout > 0 && now >= h->start_time + h->timeout) {

                reactor_log(d, LOG_DEBUG, "fd %d timed out", h->fd);
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
        reactor_log(d, LOG_DEBUG, "processed: %d", handler_count(processed));
        reactor_log(d, LOG_DEBUG, "unchanged: %d", handler_count(unchanged));
        reactor_log(d, LOG_DEBUG, "new: %d", handler_count(d->current));

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
        int count = handler_count(d->current);
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
        reactor_log(d, LOG_FATAL, "sigaction() failed %s", strerror(errno));
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
            reactor_log(
                e->reactor, 
                LOG_FATAL, 
                "exception on signal fifo, error %s", 
                strerror(errno)
            );
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

