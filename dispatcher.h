
typedef struct Dispatcher Dispatcher;



// change name Dispatcher_event_type

typedef enum {
    OK,
    EXCEPTION,
    TIMEOUT,
    CANCELLED,
} Dispatcher_event_type;


typedef struct Event Event;
struct Event
{
    Dispatcher_event_type   type;
    Dispatcher  *dispatcher;
    int         fd;
    // void        *user_state;
};

// TODO better prefixes...

typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_NONE
} Dispatcher_log_level;


// ugghhh we have to expose it... to create it...

typedef void (*Dispatcher_callback)(void *context, Event *);

Dispatcher * dispatcher_create();

Dispatcher *dispatcher_create_with_log_level(Dispatcher_log_level level);

void dispatcher_destroy(Dispatcher *);

void dispatcher_log(Dispatcher *d, Dispatcher_log_level level, const char *format, ...);

void dispatcher_on_timer(Dispatcher *d, int timeout, void *context, Dispatcher_callback callback);

// TODO - do we want timeouts for signals?
void dispatcher_on_signal(Dispatcher *d, /*int timeout,*/ void *context, Dispatcher_callback callback);

void dispatcher_on_read_ready( Dispatcher *d, int fd, int timeout, void *context, Dispatcher_callback callback);

void dispatcher_on_write_ready(Dispatcher *d, int fd, int timeout, void *context, Dispatcher_callback callback);

int dispatcher_dispatch(Dispatcher *d);

void dispatcher_cancel_all(Dispatcher *d);


