Actinium
========

Actinium is an real-time kernel implementing actor-based computation model 
similar to Communicating Sequential Processes. It does not implement threads
and synchronization primitives like semaphores and mutexes. Instead, it it
built around ports, messages and nonblocking handlers (actors).

Unlike many other implementations of such models Actinium supports memory
protection and fault tolerance. Actors run in user mode and cannot corrupt
neither kernel nor each other.

The kernel itself is implemented as just one header. It is expected that user
builds the kernel using API from this header in main.c. However, memory
protection and usermode cannot be implemented without thin layer of low-level
assembly wrappers around interrupt/syscall handlers so porting layer contains
a little piece of assembly too.

While it is not required by design most examples use hardware scheduling.
Since actors are nonblocking they may be mapped to interrupt handlers to
utilize interrupt controller as a hardware scheduler which manages preemption.
This reduces code size and speeds things up.

Actors are separately compiled binaries so they may be implemented in any
language. Currently the actor is a function handling incoming messages.
Each actor manages its own memory, copies data sections from flash, etc.
Unlike threads actors have no stack. In theory since actors are nonblocking
they may use a single stack but Actinium use a stack per priority level for
proper isolation. When a preemption occur new actor runs on the stack dedicated
to its priority level and have no access to the stack of the preempted actor.
Additionally user-implemented kernel function is used to validate port access.
Currently an actor may have just a single message at any time.

Since actors are separately compiled they may be implemented in any language.
Libraries are provided for development using C, C++20 and Rust.

The special linker script generator is used to properly align actor's 
binaries in memory.

Since actor state is well-defined it is much easier to deal with errors. On
exception an erronous actor is marked for cold restart. Restarted actor can 
check channels again and so on.

