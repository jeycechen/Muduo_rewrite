#include <mymuduo/TcpServer.h>
#include <string>
#include <mymuduo/Logger.h>
#include <functional>
#include <iostream>
class EchoServer{
public:
    EchoServer(EventLoop *loop,
        const InetAddress &addr,
        const std::string &name)
        :server_(loop,addr,name)
        ,loop_(loop){
            //注册回调函数
            server_.setConnectionCallback(
                std::bind(&EchoServer::onConnection, this, std::placeholders::_1)
            );

            server_.setMessageCallback(
                std::bind(&EchoServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
            );

            //设置合适的loop线程数量
            server_.setThreadNum(3);

        }
    void start(){
        server_.start();
    }
private:
    void onConnection(const TcpConnectionPtr& conn){
        if(conn->connected()){
            LOG_INFO("new connection : %s \n", conn->peerAddress().toIpPort().c_str());
        }
        else{
            LOG_INFO("connection close : %s \n", conn->peerAddress().toIpPort().c_str());
        }
    }

    void onMessage(const TcpConnectionPtr& conn,
        Buffer *buf, Timestamp time){
            std::string msg = buf->retrieveAllAsString();
            // std::cout << msg << std::endl;
            conn->send(msg);
            conn->shutdown(); //关闭写端 EPOLLHUP -》 closeCallback()
        }
    
    EventLoop *loop_;
    TcpServer server_;
};

int main(){
    EventLoop loop;
    InetAddress addr(8000);
    EchoServer server(&loop, addr, "ECHO~"); //Acceptor non-blocking listenfd create bin
    server.start();  //listen loopthread listenfd, acceptChannel mainLoop

    loop.loop(); // 启动mainLoop的底层Poller
    return 0;
}