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

int rd_mkdir(char* pathname) {
	int fd, ret; 
	


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

int rd_read(int fd, char *address, int numBytes) {
	int ret, fdRoot;
	struct IOParameter param;

	printf("Calling rd_read()\n");
	param.fd = fd;
	param.address = address;
	param.numBytes = numBytes; 

	fdRoot = open("/proc/ramdisk", O_RDONLY);
	ret = ioctl(fdRoot, RD_READ, &param);
	close(fdRoot);
	return ret; 
}

int rd_write(int fd, char *address, int numBytes) {
	int ret, fdRoot;
	struct IOParameter param; 

	printf("Calling rd_write()\n");

	param.fd = fd; 
	param.address = address;
	param.numBytes = numBytes;

	fdRoot = open("/proc/ramdisk", O_RDONLY);
	ret = ioctl(fdRoot, RD_WRITE, &param);
	close(fdRoot);
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































