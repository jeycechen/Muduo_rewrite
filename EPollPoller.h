#pragma once 
#include "Poller.h"
#include<vector>
#include<sys/epoll.h>
#include"Timestamp.h"
/**
 * epoll 的使用
 * epoll_create
 * epoll_ctl add/mod/del;
 * epoll_wait
*/
class Channel;

/// @brief poller类的具体实现 EPollPoller类
class EPollPoller : public Poller{
public:
    EPollPoller(EventLoop *loop);
    ~EPollPoller() override;

    //重写基类Poller的抽象方法
    Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;

private:
    static const int kInitEventListSize = 16; //初始的events_的大小

    //填写活跃的连接
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
    
    //更新channel通道
    void update(int operation, Channel *channel);
    using EventList = std::vector<struct epoll_event>;
    
    int epollfd_;
    EventList events_;
};