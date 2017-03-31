#include "bfs.h"
#include <fuse.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>


static struct fuse_operations hello_oper = {
        .init           = bfs_init,
        .getattr	= bfs_getattr,
        .readdir	= bfs_readdir,
        .mkdir      = bfs_mkdir,
        .rmdir      = bfs_rmdir,
        .unlink     = bfs_unlink,
        .mknod      = bfs_mknod,
        //.chmod      = bfs_chmod,
        //.chown      = bfs_chown,
        //.rename     = bfs_rename,
        .open		= bfs_open,
        .read		= bfs_read,
        .write      = bfs_write,
        .utimens      = bfs_utime
};
extern char disk_file_path[256];

int main(int argc,char* argv[]) {
    struct bfs_super_block super_block;
    if(argc<3){
        printf("specify mount point and disk path\n");
        return -1;
    }
    strcpy(disk_file_path,argv[--argc]);
    int diskfd=open(disk_file_path,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
    read(diskfd,&super_block,sizeof(super_block));
    if(super_block.block_size!=BLOCK_SIZE){
        printf("initialize disk\n");
        printf("%d %d %d %d %d\n",super_block.inode_count,super_block.free_inode_count,super_block.block_count,super_block.free_block_count,super_block.block_size);
        lseek(diskfd,0,SEEK_SET);
        super_block.block_size=4096;
        super_block.inode_count=0;
        super_block.free_inode_count=1024;
        super_block.block_count=0;
        super_block.free_block_count=256;
        int tmp=write(diskfd,&super_block,sizeof(super_block));
        if(tmp==-1)
            fprintf(stderr,"error write superblock\n");
        lseek(diskfd,0,SEEK_SET);

        struct bfs_inode rootnode;
        time_t time1=time(NULL);
        rootnode.i_block[0]=0;
        rootnode.i_size=0;
        update_map(diskfd,0,DATABLOCK|SET);

        seek_to_inode(diskfd,0);
        tmp=write(diskfd,&rootnode,sizeof(rootnode));
        update_map(diskfd,0,INODE|SET);
        if(tmp==-1)
            fprintf(stderr,"error write rootnode\n");
        seek_to_data_block(diskfd,0);
    }
    close(diskfd);
    return fuse_main(argc,argv,&hello_oper,NULL);

}
