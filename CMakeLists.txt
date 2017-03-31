cmake_minimum_required(VERSION 3.6)
project(BFS)

set(CMAKE_CXX_STANDARD 11)

INCLUDE_DIRECTORIES(/home/ht/workspace/linux/BFS/fuse-3.0.0/include)
LINK_DIRECTORIES(/home/ht/workspace/linux/BFS/fuse-3.0.0/lib)

set(SOURCE_FILES main.c bfs.h bfs_functs.c disk_util.c )
add_executable(BFS ${SOURCE_FILES})
add_executable(testwrite test.c)
add_executable(ttt ttt.c bfs_functs.c disk_util.c)