#pragma once
#include "Acceptor.h" 
#include "EventLoop.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include <functional>
#include "EventLoopThreadPoll.h"
#include "Callbacks.h"
#include <string>
#include <memory>
#include <unordered_map>
#include "TcpConnection.h"
#include "Buffer.h"
//对外的服务器编程使用的类
/**
 * 
*/
class TcpServer : noncopyable{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;
    using ConnetionMap = std::unordered_map<std::string,  TcpConnectionPtr>;

    enum Option{
        kNoReusePort,
        kReusePort,
    };

    TcpServer(EventLoop *loop, const InetAddress &listenAddr, const std::string &nameArg,Option option = kNoReusePort);
    ~TcpServer();

    void setThreadInitCallback(const ThreadInitCallback &cb) {threadInitCallback_ = cb;}
    void setConnectionCallback(const ConnectionCallback &cb) {connectionCallback_ = cb;}
    void setMessageCallback(const MessageCallback &cb) {messageCallback_ = cb;}
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb;}

    //设置底层subloop的个数
    void setThreadNum(int numThreads);

    //开启服务器监听
    void start();

    
    
private:
    void newConnection(int sockfd, const InetAddress &peerAddr);
    void removeConnection(const TcpConnectionPtr &conn);
    void removeConnectionInLoop(const TcpConnectionPtr &conn);

    EventLoop *loop_; //用户定义的mainLoop

    const std::string ipPort_;
    const std::string name_;
    
    std::unique_ptr<Acceptor> acceptor_; //运行在mainloop的accceptor ，任务就是监听新用户的数量
    std::shared_ptr<EventLoopThreadPoll> threadPool_; //one loop per thread

    ConnectionCallback connectionCallback_; //有新连接时的回调
    MessageCallback messageCallback_; //有读写消息的回调
    WriteCompleteCallback writeCompleteCallback_; //消息发送完成的回调

    ThreadInitCallback threadInitCallback_; //loop 线程初始化的回调
     
    std::atomic_int started_;

    int nextConnId_;
    ConnetionMap connections_; //保存所有连接
};