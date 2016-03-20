
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


#include <logger.h>
#include <reactor.h>

typedef enum {
    INTEREST_READ,
    INTEREST_WRITE,
    // INTEREST_RW,
    INTEREST_TIMEOUT // none...
} Reactor_interest_type;




typedef struct Handler Handler;
struct Handler
{
    Handler     *next;

    Reactor_interest_type type;

    int         fd;
    int         timeout;    // in millisecs

    struct timeval start_time;
    struct timeval timeout_time;

    bool        cancelled;

  // user data
    void        *context;
    void        (*callback)(void *context, Event *);
};


typedef struct Reactor Reactor;
struct Reactor
{
    // FILE        *logout;
    // Reactor_log_level log_level;

    Logger      *logger;

    Handler     *current;
    Handler     *new;

    bool        cancel_all_handlers;

    int         signal_fifo_readfd;
};



// uggh shouldn't go here
static void init_event_from_handler(Reactor *d, Reactor_event_type type, Handler *h, Event *e)
{
    memset(e, 0, sizeof(Event));
    e->reactor = d;
    e->type = type;
    e->handler = h;

    // copy some values out of the handler as convenience to user
    e->fd = h->fd;         
    e->timeout = h->timeout; 
    // int         signal;     // should be in signal user-context?
}


// Uggh, must be static as neither sa_handler or sa_sigaction support contexts
// Current limitation that can only have one reactor
static int signal_fifo_writefd;


Reactor *reactor_create(Logger *logger)
{
    Reactor *d = malloc(sizeof(Reactor));
    memset(d, 0, sizeof(Reactor));
    // d->logout = stdout;
    // d->log_level = level;
    d->logger = logger;
    logger_log(d->logger, LOG_INFO, "create");

    // set up signal fifo
    int fd[2];
    if(pipe(fd) < 0) {
        logger_log(d->logger, LOG_FATAL, "pipe() failed '%s'", strerror(errno));
        exit(EXIT_FAILURE);
    }
    d->signal_fifo_readfd = fd[0];
    signal_fifo_writefd = fd[1];

    return d;
}





/*
Reactor *reactor_create()
{
    return reactor_create_with_log_level(LOG_INFO);
}
*/


void reactor_destroy(Reactor *d)
{
    // IMPORTANT - does not destroy the logger!!!!

    logger_log(d->logger, LOG_INFO, "destroy");

    if(d->current) {
        logger_log(d->logger, LOG_FATAL, "destroy() called with unprocessed handlers");
        exit(EXIT_FAILURE);
    }

    close(d->signal_fifo_readfd);
    close(signal_fifo_writefd);

    memset(d, 0, sizeof(Reactor));
    free(d);
}

/*
void logger_log(Reactor *d, Reactor_log_level level, const char *format, ...)
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


static Handler *reactor_create_handler(Reactor *d, Reactor_interest_type type, int fd, int timeout, void *context, Reactor_callback callback)
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


void reactor_on_timer(Reactor *d, int timeout, void *context, Reactor_callback callback)
{
    reactor_create_handler(d, INTEREST_TIMEOUT, -1234, timeout, context, callback);
}


void reactor_on_read_ready(Reactor *d, int fd, int timeout, void *context, Reactor_callback callback)
{
    reactor_create_handler(d, INTEREST_READ, fd, timeout, context, callback);
}


void reactor_on_write_ready(Reactor *d, int fd, int timeout, void *context, Reactor_callback callback)
{
    reactor_create_handler(d, INTEREST_WRITE, fd, timeout, context, callback);
}

/*
void reactor_rebind_handler(Event *e)
{
    // doesn't work with signals... because of inner context?
    assert(e->reactor);
    Handler *h = e->handler;
    assert(h);
    reactor_create_handler(e->reactor, h->type, h->fd, h->timeout, h->context, h->callback);
}
*/

static int handler_count(Handler *l)
{
    int n = 0;
    for(Handler *h = l; h; h = h->next)
        ++n;

    return n;
}


void reactor_cancel_all(Reactor *d)
{
    logger_log(d->logger, LOG_INFO, "cancel_all()");
    d->cancel_all_handlers = true;
}

// IMPORTANT - THE ONLY PLACE WE TRAVERSE THE LISTS SHOULD BE run_once()
// might want some magic number checks... to check memory...


void reactor_run(Reactor *d)
{
    while(reactor_run_once(d));
}


int reactor_run_once(Reactor *d)
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

/////////////////////////

/*
    Signal/interupt handling
    strategy is to push interupt details onto a unnamed fifo pipe, and then
    read again in the desired handler/thread context
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
        logger_log(d->logger, LOG_FATAL, "sigaction() failed '%s'", strerror(errno));
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
        case READ_READY: {
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
            logger_log(e->reactor->logger, LOG_FATAL, "exception on signal fifo '%s'", strerror(errno));
            exit(EXIT_FAILURE);
            break;
        case TIMEOUT:
        case CANCELLED:
            (sc->callback)(sc->context, e);
            break;
        case WRITE_READY:
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

