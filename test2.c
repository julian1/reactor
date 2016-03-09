/*
    Testing reading stdin...
*/

#include <stdio.h>
#include <unistd.h>

#include <dispatcher.h>

void on_read_ready(void *context, Event *e)
{
  switch(e->type) {
    case OK: {
      // not sure whether to do this here, or elsewhere...
      // also eof for disconnect if socket, but not always - eg. fill
      char buf[1000];
      int n = read(e->fd, &buf, 1000);
      fprintf(stdout, "Got %d chars\n", n);
      if( n > 0)  {
        // read more
        // ok, hang on
        dispatcher_on_read_ready(e->dispatcher, e->fd, -1, context, (void *)on_read_ready);
        // dispatcher_on_write_ready(e->dispatcher, e->fd, context, (void *)on_read_ready);
      } else {
        // finish up
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

// so stdin appears to be line buffered it would be nice to change
// also handle signal...
// it's buffering behavior not the blocking behavior

/*
  Note that in case stdin is associated  with  a  terminal,
  This kernel input handling can be modified using calls like tcsetattr(3
*/
int main()
{
  Dispatcher *d = dispatcher_create();
  dispatcher_on_read_ready(d, 0, 5, NULL, (void *)on_read_ready);
  // dispatcher_cancel_all(d);
  while(dispatcher_dispatch(d));
  dispatcher_destroy(d);
  return 0;
}

