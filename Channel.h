#pragma once
#include "noncopyable.h"
#include<functional>
#include"Timestamp.h"
#include<memory>
/*
* EventLoop Channel Poller 之间的关系 
* EventLoop 包含 channel poller 在模型图上对应Demultiplex
* channel 理解为通道，封装了sockfd和其感兴趣的event ，如EPOLLIN， EPOLLOUT
* 还绑定了poller 的 返回事件
*/

class EventLoop;


class Channel: noncopyable{
public:
    // typedef std::function<void()> EventCallback();
    using EventCallback  = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;
    Channel(EventLoop *Loop, int fd);
    ~Channel();

    //fd得到poller通知以后，处理事件的
    void handleEvent(Timestamp receiveTime);

    //设置回调函数对象
    void setReadCallback(ReadEventCallback cb) {readCallback_ = std::move(cb);}
    void setWriteCallback(EventCallback cb) {writeCallback_ = std::move(cb);}
    void setCloseCallback(EventCallback cb) {closeCallback_ = std::move(cb);}
    void setErrorCallback(EventCallback cb) {errorCallback_ = std::move(cb);}

    //防止当channel被手动remove掉， channel还在执行回调操作
    void tie(const std::shared_ptr<void>&);
    int fd() const {return fd_;}
    int events() const {return events_;}
    void set_revents(int revt) {revents_ = revt;}

    //设置fd 相应的事件状态
    void enableReading() {events_ |= kReadEvent;update();}
    void disableReading() {events_ &= ~kReadEvent;update();}
    void enableWriting() {events_ |= kWriteEvent; update();}
    void disableWriting() {events_ &= !kWriteEvent; update();}
    void disableAll() {events_ = kNoneEvent; update();}

    //返回fd当前的事件状态
    bool isNoneEvent() const {return events_ == kNoneEvent;}
    bool isWriting() const {return events_ == kWriteEvent;}
    bool isReading() const {return events_ == kReadEvent;}

    int index() const { return index_;}
    void set_index(int idx) {index_ = idx;}

    // one loop per thread
    EventLoop* ownerLoop(){return loop_;}
    void remove();
private:
    void update();
    void handleEventWithGuard(Timestamp receiveTime);
    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop *loop_; //事件循环
    const int fd_; // fd， POller监听的对象
    int events_; //注册fd感兴趣的事件
    int revents_; // poller 返回的的具体的发生的事件
    int index_; // 描述当前这个channel在poller中的状态 从未添加kNew ，添加过了kAdded ，已经从poller中删除kDeleted

    std::weak_ptr<void> tie_;
    bool tied_;

    //因为channel通道里面能够获知fd最终发生的具体的事件revents，
    //所以它负责调用对应的回调函数
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};