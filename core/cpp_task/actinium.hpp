//
// @file  actinium.hpp
// @brief C++ task support library for the Actinium framework.
//

#ifndef ACTINIUM_TASK_HPP
#define ACTINIUM_TASK_HPP

#include <cstdint>
#include <coroutine>
#include <optional>
#include <functional>
#include <concepts>
#include <type_traits>

class message_header {

protected:
    std::uintptr_t size;
    std::uintptr_t padding[2];
    std::uintptr_t poisoned;
};

static_assert(sizeof(message_header) == 16);

enum syscall_id {
    DELAY =     0 << 28,
    SUBSCRIBE = 1 << 28,
    TRY_POP =   2 << 28,
    MSG_PUSH =  3 << 28,
    MSG_FREE =  4 << 28
};

extern "C" message_header* _ac_syscall(std::uint32_t arg);

template<typename T> struct message : message_header {
    T payload;
};

//
// Because of hardware MPU restrictions, only power-2-sized regions are
// allowed. Transferable message is defined as a message which size including
// the header is equal to power of two. Since exact message types are
// defined at user side and aren't known in advance this is implemented as
// a concept, so user won't be able to instantiate channels with 
// non-transferable types even without any asserts.
//
template<typename T> concept transferable = 
    (sizeof(message<T>) & (sizeof(message<T>) - 1)) == 0;

template<transferable T> class recv_channel;
template<transferable T> class send_channel;

//
// Message pointer are wrapped into 'owning' type. It may be moved but only
// the framework code may instantiate it from a raw pointer.
//
template<transferable T> class message_owner {
    message<T>* ptr_;
    message_owner(message<T>* msg) noexcept : ptr_(msg) {}
    void drop() { ptr_ = nullptr; }

public:
    message_owner(message_owner&& other) noexcept {
        ptr_ = other.ptr_;
        other.ptr_ = nullptr;
    }

    T* operator->() const { return &ptr_->payload; }

    ~message_owner() {
        if (ptr_ != nullptr) {
            ptr_ = nullptr;
            (void) _ac_syscall(syscall_id::MSG_FREE);
        }
    }

    bool is_poisoned() const {
        return ptr_->poisoned != 0;
    }

    friend class recv_channel<T>;
    friend class send_channel<T>;
};

//
// Coroutine return type. Wraps the coroutine handle.
//
class task {

public:
    struct promise_type {
        union {
            std::uint32_t syscall_arg;
            message_header* incoming_msg;
        };
        
        task get_return_object() { 
            return { 
                std::coroutine_handle<promise_type>::from_promise(*this) 
            };
        }

        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() { return {}; }
        void unhandled_exception() {}
        
        static void* alloc(std::size_t n);
        void* operator new(std::size_t n) { return alloc(n); }
        void operator delete(void*) {}
    };
    
    task(std::coroutine_handle<promise_type> h) noexcept : handle_(h) {}
    operator std::coroutine_handle<promise_type>() const { return handle_; }
    
private:
    std::coroutine_handle<promise_type> handle_;   
};

template<transferable T> class recv_channel {
    const std::uint32_t id_;
    
public:
    constexpr auto pop() const {
        class awaitable {
            const task::promise_type* promise_;
            const std::uint32_t recv_chan_id_;

        public:    
            constexpr bool await_ready() const { return false; }
            
            void await_suspend(std::coroutine_handle<task::promise_type> h) {
                h.promise().syscall_arg = syscall_id::SUBSCRIBE | recv_chan_id_;
                promise_ = &h.promise();
            }
            
            message_owner<T> await_resume() const {
                return {static_cast<message<T>*>(promise_->incoming_msg)};
            }
                       
            constexpr awaitable(std::uint32_t ident) : 
                promise_(nullptr), recv_chan_id_(ident) {}
        };
        
        return awaitable{id_};
    }
    
    std::optional<message_owner<T>> try_pop() const {
        message_header* const msg = _ac_syscall(syscall_id::TRY_POP | id_);

        if (msg) {
            return {static_cast<message<T>*>(msg)};
        } else {
            return std::nullopt;
        }
    }    
    
    consteval recv_channel(std::uint32_t ident) noexcept : id_(ident) {}
};

template<transferable T> class send_channel {
    const std::uint32_t id_;
    
public:
    void push(message_owner<T> msg) {
        msg.drop(); /* Destructor won't free message at end of the function. */
        (void) _ac_syscall(syscall_id::MSG_PUSH | id_);
    }

    consteval send_channel(std::uint32_t ident) noexcept : id_(ident) {}
};

static constexpr auto delay(std::uint32_t t) {
    class awaitable {
        const std::uint32_t delay_;

    public:
        constexpr awaitable(std::uint32_t t) noexcept : delay_(t) {}
        
        bool await_ready() const { return delay_ == 0; }
        
        void await_suspend(std::coroutine_handle<task::promise_type> h) const {
            h.promise().syscall_arg = syscall_id::DELAY | delay_;
        }
        
        void await_resume() const {}
    };
    
    return awaitable{t};
}

//
// Binds incoming messages to the actor function and advances its coroutine.
// Return syscall argument in case when the coroutine requests a blocking
// call.
//
static inline std::uint32_t bind(message_header* msg, std::function<task()> f) {
    static std::coroutine_handle<task::promise_type> s_handle;
    
    if (!s_handle) {
        s_handle = f();
    } else {
        s_handle.promise().incoming_msg = msg;
        s_handle.resume();            
    }

    return s_handle.promise().syscall_arg;
}

#endif
