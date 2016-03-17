
### REACTOR

  A reactor/demultiplexer event handling micro-framework for Linux in C

#### FEATURES

  - event demux for timers, signals, sockets, stdin/stdout, serial etc
  - supports application defined user contexts as part of the callback handler
  - synchronous management of signal interupts
  - specify timeout for any io action
  - clean cancel/terminate semantics and to enable easy resource cleanup
  - no hiding/abstracting ioctl(), tty_ioctl() etc
  - no event queue, means simpler resource lifetime management

#### NOTES

  - https://en.wikipedia.org/wiki/Reactor_pattern
  - Refs proactor versus reactor - http://www.artima.com/articles/io_design_patterns2.html
    - Reactor
      - dmux and dispatch to handler on read or write readiness
      - dmux dispatching always synchronous
    - Proactor
      - additionally handle the read or write asynchronously in kernel and then dispatch with data to handler
      - strands, apartments thread contexts...
  - For the most part a proactor can be built around a reactor
  - http://gngrwzrd.com/libgwrl/pod.html

#### TODO

  - get bind, and test that can spawn new contexts easily
  - rename Event to ReactorEvent or use reactor_event_t etc
  - simultaneous read and write handler binding
  - maybe move signal fifo handling outside pure reactor
  - change name ureactor or udemux?
  - investigate if ok to mix fopen(stdout) with open(1) for logging... (better to use stderr?)
  - signal deregistration

   investage issue when using serial-comm and the double \n\n
  - perhaps combine the read/write callback... and use
      READ_READY, WRITE_READY, EXCEPTION, TIMEOUT, CANCELLED
  - investigate - ioctl for sockets to discover number bytes in buffer already/yet to transmit. similar to boost asio
  - example simple serial comms
    - ioctl tty_ioctl set baud, rts, dtr
  - network examples
    - dns
    - socket bind/listen
  - enable cancel_all to be called in handlers while processing a cancel_all
    - WHich will make it easy to do a cancel/shutdown from any callback
  - better enum variable prefixes

#### DONE

  - done - use gettimeofday() instead of time for millisec prec.
  - done - compute smallest timeout value across all handlers and use as select() sleep timeout
  - done - change dispatcher name to reactor or demux? or separate demux and handler dispatch?
  - done - signals using fifo (maybe outside the core dispatcher)
  - done - move tests into examples...
  - done - control over debug stream
  - done - pure timers not associated with fd (outside core dispatcher if can? ).
    - eg. read stdin -> write to /dev/ttyUSB0, read /dev/ttyUSB write stdout,
  - done - timer  / timeouts
  - done - cancel all, and call all callbacks
  - done - write handler

#### NOTES

  - issue - with for signals, is that might occur while a callback is running,
          easy way to deal with this is to create a fifo queue and write it, then use
          select as way to process.

  - The  reason  that pselect() is needed is that if one wants to wait for either a
    signal or for a file descriptor to become ready, then an atomic test is needed
    to prevent race conditions.

  - but may not want, instead set the sig mask once on startup when we create the dispatcher

  - signal, sighandler_t   - behavior varies across unix  - says use sigaction instead


  - /blog - task esp8266, and control registers. screen, minicom, miniterm that I use
  this led, to writing a demux . handling of signals.

