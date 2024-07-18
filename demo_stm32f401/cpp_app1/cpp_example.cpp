//
// @file  cpp_example.cpp
// @brief Example C++ application. Sends LED on/off messages every second.
//

#include "actinium.hpp"

//
// Coroutine state allocator. Since the state is allocated only once at 
// startup it just return the buffer address.
//
void* task::promise_type::alloc(std::size_t n) {
    static unsigned char s_buffer[128];
    
    if (n > sizeof(s_buffer)) {
        for (;;); /* TODO: something better... */
    }
    
    return s_buffer;
}

//
// Message description. Control field may be either 0 or 1.
//
struct led_control {
    std::uint32_t control;
    std::uint32_t padding[3]; 
};
      
//
// Actor function.
//
task func() {
    recv_channel<led_control> src(0);
    send_channel<led_control> dst(1);
    static int led_state = 0;
    
    for (;;) {
        co_await delay(1000);
        auto msg = co_await src.pop();
        msg->control = (led_state ^= 1);
        dst.push(std::move(msg));        
    }
}

extern "C" void __libc_init_array(void);

//
// Called once by startup code at each restart of the actor.
//
extern "C" void _ac_init_once() {  
    __libc_init_array();
}

//
// Entry point of the actor.
//
extern "C" std::uint32_t main(message_header* msg) {
    return bind(msg, func);
}
