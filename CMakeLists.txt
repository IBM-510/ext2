cmake_minimum_required(VERSION 3.23)
project(ext2 C)

set(CMAKE_C_STANDARD 99)

add_executable(ext2 ext2.c ext2_fs.h ext2.h)
