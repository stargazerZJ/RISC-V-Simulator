cmake_minimum_required(VERSION 3.20)
project(simulator)

file(GLOB_RECURSE sources "src/*.cpp")

set(CMAKE_CXX_STANDARD 23)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast -march=native")

include_directories(include)

#add_executable(alu src/alu.cpp)
#
## For debug build
#add_executable(modules src/modules.cpp)
#target_compile_definitions(modules PRIVATE _DEBUG)
#
#add_executable(interpreter src/interpreter.cpp)
#target_compile_definitions(interpreter PRIVATE _DEBUG)

add_executable(code src/interpreter.cpp)
