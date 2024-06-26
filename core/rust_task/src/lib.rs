#![no_std]
#![no_main]

/**
 ******************************************************************************
 * @file      lib.rs
 * @brief     Support for Rust tasks. This code is unprivileged.
 *****************************************************************************/

pub mod task {

use core::mem;
use core::ptr;
use core::marker::PhantomData;
use core::pin::Pin;
use core::future::Future;
use core::task::{Poll, Context, RawWakerVTable, RawWaker, Waker};
use core::cell::{Cell, UnsafeCell};
use core::ops::{Deref, DerefMut};
 
const SC_DELAY: u32 = 0x00000000;
const SC_CHAN_GET: u32 = 0x10000000;
const SC_CHAN_POLL: u32 = 0x20000000;
const SC_SEND_MSG: u32 = 0x30000000;
const SC_FREE_MSG: u32 = 0x40000000;

#[repr(C)]
struct MsgHeader {
    size: u32,
    _padding: [u32; 2],
    poisoned: u32
}

extern "C" {
    fn _ac_syscall(_: u32) -> *mut MsgHeader;
}

#[repr(C)]
struct MsgWrapper<T> {
    header: MsgHeader,
    payload: T
}

unsafe fn msg_typecast<'a, T: Sized + Send>(ptr: *mut MsgHeader) -> &'a mut MsgWrapper<T> {
    let msg = &mut *(ptr as *mut MsgHeader);
    if msg.size as usize == mem::size_of::<MsgWrapper<T>>() {
        let msg = &mut *(ptr as *mut MsgWrapper<T>);
        msg
    } else {
        panic!("mismatched msg sizes");
    }
}

pub struct MsgOwner<T: Sized + Send + 'static> {
    msg_ref: &'static mut MsgWrapper<T>
}

impl<T: Sized + Send> MsgOwner<T> {
    fn new(msg_ref: &'static mut MsgWrapper<T>) -> Self {
        Self {
            msg_ref
        }
    }
    
    pub fn is_poisoned(&self) -> bool {
        self.msg_ref.header.poisoned != 0
    }    
}

impl<T: Send> Deref for MsgOwner<T> {
    type Target = T;
    fn deref(&self) -> &T {
        &self.msg_ref.payload
    }
}

impl<T: Send> DerefMut for MsgOwner<T> {
    fn deref_mut(&mut self) -> &mut T {
        &mut self.msg_ref.payload
    }
}

impl<T: Send> Drop for MsgOwner<T> {
    fn drop(&mut self) {
        unsafe {
            _ac_syscall(SC_FREE_MSG);
        }
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
    
    pub fn try_pop(&self) -> Option<MsgOwner<T>> {
        let ptr = unsafe { _ac_syscall(SC_CHAN_POLL | self.id) };
        if !ptr.is_null() {
            let msg = unsafe { msg_typecast::<T>(ptr) };
            Some(MsgOwner::new(msg))
        } else {
            None
        }
    }
    
    pub async fn get(&self) -> MsgOwner<T> {
        self.await
    }
}

enum Mailbox {
    Message(*mut MsgHeader),
    Subscription(u32),
    MessageWaiting
}

struct Global<T> {
    data: Cell<Option<T>>
}

impl<T> Global<T> {
    const fn new() -> Global<T> {
        Global { 
            data: Cell::new(None) 
        }
    }
}

unsafe impl<T> Sync for Global<T> {}

static IPC: Global<Mailbox> = Global::new();

impl<T: Sized + Send> Future for &RecvChannel<T> {
    type Output = MsgOwner<T>;
    fn poll(self: Pin<&mut Self>, _cx: &mut Context) -> Poll<Self::Output> {
        if let Some(Mailbox::Message(ptr)) = IPC.data.take() {
            let msg = unsafe { msg_typecast::<T>(ptr) };
            let owner = MsgOwner::new(msg);
            Poll::Ready(owner)
        } else { //TODO: try
            IPC.data.set(Some(Mailbox::Subscription(self.id | SC_CHAN_GET)));
            Poll::Pending
        }
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
    
    pub fn send(&mut self, _msg: MsgOwner<T>) {
        mem::forget(_msg);
        unsafe {
            _ac_syscall(self.id | SC_SEND_MSG);
        }
    }
}

pub struct Timer {
    delay: Cell<u32>
}

unsafe impl Sync for Timer {}

impl Timer {
    pub const fn new() -> Self {
        Self {
            delay: Cell::new(0)
        }
    }
    
    pub fn delay(&self, delay: u32) -> &Self {
        self.delay.set(delay);
        self
    }
}

impl Future for &Timer {
    type Output = ();
    fn poll(self: Pin<&mut Self>, _cx: &mut Context) -> Poll<Self::Output> {
        let delay = self.delay.take();
        if delay > 0 {
            IPC.data.set(Some(Mailbox::Subscription(delay | SC_DELAY)));
            Poll::Pending
        } else {
            Poll::Ready(())
        }
    }
}

pub const fn size_of<F>(_future: &impl FnOnce() -> F) -> usize {
    mem::size_of::<F>()
}

pub const fn align_of<F>(_future: &impl FnOnce() -> F) -> usize {
    mem::align_of::<F>()
}

unsafe fn cast_to<F>(p: *const (), _: impl FnOnce() -> F) -> &'static mut F {
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

pub fn call<F: Future + 'static>(ptr: *mut (), f: impl FnOnce() -> F) -> u32 {
    const VTABLE: RawWakerVTable = RawWakerVTable::new(
        |_| { RawWaker::new(ptr::null(), &VTABLE) }, |_| {}, |_| {}, |_| {}
    );

    let raw_waker = RawWaker::new(ptr::null(), &VTABLE);
    let waker = unsafe { Waker::from_raw(raw_waker) };
    let mut cx = Context::from_waker(&waker);
    
    unsafe {
        let future = cast_to(ptr, f);
        let mut pinned = Pin::new_unchecked(future);
        let _ = pinned.as_mut().poll(&mut cx);
    }
    
    if let Some(Mailbox::Subscription(syscall)) = IPC.data.take() {
        if (syscall >> 28) != 0 { // TODO: remove this hack
            IPC.data.set(Some(Mailbox::MessageWaiting));
        }
        syscall
    } else {
        panic!("no subscription");
    }
}

pub fn msg_input(msg: *mut ()) {
    if !msg.is_null() {
        if IPC.data.take().is_some() {
            IPC.data.set(Some(Mailbox::Message(msg as *mut MsgHeader)));
        }
    }
}

pub fn call_once(f: impl FnOnce() -> *mut ()) -> *mut () {
    static INIT: Global<()> = Global::new();
    static FUT_PTR: Global<*mut ()> = Global::new();
    
    if INIT.data.get().is_none() {
        let ptr = f();
        FUT_PTR.data.set(Some(ptr));
        INIT.data.set(Some(()));
    }
    FUT_PTR.data.get().unwrap()
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
        let ptr = call_once(|| { unsafe { DATA.write($task()) } });
        let syscall = call(ptr, $task);
        syscall
    }}
}
}
