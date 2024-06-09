#pragma once
#include"noncopyable.h"
#include <memory>
#include <string>
#include <atomic>
#include "InetAddress.h"
#include "Callbacks.h"
#include "Timestamp.h"
class Channel;
class EventLoop;
class Socket;
#include"Buffer.h"

/**
 *  TcpServer -> Acceptor -> 有一个新用户连接， 通过accept函数拿到connfd
 * -> TcpConnection 设置回调 -> channel 封装回调 fd -> pooler -> channel 的回调操作
 * 
*/
class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop *loop,
                    const std::string &nameArg,
                    int sockfd,
                    const InetAddress &localAddr,
                    const InetAddress &peerAddr);
    ~TcpConnection();

    EventLoop* getLoop() const {return loop_;}
    const std::string& name() const { return name_;}
    const InetAddress& localAddress() const {return localAddr_;}
    const InetAddress& peerAddress() const {return peerAddr_;}

    bool connected() const {return state_ == kConnected;}
    
    void shutdown();


    void setConnectionCallback(const ConnectionCallback& cb){
        connectionCallback_ = cb;
    }

    void setMessageCallback(const MessageCallback& cb){
        messageCallback_ = cb;
    }

    void setWriteCompleteCallbacK(const WriteCompleteCallback& cb){
        writeCompleteCallback_ = cb;
    }

    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb){
        highWaterMarkCallback_ = cb;
    }

    void setCloseCallback(const CloseCallback& cb){
        closeCallback_  = cb;
    }

    // 连接建立
    void connectEstablished();

    // 连接销毁
    void connectDestroyed();
    
    void send(const std::string &buf);

private:

    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();


    void sendInLoop(const void *message, ssize_t len);
    void shutdownInloop();

    enum StateE {kDisconnected, kConnecting, kConnected, kDisconnecting};
    void setState(StateE state){state_ = state;}

    EventLoop *loop_; // 这里不是baseloop，因为TcpConnection都是在subloop管理的
    const std::string name_;
    std::atomic_int state_;
    bool reading_;


    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;


    const InetAddress localAddr_;
    const InetAddress peerAddr_;


    ConnectionCallback connectionCallback_; //有新连接时的回调
    MessageCallback messageCallback_; //有读写消息的回调
    WriteCompleteCallback writeCompleteCallback_; //消息发送完成的回调
    CloseCallback closeCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;

    size_t highWaterMark_;
    Buffer inputBuffer_; //接收数据的缓冲区
    Buffer outputBuffer_; // 发送数据的缓冲区

};