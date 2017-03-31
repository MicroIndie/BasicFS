//
// Created by ht on 17-3-8.
//

#ifndef BFS_BFS_H
#define BFS_BFS_H

#define FUSE_USE_VERSION 30

#define BLOCK_SIZE 4096
#define SUPER_BLOCK_SIZE 16
#define INODE_BITMAP_SIZE 128
#define DATA_BITMAP_SIZE 2048
#define INODE_SIZE 64
#define DIRENT_SIZE 256
#include <fuse.h>
#include <fcntl.h>
#include <utime.h>

enum BFS_File_Type{
    BFS_DIR=1,
    BFS_FILE=2
};
enum BITMAP_OP{
    SET=1,
    EREASE=2,
    INODE=4,
    DATABLOCK=8
};
struct bfs_super_block{
    unsigned short inode_count;
    unsigned short free_inode_count;
    unsigned int block_count;
    unsigned int free_block_count;
    unsigned int block_size;
};
struct bfs_dir_entry{
    unsigned int inode;
    char name[252];
};
struct bfs_inode{
    unsigned int i_block[8];
    mode_t i_mode;
    __uid_t i_uid;
    __gid_t i_gid;
    __off_t i_size;
    enum BFS_File_Type file_type;
    unsigned int i_link_count;
};

void* bfs_init(struct fuse_conn_info *conn,
              struct fuse_config *cfg);
int bfs_getattr(const char *path, struct stat *stbuf,
                       struct fuse_file_info *fi);
int bfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi,
                       enum fuse_readdir_flags flags);
int bfs_open(const char *path, struct fuse_file_info *fi);
int bfs_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi);
int bfs_mkdir (const char *, mode_t);
int bfs_rmdir (const char *);
int bfs_unlink (const char *);
int bfs_rename(const char *, const char *);
int bfs_chmod (const char *, mode_t);
int bfs_chown (const char *, uid_t, gid_t);
int bfs_utime (const char *, const struct timespec tv[2],struct fuse_file_info *fi);
int bfs_mknod (const char *, mode_t, dev_t);
int bfs_write(const char *, const char *, size_t, off_t,struct fuse_file_info *);
void update_map(int diskfd,int index,enum BITMAP_OP op);
void seek_to_inode(int diskfd, int index);
void seek_to_data_block(int diskfd, int index);
int open_disk();
int get_free_index(int diskfd,enum BITMAP_OP op);
int goto_dir(int diskfd, char *path);
#endif //BFS_BFS_H
