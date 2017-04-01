# BasicFS
A file system using a file to simulate a disk and libfuse(https://github.com/libfuse/libfuse) to connect to VFS.
## Usage
1. This project depends on libfuse, you should install that to be able to build and run this project.
2. Run with `./bfs mount_point disk_file_path`, this will open a disk file or create and initialize one.
3. This is not a solid implementation of a file system, but you can learn some aspects of how libfuse work.
