#![no_std]
#![no_main]

/*
 * @file    lib.rs
 * @brief   Support for Rust tasks. This code is unprivileged.
 */

pub mod task {

use core::mem;
use core::ptr;
use core::ptr::{ NonNull };
use core::marker::PhantomData;
use core::pin::Pin;
use core::future::Future;
use core::task::{Poll, Context, RawWakerVTable, RawWaker, Waker};
use core::cell::{ UnsafeCell };
use core::ops::{Deref, DerefMut};
 
const SC_DELAY: u32 = 0 << 28;
const SC_CHAN_POP: u32 = 1 << 28;
const SC_CHAN_POLL: u32 = 2 << 28;
const SC_MSG_SEND: u32 = 3 << 28;
const SC_MSG_FREE: u32 = 4 << 28;

#[repr(C)]
struct MsgHeader {
    size: u32,
    _padding: u32,
    poisoned: u32
}

extern "C" {
    fn _ac_syscall(_: u32) -> *mut MsgHeader;
}

#[repr(C)]
struct Msg<T> {
    header: MsgHeader,
    payload: T
}

unsafe fn msg_typecast<'a, T: Send>(ptr: NonNull<MsgHeader>) -> &'a mut Msg<T> {
    let size = ptr.cast::<u32>().read() as usize;
    assert!(size == mem::size_of::<Msg<T>>());
    ptr.cast::<Msg<T>>().as_mut()
}

pub struct Token();

impl Token {
    fn new() -> Self {
        Token()
    }
}

pub struct Envelope<T: Sized + Send + 'static> {
    msg_ref: &'static mut Msg<T>
}

impl<T: Sized + Send> Envelope<T> {
    fn new(msg_ref: &'static mut Msg<T>) -> Self {
        Self {
            msg_ref
        }
    }
    
    pub fn is_poisoned(&self) -> bool {
        self.msg_ref.header.poisoned != 0
    }
    
    pub fn free(self) -> Token {
        mem::forget(self);
        unsafe {
            _ac_syscall(SC_MSG_FREE);
        }
        Token::new()
    }    
}

impl<T: Send> Deref for Envelope<T> {
    type Target = T;
    fn deref(&self) -> &T {
        &self.msg_ref.payload
    }
}

impl<T: Send> DerefMut for Envelope<T> {
    fn deref_mut(&mut self) -> &mut T {
        &mut self.msg_ref.payload
    }
}

impl<T: Send> Drop for Envelope<T> {
    fn drop(&mut self) {
        panic!("dropped msg");
    }
}

pub struct RecvChannel<T: Sized + Send + 'static> {
    id: u32,
    _marker: PhantomData<&'static T>
}

impl<T: Sized + Send> RecvChannel<T> {
    pub const fn new(id: u32) -> Self {
        Self {
            id,
            _marker: PhantomData
        }
    }
    
    pub fn try_pop(&self, token: Token) -> Result<Envelope<T>, Token> {
        let ptr = unsafe { _ac_syscall(SC_CHAN_POLL | self.id) };
        let msg = NonNull::new(ptr).map(|p| unsafe { msg_typecast::<T>(p) });
        let env = msg.map(Envelope::new);
        env.ok_or(token)
    }
    
    pub async fn pop(&self, _token: Token) -> Envelope<T> {
        self.await
    }
}

enum Mailbox {
    Message(NonNull<MsgHeader>),
    Subscription(u32),
    MessageWaiting
}

static mut IPC: Mailbox = Mailbox::MessageWaiting;

impl<T: Sized + Send> Future for &RecvChannel<T> {
    type Output = Envelope<T>;
    fn poll(self: Pin<&mut Self>, _cx: &mut Context) -> Poll<Self::Output> {
        unsafe {
            if let Mailbox::Message(ptr) = IPC {
                let msg = msg_typecast::<T>(ptr);
                Poll::Ready(Envelope::new(msg))
            } else {
                IPC = Mailbox::Subscription(self.id | SC_CHAN_POP);
                Poll::Pending
            }
        }
    }
}

pub struct HeaderPtr {
    ptr: NonNull<MsgHeader>
}

impl HeaderPtr {
    fn new(ptr: NonNull<MsgHeader>) -> Self {
        Self {
            ptr
        }
    }

    pub unsafe fn read<T: Send + Sized + Copy + 'static>(&self) -> T {
        let msg = self.ptr.cast::<Msg<T>>().as_ptr();
        (*msg).payload
    }
    
    pub fn free(self) -> Token {
        mem::forget(self);
        unsafe {
            _ac_syscall(SC_MSG_FREE);
        }
        Token::new()
    }
    
    pub unsafe fn convert<T: Send + Sized + 'static>(self) -> Envelope<T> {
        let msg =  msg_typecast(self.ptr);
        Envelope::new(msg)
    }
}

pub struct RawRecvChannel {
    id: u32
}

impl RawRecvChannel {
    pub const fn new(id: u32) -> Self {
        Self {
            id
        }
    }
    
    pub fn try_pop(&self, token: Token) -> Result<HeaderPtr, Token> {
        let ptr = unsafe { _ac_syscall(SC_CHAN_POLL | self.id) };
        NonNull::new(ptr).map(HeaderPtr::new).ok_or(token)
    }
    
    pub async fn pop(&self, _token: Token) -> HeaderPtr {
        self.await
    }
}

impl Future for &RawRecvChannel {
    type Output = HeaderPtr;
    fn poll(self: Pin<&mut Self>, _cx: &mut Context) -> Poll<Self::Output> {
        unsafe {
            if let Mailbox::Message(ptr) = IPC {
                Poll::Ready(HeaderPtr::new(ptr))
            } else {
                IPC = Mailbox::Subscription(self.id | SC_CHAN_POP);
                Poll::Pending
            }
        }
    }
}

impl Drop for HeaderPtr {
    fn drop(&mut self) {
        panic!("dropped msg");
    }
}

pub struct SendChannel<T: Sized + Send + 'static> {
    id: u32,
    _marker: PhantomData<&'static T>
}

impl<T: Sized + Send> SendChannel<T> {
    pub const fn new(id: u32) -> Self {
        Self {
            id,
            _marker: PhantomData
        }
    }
    
    pub fn send(&mut self, msg: Envelope<T>) -> Token {
        mem::forget(msg);
        unsafe {
            _ac_syscall(self.id | SC_MSG_SEND);
        }
        Token::new()
    }
}

pub struct Timer {
    delay: u32
}

pub fn delay(ticks: u32) -> Timer {
    Timer { delay: ticks }
}

impl Future for Timer {
    type Output = ();
    fn poll(mut self: Pin<&mut Self>, _cx: &mut Context) -> Poll<Self::Output> {
        if self.delay > 0 {
            unsafe {
                IPC = Mailbox::Subscription(self.delay | SC_DELAY);
            }
            self.delay = 0;
            Poll::Pending
        } else {
            Poll::Ready(())
        }
    }
}

pub const fn size_of<F>(_future: &impl FnOnce(Token) -> F) -> usize {
    mem::size_of::<F>()
}

pub const fn align_of<F>(_future: &impl FnOnce(Token) -> F) -> usize {
    mem::align_of::<F>()
}

unsafe fn cast_to<F>(p: *const (), _: impl FnOnce(Token) -> F) -> &'static mut F {
    &mut *(p as *mut F)
}

pub struct FutureStorage<const N: usize, const A: usize> {
    future: UnsafeCell<[u8; N]>,
}

unsafe impl<const N: usize, const A: usize> Sync for FutureStorage<N, A> {}

impl<const N: usize, const A: usize> FutureStorage<N, A> {
    pub const fn new() -> Self {
        FutureStorage {
            future: UnsafeCell::new([0; N]),
        }
    }
    
    fn get(&self) -> *mut () {
        let ptr = self.future.get();
        let nptr = (ptr as usize).next_multiple_of(A);
        nptr as *mut ()
    }
    
    pub unsafe fn write<F: Future>(&self, f: F) -> *mut () {
        let ptr = self.get();
        let future_storage = ptr as *mut F;
        future_storage.write(f);
        ptr
    }
}

pub unsafe fn call<F: Future + 'static>(ptr: *mut (), f: impl FnOnce(Token) -> F) -> u32 {
    const VTABLE: RawWakerVTable = RawWakerVTable::new(
        |_| { RawWaker::new(ptr::null(), &VTABLE) }, |_| {}, |_| {}, |_| {}
    );

    let raw_waker = RawWaker::new(ptr::null(), &VTABLE);
    let waker = Waker::from_raw(raw_waker);
    let mut cx = Context::from_waker(&waker);
    
    let future = cast_to(ptr, f);
    let mut pinned = Pin::new_unchecked(future);
    let _ = pinned.as_mut().poll(&mut cx);

    if let Mailbox::Subscription(syscall) = IPC {
        if (syscall >> 28) != 0 {
            IPC = Mailbox::MessageWaiting;
        }
        syscall
    } else {
        panic!("no subscription");
    }
}

pub fn msg_input(msg: *mut ()) {
    unsafe {
        if !msg.is_null() {
            let ptr = NonNull::new_unchecked(msg as *mut MsgHeader);
            IPC = Mailbox::Message(ptr);
        }
    }
}

pub fn call_once(f: impl FnOnce(Token) -> *mut ()) -> *mut () {
    static mut INIT: bool = false;
    static mut FUT_PTR: *mut () = ptr::null_mut();
    
    unsafe {
        if !INIT{
            let ptr = f(Token::new());
            FUT_PTR = ptr;
            INIT = true;
        }

        FUT_PTR
    }
}

#[macro_export]
macro_rules! bind {
    ($msg:ident, $task:ident) => {{
        use ac::task::FutureStorage;
        use ac::task::{size_of, align_of, msg_input, call_once, call};

        const SZ: usize = size_of(&$task);
        const ALIGN: usize = align_of(&$task);
        static DATA: FutureStorage<{SZ + ALIGN}, {ALIGN}> = FutureStorage::new();

        msg_input($msg);
        let ptr = call_once(|token| { unsafe { DATA.write($task(token)) } });
        let syscall = unsafe { call(ptr, $task) };
        syscall
    }}
}
}
