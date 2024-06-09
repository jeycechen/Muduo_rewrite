#!/bin/bash

set -e

# 如果没有build目录就创建build目录
if [ ! -d "$PWD/build" ]; then
    mkdir "$PWD/build"
fi

rm -rf "$PWD/build/*"

cd "$PWD/build" && cmake .. && make 

# 回到项目根目录
cd ..

# 拷贝头文件到/usr/include/mymuduo so库到 /usr/lib
if [ ! -d /usr/include/mymuduo ]; then
    mkdir /usr/include/mymuduo
fi

for header in `ls *.h`
do
    cp $header /usr/include/mymuduo
done

cp "$PWD/lib/libmymuduo.so" /usr/lib

# 刷新一下动态库缓存
ldconfig
