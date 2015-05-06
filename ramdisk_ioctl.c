#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdio.h>

#include "ramdisk.h"

int rd_creat(char *pathname) {
	int fd, ret;
	printf("Calling rd_creat()...\n");
	// printf("Path: %s\n", pathname);

	fd = open("/proc/ramdisk", O_RDONLY);
	ret = ioctl(fd, RD_CREAT, pathname);
	close(fd);
	return ret;
}

int rd_mkdir(char* pathname) {
	int fd, ret;
	printf("Calling rd_mkdir()...\n");
	// printf("Path: %s\n", pathname);

	fd = open("/proc/ramdisk", O_RDONLY);
	ret = ioctl(fd, RD_MKDIR, pathname);
	close(fd);
	return ret;
}

int rd_open(char *pathname) {
	int fd, ret;
	printf("Calling rd_open()...\n");
	// printf("Path: %s\n", pathname);

	fd = open("/proc/ramdisk", O_RDONLY);
	ret = ioctl(fd, RD_OPEN, pathname);
	close(fd);
	return ret;
}

int rd_close(int fd) {
	int ret, fdd;

	printf("Calling rd_close()\n");
	// printf("fd: %d\n", fd);

	fdd = open("/proc/ramdisk", O_RDONLY);
	ret = ioctl(fdd, RD_CLOSE, &fd);
	close(fdd);
	return ret;
}

int rd_read(int fd, char *address, int numBytes) {
	int ret, fdd;
	struct IOParameter param;

	printf("Calling rd_read()\n");
	param.fd = fd;
	param.address = address;
	param.numBytes = numBytes;

	fdd = open("/proc/ramdisk", O_RDONLY);
	ret = ioctl(fdd, RD_READ, &param);
	close(fdd);
	return ret;
}

int rd_write(int fd, char *address, int numBytes) {
	int ret, fdd;
	struct IOParameter param;

	printf("Calling rd_write()\n");

	param.fd = fd;
	param.address = address;
	param.numBytes = numBytes;

	fdd = open("/proc/ramdisk", O_RDONLY);
	ret = ioctl(fdd, RD_WRITE, &param);
	close(fdd);
	return ret;
}

int rd_lseek(int fd, int offset) {
	printf("Calling rd_lseek()...\n");
	// printf("fd: %d\n", fd);
	// printf("offset: %d\n", offset);

	int ret, fdd;
	struct IOParameter param;

	param.fd = fd;
	param.address = NULL;
	param.numBytes = offset;

	fdd = open("/proc/ramdisk", O_RDONLY);
	ret = ioctl(fdd, RD_LSEEK, &param);
	close(fdd);
	return ret;
}

int rd_unlink(char *pathname) {
	int fd, ret;
	printf("Calling rd_unlink()...\n");
	// printf("Path: %s\n", pathname);

	fd = open("/proc/ramdisk", O_RDONLY);
	ret = ioctl(fd, RD_UNLINK, pathname);
	close(fd);
	return ret;
}

int rd_readdir(int fd, char *address) {
	int ret, fdd;
	struct IOParameter param;
	printf("Calling rd_readdir()...\n");
	// printf("fd: %d\n", fd);
	// printf("address: %s\n", address);

	param.fd = fd;
	param.address = address;
	param.numBytes = 0;

	fdd = open("/proc/ramdisk", O_RDONLY);
	ret = ioctl(fdd, RD_READDIR, &param);
	close(fdd);
	return ret;
}
