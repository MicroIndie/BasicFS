
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

struct test{
    short a;
    short b;
    int c;
    int d;
    int e;
};

int main(){
    struct test t;
    int fd=open("test",O_WRONLY,S_IRWXU|S_IRGRP|S_IROTH);

    read(fd,&t,sizeof(t));
    printf("%hd %hd %d %d %d",t.a,t.b,t.c,t.d,t.e);

    t.a=0;
    t.b=256;
    t.c=4;
    t.d=1024;
    t.e=4096;

    lseek(fd,0,SEEK_SET);
    write(fd,&t,sizeof(t));
    close(fd);
}