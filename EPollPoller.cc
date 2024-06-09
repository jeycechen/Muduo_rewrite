#include "EPollPoller.h"
#include "Logger.h"
#include <errno.h>
#include "Channel.h"
#include "strings.h"
#include <unistd.h>
//channel 未添加到Poller中
const int kNew = -1; //channel 的成员index_ = -1
//channel 已添加到Poller 中
const int kAdded = 1;
//channel 从poller中删除
const int kDeleted = 2;

EPollPoller::EPollPoller(EventLoop *loop) 
    : Poller(loop)
    , epollfd_(::epoll_create1(EPOLL_CLOEXEC)) /*EPOLL_CLOEXEC 标志用于设置 close-on-exec (FD_CLOEXEC) 文件描述符标志。
                                                当设置了 FD_CLOEXEC 标志的文件描述符在执行 exec 系列函数时，会自动关闭该文件描述符。
                                                这有助于防止文件描述符泄漏到子进程中，提升安全性。*/
    , events_(kInitEventListSize) {
    if(epollfd_ < 0){ //创建监听的epoll文件描述符号
        LOG_FATAL("epoll_create error:%d \n", errno);
    }
}

EPollPoller::~EPollPoller(){
    ::close(epollfd_);
}

//channel update -> eventloop updatechannel -> EPollPoller updateChannel
/**
 *              EventLoop 包含了 Channel  + Poller
 *        ChannelList   Poller
 *                      ChannelMap <fd, Channel*>
*/
void EPollPoller::updateChannel(Channel *channel){
    const int index = channel->index();
    LOG_INFO("func=%s fd=%d events=%d, index=%d \n",__FUNCTION__, channel->fd(), channel->events(), index);
    if(index == kNew || index == kDeleted){
        if(index == kNew)
        {
            int fd = channel->fd(); //从来没添加过，则添加到map
            channels_[fd] = channel;
        }
        
        // index = kDeleted 以前添加过，但是现在想重新添加，设置为k_Added; 这个时候channels_内部还有这个channel的信息，所以不需要在上面添加
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel); //往epoll实例中注册这个channel->fd
    }
    else{ // 表示channel 已经在poller 注册过了
        int fd = channel->fd();
        if(channel->isNoneEvent()){  //说明对任何事情都不感兴趣，那么停止监听
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }
        else{
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels){
    //实际上使用LOG_DEBUG更合理
    LOG_INFO("func=%s => fd total count:%lu",__FUNCTION__, channels_.size());

    /**
     *  events_.begin(); 迭代器
     *  *events_.begin(); 具体的值
     * &*events_.begin(); 数组首元素的地址
    */
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    int saveErrno = errno;  //多个线程在执行poll ，而errno是全局的 为了保护errno， 先保存下来
    Timestamp now(Timestamp::now());

    if(numEvents > 0){
        LOG_INFO("%d events happened\n", numEvents);
        fillActiveChannels(numEvents, activeChannels);
        if(numEvents == events_.size()){ // 发生事件的数量和events_的大小一样 可能有更多的事件发生了，但是容量不够，这里使用的是LT模式，下次还会上报，需要扩容
            events_.resize(events_.size() * 2);
        }
    }
    else if(numEvents == 0){ // 超时了
        LOG_DEBUG("func=%s timeout! \n", __FUNCTION__);
    }
    else{
        if(saveErrno != EINTR){ //如果报错不是外部中断，那就说明是真的发生错误了，打印错误日志
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll err!");
        }
    }
    return now;
}


//填写活跃的连接
void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const{
    for(int i=0;i<numEvents;i++){
        Channel *channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel); // 这样EventLoop 就拿到了它的poller给它返回的所有发生事件的channel列表了
    }
}
    
//更新channel通道 epoll_ctl add/mod/del 
void EPollPoller::update(int operation, Channel *channel){
    epoll_event event;
    int fd = channel->fd();
    bzero(&event, sizeof event);

    event.events = channel->events();
    event.data.fd = fd;
    event.data.ptr = channel;
    

    if(::epoll_ctl(epollfd_, operation, fd, &event) < 0){
        if(operation == EPOLL_CTL_DEL){
            LOG_ERROR("epoll_ctl del error:%d\n", errno);
        }
        else{
            LOG_FATAL("epoll_ctl add/mod error:%d\n", errno);
        }
    }
}

//从poller中 删除 channel
void EPollPoller::removeChannel(Channel *channel){
    int fd = channel->fd();
    channels_.erase(fd); //从map表中删除
    LOG_INFO("func=%s fd=%d \n",__FUNCTION__, channel->fd());
    int index = channel->index();
    if(index == kAdded){ //如果还有添加的epoll监听状态，需要修改epoll fd
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew); //设置成从来没有添加过的状态 ，初始状态
}
