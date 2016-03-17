
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include <reactor.h>


typedef struct Context Context;
struct Context {
    // void *inner_context;
};


// we can open two files, and let it process...

static void on_read_ready(Context *x, Event *e)
{
    // Context *context = (Context *)e->context;

    switch(e->type) {
        case OK: {
            // not sure whether to do this here, or elsewhere...
            // also eof for disconnect if socket, but not always - eg. fill
            char buf[1000];
            int n = read(e->fd, &buf, 1000);
            fprintf(stdout, "OK got %d chars\n", n);
            if( n > 0)  {
                // read more
                reactor_on_read_ready(e->reactor, e->fd, -1, x, (void *)on_read_ready);
            } else {
                // finish up
                close(e->fd);
                free(x);
            }
            break;
        }
        // anything else
        default:
            fprintf(stdout, "event unknown");
            // clean up user state
            close(e->fd);
            free(x);
            break;
    }

}

// must be composible

void reactor_read(Reactor *d, char *filename)
{
    // int fd = open("/dev/random", O_RDWR | O_NONBLOCK);
    int fd = open(filename, O_RDWR | O_NONBLOCK);
    if(fd <= 0) {
        fprintf(stdout, "error opening fd");
    } else {
        Context * context = malloc(sizeof(Context));
        memset(context, 0, sizeof(Context));
        reactor_on_read_ready(d, fd, -1, context, (void *)on_read_ready);
    }
}

// we want to do stdin...
// have to be careful, don't accidently bind the same fd in more than once

int main()
{
    Reactor *d = reactor_create();
    // reactor_init(&d);
    reactor_read(d,"main.c");
    // reactor_read(&d,"main.c");
    while(reactor_run_once(d));
/*
    fd = open("/dev/random", O_RDWR);
*/
    return 0;
}

