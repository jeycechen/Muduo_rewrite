#include "Buffer.h"
#include <sys/uio.h>
#include <errno.h>
#include <sys/unistd.h>
/**
 * 从fd上读取数据， poller LT模式
 * Buffer缓冲区是有大小的， 但是从fd上读数据的时候，却不知道tcp数据的最终大小
*/
ssize_t Buffer::readFd(int fd, int* saveErrno){ // 从fd读取数据 到 buffer  inputbuffer 会调用这个
    char extrabuf[65536] = {0}; //栈上的内存空间 64kB
    struct iovec vec[2];
    const size_t writeable = writeableBytes(); // 这是buffer底层缓冲区剩余的可写大小

    vec[0].iov_base = begin() + writerIndex_; // 
    vec[0].iov_len = writeable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    const int iovcnt = (writeable < sizeof extrabuf) ? 2 : 1; 
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if(n < 0){
        *saveErrno = errno;
    }
    else if (n < writeable){ // buffer的可写缓冲区已经够存储读出来的数据
        writerIndex_ += n;
    }
    else{   //标识extrabuf 也写入了数据
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writeable); //writerIndex_ 开始写 n - writable 大小的数据
    }
    return n; 
}


    //通过fd 发送数据 
ssize_t Buffer::writeFd(int fd, int* saveErrno){ //把  缓冲区的数据发送出去，  outputbuffer会调用这个
    ssize_t n = ::write(fd, peek(), readableBytes());
    if(n < 0){
        *saveErrno = errno;
    }
    return n;
}
