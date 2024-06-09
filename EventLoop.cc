#include "EventLoop.h"
#include "Logger.h"
#include <unistd.h>
#include <fcntl.h>
#include<sys/eventfd.h>
#include<errno.h>
#include"Poller.h"
#include"Channel.h"
// 防止一个线程创建多个EventLoop
__thread EventLoop *t_loopThisThread = nullptr;

//定义默认的Poller IO复用接口的超时时间
const int kPollTimeMs = 10000;

//创建wakeup fd 用来唤醒subreactor 处理新来的channel
int createEventfd(){
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if(evtfd < 0){
        LOG_FATAL("eventfd error:%d",errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , callingPendingFunctors_(false)
    , threadId_(CurrentThread::tid())
    , poller_(Poller::newDefaultPoller(this))
    , wakeupFd_(createEventfd())
    , wakeupChannel_(new Channel(this, wakeupFd_)) 
    {
        LOG_DEBUG("EventLoop created %p in thread %d \n",this,threadId_);
        if(t_loopThisThread){
            LOG_FATAL("Another EventLoop %p exists in this thread %d\n", t_loopThisThread, threadId_);
        }
        else{
            t_loopThisThread = this;
        }

        //设置wakeupfd的事件类型以及发生事件的回调操作
        wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
        
        //每一个eventloop都将监听wakeupChannel的EpollIN读取事件了；
        wakeupChannel_->enableReading(); // 监听 可读事件
    }

EventLoop::~EventLoop(){
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopThisThread = nullptr;
}

void EventLoop::loop(){
    looping_ = true;
    quit_ = false;
    LOG_INFO("EventLoop %p start looping \n", this);
    
    while(!quit_){
        activateChannels_.clear();
        //监听两类fd ，一种是client 的fd， 一种是wakeupfd
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activateChannels_);

        for(Channel *channel: activateChannels_){
            // currentActiveChannel_ = channel;
            // poller 监听哪些channel发生事件了，然后上报给EventLoop，通知channel处理相应的事件；
            channel->handleEvent(pollReturnTime_);
        }

        // 执行当前EventLoop 事件循环需要处理的回调操作？

        /**
         * IO线程 mainLoop accept (channel 打包了fd 和 回调函数)  
         * mainLoop 事先注册一个回调cb 需要subloop来执行
         * 
         * wakeup subloop后， 执行之前mainloop注册 的cb操作 也就是新的channel 新的用户
         * 
        */
        doPendingFunctors();    //wake up 没有设置回调函数，它的回调函数由这个函数负责执行
    }

    LOG_INFO("EventLoop %p stop looping\n", this);
    looping_ = false;
}

// 退出事件循环， 1.loop在自己的线程中调用quit 
//              2.在非loop的线程中，调用loop的quit


/**
 *                mainLoop
 * 
 *                                        None =============================== 生产者-消费者的 线程安全的 队列
 * 
 * subLoop1       subLoop2        subLoop3
*/
void EventLoop::quit(){
    quit_ = true;

    if(!isInLoopThread()){ //如果是在其他线程中 调用了quit ，在subloop（worker）中调用了mainloop的quit
        wakeup();
    }
}

void EventLoop::handleRead(){
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if(n != sizeof one){
        LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8", n);
    }
}


//在当前loop中执行cb
void EventLoop::runInLoop(Functor cb){
    if(isInLoopThread()){ // 在当前的loop线程中 执行callback
        cb();
    }
    else
    {
        // 在非当前loop线程中执行cb， 就需要唤醒loop所在线程，执行cb
        queueInLoop(cb);
    }
}
//把cb放入队列中，唤醒loop所在的线程，执行cb
void EventLoop::queueInLoop(Functor cb){
    {   //可能会有多个loop访问这个vector，所以需要加锁
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }
    //唤醒相应的需要执行 上面回调的函数的Loop线程
    if(!isInLoopThread() || callingPendingFunctors_){  // callingPendingFunctors_待解释 当前线程正在执行回调
        wakeup(); //唤醒loop所在线程；
    }
    /**
     *  1、唤醒其他loop执行回调
     *  2、确实是本loop执行回调，但是本loop正在执行回调，也就是callPendingFunctors_ = true, 那么就唤醒，
     *    这样 本loop的下一轮循环 就不会被poll阻塞，相当于poll已经有了一个可以读写的fd了，于是直接返回 就不会被阻塞住
    */
}

//唤醒loop所在的线程 向wakeupfd_ 写入数据, wakeupChannel就发生可读事件
//当前loop线程就会被唤醒
void EventLoop::wakeup(){
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_,&one, sizeof one);
    if(n != sizeof one){
        LOG_ERROR("EventLoop::wakeup write %lu instead of 8 \n", n);
    }
}

//eventloop的方法 =》 调用poller的方法
void EventLoop::updateChannel(Channel *channel){
    poller_->updateChannel(channel);
}
void EventLoop::removeChannel(Channel *channel){
    poller_->removeChannel(channel);
}
bool EventLoop::hasChannel(Channel *channel){
    return poller_->hasChannel(channel);
}


//执行回调
void EventLoop::doPendingFunctors(){
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_); 
        functors.swap(pendingFunctors_); // 提升 并发！ 减少锁的粒度
    }
    for(const Functor& functor: functors){
        functor(); //执行当前loop需要执行的回调操作
    }
    
    callingPendingFunctors_ = false;
} 

