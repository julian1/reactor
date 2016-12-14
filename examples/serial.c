/*
  alternatives
    - miniterm.py
    - screen

  - bus pirate <link>
    ./examples/34970a.out -d /dev/ttyUSB0 -s 115200 -p 8n1

  - esp8266 
    To fix echo (need to go in init.lua to persist over reboots)
    uart.setup( 0, 9600, 8, 0, 1, 0)
    // uart.setup(id, baud, databits, parity, stopbits, echo )

  - cisco switch

  - 34970a - data acquisition

  - arduino
    Arduino, good defaults
    stty -F /dev/ttyUSB0 cs8 9600 ignbrk -brkint -icrnl \
      -imaxbel -opost -onlcr -isig -icanon -iexten -echo \
      -echoe -echok -echoctl -echoke noflsh -ixon -crtscts




  TODO 
    - make the speed and 8n1 a configuration option. to generalize...
    - factor into separate project and use git submodule to build..       
    - capture ctrl-c ? seems to work without it.
    - add a timeout to disconnect...  

  // perl script o format data from fetch eg. 4 columns.
  perl -pe 's{,}{++$n % 4 ? $& : "\n"}ge' data.txt

  eg.
    echo '*idn?' | ./examples/34970a.out  

    # Use rlwrap for history!
    rlwrap ./examples/34970a.out  

    TODO
    - Use signal handler to close.
    - maybe ctrl-z to reset?
    - rename devcat /device cat...

    - done - There's a problem that after use esptool, then baud is set wrong

*/

#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <stdbool.h> // c99
#include <stdlib.h>

#include <reactor.h>

typedef struct Context Context;
struct Context
{
    Reactor *reactor;
    int     device_fd;

};


static void on_device_event(Context *context, Event *e)
{
    assert(context->device_fd == e->fd);

    switch(e->type) {
        case WRITE_READY: {
            // we don't get this with serial prompt?
            fprintf(stdout, "on write ready?\n");
            reactor_on_read_ready( context->reactor, e->fd, -1, context, (void *)on_device_event);
            break;
        } 
        case READ_READY: {
            char buf[1000];
            int n = read(e->fd, &buf, 1000);
            //fprintf(stdout, "Got %d chars from device\n", n);
            if( n > 0)  {
                write(1, &buf, n); // buffer - or bind a handler to know this won't block?????
                reactor_on_read_ready( context->reactor, e->fd, -1, context, (void *)on_device_event);
            } else {
                // finish up
                fprintf(stdout, "device closed?");
                close(e->fd);
            }
            break;
        }
        // anything else
        default:
            fprintf(stdout, "unknown event");
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
            char buf[1001];
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
            fprintf(stdout, "unknown event on fd %d", e->fd);
            break;
    }
}


int main(int argc, char **argv)
{
    Context context;
    memset(&context, 0, sizeof(Context));

    context.reactor = reactor_create();

    char **arg;
    const char *filename = NULL;
    const char *speed = NULL;
    const char *parity = NULL;
    //int i;
    struct termios options;

    // decode arguments and values,
    for(arg = argv; arg < argv + argc; ++arg) {

        if(**arg == '-' && arg + 1 >= argv + argc) {
            fprintf(stdout, "flag '%s' missing argument\n", *arg);
            exit(123);
        }

        if(strcmp(*arg, "-s") == 0) 
            speed = *++arg;

        else if(strcmp(*arg, "-d") == 0) 
            filename = *++arg;

        else if(strcmp(*arg, "-p") == 0)
            parity = *++arg;
    }



    // reset config!
    memset(&options, 0, sizeof(struct termios));

    // get existing config
    // tcgetattr(context.device_fd, &options);

    // speed
    if(speed && strcmp(speed, "57600") == 0) {
        cfsetispeed(&options, B57600 );
        cfsetospeed(&options, B57600);
    } 
    else if(speed && strcmp(speed, "115200") == 0) {
        cfsetispeed(&options, B115200);
        cfsetospeed(&options, B115200);
    } 
    else {
        fprintf(stdout, "unknown speed '%s'\n", speed ? speed : "");
        exit(123);
    } 

    // parity
    if(parity && strcmp(parity, "8n1") == 0) {
        /*
        https://www.cmrr.umn.edu/~strupp/serial.html
         No parity (8N1):
        default
        */
        options.c_cflag &= ~PARENB;
        options.c_cflag &= ~CSTOPB;
        options.c_cflag &= ~CSIZE;
        options.c_cflag |= CS8;
    } 
    else if(parity && strcmp(parity, "7e1") == 0) {
        options.c_cflag |= PARENB;
        options.c_cflag &= ~PARODD;
        options.c_cflag &= ~CSTOPB;
        options.c_cflag &= ~CSIZE;
        options.c_cflag |= CS7;
    }
    else {
        fprintf(stdout, "unknown parity '%s'\n", parity ? parity : "");
        exit(123);
    } 

    // device
    if(!filename) {
        fprintf(stdout, "device not specified\n");
        exit(123);
    } 



    fprintf(stdout, "opening %s\n", filename);

    context.device_fd = open(filename, O_RDWR | O_NONBLOCK);
    if(context.device_fd <= 0) {
        // TODO errno
        fprintf(stdout, "error opening %d\n", context.device_fd);
        exit(123);
    }

    fprintf(stdout, "fd is %d\n", context.device_fd);

    // usleep(50000); // reveals that reset is a race

    /*
      // turning pin on or off
    serial = serial & ~TIOCM_DTR & ~TIOCM_RTS;
    if(ioctl(context.device_fd, TIOCMSET, &serial) < 0) {
        assert(0);
    }
    */

    // echo off, echo newline off, canonical mode off,
    // extended input processing off, signal chars off
    // options.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);

    // raw output
    // cfmakeraw(&options);
    // options.c_oflag = 0;

    tcflush(context.device_fd, TCIFLUSH);
    tcsetattr(context.device_fd, TCSANOW, &options);

    reactor_on_read_ready(context.reactor, context.device_fd, -1, &context, (void *)on_device_event);
    reactor_on_read_ready(context.reactor, 0, -1, &context, (void *)on_read_stdin);
    // reactor_on_timer(d, 500, &context, (void *)on_timeout_1);

    // reactor_cancel_all(d);
    reactor_run(context.reactor);
    reactor_destroy(context.reactor);

    return 0;
}

