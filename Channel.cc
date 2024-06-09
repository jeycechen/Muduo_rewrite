#include "Channel.h"
#include <sys/epoll.h>
#include "EventLoop.h"
#include "Logger.h"

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI; // 有数据可读 ｜| 有紧急数据可读
const int Channel::kWriteEvent = EPOLLOUT; //有数据需要写

Channel::Channel(EventLoop *Loop, int fd)
    : loop_(Loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false) {}

Channel::~Channel() {

}


// channelde tie方法在哪里调用？ 目的是什么？
// TcpConnection -> Channel channel底层绑定这个 防止tcpConnection被人remove 然后channe又去调用回调方法（tcp connection的成员函数）
// 但是tcpconnection 已经被人remove掉了，会出现为定义的行为，使用tie函数就是为了防止这个情况出现
// 一个tcpConncetion新连接创立的时候在 tcpConnection 中调用
void Channel::tie(const std::shared_ptr<void> &obj){
    tie_ = obj;
    tied_ = true;
}

/*
*当改变channel所表示的fd的events的事件之后， update负责在poller里面更改fd相应的事件epoll_ctl
* EventLoop -> ChannelList Polller
*/
void Channel::update(){
    //通过channel 所属的EventLoop ，调用poller的相应方法，注册fd的events事件
    loop_->updateChannel(this);
}

// 在channel所属的EventLoop中， 把当前的channel删除掉
void Channel::remove(){
    loop_->removeChannel(this);
}

// fd得到poller通知以后，处理事件的
void Channel::handleEvent(Timestamp receiveTime)
{
    
    if(tied_){
        /**
        * 这种模式常见于需要临时确保对象存活的场景。通过提升std::weak_ptr 为 std::shared_ptr，
        *  可以确保在使用对象的过程中对象不会被析构，从而避免悬空指针和潜在的未定义行为。
        */
        std::shared_ptr<void> guard = tie_.lock(); // 确保loop_ 属于的 Tcp Connection还活着，因为channel的函数都是TcpConncetion设置的
        if(guard){
            handleEventWithGuard(receiveTime);
        }
    }
    else{
        handleEventWithGuard(receiveTime);
    }
}

//根据poller通知的channel发生的具体事件，调用对应的回调 __FUNCTION__ __LINE__
void Channel::handleEventWithGuard(Timestamp receiveTime){ 
    LOG_INFO("channel handleEvent revents:%d\n", revents_);
    if((revents_ & EPOLLHUP) && !(revents_ && EPOLLIN)){
        if(closeCallback_){ // 出问题了，就关掉 //判断绑定器设置的回调函数对象是否还在存在
            closeCallback_();
        }
    }

    if (revents_ & EPOLLERR){
        if(errorCallback_){
            errorCallback_();
        }
    }

    if (revents_ & (EPOLLIN | EPOLLPRI)){
        if(readCallback_){
            readCallback_(receiveTime);
        }
    }

    if(revents_ & EPOLLOUT){
        if(writeCallback_){
            writeCallback_();
        }
    }

}