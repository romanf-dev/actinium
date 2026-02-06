API
===

Development an Actinium application includes at least two steps:
- writing privileged part of the app in C
- writing unprivileged actors in either C, C++ or Rust

Also it is possible to run actors in privileged mode and these actors
are able to communicate via IPC with the rest of the system.


Privileged API
--------------

Privileged code is responsible for platform initialization, creation of
actors and channels and interrupt redirection.

Initialization of the global data, runqueues, etc. This function must
be called on every CPU in the multiprocessor system.

        void ac_context_init();

Set stack for priority specified. Pointer/size are subject for MPU 
restrictions: memory must be power-2-sized and aligned to its size.
This function sets stack on calling CPU so on multi-CPU system it must
be called on every CPU and for every priority used on that CPU.

        void ac_context_stack_set(
            unsigned int priority, 
            size_t size, 
            void* ptr
        );

If the system has a tick source then tick handler should be called.
This function is also CPU-local.

        void ac_context_tick(void);

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
            size_t block_size
        );

Default channel with no associated memory. Cannot be message parent.

        void ac_channel_init(struct ac_channel_t* chan);

Actor initialization. Task descriptor is a struct describing actor 
memory: flash and SRAM base address and size.

        void ac_actor_init(
            struct ac_actor_t* actor, 
            unsigned int vector,
            const struct ac_actor_descr_t* descr
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

Hard restart for the specified actor:

        void ac_actor_restart(struct ac_actor_t* actor);


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

When error condition is detected (like exception) then the user-provided
function is called during error processing. It may be used to customize 
error handling behavior. For example, faulty actor may be just restarted,
or it may have internal counter to be restarted only N times, or some 
message may be sent into a channel to notify another actor, etc.

        extern void ac_actor_error(struct ac_actor_t* src);




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
invalid memory refs in safe Rust when someone allocates a new message while
holding previous one since two messages are unrelated from the Rust type 
system's point of view. To address this issue the concept of tokens is used.

A token itself is an empty struct which represents the 'right to allocate a 
new message'. Initial token is passed as a parameter into the actor function 
and should be passed into all functions that may allocate a message like pop or
try_pop. Also, token has private constructor so it cannot be created inside 
the actor. Thus, actor is allowed to own a single message which it recevied 
in exchange to its initial token.

When an actor loses its message ownership via free of send, the kernel returns
a new token so actor may allocate new message using it etc.

In other words, any actor at any time owns either single token or single message
so the protocol is enforced at compile-time and runtime exceptions because of 
obsolete messages are impossible.


### Envelope

Message ownership is represented as Envelope which hides underlying raw pointer.

        struct Envelope<T>

Please note that generic message type contains 12-byte header, so T type 
must have such a size to make the whole message power-of-2-sized. 
For example if your message payload is 4-bytes long you have to include 
explicit 16-bytes padding in order to get 32-bytes message 
(12 bytes header + 4 bytes payload + 16 bytes padding).
This is the MPU hardware restriction and is required for all ARMv7-M chips.

Messages should never be dropped implicitly. Dropping of a message causes
loss of the token so an actor cannot proceed. If a message is not needed 
anymore use free.

Traits: Deref, DerefMut, Drop

Methods:

        fn is_poisoned(&self) -> bool
        fn free(self) -> Token


### Delay

Represents a delay.

        async fn delay(ticks: u32)


### RecvChannel

Channel for receiving.

        struct RecvChannel<T>

Methods:

        const fn new(id: u32) -> Self
        fn try_pop(&self, Token) -> Result<Envelope<T>, Token>
        async fn pop(&self, Token) -> Envelope<T>


### SendChannel

Channel for sending. If some channel allows different message types then
it is possible to have multiple SendChannel objects with different types
but with the same id.

        struct SendChannel<T>

Methods:

        const fn new(id: u32) -> Self
        fn send(&mut self, msg: Envelope<T>) -> Token

### RawRecvChannel

RecvChannel expects that each message received has the same type, it does not 
work for arbitrary messages. Sometimes it is useful to have a single channel
for different message types, i.e. interrupt notifications and user requests
for some server. In this case the RawRecvChannel should be used.
Since the type of message isn't known in advance it has the slighly different 
API.
        const fn new(id: u32) -> Self
        fn try_pop(&self, Token) -> Result<HeaderPtr, Token>
        async fn pop(&self, Token) -> HeaderPtr

Instead of Envelope<T> it returns HeaderPtr - a wrapped pointer to unknown
message payload. This type has additional functions allowing application 
to check the type at runtime and then convert the pointer into specific
Envelope.

        fn free(self) -> Token
        unsafe fn read<T>(&self) -> T
        unsafe fn convert<T>(self) -> Envelope<T>

The read function reads the payload as the specified type.

