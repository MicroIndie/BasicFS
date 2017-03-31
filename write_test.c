#include<stdio.h>
#include<unistd.h>
#include<fcntl.h>

int main(){
	int fd=open("tmp/test",O_RDWR|O_CREAT,S_IRWXU);
	if(fd==-1)
		perror("");
	char b='c';
	for(int i=0;i<4096*7;i++){
		write(fd,&b,1);
	}
	close(fd);
}
