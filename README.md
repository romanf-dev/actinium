Actinium
========

Actinium is a microcontroller framework implementing actor-based execution model.
It provides both hardware-assisted scheduling and memory protection for fault 
isolation.
Please note it is still in pre-alpha stage and isn't ready for any use except 
research and experiments. This readme is also incomplete and will be updated 
over time...


Features
--------

- Preemptive multitasking (number of priority levels is defined by hardware)
- Cooperative scheduling within a single priority level
- Actors are separately compiled and run in unprivileged mode
- Region-based memory protection using MPU
- Message passing IPC
- Actors or tasks can't be created or destroyed at runtime
- Timer services with 'sleep for' functionality
- Only ARM Cortex-M3/M4 (with MPU) are supported at now


Design principles
-----------------

1.  **Actor-based execution model (Hoare's CSP).**
    Most of MCU-based embedded systems are reactive so full-fledged threads
    are often too heavyweight because of the requirement to allocate a stack
    for each thread. Actors use one stack per **priority level**.
2.  **Hardware-assisted scheduling.**
    Modern interrupt controllers implement priority-based actor model in 
    hardware. Actor's priorities are mapped to interrupt priorities, thus
    scheduling and preemption operations are effectively implemented by
    the interrupt controller. It boosts performance and reduces code size.
3.  **Memory protection.**
    Actinium is designed for embedded systems with reliability requirements.
    Any actor may be compiled as separate executable and isolated within 
    memory regions using MPU (memory protection unit). Actor crash may not
    cause the whole system to fail.
4.  **Zero-copy message-passing communication.**
    Actors communicate using messages and channels. However, messages are
    always passed by references so sending 1Kb and 1Gb message takes exactly
    the same time. Message passing is the only communication method at now.
5.  **Fault-tolerance and 'let it crash' principle.**
    When the framework encounters crash condition, e.g. invalid memory reference,
    wrong syscall number, exception, etc. the erronous actor is automatically 
    marked for restart.


Files
-----


| file                           | description                                      |
|--------------------------------|--------------------------------------------------|
|core/arch/armv7m/ac_port.h      | hardware abstraction layer interface for ARMv7-M |
|core/arch/armv7m/actinium.s     | low-level interrupt/syscall entries              |
|core/arch/armv7m/core*.h        | CMSIS headers by ARM                             |
|core/arch/armv7m/task_startup.s | low-level task initialization                    |
|core/actinium.h                 | cross-platform framework functions               |
|core/task.ld                    | linker script for tasks                          |
|core/ldgen.sh                   | linker script generator                          |



The demo
--------

The demo contain two tasks or actors: sender and controller. The sender sends
requests to toggle LED to controller who has access to the corresponding 
peripheral. Both actors constantly crash after few activations but they're 
gently restarted by exceptions and the system continues to work.
This demonstartes 'let it crash' principle: even when no task is reliable 
the whole system works as designed.


| file      | description                        |
|-----------|------------------------------------|
| main.c    | privileged part of the application |
| led_msg.h | IPC message description            |
| task*.c   | task code                          |


FAQ
---

TODO


Why Actinium?
-------------

It is two words 'actor' and 'tiny' combined in a way to form chemical element 
name. Also it emphasizes that this is an evolution of the magnesium framework.

