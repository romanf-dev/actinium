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
Msg type is used for exact message type identification to avoid 
message posting into a wrong channel. Because compile-time typeid is
not available in C it is the user responsibility to set message
types properly. Type must match in any channels which are used to 
hold the same message type. The type itself is any nonzero integer.

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

Allow actor to access a region. Only one region per actor is allowed at
now. Attribute may be one of the following: AC_ATTR_RW, AC_ATTR_RO and
AC_ATTR_DEV for memory-mapped devices.

        void ac_actor_allow(
            struct ac_actor_t* actor,
            size_t size,
            void* ptr,
            unsigned int attr
        );

The framework expects these functions to be implemented by the user:
When any actor tries to post/subscribe this function is called to
translate the channel id into object pointer.
Actor structure may be augmented on the user side with any security-
related members so they may be accessed inside this function using
containing_record-like macro on 'caller'.
Additionally, is_write parameter may be used for fine-grained channel
access, for example, to restrict some actor only to read some channel.
If access is not granted the function should return 0.

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


Trap handler does not accept frame pointer as frame may be broken by the user
and it is not safe to access the stack.


Unprivileged API (Rust)
-----------------------

The library provides macro 'bind' which is intended to bind message
passed as a parameter into main function to async function with 
the following signature:

        async fn actor(token: Token) -> !

Default implementation of the main():

        #[no_mangle]
        pub fn main(msg: *mut ()) -> u32 {
            bind!(msg, actor)
        }

It may be implemented as procedural macro to hide wrappers but I dislike any
'hidden logic' and macro-generated functions.

An actor may own only a single message at any time. When actor allocates a new
one then previous message is implicitly freed by the kernel. This may lead to
undefined behavior in safe Rust when someone allocates a new message while
holding previous one since two messages are unrelated from the Rust type 
system's point of view. To address this issue the concept of tokens are used.

A token itself is an empty struct which represents the 'right to allocate a 
new message'. Initial token is passed as a parameter into the actor function 
and should be passed into all function that may allocate a message like pop or
try_pop. Also, token has private constructor so it cannot be created inside 
the actor. Thus, actor is allowed to own a single message which it recevied 
in exchange to its initial token.

When an actor loses its message ownership via free of send, the kernel returns
a new token so actor may allocate new message using it etc.

In other words, any actor at any time own either single token or single message
so the protocol is enforced at compile-time and runtime UB is impossible within
safe code.


### Envelope

Opaque struct representing a message.

        struct Envelope<T>

Please note that generic message type contains 16-byte header, so T type 
must have such a size to make the whole message power-of-2-sized. 
For example if your message payload is 4-bytes long you have to include 
explicit 12-bytes padding in order to get 32-bytes message 
(16 bytes header + 4 bytes payload + 12 bytes padding).
This is the MPU hardware restriction and is required for all ARMv7-M chips.

Traits: Deref, DerefMut, Drop

Methods:

        fn is_poisoned(&self) -> bool
        fn free(self) -> Token


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
        fn try_pop(&self, Token) -> Result<Envelope<T>, Token>
        async fn pop(&self, Token) -> Envelope<T>


### SendChannel

Channel for sending.

        struct SendChannel<T>

Methods:

        const fn new(id: u32) -> Self
        fn send(&mut self, msg: Envelope<T>) -> Token

