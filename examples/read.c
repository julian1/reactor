
// testing reading stdin

#include <stdio.h>
#include <unistd.h>

#include <ureactor.h>

// OK. we have a problem with the reactor context...
// in the event.. is the inner context...

UReactor *d;

static void on_read_ready(void *context, Event *e)
{
    switch(e->type) {
      case READ_READY: {
          char buf[1000];
          int n = read(e->fd, &buf, 1000);
          fprintf(stdout, "read %d chars\n", n);
          if(n > 0)  {
              // read more
              ureactor_on_read_ready(d, e->fd, -1, context, (void *)on_read_ready);
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
  d = ureactor_create();
  ureactor_on_read_ready(d, stdin_fd, -1, NULL, (void *)on_read_ready);
  ureactor_run(d);
  ureactor_destroy(d);
  return 0;
}

