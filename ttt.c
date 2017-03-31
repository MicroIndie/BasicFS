//
// Created by ht on 17-3-12.
//
#include "bfs.h"
#include <unistd.h>
#include <stdio.h>
#include <memory.h>

extern char disk_file_path[256];

int main(){

    char t[1000];
    strcpy(disk_file_path,"/var/tmp/test");
    bfs_getattr("/haha/hehe",t,t);
}

