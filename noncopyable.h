#pragma once
/*
* nocopyable 被继承之后， 派生类对象可回忆正常构造和析构，
* 但是派生类对象无法进行拷贝构造和赋值操作
*/
class noncopyable{
public:
    noncopyable(const noncopyable&) = delete;
    void operator=(const noncopyable&) = delete;
protected:
    noncopyable() = default;
    ~noncopyable() = default;
};