#pragma once

#include <coroutine>
#include <future>
#include <iostream>

// 协程返回类型，用于异步任务
struct AsyncTask {
    struct promise_type{
        AsyncTask get_return_object(){ return {}; }
        std::suspend_never initial_suspend(){ return {};}
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void(){}
        void unhandled_expection(){std::terminate();}
    };
};

// 线程包装池
template <typename Func>
auto run_async(Func&& func) {
    return std::async(std::launch::async, std::forward<Func>(func));
}