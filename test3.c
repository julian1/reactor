
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <termios.h>

#include <dispatcher.h>

typedef struct Context Context;
struct Context
{
    int device_fd;

};

/*
void on_wrote_device(Context *context, Event *e)
{
    fprintf(stdout, "on wrote device?\n");
}
*/

void on_read_device(Context *context, Event *e)
{
    assert(context->device_fd == e->fd);

    switch(e->type) {
        case OK: {
            char buf[1000];
            int n = read(e->fd, &buf, 1000);
            // fprintf(stdout, "Got %d chars from device\n", n);
            if( n > 0)  {
                write(1, &buf, n); // buffer - or bind a handler to know this won't block?????
                dispatcher_on_read_ready( e->dispatcher, e->fd, -1, context, (void *)on_read_device);
                // dispatcher_on_write_ready(e->dispatcher, e->fd, -1, context, (void *)on_wrote_device);
            } else {
                // finish up
                fprintf(stdout, "device closed?");
                close(e->fd);
            }
            break;
        }
        // anything else
        default:
            fprintf(stdout, "event unknown");
            close(e->fd);
            break;
    }
}


void on_read_stdin(Context *context, Event *e)
{
    switch(e->type) {
        case OK: {
            // not sure whether to do this here, or elsewhere...
            // also eof for disconnect if socket, but not always - eg. fill
            char buf[1000];
            int n = read(e->fd, &buf, 1000);
            // fprintf(stdout, "Got %d chars\n", n);
            if(n > 0) {
                write(context->device_fd, &buf, n);
                dispatcher_on_read_ready(e->dispatcher, e->fd, -1, context, (void *)on_read_stdin);
            }
            break;
        }
        // anything else
        default:
            fprintf(stdout, "event unknown on fd %d", e->fd);
            break;
    }
}

// so stdin appears to be line buffered it would be nice to change
// also handle signal...
// it's buffering behavior not the blocking behavior


/*
  Note that in case stdin is associated  with  a  terminal,
  This kernel input handling can be modified using calls like tcsetattr(3
*/
int main()
{
    Context context;

    const char *filename = "/dev/ttyUSB0";
    context.device_fd = open(filename, O_RDWR | O_NONBLOCK);
    if(context.device_fd <= 0) {
        fprintf(stdout, "error opening %s\n", filename);
        assert(0);
    }
    // get settings..
    struct termios argp;
    ioctl(context.device_fd, TCGETS, &argp);

    fprintf(stdout, "ispeed %d", argp.c_ispeed );
    fprintf(stdout, "ospeed %d", argp.c_ospeed );
//    speed_t c_ospeed;

    Dispatcher *d = dispatcher_create();
    dispatcher_on_read_ready(d, context.device_fd, -1, &context, (void *)on_read_device);
    dispatcher_on_read_ready(d, 0, -1, &context, (void *)on_read_stdin);
    // dispatcher_cancel_all(d);
    while(dispatcher_dispatch(d));
    dispatcher_destroy(d);
    return 0;
/**/
}

