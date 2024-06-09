#pragma once 
#include "noncopyable.h"
#include<functional>
#include<memory>
#include<atomic>

#include<vector>
#include<mutex>
#include "Timestamp.h"
#include "CurrentThread.h"
// 事件循环类， 主要包含了两个大模块 Channel poller（epoll的抽象）
class Channel;
class Poller;


//事件循环类，主要包含两个大模块， Channel Poller
class EventLoop : noncopyable{
public:
    using Functor = std::function<void()>;
    EventLoop();
    ~EventLoop();

    //开启事件循环
    void loop();
    //退出事件循环
    void quit();

    Timestamp pollReturnTime() const {return pollReturnTime_;}

    //在当前loop中执行cb
    void runInLoop(Functor cb);
    //把cb放入队列中，唤醒loop所在的线程，执行cb
    void queueInLoop(Functor cb);
    //唤醒loop所在的线程
    void wakeup();

    //eventloop的方法 =》 调用poller的方法
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    bool hasChannel(Channel *channel);

    //判断eventloop对象是否在自己的线程里面
    bool isInLoopThread() const {return threadId_ == CurrentThread::tid();}

private:
    void handleRead(); //wake up
    void doPendingFunctors(); //执行回调

    using ChannelList = std::vector<Channel*>;
    std::atomic_bool looping_; //原子操作，通过CAS 实现的
    std::atomic_bool quit_; //标志退出loop循环
    const pid_t threadId_; //记录当前线程的id；
    Timestamp pollReturnTime_;// poller返回发生事件的channels的时间点
    std::unique_ptr<Poller> poller_; //eventloop所管理的poller


    int wakeupFd_; // 主要作用是当mainloop获取一个新用户的channel ，通过轮询算法选择一个subreactor，通过该成员变量唤醒subreactor 处理 channel
    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activateChannels_;
    // Channel *currentActiveChannel_;

    std::atomic_bool callingPendingFunctors_; //标识当前loop是否有需要执行的回调操作
    std::vector<Functor> pendingFunctors_; //存储这个loop需要执行的回调操作
    std::mutex mutex_; //互斥锁 用户保护上面的vector容器的线程安全操作
};