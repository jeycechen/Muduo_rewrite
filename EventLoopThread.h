#pragma once
#include "noncopyable.h"
#include<functional>
#include "Thread.h"
#include <mutex>
#include <string>
#include <condition_variable>
#include<memory>


class EventLoop;

class EventLoopThread : noncopyable{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;
    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(), const std::string& name = std::string());


    ~EventLoopThread();
    EventLoop* startLoop();

private:
    void threadFunc();
    EventLoop *loop_;
    bool existing_;
    Thread thread_;
    
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;
};