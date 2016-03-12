
A nano reactor / demultiplexor event framework over p/select()


Refs proactor versus reactor
http://www.artima.com/articles/io_design_patterns2.html
  - Reactor
    - dispatch to handler on read or write readiness
    - dispatching always syncronous
  - Proactor
    - additionally handle the read or write asynchronously in kernel and then dispatch with data to handler
    - strands...

  - For the most part a proactor can be built around a reactor

TODO
  - signals (outside the core dispatcher using fifo) 
  - done - pure timers not associated with fd (outside core dispatcher if can? ).

  - change name dispatcher to demux? or separate demux and handler dispatch?

  - use gettimeofday instead of time for millisec prec. 

  - investage issue when using serial-comm and the double \n\n 

  - perhaps combine the read/write callback... and use
      READ_READY, WRITE_READY, EXCEPTION, TIMEOUT, CANCELLED

  - do we still want to block

  - investigate - for sockets number bytes in buffer yet to transmit.

  - simple serial comms 
      - ioctl tty_ioctl set baud, rts, dtr
  - signals and registration
  - dns
  - socket bind/listen

  - issue if in handler - want to call cancel all, but we're already
      in the cancel all 


DONE
  - eg. read stdin -> write to /dev/ttyUSB0, read /dev/ttyUSB write stdout,
  - done timer  / timeouts
  - done cancel all, and call all callbacks 
  - done - write handler

NOTES

  issue - with for signals, is that might occur while a callback is running,  
          easy way to deal with this is to create a fifo queue and write it, then use 
          select as way to process. 


  The  reason  that pselect() is needed is that if one wants to wait for either a
  signal or for a file descriptor to become ready, then an atomic test is needed
  to prevent race conditions. 

  - but may not want, instead set the sig mask once on startup when we create the dispatcher

  - signal, sighandler_t   - behavior varies across unix  - says use sigaction instead
  - 


