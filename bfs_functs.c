//
// Created by ht on 17-3-8.
//
#include <string.h>
#include <memory.h>
#include <errno.h>
#include "bfs.h"
#include <fuse.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>

char disk_file_path[256];

void *bfs_init(struct fuse_conn_info *conn,
               struct fuse_config *cfg) {
    return NULL;
}

int bfs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    int res = 0;
    fprintf(stderr, "\t\t\t\t\t\t\tpath:::%s\n", path);
    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else {
        char *last_sep = strrchr(path, '/') + 1;
        int diskfd = open_disk();
        int dir_inode_index = goto_dir(diskfd, path);
        printf("dir inode index %d\n", dir_inode_index);
        res = -ENOENT;
        if (dir_inode_index != -1) {
            seek_to_inode(diskfd, dir_inode_index);
            struct bfs_inode dir_node;
            read(diskfd, &dir_node, sizeof(struct bfs_inode));
            char entry[BLOCK_SIZE / DIRENT_SIZE][DIRENT_SIZE];
            printf("dir size:%d\n", dir_node.i_size);
            for (int i = 0; i < dir_node.i_size / (DIRENT_SIZE); i++) {
                seek_to_data_block(diskfd, dir_node.i_block[i * DIRENT_SIZE / BLOCK_SIZE]);
                read(diskfd, entry, BLOCK_SIZE);
                struct bfs_dir_entry *dir_entry = (struct bfs_dir_entry *) entry[i % (BLOCK_SIZE / DIRENT_SIZE)];
                if (!strcmp(last_sep, dir_entry->name)) {
                    int filenode = dir_entry->inode;
                    seek_to_inode(diskfd, filenode);
                    struct bfs_inode node;
                    read(diskfd, &node, sizeof(node));
                    stbuf->st_mode = node.i_mode;
                    stbuf->st_nlink = node.i_link_count;
                    stbuf->st_size = node.i_size;
                    stbuf->st_gid = node.i_gid;
                    stbuf->st_uid = node.i_uid;
                    printf("file type:%d", node.file_type);
                    if (node.file_type == BFS_FILE) {
                        stbuf->st_mode = S_IFREG | 0666;
                    } else {
                        stbuf->st_mode = S_IFDIR | 0755;
                    }
                    stbuf->st_blocks = node.i_size / BLOCK_SIZE;
                    res = 0;
                    break;
                }
            }
            close(diskfd);
        }
    }
    return res;
}

int bfs_mkdir(const char *path, mode_t mode) {
    int diskfd = open_disk();
    char *last_split = strrchr(path, '/');
    //read root inode info
    int dir_inode_index = goto_dir(diskfd, path);
    printf("dir inode index mkdir:%d\n", dir_inode_index);
    seek_to_inode(diskfd, dir_inode_index);
    struct bfs_inode rootnode, newnode;
    read(diskfd, &rootnode, sizeof(rootnode));

    //update root dir entry
    unsigned int inodeindex = get_free_index(diskfd, INODE);
    update_map(diskfd, inodeindex, SET | INODE);
    seek_to_data_block(diskfd, rootnode.i_block[rootnode.i_size / DIRENT_SIZE / (BLOCK_SIZE / DIRENT_SIZE)]);
    lseek(diskfd, rootnode.i_size % BLOCK_SIZE, SEEK_CUR);
    struct bfs_dir_entry new_entry;
    strcpy(new_entry.name, last_split + 1);
    new_entry.inode = inodeindex;
    write(diskfd, &new_entry, sizeof(new_entry));
    //update root inode
    rootnode.i_size += DIRENT_SIZE;
    if (rootnode.i_size % DIRENT_SIZE == 0) {
        unsigned int data_block = get_free_index(diskfd, DATABLOCK);
        rootnode.i_block[rootnode.i_size / DIRENT_SIZE] = data_block;
        update_map(diskfd, data_block, SET | DATABLOCK);
    }
    seek_to_inode(diskfd, dir_inode_index);
    write(diskfd, &rootnode, sizeof(rootnode));
    //assign new inode
    unsigned int data_block_index = get_free_index(diskfd, DATABLOCK);
    update_map(diskfd, data_block_index, SET | DATABLOCK);
    newnode.i_size = 0;
    newnode.file_type = BFS_DIR;
    newnode.i_mode = mode;
    newnode.i_link_count = 1;
    newnode.i_gid = fuse_get_context()->gid;
    newnode.i_uid = fuse_get_context()->uid;
    newnode.i_block[0] = data_block_index;
    newnode.i_size = 0;
    seek_to_inode(diskfd, inodeindex);
    write(diskfd, &newnode, sizeof(newnode));

    close(diskfd);
    return 0;
}

int bfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                off_t offset, struct fuse_file_info *fi,
                enum fuse_readdir_flags flags) {
    int diskfd = open_disk();
    char entries[BLOCK_SIZE / DIRENT_SIZE][DIRENT_SIZE];
    char *last_sep = strrchr(path, '/');
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    if (!strcmp(path, "/")) {
        seek_to_inode(diskfd, 0);
        struct bfs_inode node;
        read(diskfd, &node, sizeof(node));
        for (int j = 0; j < node.i_size / (DIRENT_SIZE); j++) {
            struct bfs_dir_entry *dir_entry = (struct bfs_dir_entry *) entries[j % (BLOCK_SIZE / DIRENT_SIZE)];
            seek_to_data_block(diskfd, node.i_block[j * DIRENT_SIZE / BLOCK_SIZE]);
            read(diskfd, entries, BLOCK_SIZE);
            struct bfs_dir_entry *entry = (struct bfs_dir_entry *) entries[j % (BLOCK_SIZE / DIRENT_SIZE)];
            filler(buf, dir_entry->name, NULL, 0, 0);
        }
    } else {
        int dir_inode_index = goto_dir(diskfd, path);
        if (dir_inode_index != -1) {
            seek_to_inode(diskfd, dir_inode_index);
            struct bfs_inode dir_node;
            read(diskfd, &dir_node, sizeof(struct bfs_inode));
            for (int i = 0; i < dir_node.i_size / (DIRENT_SIZE); i++) {
                seek_to_data_block(diskfd, dir_node.i_block[i * DIRENT_SIZE / BLOCK_SIZE]);
                read(diskfd, entries, BLOCK_SIZE);
                struct bfs_dir_entry *dir_entry = (struct bfs_dir_entry *) entries[i % (BLOCK_SIZE / DIRENT_SIZE)];
                if (!strcmp(last_sep + 1, dir_entry->name)) {
                    int filenode = dir_entry->inode;
                    seek_to_inode(diskfd, filenode);
                    struct bfs_inode node;
                    read(diskfd, &node, sizeof(node));
                    printf("entry node size:%d\n", node.i_size);
                    for (int j = 0; j < node.i_size / (DIRENT_SIZE); j++) {
                        seek_to_data_block(diskfd, node.i_block[j * DIRENT_SIZE / BLOCK_SIZE]);
                        read(diskfd, entries, BLOCK_SIZE);
                        struct bfs_dir_entry *dir_entry = (struct bfs_dir_entry *) entries[j %
                                                                                           (BLOCK_SIZE / DIRENT_SIZE)];
                        filler(buf, dir_entry->name, NULL, 0, 0);
                    }
                    break;
                }
            }

        }
    }
    close(diskfd);
    return 0;
}

int bfs_open(const char *path, struct fuse_file_info *fi) {
    int res;

    return 0;
}

int bfs_read(const char *path, char *buf, size_t size, off_t offset,
             struct fuse_file_info *fi) {
    int diskfd = open_disk();
    char *last_sep = strrchr(path, '/');
    char entries[BLOCK_SIZE / DIRENT_SIZE][DIRENT_SIZE];
    int dir_inode_index = goto_dir(diskfd, path);
    if (dir_inode_index != -1) {
        seek_to_inode(diskfd, dir_inode_index);
        struct bfs_inode dir_node;
        int filesize = -1;
        read(diskfd, &dir_node, sizeof(struct bfs_inode));
        for (int i = 0; i < dir_node.i_size / (DIRENT_SIZE); i++) {
            seek_to_data_block(diskfd, dir_node.i_block[i * DIRENT_SIZE / BLOCK_SIZE]);
            read(diskfd, entries, BLOCK_SIZE);
            struct bfs_dir_entry *dir_entry = (struct bfs_dir_entry *) entries[i % (BLOCK_SIZE / DIRENT_SIZE)];
            if (!strcmp(last_sep + 1, dir_entry->name)) {
                int filenode = dir_entry->inode;
                seek_to_inode(diskfd, filenode);
                struct bfs_inode node;
                read(diskfd, &node, sizeof(node));
                for (int j = 0; j < size; j++) {
                    seek_to_data_block(diskfd, node.i_block[(offset + j) / BLOCK_SIZE]);
                    lseek(diskfd, (offset + j) % BLOCK_SIZE, SEEK_CUR);
                    read(diskfd, &buf[j], 1);
                }
                filesize = node.i_size;
                break;
            }
        }
        close(diskfd);
        if (filesize == -1)
            return -ENOENT;
        if (filesize <= offset)
            return 0;
        if (filesize <= offset + size)
            return filesize - offset;
        return size;
    }
    return -ENOENT;

}

int bfs_rmdir(const char *path) {
    int diskfd = open_disk();
    int dir_inode_index = goto_dir(diskfd, path);
    char *last_sep = strrchr(path, '/');
    printf("\t\t\tgoto dir %s:%d\n", path, dir_inode_index);
    int found = 0, res = 0;
    char entries[BLOCK_SIZE / DIRENT_SIZE][DIRENT_SIZE];
    if (dir_inode_index != -1) {
        seek_to_inode(diskfd, dir_inode_index);
        struct bfs_inode dir_node;
        read(diskfd, &dir_node, sizeof(struct bfs_inode));
        int i;
        for (i = 0; i < dir_node.i_size / (DIRENT_SIZE); i++) {
            seek_to_data_block(diskfd, dir_node.i_block[i * DIRENT_SIZE / BLOCK_SIZE]);
            read(diskfd, entries, BLOCK_SIZE);
            struct bfs_dir_entry *dir_entry = (struct bfs_dir_entry *) entries[i % (BLOCK_SIZE / DIRENT_SIZE)];
            if (!strcmp(last_sep + 1, dir_entry->name)) {
                found = 1;
                printf("found entry\n");
                int filenode = dir_entry->inode;
                seek_to_inode(diskfd, filenode);
                struct bfs_inode node;
                read(diskfd, &node, sizeof(node));
                printf("entry node size:%d\n", node.i_size);
                update_map(diskfd, dir_entry->inode, INODE | EREASE);
                for (int j = 0; j < node.i_size / BLOCK_SIZE; j++)
                    update_map(diskfd, node.i_block[j], DATABLOCK | EREASE);
                break;
            }
        }
        if (!found)
            res = -ENONET;
        else {
            for (; i < dir_node.i_size / (DIRENT_SIZE); i++) {
                seek_to_data_block(diskfd, dir_node.i_block[i * DIRENT_SIZE / BLOCK_SIZE]);
                read(diskfd, entries, BLOCK_SIZE);
                memcpy(entries[i % (BLOCK_SIZE / DIRENT_SIZE)], entries[(i + 1) % (BLOCK_SIZE / DIRENT_SIZE)],
                       sizeof(DIRENT_SIZE));
                seek_to_data_block(diskfd, dir_node.i_block[i * DIRENT_SIZE / BLOCK_SIZE]);
                write(diskfd, entries, BLOCK_SIZE);
            }
            seek_to_inode(diskfd, dir_inode_index);
            dir_node.i_size -= DIRENT_SIZE;
            write(diskfd, &dir_node, sizeof(struct bfs_inode));
        }
    }
    close(diskfd);
    return res;
}

int bfs_unlink(const char *path) {
    int diskfd = open_disk();
    char *last_sep = strrchr(path, '/');
    char entries[BLOCK_SIZE / DIRENT_SIZE][DIRENT_SIZE];
    int dir_inode_index = goto_dir(diskfd, path);
    int found=0,res=0;
    if (dir_inode_index != -1) {
        seek_to_inode(diskfd, dir_inode_index);
        struct bfs_inode dir_node;
        read(diskfd, &dir_node, sizeof(struct bfs_inode));
        int i;
        for (i = 0; i < dir_node.i_size / (DIRENT_SIZE); i++) {
            seek_to_data_block(diskfd, dir_node.i_block[i * DIRENT_SIZE / BLOCK_SIZE]);
            read(diskfd, entries, BLOCK_SIZE);
            struct bfs_dir_entry *dir_entry = (struct bfs_dir_entry *) entries[i % (BLOCK_SIZE / DIRENT_SIZE)];
            if (!strcmp(last_sep + 1, dir_entry->name)) {
                int filenode = dir_entry->inode;
                seek_to_inode(diskfd, filenode);
                struct bfs_inode node;
                read(diskfd, &node, sizeof(node));
                for(int j=0;j<=node.i_size/BLOCK_SIZE;j++){
                    update_map(diskfd,node.i_block[j],DATABLOCK|EREASE);
                }
                found=1;
                break;
            }
        }
        if (!found)
            res = -ENONET;
        else {
            for (; i < dir_node.i_size / (DIRENT_SIZE); i++) {
                seek_to_data_block(diskfd, dir_node.i_block[i * DIRENT_SIZE / BLOCK_SIZE]);
                read(diskfd, entries, BLOCK_SIZE);
                printf("copy %s\n",entries[(i+1)%(BLOCK_SIZE/DIRENT_SIZE)]);
                memcpy(entries[i % (BLOCK_SIZE / DIRENT_SIZE)], entries[(i + 1) % (BLOCK_SIZE / DIRENT_SIZE)],
                       sizeof(DIRENT_SIZE));
                seek_to_data_block(diskfd, dir_node.i_block[i * DIRENT_SIZE / BLOCK_SIZE]);
                write(diskfd, entries, BLOCK_SIZE);
            }
            seek_to_inode(diskfd, dir_inode_index);
            dir_node.i_size -= DIRENT_SIZE;
            write(diskfd, &dir_node, sizeof(struct bfs_inode));
        }
    }
    close(diskfd);
    return res;
}

int bfs_rename(const char *from, const char *to) {
    int res;

    res = rename(from, to);
    if (res == -1)
        return -errno;

    return 0;
}

int bfs_chmod(const char *path, mode_t mode) {
    int res;

    res = chmod(path, mode);
    if (res == -1)
        return -errno;

    return 0;
}

int bfs_chown(const char *path, uid_t uid, gid_t gid) {
    int res;

    res = lchown(path, uid, gid);
    if (res == -1)
        return -errno;

    return 0;
}

int bfs_utime(const char *path, const struct timespec tv[2],
              struct fuse_file_info *fi) {
    return 0;
}

int bfs_mknod(const char *path, mode_t mode, dev_t rdev) {
    int diskfd = open_disk();
    char *last_split = strrchr(path, '/');
    //read root inode info
    int dir_inode_index = goto_dir(diskfd, path);
    printf("dir inode index mkdir:%d\n", dir_inode_index);
    seek_to_inode(diskfd, dir_inode_index);
    struct bfs_inode rootnode, newnode;
    read(diskfd, &rootnode, sizeof(rootnode));

    //update root dir entry
    unsigned int inodeindex = get_free_index(diskfd, INODE);
    update_map(diskfd, inodeindex, SET | INODE);
    seek_to_data_block(diskfd, rootnode.i_block[rootnode.i_size / DIRENT_SIZE / (BLOCK_SIZE / DIRENT_SIZE)]);
    lseek(diskfd, rootnode.i_size % BLOCK_SIZE, SEEK_CUR);
    struct bfs_dir_entry new_entry;
    strcpy(new_entry.name, last_split + 1);
    new_entry.inode = inodeindex;
    write(diskfd, &new_entry, sizeof(new_entry));
    //update root inode
    rootnode.i_size += DIRENT_SIZE;
    if (rootnode.i_size % DIRENT_SIZE == 0) {
        unsigned int data_block = get_free_index(diskfd, DATABLOCK);
        rootnode.i_block[rootnode.i_size / DIRENT_SIZE] = data_block;
        update_map(diskfd, data_block, SET | DATABLOCK);
    }
    seek_to_inode(diskfd, dir_inode_index);
    write(diskfd, &rootnode, sizeof(rootnode));
    //assign new inode
    unsigned int data_block_index = get_free_index(diskfd, DATABLOCK);
    update_map(diskfd, data_block_index, SET | DATABLOCK);
    newnode.i_size = 0;
    newnode.file_type = BFS_FILE;
    newnode.i_mode = mode;
    newnode.i_link_count = 1;
    newnode.i_gid = fuse_get_context()->gid;
    newnode.i_uid = fuse_get_context()->uid;
    newnode.i_block[0] = data_block_index;
    newnode.i_size = 0;
    seek_to_inode(diskfd, inodeindex);
    write(diskfd, &newnode, sizeof(newnode));

    close(diskfd);
    return 0;
}

int bfs_write(const char *path, const char *buf, size_t size, off_t offset,
              struct fuse_file_info *info) {
    int diskfd = open_disk();
    char *last_sep = strrchr(path, '/');
    char entries[BLOCK_SIZE / DIRENT_SIZE][DIRENT_SIZE];
    int dir_inode_index = goto_dir(diskfd, path);
    if (dir_inode_index != -1) {
        seek_to_inode(diskfd, dir_inode_index);
        struct bfs_inode dir_node;
        read(diskfd, &dir_node, sizeof(struct bfs_inode));
        for (int i = 0; i < dir_node.i_size / (DIRENT_SIZE); i++) {
            seek_to_data_block(diskfd, dir_node.i_block[i * DIRENT_SIZE / BLOCK_SIZE]);
            read(diskfd, entries, BLOCK_SIZE);
            struct bfs_dir_entry *dir_entry = (struct bfs_dir_entry *) entries[i % (BLOCK_SIZE / DIRENT_SIZE)];
            if (!strcmp(last_sep + 1, dir_entry->name)) {
                int filenode = dir_entry->inode;
                seek_to_inode(diskfd, filenode);
                struct bfs_inode node;
                read(diskfd, &node, sizeof(node));
                for(int j=node.i_size/BLOCK_SIZE+1;j<=(offset+size)/BLOCK_SIZE;j++){
                    node.i_block[j]=get_free_index(diskfd,DATABLOCK);
                    update_map(diskfd,node.i_block[j],DATABLOCK|SET);
                }
                for (int j = 0; j < size; j++) {
                    seek_to_data_block(diskfd, node.i_block[(offset + j) / BLOCK_SIZE]);
                    lseek(diskfd, (offset + j) % BLOCK_SIZE, SEEK_CUR);
                    write(diskfd, &buf[j], 1);
                }
                node.i_size += size;
                seek_to_inode(diskfd, filenode);
                write(diskfd, &node, sizeof(node));
            }
        }
    }
    close(diskfd);
    return size;
}