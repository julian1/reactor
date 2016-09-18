/*
    Example showing use of uart/serial port to communicate with esp8266 computer

    Notes, 
      To fix echo (need to go in init.lua to persist over reboots)
      uart.setup( 0, 9600, 8, 0, 1, 0)
      // uart.setup(id, baud, databits, parity, stopbits, echo )

      Arduino, good defaults
      stty -F /dev/ttyUSB0 cs8 9600 ignbrk -brkint -icrnl -imaxbel -opost -onlcr -isig -icanon -iexten -echo -echoe -echok -echoctl -echoke noflsh -ixon -crtscts

      # Can use rlwrap for history!
      rlwrap ./examples/serial.out

      Ok, it's resetting when we connect - need to test ioctl get
          - else require a de-bounce cap or something

      TODO
      - Use signal handler to close.
      - maybe ctrl-z to reset?
      - rename devcat /device cat...

      - done - There's a problem that after use esptool, then baud is set wrong

      Refs
          http://stackoverflow.com/questions/4968529/how-to-set-baud-rate-to-307200-on-linux
          https://en.wikibooks.org/wiki/Serial_Programming/termios
          http://www.tldp.org/HOWTO/text/Serial-Programming-HOWTO
          http://unix.stackexchange.com/questions/117037/how-to-send-data-to-a-serial-port-and-see-any-answer
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

    // usleep(50000); // reveals that reset is a race

    // ok, it looks like when we do a read, actually sets
    // some hard defaults!. So all we need to do is not do a read
    // before setting
    // no it's still a race condition - but can improve with a cap
    int serial = 0;
    /* if(ioctl(context.device_fd, TIOCMGET, &serial) < 0) {
        assert(0);
    } */
    serial = serial & ~TIOCM_DTR & ~TIOCM_RTS;
    if(ioctl(context.device_fd, TIOCMSET, &serial) < 0) {
        assert(0);
    }

    struct termios options;

    // reset config!
    memset(&options, 0, sizeof(struct termios));

    // get existing config
    // tcgetattr(context.device_fd, &options);

    // set speed
    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);

    // echo off, echo newline off, canonical mode off,
    // extended input processing off, signal chars off
    // options.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);

    // raw output
    // cfmakeraw(&options);
    // options.c_oflag = 0;

    tcflush(context.device_fd, TCIFLUSH);
    tcsetattr(context.device_fd, TCSANOW, &options);

    reactor_on_read_ready(context.reactor, context.device_fd, -1, &context, (void *)on_read_device);
    reactor_on_read_ready(context.reactor, 0, -1, &context, (void *)on_read_stdin);
    // reactor_on_timer(d, 500, &context, (void *)on_timeout_1);

    // reactor_cancel_all(d);
    reactor_run(context.reactor);
    reactor_destroy(context.reactor);

    return 0;
}

