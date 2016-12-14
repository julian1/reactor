
### REACTOR

  A reactor/demultiplexer event micro-framework for Linux in C

  *The reactor design pattern is an event handling pattern for handling service
  requests delivered concurrently to a service handler by one or more inputs. The
  service handler then demultiplexes the incoming requests and dispatches them
  synchronously to the associated request handlers.*[1]

#### FEATURES
  - tiny code-base, easy to understand, simple to modify
  - use the same event pattern to handle events from timers, signals, sockets, stdin/stdout, files, serial/uart etc
  - callback handlers take user-context
  - can specify timeout for completion of any io-action
  - serialization of signal interupts so that the handler is called in correct threading context
  - clean cancel/terminate events for easy resource cleanup
  - no attempt to hide/abstract ioctl(), tty_ioctl() gives complete control over descriptor properties
  - no dispatch queue, means no server lag, and simpler lifetime management of resources

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


#### TODO

  - add more network examples
    - websockets - would be really good. could then create a shell.
    - postgres async connection
    - http
    - dns async - server/forwarder and client
    - telnet socket bind/listen

  - maybe support cancelling individual handlers (according to fd and type)
    - or just return the handler for user to call action on?
  - investigate if ok to mix fopen(stdout) with open(1) for logging... (better to use stderr?)
  - signal deregistration
  - fix device echo issue in serial comms example
  - investigate - for proactor support - ioctl for sockets to discover number bytes in buffer already/yet to transmit. similar to boost asio
  - better enum variable prefixes - eg. prefix with 'reactor'



