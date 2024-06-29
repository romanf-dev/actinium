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

        void ac_context_init(
            uintptr_t flash_base, 
            size_t flash_size
        );

        void ac_context_stack_set(
            unsigned int prio, 
            size_t size, 
            void* ptr
        );

        void noreturn ac_kernel_start(void);

        void ac_channel_init_ex(
            struct ac_channel_t* chan, 
            size_t total_length,
            void* ptr,
            size_t block_size,
            int msg_type
        );

        void ac_channel_init(
            struct ac_channel_t* chan, 
            int msg_type
        );

        void ac_actor_init(
            struct ac_actor_t* actor, 
            unsigned int vect,
            unsigned int task_id
        );

        void ac_actor_allow(
            struct ac_actor_t* actor,
            size_t size,
            void* base,
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

