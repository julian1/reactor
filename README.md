
#### DEMUX/REACTOR

  A micro demultiplexor/reactor event framework for linux in C using p/select()

#### FEATURES

  - event demux for timers, sockets, stdin/stdout, serial, signals
  - support for a user created context to be passed to the callback handler
  - sensible/synchronous handling of interupts
  - clean cancel/terminate semantics to allow easy/custom resource cleanup

#### NOTES

  - Refs proactor versus reactor
    - http://www.artima.com/articles/io_design_patterns2.html
      - Reactor
        - dmux and dispatch to handler on read or write readiness
        - dmux dispatching always synchronous
      - Proactor
        - additionally handle the read or write asynchronously in kernel and then dispatch with data to handler
        - strands...
  - For the most part a proactor can be built around a reactor
  - http://gngrwzrd.com/libgwrl/pod.html

#### TODO

  - signals (outside the core dispatcher using fifo)
  - change dispatcher name to reactor or demux? or separate demux and handler dispatch?
  - use gettimeofday instead of time for millisec prec.
  - compute lowest possible timeout value (eg. next possible timeout ) as select sleep time
    - Currently, timer resolution is low - order of 100ms - designed for basic network operations only
  - investage issue when using serial-comm and the double \n\n
  - perhaps combine the read/write callback... and use
      READ_READY, WRITE_READY, EXCEPTION, TIMEOUT, CANCELLED
  - investigate - ioctl for sockets to discover number bytes in buffer yet to transmit. like boost asio
  - example simple serial comms
    - ioctl tty_ioctl set baud, rts, dtr
  - network examples
    - dns
    - socket bind/listen
  - enable cancel_all to be called in handlers while processing a cancel_all
    - WHich will make it easy to do a cancel/shutdown from any callback

#### DONE

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
  -


