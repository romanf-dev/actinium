#![no_std]
#![no_main]

pub mod task {

use core::marker::PhantomData;
use core::pin::Pin;
use core::task::Poll;
use core::task::Context;
use core::future::Future;
use core::mem;
use core::cell::UnsafeCell;
use core::cell::Cell;
use core::ptr;
use core::task::RawWakerVTable;
use core::task::RawWaker;
use core::task::Waker;
use core::ops::Deref;
use core::ops::DerefMut;
 
extern "C" {
    fn _ac_syscall(_: u32) -> *mut ();
}

const SC_DELAY: u32 = 0x00000000;
const SC_CHAN_GET: u32 = 0x10000000;
const SC_POOL_GET: u32 = 0x20000000;
const SC_CHAN_POLL: u32 = 0x30000000;
const SC_POOL_POLL: u32 = 0x40000000;
const SC_SEND_MSG: u32 = 0x50000000;
const SC_FREE_MSG: u32 = 0x60000000;

#[repr(C)]
struct MsgWrapper<T> {
    _header: [u32; 4],
    payload: T
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

struct MsgSource<T: Sized + Send + 'static> {
    id: u32,
    try_syscall_mask: u32,
    get_syscall_mask: u32,
    _marker: PhantomData<&'static T>
}

impl<T: Sized + Send> MsgSource<T> {
    const fn new(id: u32, is_pool: bool) -> Self {
        MsgSource {
            id,
            try_syscall_mask: if is_pool { SC_POOL_POLL } else { SC_CHAN_POLL },
            get_syscall_mask: if is_pool { SC_POOL_GET } else { SC_CHAN_GET },
            _marker: PhantomData
        }
    }
    
    fn try_pop(&self) -> Option<MsgOwner<T>> {
        let ptr = unsafe { _ac_syscall(self.try_syscall_mask | self.id) };
        if !ptr.is_null() {
            let msg = unsafe { &mut *(ptr as *mut MsgWrapper<T>) };
            Some(MsgOwner::new(msg))
        } else {
            None
        }
    }
}

enum Mailbox {
    Message(*mut ()),
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

impl<T: Sized + Send> Future for &MsgSource<T> {
    type Output = MsgOwner<T>;
    fn poll(self: Pin<&mut Self>, _cx: &mut Context) -> Poll<Self::Output> {
        if let Some(Mailbox::Message(ptr)) = IPC.data.take() {
            let msg = unsafe { &mut *(ptr as *mut MsgWrapper<T>) };
            let owner = MsgOwner::new(msg);
            Poll::Ready(owner)
        } else {
            let subscr_syscall = self.id | self.get_syscall_mask;
            IPC.data.set(Some(Mailbox::Subscription(subscr_syscall))); //TODO: try
            Poll::Pending
        }
    }
}

pub struct RecvChannel<T: Sized + Send + 'static> {
    src: MsgSource<T>
}

impl<T: Sized + Send> RecvChannel<T> {
    pub const fn new(id: u32) -> Self {
        Self {
            src: MsgSource::<T>::new(id, false)
        }
    }
    
    pub fn try_pop(&self) -> Option<MsgOwner<T>> {
        self.src.try_pop()
    }
    
    pub async fn get(&self) -> MsgOwner<T> {
        (&self.src).await
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

pub struct Pool<T: Sized + Send + 'static> {
    src: MsgSource<T>
}

impl<T: Sized + Send> Pool<T> {
    pub const fn new(id: u32) -> Self {
        Pool {
            src: MsgSource::<T>::new(id, true)
        }
    }
    
    pub fn try_pop(&self) -> Option<MsgOwner<T>> {
        self.src.try_pop()
    }
    
    pub async fn get(&self) -> MsgOwner<T> {
        (&self.src).await
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
        loop {}
    }
}

pub fn msg_input(msg: *mut ()) {
    if !msg.is_null() {
        if let Some(_) = IPC.data.take() {   
            IPC.data.set(Some(Mailbox::Message(msg)));
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
        use ac::task::size_of;
        use ac::task::align_of;
        use ac::task::msg_input;
        use ac::task::call_once;
        use ac::task::call;
        use ac::task::FutureStorage;
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
