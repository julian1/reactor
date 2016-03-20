
#include <signal_.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>

/*
    Signal/interupt handling
    strategy is to push interupt details onto a unnamed fifo pipe, and then
    re-read again in the desired handler/thread context
*/

// Uggh, must be static as neither sa_handler or sa_sigaction support contexts
// Current limitation that can only have one reactor
static int signal_fifo_writefd;


struct Signal
{
    Logger      *logger;
    Reactor     *reactor;
    int         signal_fifo_readfd;
};


Signal *signal_create(Logger *logger, Reactor *reactor)
{
    Signal *s = (void *)malloc(sizeof(Signal));
    memset(s, 0, sizeof(Signal));

    s->logger = logger;
    s->reactor = reactor;
    
    // set up signal fifo
    int fd[2];
    if(pipe(fd) < 0) {
        logger_log(s->logger, LOG_FATAL, "pipe() failed '%s'", strerror(errno));
        exit(EXIT_FAILURE);
    }
    s->signal_fifo_readfd = fd[0];
    signal_fifo_writefd = fd[1];

    return s;
}

void signal_destroy(Signal *s)
{
    close(s->signal_fifo_readfd);
    close(signal_fifo_writefd);

    memset(s, 0, sizeof(Signal));
    free(s);
}

/*
typedef struct Context Context;
struct Context
{
    int signal;
};
*/

// change name to just context

// uggh
typedef struct SignalContext SignalContext;
struct SignalContext
{
    Signal *signal;
    // int signal;

    void *context;
    Signal_callback callback;
};



static void signal_sigaction(int signal, siginfo_t *siginfo, ucontext_t *ucontext)
{
    // fprintf(stdout, "my_sa_sigaction -> got signal %d from pid %d\n", signal, siginfo->si_pid);
    // Context s;
    //s.signal = signal;
    write(signal_fifo_writefd, siginfo, sizeof(siginfo_t));
}


void signal_register_signal( Signal *d, int signal )
{
    struct sigaction act;
    memset(&act, 0, sizeof(sigaction));

    // Use sa_sigaction over sa_handler for additional detail
    // act.sa_handler = my_sa_handler;
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = (void (*)(int, siginfo_t *, void *))signal_sigaction;

    int ret = sigaction(signal, &act, NULL);
    if(ret != 0) {
        logger_log(d->logger, LOG_FATAL, "sigaction() failed '%s'", strerror(errno));
        exit(EXIT_FAILURE);
    }
}


void signal_deregister_signal( Signal *d, int signal )
{
    // TODO
    assert(0);
}


static void signal_on_signal_fifo_read_ready(SignalContext *sc, Event *e)
{
    SignalEvent signal_event;
    memset(&signal_event, 0, sizeof(SignalEvent));

    switch(e->type) {
        case READ_READY: {
            // Context s;
           // siginfo_t s;

            if(read(e->fd, &signal_event.siginfo, sizeof(siginfo_t)) == sizeof(siginfo_t))  {
                // fprintf(stdout, "got signal %d\n", s.signal);
                // call the inner function
                // fill in the signal number for event and call the callback
                // e->signal = s.signal;

                signal_event.type = SIGNAL_SIGNAL;
                (sc->callback)(sc->context, &signal_event);
            } else {
                // something went wrong...
                assert(0);
            }
            break;
        }
        case EXCEPTION:
            // handle locally, instead of passing along...
            logger_log(sc->signal->logger, LOG_FATAL, "exception on signal fifo '%s'", strerror(errno));
            exit(EXIT_FAILURE);
            break;
        case TIMEOUT:
            signal_event.type = SIGNAL_TIMEOUT;
            (sc->callback)(sc->context, &signal_event);
            break;
        case CANCELLED:
            signal_event.type = SIGNAL_CANCELLED;
            (sc->callback)(sc->context, &signal_event);
            break;
        case WRITE_READY:
            assert(0);
    }

    memset(sc, 0, sizeof(SignalContext));
    free(sc);
}


void signal_on_signal(Signal *d, int timeout, void *context, Signal_callback callback)
{
    SignalContext *sc = malloc(sizeof(SignalContext));
    memset(sc, 0, sizeof(SignalContext));
    // sc->signal = signal;
    sc->context = context;
    sc->callback = callback;
    reactor_on_read_ready( d->reactor, d->signal_fifo_readfd, timeout, sc,    
        (void (*)(void *, Event *))signal_on_signal_fifo_read_ready);
}

