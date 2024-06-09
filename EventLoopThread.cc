#include "EventLoopThread.h"
#include "EventLoop.h"


EventLoopThread::EventLoopThread(const ThreadInitCallback &cb, const std::string &name)
    : loop_(nullptr), existing_(false), thread_(std::bind(&EventLoopThread::threadFunc, this), name), mutex_(), cond_(), callback_(cb)
{
}

EventLoopThread::~EventLoopThread()
{
    existing_ = true;
    if (loop_ != nullptr)
    {
        loop_->quit();
        thread_.join(); // 等待子线程执行结束
    }
}

EventLoop *EventLoopThread::startLoop()
{
    thread_.start(); // 启动底层的新线程

    EventLoop *loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while(loop_ == nullptr){  // 虚假唤醒避免
            cond_.wait(lock); // 等到 loop_非空 loop 启动
        }
        loop = loop_;
    }
    return loop;
}

// 下面这个方法 是在 单独的新线程运行的 ，每次start ， 底层的 Thread类 启动之后都是一个新的线程
void EventLoopThread::threadFunc() // 线程具体的执行函数 per thread
{           //有一个创建在栈上的EventLoop 对象， one loop 
    EventLoop loop; // 创建一个独立的eventloop，和上面的线程是一一对应的，one loop per thread

    if (callback_)
    {
        callback_(&loop); // 线程 初始化回调
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one(); //通知loop_ 已经是非空 ，也就是这个时候 EventLoop 对象已经和线程绑定成功， 通知可以返回继续执行
    }
    
    loop.loop(); // EventLoop loop => Poller.poll
    
    std::unique_lock<std::mutex> lock(mutex_); //出现异常或者主动关闭掉才会执行到这，否则一直在执行上面那个loop
    loop_ = nullptr;
}