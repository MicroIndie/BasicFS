all:BFS UNMOUNT
UNMOUNT:
	sudo umount tmp
BFS:
	libtool --mode=link gcc -Wall -o bfs main.c bfs_functs.c disk_util.c `pkg-config fuse3 --cflags --libs` /home/ht/workspace/linux/BFS/fuse-3.0.0/lib/libfuse3.la
