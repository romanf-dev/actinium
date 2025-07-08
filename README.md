
Actinium is an experimental real-time kernel for separately compiled
coroutines. It provides both hardware-assisted scheduling and memory 
protection for fault isolation. Also, it is small and simple.
Kernel code is currently written in C but unprivileged tasks may be 
written in either C, C++ or Rust.


Features
--------

- Preemptive multitasking
- Hard real-time capability
- Tasks are separately compiled and run in unprivileged mode
- Region-based memory protection
- Zero-copy message passing communication
- Timer facility
- No heap required, no dynamic memory allocation in the kernel
- ARM Cortex-M3/M33+ (with MPU) and RISC-V (with PMP) are supported


Design principles
-----------------

1.  **Actor-based execution model.**
    Most of MCU-based embedded systems are reactive so full-fledged threads
    are often too heavyweight because of the requirement to allocate a stack
    for each thread. Actors use one stack per **priority level** as they are
    run-to-completion routines. Modern languages allow to implement actors
    that look and behave as threads but need less resources. Finally, actors
    simplify real-time analysis.
2.  **Hardware-assisted scheduling.**
    Modern interrupt controllers implement prioritized actor model in 
    hardware. Actor's priorities are mapped to interrupt priorities, thus
    scheduling and preemption operations are effectively implemented by
    the interrupt controller. It boosts performance and reduces code size.
3.  **Memory protection.**
    Actinium is designed for embedded systems with reliability requirements.
    Each actor is compiled as separate binary executable and isolated within 
    memory regions using MPU (memory protection unit). Actor's crash may not
    cause the whole system to fail.
4.  **Zero-copy message-passing communication.**
    Actors communicate using messages and channels. However, messages are
    always passed by reference, so sending 1Kb and 1Gb message takes exactly
    the same time. Message passing is the only communication method at now.
5.  **Fault-tolerance and 'let it crash' principle.**
    When the framework encounters crash condition, e.g. invalid memory 
    reference, wrong syscall number, exception, etc. the erronous actor 
    is automatically marked for restart. Clients and servers may not
    trust each other, IPC protocol ensures possibility for system recovery.


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
  preemption is also detected.


The kernel does not use neither any pointers to unprivileged data nor 
callbacks, so syscall interface may not by subjected to this type of attacks.
In the current version stacks are not cleared between actor activations for 
performance reasons. This may cause data leaks between actors, but may be 
fixed if needed. Actors should not have access to DMA controller, otherwise 
any protection may be compromised.

When actor crashes while holding a message the message is freed and marked
as 'poisoned'. This mechanism allows to implement reliable request-reply
protocol even when both client and server are unreliable.


Files
-----

| file | description |
|------|-------------|
|arch | porting layers |
|usr | usermode libraries for supported languages |
|examples | demo projects |
|tests | hosted unit tests |
|actinium.h | the actinium kernel |
|ldgen.sh | linker script generator |


The demo (for STM32 black pill)
-------------------------------

The demo contains two tasks: sender and controller. The 'sender' sends
requests to toggle LED to 'controller' who has access to the corresponding 
peripheral. Both tasks crash periodically after few activations but they're
restarted by exceptions and the system continues to work.
This demonstrates 'let it crash' principle: even when no task is reliable 
the whole system works as designed.
To build the demo run

        make

in the demo folder (provided that arm-none-eabi- toolchain is available via 
PATH). This yields image.elf file containing all the tasks and the kernel
ready for flashing.

Original tasks (task0.c and task1.c) are simple and written in C. However,
since actors are inherently 'async' writing them in C requres some efforts
because of no async/await functionality in the language. 

There are two demo modifications: 'sender' in C++20 and 'controller' in Rust.

Modified controller interprets 'LED on' and 'LED off' messages as 'blink once' 
and 'blink  twice' so it is visually distinguishable from C demo. 
To build the demo modification run:

        make rust_example

and/or: 

        make cpp_example

Then run:

        make

To build the executable image with modified tasks.


Why Actinium?
-------------

It is two words 'actor' and 'tiny' combined in a way to form chemical element 
name. Also it emphasizes that this is an evolution of the magnesium framework.

