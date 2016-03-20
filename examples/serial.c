
/*
    # ESP8266
    # Can use rlwrap for history!
    rlwrap ./examples/serial.out

    There's an issue, with device echoing the input.

    Ok, it's resetting when we connect - need to test ioctl get
        - and not reset unless need.
        -  or else require a de-bounce cap or something
        - or we need to set hardware handshake off initially...

    done - There's a problem that after use esptool, then baud is set wrong

    - Note that in case stdin is associated  with  a  terminal,
    This kernel input handling can be modified using calls like tcsetattr(3

    - Use signal handler to close.
    - maybe ctrl-z to reset?

    refs
        http://stackoverflow.com/questions/4968529/how-to-set-baud-rate-to-307200-on-linux
        https://en.wikibooks.org/wiki/Serial_Programming/termios
        http://www.tldp.org/HOWTO/text/Serial-Programming-HOWTO

        http://unix.stackexchange.com/questions/117037/how-to-send-data-to-a-serial-port-and-see-any-answer

    netcat devcat /device cat...

    maybe sending too much data? or need to convert CR to LF
*/

#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <stdbool.h> // c99

#include <reactor.h>

typedef struct Context Context;
struct Context
{
    Reactor *reactor;
    int     device_fd;

};

/*
void on_wrote_device(Context *context, Event *e)
{
    fprintf(stdout, "on wrote device?\n");
}
*/

static void on_read_device(Context *context, Event *e)
{
    assert(context->device_fd == e->fd);

    switch(e->type) {
        case READ_READY: {
            char buf[1000];
            int n = read(e->fd, &buf, 1000);
            //fprintf(stdout, "Got %d chars from device\n", n);
            if( n > 0)  {
                write(1, &buf, n); // buffer - or bind a handler to know this won't block?????
                reactor_on_read_ready( context->reactor, e->fd, -1, context, (void *)on_read_device);
                // reactor_on_write_ready(e->reactor, e->fd, -1, context, (void *)on_wrote_device);
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


static void on_read_stdin(Context *context, Event *e)
{
    switch(e->type) {
        case READ_READY: {
            // not sure whether to do this here, or elsewhere...
            // also eof for disconnect if socket, but not always - eg. fill
            char buf[1000];
            int n = read(e->fd, &buf, 1000);
            // fprintf(stdout, "Got %d chars from stdin\n", n);
            if(n > 0) {
                write(context->device_fd, &buf, n);
                reactor_on_read_ready(context->reactor, e->fd, -1, context, (void *)on_read_stdin);
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


static void set_modem_pin(int device_fd, int flag, bool pin)
{
    int argp;
    ioctl(device_fd, TIOCMGET, &argp);
    if(ioctl(device_fd, TIOCMGET, &argp) < 0) {
        // fatal...
        assert(0);
    } else {
        if(pin) {
            // pull high, led on, clear dtr bit
            argp &= ~ flag;
            if(ioctl(device_fd, TIOCMSET, &argp) < 0) {
                assert(0);
            }
        }
        else {
            // pull low, led off, set dtr bit
            argp |= flag ;
            if(ioctl(device_fd, TIOCMSET, &argp) < 0) {
                assert(0);
            }
        }
    }
}

/*
static void on_timeout_1(Context *context, Event *e)
{
    // blink rts and dtr
    static bool led_state = 0;  // move to context
    led_state = !led_state;
    // fprintf(stdout, "led state %d\n", led_state);
    set_modem_pin(context->device_fd, TIOCM_DTR, led_state);
    set_modem_pin(context->device_fd, TIOCM_RTS, !led_state);
    reactor_on_timer(e->reactor, 500, context, (void *)on_timeout_1);
}
*/


int main()
{
    Context context;
    memset(&context, 0, sizeof(Context));

    context.reactor = reactor_create();

    const char *filename = "/dev/ttyUSB0";
    context.device_fd = open(filename, O_RDWR | O_NONBLOCK);
    if(context.device_fd <= 0) {
        fprintf(stdout, "error opening %s\n", filename);
        assert(0);
    }

    // get data
    struct termios options;
    tcgetattr(context.device_fd, &options);

    // set speed
    cfsetispeed(&options, B9600);
    cfsetospeed(&options, B9600);

    // echo off, echo newline off, canonical mode off,
    // extended input processing off, signal chars off
    options.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);
    
    // raw output
    // options.c_oflag = 0;

    // caconical - disable all echo, l- 
    // options.c_lflag = ICANON ;

    tcflush(context.device_fd, TCIFLUSH);   
    tcsetattr(context.device_fd, TCSANOW, &options);

    set_modem_pin(context.device_fd, TIOCM_DTR, true); // gpio0, high = normal boot, low flash
    set_modem_pin(context.device_fd, TIOCM_RTS, true); // reset, high don't reset

    if(true) {
        reactor_on_read_ready(context.reactor, context.device_fd, -1, &context, (void *)on_read_device);
        reactor_on_read_ready(context.reactor, 0, -1, &context, (void *)on_read_stdin);
    }
    // reactor_on_timer(d, 500, &context, (void *)on_timeout_1);

    // reactor_cancel_all(d);
    reactor_run(context.reactor);
    reactor_destroy(context.reactor);

    return 0;
}

