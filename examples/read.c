
// testing reading stdin

#include <stdio.h>
#include <unistd.h>

#include <reactor.h>

// OK. we have a problem with the reactor context...
// in the event.. is the inner context...

Reactor *r;

static void on_read_ready(void *context, Event *e)
{
    switch(e->type) {
      case READ_READY: {
          char buf[1000];
          int n = read(e->fd, &buf, 1000);
          fprintf(stdout, "read %d chars\n", n);
          if(n > 0)  {
              // read more
              reactor_on_read_ready(r, e->fd, -1, context, (Demux_callback)on_read_ready);
          } else {
              // finish
              close(e->fd);
          }
          break;
      }
      // anything else
      default:
          fprintf(stdout, "oops unknown event");
          close(e->fd);
          break;
    }
}

/*
  Note that in case stdin is associated with a terminal,
  This kernel input handling can be modified using calls like tcsetattr(3)
*/

int main()
{
  int stdin_fd = 0;
  r = reactor_create();
  reactor_on_read_ready(r, stdin_fd, -1, NULL, (Demux_callback)on_read_ready);
  reactor_run(r);
  reactor_destroy(r);
  return 0;
}

