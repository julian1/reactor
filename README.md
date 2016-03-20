
### REACTOR

  A reactor/demultiplexer event micro-framework for Linux in C

#### FEATURES

  - event demux for timers, signals, sockets, stdin/stdout, serial etc
  - support for application user-contexts in the callback handler
  - specify timeout for any io action
  - synchronous management of signal interupts
  - clean cancel/terminate semantics for easy cleanup of resources 
  - no hiding/abstracting ioctl(), tty_ioctl() etc
  - no queue lag for simpler lifetime management

#### BUILD
```
  $ cd ~/reactor
  $ ./build.sh 
  $ ./examples/timeout.out
  ...
```

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
  - maybe rename Event to ReactorEvent or use reactor_event_t etc... becomes more complicated
  - maybe rename to use lowercase underscore t
  - maybe support cancelling individual handlers (lookup by fd and type)
    - or just return the handler for user to call action on?
  - investigate if ok to mix fopen(stdout) with open(1) for logging... (better to use stderr?)
  - signal deregistration
  - fix device echo issue with serial comms
  - investigate - for proactor ioctl for sockets to discover number bytes in buffer already/yet to transmit. similar to boost asio
  - network examples
    - http
    - dns
    - telnet socket bind/listen
  - better enum variable prefixes - eg. prefix with 'reactor'

#### DONE

  - done - move signal fifo handling outside pure reactor, also logging. for modularity. delegate
  - no - rename d-> vars to r->
  - no - maybe rename ureactor or udemux?
  - done - combine the read/writ callback, then type the event we're interested in
      - will simplify, and make rebind(e) possible.
      - simultaneous read and write handler binding
      - READ_READY, WRITE_READY, EXCEPTION, TIMEOUT, CANCELLED
  - done - issue with binding both read/write is may want to cancel one in response to the other?
      - reason to encode the action... rather
      - makes cancelling, as easy as not rebinding
  - done - example simple serial comms
    - ioctl tty_ioctl set baud, rts, dtr
  - done - get bind() and listen() and check that can spawn new contexts easily
  - done - fix cancel_all
    - make it easy to do a cancel/shutdown from any callback
  - done - investage issue when using serial-comm and the double \n\n
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


  - /blog - task esp8266, and control registers. screen, minicom, miniterm that I use
  this led, to writing a demux . handling of signals.

