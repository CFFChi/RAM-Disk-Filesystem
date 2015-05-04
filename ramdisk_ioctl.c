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

int rd_close(int fd2) {
	int fd1 = open("/proc/ramdisk", O_RDONLY);

	printf("Calling rd_close()\n");
	printf("fd: %d\n", fd2);

	int ret = ioctl(fd1, RD_CLOSE, &fd2);
	close(fd1);
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

