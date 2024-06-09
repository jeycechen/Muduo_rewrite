#pragma once
#include <string>
#include"noncopyable.h"
// using namespace std;
//定义日志的级别 INFO ERROR(不影响正常执行) FATAL DEBUG

//LOG_INFO("%s %d", arg1 , arg2)


/**
 * #define 的换行符后面不能接空格 否则会报错
*/

#define LOG_INFO(logmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::instance(); \
        logger.setLogLevel(INFO); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \
    } while(0)

#define LOG_ERROR(logmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::instance(); \
        logger.setLogLevel(ERROR); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \
    }while(0)

#define LOG_FATAL(logmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::instance(); \
        logger.setLogLevel(FATAL); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \
        exit(-1); \
    }while(0)

//Fatal-error 打印完信息直接core dump
#ifdef MUDEBUG
#define LOG_DEBUG(logmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::instance(); \
        logger.setLogLevel(DEBUG); \
        char buf[1024] = {0}; \
        snprintf(buf, 1024, logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \
    }while(0)
#else
#define LOG_DEBUG(logmsgFromat, ...)
#endif
enum LogLevel{
    INFO, //普通信息
    ERROR, //错误信息
    FATAL, //core dump信息
    DEBUG, //调试信息
};

//输出一个日志类
class Logger: noncopyable{
public:
    //获取日志唯一的示例对象
    static Logger& instance();

    //设置日志级别
    void setLogLevel(int level);

    void log(std::string msg);
private:
    int logLevel_;
    Logger(){}
};