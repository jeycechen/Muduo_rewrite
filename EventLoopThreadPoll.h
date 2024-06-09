#pragma once 
#include "noncopyable.h"
#include <functional>
#include <string>
#include <vector>
#include <memory>
class EventLoop;
class EventLoopThread;

class EventLoopThreadPoll : noncopyable{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThreadPoll(EventLoop *baseLoop, const std::string &name);
    ~EventLoopThreadPoll();

    void setThread(int numThreads) {numThreads_ = numThreads;}

    void start(const ThreadInitCallback &cb = ThreadInitCallback());

    //如果工作在多线程中，baseloop 会默认以轮询的方式分配Channel给subloop
    EventLoop* getNextLoop();

    std::vector<EventLoop*> getAllLoops();

    bool started() const { return started_;}

    const std::string& name() const {return name_;}
private:
    EventLoop *baseLoop_; // EventLoop loop;
    std::string name_;
    bool started_;
    int numThreads_;
    int next_;
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop*> loops_;

};