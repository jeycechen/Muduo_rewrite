#include "Poller.h"
#include<stdlib.h>
//为什么使用一个单独的文件DefaultPoller来保存DefaultPoller的具体实现

/*
* 也就是为什么不直接在Poller.cc直接实现， 因为这样的话，就会出现基类（抽象类）
* 包含派生类的头文件，这样不好
* 所以额外 new一个新的文件DefaultPoller.cc， 在这个文件里面包含EpollPoller.h
* PollPoller.h 就好了
*/

#include "EPollPoller.h"
Poller* Poller::newDefaultPoller(EventLoop *loop){
    if(::getenv("MUDUO_USE_POLL")){
        return nullptr;     //生成poll的实例
    }
    else{
        return new EPollPoller(loop); //生成epoll的实例
    }
}