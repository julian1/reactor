
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


// change name Dispatcher_log_level

typedef enum {
    LOG_DEBUG,
    LOG_INFO
} Dispatcher_log_level;



/*
#define OK          1
#define EXCEPTION   2
#define TIMEOUT     3
#define CANCELLED   4
*/

// ugghhh we have to expose it... to create it...

typedef void (*Dispatcher_callback)(void *context, Event *);

Dispatcher * dispatcher_create();

void dispatcher_destroy(Dispatcher *);

void dispatcher_log(Dispatcher *d, Dispatcher_log_level level, const char *format, ...);

void dispatcher_on_timer(Dispatcher *d, int timeout, void *context, Dispatcher_callback callback);

// TODO - do we want timeouts for signals?
void dispatcher_on_signal(Dispatcher *d, /*int timeout,*/ void *context, Dispatcher_callback callback);

void dispatcher_on_read_ready( Dispatcher *d, int fd, int timeout, void *context, Dispatcher_callback callback);

void dispatcher_on_write_ready(Dispatcher *d, int fd, int timeout, void *context, Dispatcher_callback callback);

int dispatcher_dispatch(Dispatcher *d);

void dispatcher_cancel_all(Dispatcher *d);

