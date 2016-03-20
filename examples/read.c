
// testing reading stdin

#include <stdio.h>
#include <unistd.h>

#include <reactor.h>


static void on_read_ready(Reactor *r, Event *e)
{
    switch(e->type) {
      case READ_READY: {
          char buf[1000];
          int n = read(e->fd, &buf, 1000);
          fprintf(stdout, "read %d chars\n", n);
          if(n > 0)  {
              // read more
              reactor_on_read_ready(r, e->fd, -1, r, (Demux_callback)on_read_ready);
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


int main()
{
  int stdin_fd = 0;
  Reactor *r = reactor_create();
  reactor_on_read_ready(r, stdin_fd, -1, r, (Demux_callback)on_read_ready);
  reactor_run(r);
  reactor_destroy(r);
  return 0;
}

