#include "TcpServer.h"
#include "Logger.h"
#include <string.h>
#include "TcpConnection.h"

static EventLoop* CheckLoopNotNull(EventLoop *loop){
    if(loop == nullptr){
        LOG_FATAL("%s:%s:%d mainloop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpServer::TcpServer(EventLoop *loop, const InetAddress &listenAddr,const std::string &nameArg, Option option)
    : loop_(loop)
    , ipPort_(listenAddr.toIpPort())
    , name_(nameArg)
    , acceptor_(new Acceptor(loop, listenAddr, option == kReusePort))
    , threadPool_(new EventLoopThreadPoll(loop, name_))
    , connectionCallback_()
    , messageCallback_()
    , nextConnId_(1)
    , started_(0)
    {   

        // 当有新用户连接时，会执行 TcpServer::newConnection 的回调
        acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));
}

/**
 * mainloop （tcpserver） 做的事情
 * 根据 轮询算法选择一个subloop
 * 唤醒subloop
 * 把当前donnfd封装成channel 分发给subloop
 * 
*/
TcpServer::~TcpServer(){
    for(auto& item: connections_){
        TcpConnectionPtr conn(item.second); //这个局部的shared_ptr智能指针对象，出}，可以自动释放new出来的TcpConnection对象资源了；
        item.second.reset();//释放对象

        // 销毁连接
        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn)
        );
    }
}


//设置底层subloop的个数
void TcpServer::setThreadNum(int numThreads){
    threadPool_->setThread(numThreads);
}

//开启服务器监听
void TcpServer::start(){
    if(started_++ == 0){ //防止一个Tcpserver 被start多次 //防呆设计
        threadPool_->start(threadInitCallback_);
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
    
}
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr){
    EventLoop *ioloop = threadPool_->getNextLoop();
    char buf[64] = {0};
    snprintf(buf,sizeof buf,"-%s#%d",ipPort_.c_str(),nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;
    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from [%s] \n",
            name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());

    // 通过sockfd获取其绑定的本机ip地址和端口信息
    sockaddr_in local;
    ::bzero(&local, sizeof local);
    socklen_t addrlen = sizeof local;
    if(::getsockname(sockfd, (sockaddr*) &local, &addrlen) < 0){
        LOG_ERROR("sockets::LocalAddr \n");
    }

    InetAddress localAddr(local);

    TcpConnectionPtr conn(new TcpConnection(ioloop, connName, sockfd, localAddr, peerAddr)) ;
    connections_[connName] = conn;

    // 下面的回调都是用户设置给TcpServer -> TcpConnection -> channel -> poller -> notify channel调用回调函数
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallbacK(writeCompleteCallback_);

    //设置了如何关闭连接的回调
    conn->setCloseCallback(std::bind(
        &TcpServer::removeConnection, this, std::placeholders::_1)
    );

    // 直接调用
    ioloop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));

}
void TcpServer::removeConnection(const TcpConnectionPtr &conn){
    loop_->runInLoop(
        std::bind(&TcpServer::removeConnectionInLoop, this, conn)
    );
}
void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn){
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] -connection %s \n",
        name_.c_str(), conn->name().c_str());

    connections_.erase(conn->name());
    EventLoop *ioloop = conn->getLoop();
    ioloop->queueInLoop(
        std::bind(&TcpConnection::connectDestroyed, conn)
    );
}