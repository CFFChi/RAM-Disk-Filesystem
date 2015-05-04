#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdio.h>

#include "ramdisk.h"
#include "ramdisk_ioctl.h"

int rd_creat(char *pathname) {
	int fd, ret;
	printf("Calling rd_creat()...\n");
	printf("Path: %s\n", pathname);

	fd = open("/proc/ramdisk", O_RDONLY);
	ret = ioctl(fd, RD_CREAT, pathname);
	close(fd);
	return ret;
}

int rd_open(char *pathname) {
	int fd, ret; 
	printf("Calling rd_open()...\n");
	printf("Path: %s\n", pathname);

	fd = open("/proc/ramdisk", O_RDONLY);
	ret = ioctl(fd, RD_OPEN, pathname);
	close(fd);
	return ret;
}

int rd_close(int fd) {
	int ret, fdRoot;

	printf("Calling rd_close()\n");
	printf("fd: %d\n", fd);

	fdRoot = open("/proc/ramdisk", O_RDONLY);
	ret = ioctl(fdRoot, RD_CLOSE, &fd);
	close(fdRoot);
	return ret; 
}

int rd_write(int fd, char *address, int num_bytes) {
	int ret; 
	struct IOParameter param; 

	param.fd = fd; 
	param.address = address;
	param.numBytes = num_bytes;

	fd = open("/proc/ramdisk", O_RDONLY);
	ret = ioctl(fd, RD_WRITE, &param);
	return ret;
}

int rd_unlink(char *pathname) {
	int fd, ret; 
	printf("Calling rd_unlink()...\n");
	printf("Path: %s\n", pathname);

	fd = open("/proc/ramdisk", O_RDONLY);
	ret = ioctl(fd, RD_UNLINK, pathname);
	close(fd);
	return ret; 
}


int rd_lseek(int fd, int offset) {
	int ret, fdRoot; 
	struct IOParameter param; 

	param.fd = fd;
	param.address = NULL;
	param.numBytes = offset; 

	fdRoot = open("/proc/ramdisk", O_RDONLY);
	ret = ioctl(fdRoot, RD_LSEEK, &param);
	close(fd);
	return ret; 
}































