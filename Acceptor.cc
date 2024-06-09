#include "Acceptor.h"
#include <sys/socket.h>
#include <sys/types.h>
#include "Logger.h"
#include <errno.h>
#include "InetAddress.h"
#include <unistd.h>

static int createNonblocking(){
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if(sockfd < 0){
        LOG_FATAL("%s:%s:%d listen socket create err:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
    }
    return sockfd;
}
Acceptor::Acceptor(EventLoop *loop, const InetAddress& listenAddr, bool reuseport)
    : loop_(loop)
    , acceptSocket_(createNonblocking()) // 创建套接字
    , acceptChannel_(loop, acceptSocket_.fd())
    , listenning_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr);// 绑定

    //TcpSercer::start() Acceptor.listen 有新用户连接 要执行一个回调， connfd =》channel =》subloop

    //baseLoop => acceptChannel_(listenfd) => 对应的loop执行channel的回调
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));

}
Acceptor::~Acceptor(){
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}


void Acceptor::listen(){
    listenning_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading(); //  注册到poller 里面
}


//listenfd 有事件发生了，就是有新用户连接了

void Acceptor::handleRead(){
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if(connfd >= 0){
        if(newConnectionCallback_){
            newConnectionCallback_(connfd, peerAddr); // 轮询找到subLoop， 唤醒，分发当前新客户端的Channel
        }
        else{
            ::close(connfd);
        }
    }
    else{
        LOG_ERROR("%s:%s:%d listen socket accept err:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
        if(errno == EMFILE){
            LOG_ERROR("%s:%s:%d sockfd reached limit err:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
        }
    }
}