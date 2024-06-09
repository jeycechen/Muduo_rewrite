#include "EventLoopThreadPoll.h"
#include "EventLoopThread.h"
#include <memory>

EventLoopThreadPoll::EventLoopThreadPoll(EventLoop *baseLoop, const std::string &name)
    : baseLoop_(baseLoop)
    , name_(name)
    , started_(false)
    , numThreads_(0)
    , next_(0) {}
EventLoopThreadPoll::~EventLoopThreadPoll(){
    // 不用delete loop 它是一个栈上的对象
}


void EventLoopThreadPoll::start(const ThreadInitCallback &cb){
    started_ = true;

    for(int i = 0; i < numThreads_; i++){
        char buf[name_.size() + 32]; // 使用线程池的名字 + 下标 去命名底层线程
        snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);
        EventLoopThread *t = new EventLoopThread(cb, buf);
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        loops_.push_back(t->startLoop()); //底层创建线程， 绑定一个新的EventLoop 并且返回该loop的地址
    }

    //整个服务端只有一个线程，运行着baseloop
    if(numThreads_ == 0 && cb){
        cb(baseLoop_);
    }
}

    //如果工作在多线程中，baseloop 会默认以轮询的方式分配Channel给subloop
EventLoop* EventLoopThreadPoll::getNextLoop(){
    EventLoop* loop = baseLoop_;

    if(!loops_.empty()){ //通过轮询获得下一个处理事件的loop； 
        loop = loops_[next_];
        ++next_;
        if(next_ >= loops_.size()){
            next_ = 0;
        }
    }
    return loop;
}

std::vector<EventLoop*> EventLoopThreadPoll::getAllLoops(){
    if(loops_.empty()){
        return std::vector<EventLoop*>(1, baseLoop_);
    }
    return loops_;
    
}