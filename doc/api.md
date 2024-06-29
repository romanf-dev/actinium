API
===

Development an Actinium application includes at least two steps:
- writing privileged part of the app in C
- writing unprivileged actors in C or Rust

Also it is possible to run actors in privileged mode and these actors
are able to communicate via IPC with the rest of the system.


Privileged API
--------------

Privileged code is responsible for platform initialization, creation of
actors and channels and interrupt redirection.

Initialization of the global data, runqueues, etc.

        void ac_context_init(
            uintptr_t flash_base, 
            size_t flash_size
        );

Set stack for priority specified. Pointer/size are subject for MPU 
restrictions: memory must be power-2-sized and aligned to its size.

        void ac_context_stack_set(
            unsigned int priority, 
            size_t size, 
            void* ptr
        );

Start scheduling loop.

        void noreturn ac_kernel_start(void);

By default channels have no messages until someone puts them into.
Extended API for channels specifies memory pool for initial message
allocations. When a message is allocated from such channel the latter
is set as the message parent. Freeing message always puts it into the
parent channel despite any further owners of the message.

        void ac_channel_init_ex(
            struct ac_channel_t* chan, 
            size_t total_length,
            void* ptr,
            size_t block_size,
            int msg_type
        );

Default channel with no associated memory. Cannot be message parent.

        void ac_channel_init(
            struct ac_channel_t* chan, 
            int msg_type
        );

Actor initialization. Task_id is the number of application stored on
the flash. ldgen script puts tasks alphabetically, so task_id 0 is the
first object file in the build folder in alphabetical order.

        void ac_actor_init(
            struct ac_actor_t* actor, 
            unsigned int vector,
            unsigned int task_id
        );

Allow actor to access a region. Only one region per actor is allowed.

        void ac_actor_allow(
            struct ac_actor_t* actor,
            size_t size,
            void* ptr,
            unsigned int attr
        );

The framework expects these functions to be implemented by the user:

        extern struct ac_channel_t* ac_channel_validate(
            struct ac_actor_t* caller, 
            unsigned int handle,
            bool is_write
        );

These functions are wrappers for internal functions. They may be used for
some extra logic or for debugging/logging etc. Default implementations
are as follows:

        extern void* ac_intr_handler(uint32_t vect, void* frame) {
            return _ac_intr_handler(vect, frame);
        }

        extern void* ac_svc_handler(uint32_t arg, void* frame) {
            return _ac_svc_handler(arg, frame);
        }

        extern void* ac_trap_handler(uint32_t exception_id) {
            return ac_actor_exception();
        }


Unprivileged API (Rust)
-----------------------

The library provides macro 'bind' which is intended to bind message
passed as a parameter into main function with async function with 
the following signature:

        async fn actor() -> !

Default implementation of the main():

        #[no_mangle]
        pub fn main(msg: *mut ()) -> u32 {
            bind!(msg, actor)
        }


### Envelope

Opaque struct representing a message.

        struct Envelope<T>

Traits: Deref, DerefMut, Drop

Methods:

        fn is_poisoned(&self) -> bool

### Timer

Represents a delay.

        struct Timer

Methods:

        const fn new() -> Self
        async fn delay(&self, delay: u32)

### RecvChannel

Channel for receiving.

        struct RecvChannel<T>

Methods:

        const fn new(id: u32) -> Self
        fn try_pop(&self) -> Option<Envelope<T>>
        async fn get(&self) -> Envelope<T>

### SendChannel

Channel for sending.

        struct SendChannel<T>

Methods:

        const fn new(id: u32) -> Self
        fn send(&mut self, msg: Envelope<T>)

Please note that msg argument in 'send' is not really used. The actor always
sends a message it currently owns. Envelope should be passed into send function
to avoid call of the destructor that frees the owned message.


