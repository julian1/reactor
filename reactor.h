
typedef struct Reactor Reactor;

typedef enum {
    READ_READY,
    WRITE_READY,
    EXCEPTION,
    TIMEOUT,
    CANCELLED,
    // SIGNAL...
} Reactor_event_type;


typedef struct Event Event;

typedef void (*Reactor_callback)(void *context, Event *);

typedef struct Handler Handler;

struct Event
{
    Reactor_event_type type;
    Reactor     *reactor;

    Handler     *handler;   // opaque to caller space, but enables rebinding...

    // this mostly duplicates  handler, but makes it easier for end user ..
    int         fd;         // for socket,stdin,file,device etc
    int         timeout;    // associated with handler, not just timer, can make rebinding easier
    int         signal;     // for signals
/*
    // support for rebinding...
    void        *context;
    Reactor_callback callback; 
*/
};


// TODO better prefixes for enum values...

// ALL < DEBUG < INFO < WARN < ERROR < FATAL < OFF.

typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_FATAL,
    LOG_NONE
} Reactor_log_level;



// setup and run
Reactor * reactor_create();

Reactor *reactor_create_with_log_level(Reactor_log_level level);

void reactor_destroy(Reactor *);

int reactor_run_once(Reactor *d);

void reactor_run(Reactor *d);

void reactor_cancel_all(Reactor *d);

void reactor_log(Reactor *d, Reactor_log_level level, const char *format, ...);


typedef struct Handler Handler;

void reactor_cancel_handler(Handler *h);

// events - should return the handler - to enable easy cancel?
// 
void reactor_on_timer(Reactor *d, int timeout, void *context, Reactor_callback callback);

void reactor_on_read_ready( Reactor *d, int fd, int timeout, void *context, Reactor_callback callback);

void reactor_on_write_ready(Reactor *d, int fd, int timeout, void *context, Reactor_callback callback);

// void reactor_rebind_handler(Event *e);

// signal stuff
void reactor_register_signal(Reactor *d, int signal);

void reactor_deregister_signal(Reactor *d, int signal);

void reactor_on_signal(Reactor *d, int timeout, void *context, Reactor_callback callback);


