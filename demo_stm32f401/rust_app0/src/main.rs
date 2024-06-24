#![no_std]
#![no_main]

use core::ptr::write_volatile;
use core::panic::PanicInfo;
use ac::task::RecvChannel;
use ac::task::Timer;
use ac::bind;

struct Msg {
    control: u32
}

#[panic_handler]
pub fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

async fn controller() -> ! {   
    static CHAN: RecvChannel<Msg> = RecvChannel::new(1);
    static TIMER: Timer = Timer::new();
    let ptr = 0x40020818 as *mut u32;
    
    loop {
        let msg = CHAN.get().await;
        let n = msg.control + 1;
            
        for _ in 0..n {
            unsafe { write_volatile(&mut *ptr, 1 << 13); }
            TIMER.delay(100).await;
            unsafe { write_volatile(&mut *ptr, 1 << 29); }
            TIMER.delay(100).await;
        }
    }
}

#[no_mangle]
pub fn main(msg: *mut ()) -> u32 {
    bind!(msg, controller)
}

