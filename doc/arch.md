Architecture
============

        unprivileged CPU mode
        
        +------+  +------+  +------+  +------+
        | task |  | task |  | task |  | task |
        +------+  +------+  +------+  +------+
           ^        ^ ^        ^         ^
           |        | |        |         |     IPC
        +------------------------------------+
        |  |        | |        |         |   |
        |  +--------+ +--------+         V   | 
        |                           +------+ |
        |           CORE            | task | |
        |                           +------+ |
        | privileged CPU mode                |
        +------------------------------------+


Executable image consists of a number of tasks and a core containing system
services. Each task contains exactly one actor which is a run-to-completion
stackless coroutine. Actors communicate using channels and messages.
Tasks run in memory-protected domain and have no direct access to 
neither core nor peripherals. It is assumed that executable image is 
deployed as a whole, so tasks can't be created or destroyed at runtime.

Each task is assigned a fixed priority and each priority corresponds to
some (unused by the user application) interrupt vector. This approach 
allows to use interrupt controller as a hardware-implemented scheduler.

For example, figure below depicts three vectors (5, 2, 1) assigned to 
three runqueues.

        +---+---+---+---+---+---+---+---+
        | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 | <- ISR vectors
        +---+---+---+---+---+---+---+---+
                  |           |   |
                  |   +-------+   |
                  |   |   +-------+
                  |   |   |
                  V   V   V
                +---+---+---+
                | H | M | L |  <- Runqueues for high (H), medium (M) and 
                +---+---+---+     low (L) priority
                  |   |   |
                  V   V   V
                  O   O   O    <- Tasks
                  O       O
                  O  
        
Note that vector is assigned to priority level, not actor. So number of
vectors that have to be reserved for scheduling is always finite and limited,
whereas number of tasks/actors is unlimited.

Most Cortex-M chips define 'priority bits' for NVIC as 3-5, so number 
of distinct priority levels is 8-32.
Other vectors unused by scheduling behave as expected and are not used by the
framework in any way.

Syscalls are the only way to communicate with the core and the other tasks.
Syscalls are divided into two classes: synchronous and asynchronous. 
Synchronous syscalls act like normal functions - they return some value to 
the caller and the execution continues. Asynchronous ones suspend actor 
execution until some condition is satisfied.


| syscall  | synchronous | description |
|----------|-------------|-------------|
|delay     |   |re-activates actor execution after the specified period |
|subscribe |   |wait for new messages |
|try_pop   | o |polls a channel synchronously |
|send      | o |post the currently owned message into a channel |
|free      | o |free the owned message |


Using devices/interrupts
------------------------

Actors who need to communicate with devices may be granted access to 
peripheral memory. Interrupts are not directly accessible and should 
be redirected to channels as messages by the kernel part of application if 
needed.

