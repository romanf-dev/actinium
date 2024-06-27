Actinium
========

Actinium is a microcontroller framework (or kernel) implementing actor-based 
execution model. It provides both hardware-assisted scheduling and memory 
protection for fault isolation.
Framework code is currently written in C but unprivileged tasks may be 
written in either C or Rust.
Please note it is still in pre-alpha stage and **isn't ready** for any use 
except research and experiments. This readme is also incomplete and will 
be updated over time...


Features
--------

- Preemptive multitasking
- Hard real-time capability
- Tasks are separately compiled and run in unprivileged mode
- Region-based memory protection
- Zero-copy message passing communication
- Timer facility
- No heap required, no dynamic memory allocation in the kernel
- Only ARM Cortex-M3/M4 (with MPU) are supported at now


Design principles
-----------------

1.  **Actor-based execution model.**
    Most of MCU-based embedded systems are reactive so full-fledged threads
    are often too heavyweight because of the requirement to allocate a stack
    for each thread. Actors use one stack per **priority level** as they are
    run-to-completion routines.
2.  **Hardware-assisted scheduling.**
    Modern interrupt controllers implement priority-based actor model in 
    hardware. Actor's priorities are mapped to interrupt priorities, thus
    scheduling and preemption operations are effectively implemented by
    the interrupt controller. It boosts performance and reduces code size.
3.  **Memory protection.**
    Actinium is designed for embedded systems with reliability requirements.
    Any actor is compiled as separate binary executable and isolated within 
    memory regions using MPU (memory protection unit). Actor crash may not
    cause the whole system to fail.
4.  **Zero-copy message-passing communication.**
    Actors communicate using messages and channels. However, messages are
    always passed by reference, so sending 1Kb and 1Gb message takes exactly
    the same time. Message passing is the only communication method at now.
5.  **Fault-tolerance and 'let it crash' principle.**
    When the framework encounters crash condition, e.g. invalid memory 
    reference, wrong syscall number, exception, etc. the erronous actor 
    is automatically marked for restart.


Architecture
------------

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


Reliability
-----------

The system is protected from the following bugs/attack vectors:

- **channel misuse**: channels that are used by unprivileged actors 
  are identified by ids, not pointers. Ids are validated before use, 
  therefore actors cannot post to/get messages from arbitrary channels, only
  allowed ones may be used. Also it is impossible to post message of wrong 
  type since both messages and channels have type ids.
- **message leaks**: actor is allowed to own just a single message at any 
  time. Post/get another message causes the owned message to be freed.
- **memory unsafety**: invalid instructions or memory references beyond 
  allowed areas cause exceptions and restart of the actor.
- **stack corruption**: insufficient stack space including the case of 
  preemption is also detected and causes the actor to restart.


The kernel does not use neither any pointers to unprivileged data nor 
callbacks, so syscall interface may not by subjected to this type of attacks.
In the current version stacks are not cleared between actor activations for 
performance reasons. This may cause data leaks between actors, but may be 
fixed if needed. Actors should not have access to DMA controller, otherwise 
any protection may be compromised.

When actor crashes while holding a message the message is freed as marked
as 'poisoned'. This mechanism allows to implement reliable request-reply
protocol even when both client and server unreliable.


Memory regions and MPU
----------------------

Currently, 5 regions are used for each unprivileged actor.
- Code (flash)
- Data (SRAM, also includes .bss)
- Stack
- Currently owned message
- ‘User’ region for peripheral access (optional)

I plan to remove restrictions and allow to set the number of regions per 
actor individually but this is not implemented yet.

Because of hardware restrictions of the MPU, messages should be:
- at least 32 bytes size
- aligned to its size
- sized to power of 2

A task may have access to a single message at any moment.
After flashing the MCU memory looks like the following:



               Memory layout                 MPU regions when Task 0 runs

        +--------------------------+         +--------------------------+
        |        ISR stack         |         |                          |
        +--------------------------+         +--------------------------+
        |           ...            |         |                          |
        +--------------------------+         +--------------------------+
        |    Task n data & bss     |         |                          |
        +--------------------------+         +--------------------------+
        |    Task 0 data & bss     |         |    Task 0 data & bss     |
        +--------------------------+         +--------------------------+
        |Stacks for each prio level|         | Stack for T0 prio |      |
        +--------------------------+         +--------------------------+
        |  Aligned Message pools   |         | Owned msg |  no access   |
        +--------------------------+         +--------------------------+
        | Core data, runqueues, etc|         |                          |
        +--------------------------+ <-SRAM  +--------------------------+
        |           ...            |         |                          |
        +--------------------------+         +--------------------------+
        |   Task n code & rodata   |         |                          |
        +--------------------------+         +--------------------------+
        |   Task 0 code & rodata   |         |   Task 0 code & rodata   |
        +--------------------------+         +--------------------------+
        |  Core code, ISRs, init   |         |                          |
        +--------------------------+         +--------------------------+
        |       ISR vectors        |         |                          |
        +--------------------------+ <-FLASH +--------------------------+



Using devices/interrupts
------------------------

Actors who need to communicate with devices may be granted access to 
peripheral memory. Interrupts are not directly accessible and should 
be redirected to channels as messages by the kernel part of application if 
needed.


Build process
-------------

Building is a little bit complex since we have to pack many semi-independent
tasks into single ELF with proper alignment. 

First, the core part containing MCU initialization is compiled using its
linker script into relocatable kernel.0 file. Note that extension is 
numeric 0, not o. This file is expected to have vector table and have to be
placed first.

Second, user tasks are compiled into regular relocatable object files using
framework-provided linker script. It has no deal with addresses, just
rearrange sections.
All relocatable files should have sections text/data/bss (possibly zero-sized),
other sections are ignored.

Third, ldgen.sh script extracts section sizes for *.o and kernel.0 files and
generates linker script packing the files together with proper alignment.
Also it provides section sizes in a special symbol so kernel may
further use this info when spawning tasks.
That's it.


                       +-----------+         text/data/bss sizes    +------------------+
                       |  task.ld  |             +----------------->|      ldgen.sh    |
                       +-----------+             |            ^     +------------------+
            section grouping |                   |            |            |
         +--------+          V      +----------------------+  |            |  section alignment
         | task.c |---------------> | relocatable  task.o  |--+--+         V  & relocation
         +--------+                 +----------------------+  |  |  +------------------+
                                                              |  |  | generated script |
                       +-----------+                   +------+  |  +------------------+        
                       | kernel.ld |                   |         |         |  final linking
                       +-----------+                   |         |         V
            section grouping |                         |         |  +------------------+
         +--------+          V      +----------------------+     V  |    executable    |
         | main.c |---------------> | relocatable kernel.0 |------->|       image      |
         +--------+                 +----------------------+        +------------------+
                                                                    


Files
-----


| file at /core  | description |
|----------------|-------------|
|arch/armv7m/ac_port.h | hardware abstraction layer interface for ARMv7-M |
|arch/armv7m/actinium.s | low-level interrupt/syscall entries |
|arch/armv7m/core*.h | CMSIS headers by ARM |
|arch/armv7m/task_startup.s | low-level task initialization |
|actinium.h | cross-platform framework functions |
|rust_task | library for Rust tasks support |
|task.ld | linker script for tasks |
|ldgen.sh | linker script generator to place kernel and actors in memory |


The demo
--------

The demo contains two tasks: sender and controller. The sender sends
requests to toggle LED to 'controller' who has access to the corresponding 
peripheral. Both actors constantly crash after few activations but they're 
restarted by exceptions and the system continues to work.
This demonstrates 'let it crash' principle: even when no task is reliable 
the whole system works as designed.
To build the demo run

        make

in the demo folder (provided that arm-none-eabi- toolchain is available via 
PATH). This yields image.elf file containing all the tasks and the kernel
ready for flashing.


The Rust demo
-------------

The previous demo is fully written in C. Writing async code in C requires
some efforts because of no async/await functionality in the language. The
framework is designed for async actors so Rust is more suitable for writing
them. The Rust demo replaces 'controller' task. It interprets 'LED on' and
'LED off' messages as 'blink once' and 'blink twice' so it is visually 
different than C demo. To build the Rust demo run:

        make rust_example

It yields app0.o file. Then run:

        make

to build full executable image.


How to use
----------

- set include dirs and include actinium.h into your application
- install ac_intr_entry as interrupt handler for vectors dedicated to actors
- install ac_trap_entry and ac_svc_entry as handlers for exceptions and syscall
- set priority for actor's vectors using NVIC API and enable them
- create actors and channels inside your main
- implement validation function which maps ids to channel objects
- call ac_kernel_start at end of your main

After code is built into relocatables, rename kernel relocatable into kernel.0 
(numeric 0, not O). o-files in the folder will be considered as applications
sorted alphabetically. The first file in alphabatical order will become actor
with task_id = 0, second - with task_id = 1, etc.
Finally, run ldgen.sh script with two parameters as base addresses of flash 
and SRAM. Example for STM32F401:

        ldgen.sh 0x08000000 0x20000000

image.elf file containing the kernel and all the tasks should appear.


API description
---------------

TODO


Why Actinium?
-------------

It is two words 'actor' and 'tiny' combined in a way to form chemical element 
name. Also it emphasizes that this is an evolution of the magnesium framework.

