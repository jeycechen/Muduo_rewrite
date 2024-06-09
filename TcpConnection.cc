#include "Logger.h"
#include "TcpConnection.h"
#include"Socket.h"
#include "Channel.h"
#include "EventLoop.h"
#include <functional>
#include <errno.h>
#include"Logger.h"
#include<sys/socket.h>
#include<sys/types.h>
#include "InetAddress.h"
#include "strings.h"
#include <string>

static EventLoop* CheckLoopNotNull(EventLoop *loop){
    if(loop == nullptr){
        LOG_FATAL("%s:%s:%d mainloop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop,
                    const std::string &nameArg,
                    int sockfd,
                    const InetAddress &localAddr,
                    const InetAddress &peerAddr)
                    : loop_(CheckLoopNotNull(loop))
                    , name_(nameArg)
                    , state_(kConnecting)
                    , reading_(true)
                    , socket_(new Socket(sockfd))
                    , channel_(new Channel(loop, sockfd))
                    , localAddr_(localAddr)
                    , peerAddr_(peerAddr)
                    , highWaterMark_(64*1024*1024) //64MB
                    {   

                        //给channel设置响应的回调函数， poller 给 channel通知响应的事件发生了，就会调用这些回调函数
                        channel_->setReadCallback(std::bind(&TcpConnection::handleRead,this,std::placeholders::_1));
                        channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
                        channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
                        channel_->setErrorCallback(std::bind(&TcpConnection::handleError,this));

                        LOG_INFO("TcpConncetion:::ctor[%s] ad fd = %d \n", name_.c_str(), sockfd);
                        socket_->setKeepAlive(true); 
                    }
TcpConnection::~TcpConnection(){
    LOG_INFO("TcpConncetion:::dtor[%s] ad fd=%d state = %d\n", name_.c_str(), channel_->fd(),(int)state_);
}

void TcpConnection::send(const std::string &buf){
    if(loop_->isInLoopThread()){
        sendInLoop(buf.c_str(), buf.size());
    }
    else{
        loop_->runInLoop(std::bind(
            &TcpConnection::sendInLoop, this, buf.c_str(),buf.size()
        ));
    }
}
/**
 * 发送数据，应用写的快，而内核发送数据满，需要把待发送数据写入缓冲区，
 * 而且需要处理高水位
*/
void TcpConnection::sendInLoop(const void *message, ssize_t len){
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;
    
    //之前调用过该connection的shutdown，不能再进行发送了, 连接已经断开
    if(state_ == kDisconnected){
        LOG_ERROR("disconnected, give up writing! \n");
        return;
    }

    if(!channel_->isWriting() && outputBuffer_.readableBytes() == 0){
        // 表示 channel 第一次开始写数据， 而且缓冲区没有待发送数据
        nwrote = ::write(channel_->fd(), message, len);
        if(nwrote > 0){
            remaining = len - nwrote;
            //   数据一次性发送完成， 就不用再给channel设置epollout事件感兴趣
            if(remaining == 0 && writeCompleteCallback_){
                loop_->queueInLoop(std::bind(
                    writeCompleteCallback_,shared_from_this()
                ));
            }
        }
        else{ //出错
            nwrote = 0;
            if(errno != EWOULDBLOCK){
                LOG_ERROR("TcpConnection::sendInLoop");
                if(errno == EPIPE || errno == ECONNRESET){ //SIGPIPE RESET
                    faultError = true;
                }
            }
        }
    }
    /*说明当前这一次write，并没有把数据全部发送出去，剩余的数据
    * 需要保存到缓冲区中，然后给channel注册 epollout事件 待下次发送
    * poller 发现tcp的发送缓冲区有空间，就会通知相应的sock->channel，调用writeCallback_回调方法
    * 也就是调用TcpConnection::handleWrite方法，把发送缓冲区中的数据全部发送完成
    */
    if(!faultError && remaining > 0){ 
        // 目前发送缓冲区剩余的待发送的数据长度
        size_t oldLen = outputBuffer_.readableBytes();
        // 这里的判断 就是原先缓冲区需要发送的数据 加上 这次剩余的数据如果大于的高水位标识 就执行高水位回调函数
        if(oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_){
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        }
    }
    outputBuffer_.append((char*)message + nwrote, remaining); // 剩余的数据刷入缓存，以便下次发送
    if(!channel_->isWriting()){  //现在缓冲区有数据，所以需要设置对写事件感兴趣
        channel_->enableWriting(); // 注册channel的写事件
    }
}

// 连接建立
void TcpConnection::connectEstablished(){
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading(); //向poller注册channel的eppollin事件

    //连接建立，调用回调
    connectionCallback_(shared_from_this());
}

// 连接销毁
void TcpConnection::connectDestroyed(){
    if(state_ == kConnected){
        setState(kDisconnected);
        channel_->disableAll(); //设置channel对所有事件都不感兴趣，从poller中delete掉
        connectionCallback_(shared_from_this());
    }
    channel_->remove(); //把channel从poller 中删除掉
}


void TcpConnection::handleRead(Timestamp receiveTime){
    int saveErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &saveErrno);

    if(n > 0){
        //  已建立连接的用户，有可读事件发生了，调用用户传入的回调操作onMessage
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if(n==0){
        handleClose();
    }
    else{
        errno = saveErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}
void TcpConnection::handleWrite(){

    if(channel_->isWriting()) {
        //判断是否可写  
        int saveErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &saveErrno);

        if(n > 0){
            outputBuffer_.retrieve(n);
            if(outputBuffer_.readableBytes() == 0){ //表示已经发送完毕了
                channel_->disableWriting(); //取消关注写事件 已经写完了，不需要写了
                LOG_DEBUG("TcpConnection::handleWrite disableWeiting \n");
                if(writeCompleteCallback_){
                    loop_->queueInLoop( // 为什么是queueInLoop？
                        std::bind(&TcpConnection::writeCompleteCallback_, shared_from_this()));
                }

                if(state_ == kDisconnecting){ //写完数据判断是否正在关闭
                    shutdownInloop();
                }
            }
        }
        else{
             //出错
             LOG_ERROR("TcpConnection::hanleWrite error:%d \n", saveErrno);
        }
    }
    else{ //不可写
        LOG_ERROR("TcpConnection::handleWrite fd=%d is down, no more writing \n", channel_->fd());
    }
}

// poller -> channel::closeCallback -> TcpConnection::handleClose -> TcpServer::removeConnection
void TcpConnection::handleClose(){
    LOG_INFO("fd=%d state=%d \n",channel_->fd(),(int)state_);
    setState(StateE::kDisconnecting);
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr); //执行连接关闭的回调
    closeCallback_(connPtr); //关闭连接的回调  执行的是TcpServer的remove Connection回调
}
void TcpConnection::handleError(){
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if(::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0){
        err = errno;
    }
    else{
        errno = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s -- SO_ERROR:%d \n", name_.c_str(), err);
}



void TcpConnection::shutdown(){
    if(state_ == kConnected){
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInloop, this));
    }
}
void TcpConnection::shutdownInloop(){
    if(!channel_->isWriting()){ //说明outputbufer中的数据已经全部发送完成
        socket_->shutdownWrite(); //关闭写端
    }
}