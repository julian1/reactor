
typedef struct Dispatcher Dispatcher;

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
    int         fd;       // if socket,stdin,file,device etc
    int         signal;   // if signal
};

// TODO better prefixes for enum values...

// ALL < DEBUG < INFO < WARN < ERROR < FATAL < OFF.

typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_FATAL,
    LOG_NONE
} Dispatcher_log_level;


typedef void (*Dispatcher_callback)(void *context, Event *);



// setup and run
Dispatcher * dispatcher_create();

Dispatcher *dispatcher_create_with_log_level(Dispatcher_log_level level);

void dispatcher_destroy(Dispatcher *);

int dispatcher_run_once(Dispatcher *d);

void dispatcher_run(Dispatcher *d);

void dispatcher_cancel_all(Dispatcher *d);

void dispatcher_log(Dispatcher *d, Dispatcher_log_level level, const char *format, ...);

// events
void dispatcher_on_timer(Dispatcher *d, int timeout, void *context, Dispatcher_callback callback);

void dispatcher_on_read_ready( Dispatcher *d, int fd, int timeout, void *context, Dispatcher_callback callback);

void dispatcher_on_write_ready(Dispatcher *d, int fd, int timeout, void *context, Dispatcher_callback callback);

// signal stuff
void dispatcher_register_signal(Dispatcher *d, int signal);

void dispatcher_on_signal(Dispatcher *d, int timeout, void *context, Dispatcher_callback callback);

