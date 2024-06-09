cmake_minimum_required(VERSION 3.0)
project(mymuduo)

# cmake => makefile => make 

#mymuduo 最终编译为SO动态库 ，设置动态库的路径
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

#设置调试信息
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11 -fPIC")

## 定义参与编译的源文件
aux_source_directory(. SRC_LIST)

#编译生成动态库mymuduo
add_library(mymuduo SHARED ${SRC_LIST})