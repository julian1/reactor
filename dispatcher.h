
typedef struct Dispatcher Dispatcher;


// change to D_OK etc or DEMUX_OK, DMUX_EXCEPTION etc or use enum? .

typedef enum {
    OK,
    EXCEPTION,
    TIMEOUT,
    CANCELLED,
} Event_type;


typedef struct Event Event;
struct Event
{
    Event_type   type;
    Dispatcher  *dispatcher;
    int         fd;
    // void        *user_state;
};

/*
#define OK          1
#define EXCEPTION   2
#define TIMEOUT     3
#define CANCELLED   4
*/

// ugghhh we have to expose it... to create it...

typedef void (*pcallback)(void *context, Event *);

Dispatcher * dispatcher_create();

void dispatcher_destroy(Dispatcher *);

// move this outside ...
void dispatcher_on_timer(Dispatcher *d, int timeout, void *context, pcallback callback);

void dispatcher_on_signal(Dispatcher *d, /*int timeout,*/ void *context, pcallback callback);

void dispatcher_on_read_ready( Dispatcher *d, int fd, int timeout, void *context, pcallback callback);

void dispatcher_on_write_ready(Dispatcher *d, int fd, int timeout, void *context, pcallback callback);

int dispatcher_dispatch(Dispatcher *d);

void dispatcher_cancel_all(Dispatcher *d);

