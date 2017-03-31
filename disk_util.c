//
// Created by ht on 17-3-8.
//

#include "bfs.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <asm/errno.h>
#include <string.h>
#include <memory.h>

extern char disk_file_path[256];

int open_disk() {
    int diskfd = open(disk_file_path, O_RDWR);
    if (diskfd == -1) {
        perror(disk_file_path);
        exit(-1);
    }
    return diskfd;
}

void update_map(int diskfd, int index, enum BITMAP_OP op) {
    if (op & INODE)
        lseek(diskfd, SUPER_BLOCK_SIZE, SEEK_SET);
    else
        lseek(diskfd, SUPER_BLOCK_SIZE + INODE_BITMAP_SIZE, SEEK_SET);
    lseek(diskfd, index / 32, SEEK_CUR);//go to byte
    int map;
    int tmp = read(diskfd, &map, 4);
    if (tmp == -1)
        fprintf(stderr, "error update inode map\n");
    int bitindex = index % 32;
    int mask = 1 << bitindex;
    if (op & SET)
        map |= mask;
    else
        map ^= mask;
    lseek(diskfd, -4, SEEK_CUR);
    write(diskfd, &map, 4);

}

void seek_to_inode(int diskfd, int index) {
    lseek(diskfd, SUPER_BLOCK_SIZE + INODE_BITMAP_SIZE + DATA_BITMAP_SIZE + INODE_SIZE * index, SEEK_SET);
}

void seek_to_data_block(int diskfd,  int index) {
    lseek(diskfd, SUPER_BLOCK_SIZE + INODE_BITMAP_SIZE + DATA_BITMAP_SIZE + BLOCK_SIZE * (16 + index), SEEK_SET);
}

int get_free_index(int diskfd, enum BITMAP_OP op) {
    if (op == INODE)
        lseek(diskfd, SUPER_BLOCK_SIZE, SEEK_SET);
    else if (op == DATABLOCK)
        lseek(diskfd, SUPER_BLOCK_SIZE + INODE_BITMAP_SIZE, SEEK_SET);
    int map;
    int bytecount = 0;
    while (1) {
        int tmp = read(diskfd, &map, 4);
        if (tmp == -1)
            fprintf(stderr, "failed read inode bitmap\n");
        int bitcount = 0;
        int i = 1;
        while (i) {
            if (!(map & i)) {
                return bitcount + bytecount * 32;
            }
            i <<= 1;
            ++bitcount;
        }
        ++bytecount;
        if (bytecount > INODE_BITMAP_SIZE) {
            fprintf(stderr, "no free inode\n");
            exit(-1);
        }
    }
}



int goto_dir(int diskfd, char *path) {
    char *first_sep = strchr(path, '/');
    char *next_sep = strchr(first_sep + 1, '/');
    char *last_sep = strrchr(path, '/');
    int inode_index = 0;
    char entry[BLOCK_SIZE / DIRENT_SIZE][DIRENT_SIZE];
    while (first_sep!=NULL&&strcmp(first_sep, last_sep)) {
        seek_to_inode(diskfd, inode_index);
        struct bfs_inode inode;
        read(diskfd, &inode, sizeof(inode));
        int found = 0;
        for (int i = 0; i < inode.i_size / (DIRENT_SIZE); i++) {
            seek_to_data_block(diskfd, inode.i_block[i * DIRENT_SIZE / BLOCK_SIZE]);
            read(diskfd, entry, BLOCK_SIZE);
            struct bfs_dir_entry *dir_entry = (struct bfs_dir_entry *) entry[i % (BLOCK_SIZE / DIRENT_SIZE)];
            if (!strncmp((first_sep + 1), dir_entry->name, (int)(next_sep-first_sep-1))) {
                fprintf(stderr, "\t\t\t\t\t\t\tfilename:::%s\n", dir_entry->name);
                inode_index = dir_entry->inode;
                first_sep = next_sep;
                next_sep = strchr(first_sep + 1, '/');
                found = 1;
                break;
            }
        }
        if (!found) return -1;
    }
    return inode_index;
}