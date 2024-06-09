#pragma once
#include "Logger.h"
#include <vector>
#include <string>

//网络库底层的缓冲器类型
/**
 *  +----------------------+--------------------------+---------------------+
 *  ｜ prependable bytes   | readable bytes (CONTENT)  |  writeable bytes    |
 *  +----------------------+--------------------------+---------------------+
 *  |                       |                           |                   |
 *  0      <=          readerIndex    <=        writerIndex   <=          size
*/
class Buffer{
public:
    static const size_t kCheapPrepend = 8;
    static const size_t KInitialSiez = 1024;

    explicit Buffer(size_t initialSize = KInitialSiez)
        : buffer_(kCheapPrepend + initialSize)
        , readerIndex_(kCheapPrepend)
        , writerIndex_(kCheapPrepend)
        {}
    
    ~Buffer() {};

    size_t readableBytes() const {return writerIndex_ - readerIndex_;}

    size_t writeableBytes() const { return buffer_.size() - writerIndex_;}

    size_t prependableBytes() const {return readerIndex_;}

    void retrieve(size_t len){ // 检索， 调整index
        if(len < readableBytes()){
            readerIndex_ += len; //应用只读取了只读缓冲区的一部分，就是len，还剩下 readerIndex_ += len 到 writerIndex_这些数据没读
        }
        else{ // len == readableBytes();
            retrieveAll();
        }
    }

    void retrieveAll(){
        readerIndex_ = writerIndex_ = kCheapPrepend;
    }
    // 把 onMessage函数上报的buffer数据,转换成string类型的数据返回
    std::string retrieveAllAsString(){
        return retrieveAllAsString(readableBytes()); //应用可读取数据的长度
    }

    std::string retrieveAllAsString(size_t len){
        std::string result(peek(), len);
        retrieve(len); //上面一句把缓冲区可读的数据，已经读取出来，这一步需要对缓冲区Index 更新操作
        return result;
    }

    void ensureWriteableBytes(size_t len){
        if(writeableBytes() < len){
            makeSpace(len); // 扩容函数
        }
        else{
            return;
        }
    }

    // 把【data， data+len] 上的数据，添加到writeable缓冲区当中
    void append(const char *data, size_t len){
        ensureWriteableBytes(len);
        std::copy(data, data+len, beginWrite());
        writerIndex_ += len;
    }

    char *beginWrite(){
        return begin() + writerIndex_;
    }
    const char * beginWrite() const{
        return begin() + writerIndex_;
    }
    //从fd上读取数据
    ssize_t readFd(int fd, int* saveErrno);

    //通过fd 发送数据
    ssize_t writeFd(int fd, int* saveErrno);
private:
    char* begin(){
        //  获取首地址指针
        return &(*buffer_.begin());
    }

    const char* begin() const{
        //  获取首地址指针
        return &(*buffer_.begin());
    }
     
    //返回缓冲区中可读数据的起始地址
    const char* peek() const{
        return begin() + readerIndex_;
    }

    void makeSpace(size_t len){
        /**
         *  kCheapPrepend | reader |  writer
         *  kCheapPrepend |       len                   |
        */
       if(writeableBytes() + prependableBytes() < len + kCheapPrepend){
            buffer_.resize(writerIndex_ + len); //如果现在的空间腾出来也不够，那就没有必要腾出来，直接往后叠加空间就好了
       }
       else{ // 是否需要搬移数据； 如果现在的buffersize空间是足够的，那么就把已经读完的数据腾出来，-> 搬移数据
            size_t readable = readableBytes();
            std::copy(begin() +readerIndex_,
                      begin() + writerIndex_,
                      begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
       }
    }
    
    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};