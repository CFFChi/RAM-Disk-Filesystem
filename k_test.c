#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ramdisk.h"

// TEST1 : Creates 3 files in root and unlinks
// TEST2 : Creates a dir within a dir, and create two dir then read it content
// TEST3 : Creates a file in root, opens it, writes bunch of 'A', read contents, and close it
// TEST4 : Creates a file inside a directory, opens it, writes bunch of 'B', read contents, and close it


int test1() {
	int ret1, ret2, ret3;
	char* fileA = "/fileA";
	char* fileB = "/fileB";
	char* fileC = "/fileC";

	/* Regular File Creation */
	ret1 = rd_creat(fileA);
	ret2 = rd_creat(fileB);
	ret3 = rd_creat(fileC);
	if (!ret1 | !ret2 | !ret3) {
		printf("Created fileA, fileB, fileC in /\n");
	} else {
		printf("Error creating fileA, fileB, and fileC in /\n");
	}

	/* Regular File Deletion */
	ret1 = rd_unlink(fileA);
	ret2 = rd_unlink(fileB);
	ret3 = rd_unlink(fileC);
	if (!ret1 | !ret2 | !ret3) {
		printf("Unlinked fileA, fileB, fileC in /\n");
	} else {
		printf("Error unlinking fileA, fileB, and fileC in /\n");
	}
	return 0;
}

int test2() {
	int ret, ret1, ret2, ret3, inode, fd;
	char* dir1 = "/dir1";
	char* dir2 = "/dir1/foo";
	char* dir3 = "/dir1/foo/bar";
	char* dir4 = "/dir1/foo/boo";

	char address[1024];
	char data[1024];
	memset(data, '0', sizeof(data));
	memset(address, 0, sizeof(address));

	ret1 = rd_mkdir(dir1);
	ret2 = rd_mkdir(dir2);
	ret3 = rd_mkdir(dir3);
	ret3 = rd_mkdir(dir4);
	if (!ret1 | !ret2 | !ret3) {
		printf("Created directories dir1, dir2, dir3 in /\n");
	} else {
		printf("Error creating directories dir1, dir2, dir3 in /\n");
		return -1;
	}

	if ((fd = rd_open(dir2)) < 0) {
		printf("Error opening directory\n");
		return -1;
	}

	while ((ret = rd_readdir(fd, address))) {
		if (ret < 0) {
			printf("Error reading directory\n");
			return -1;
		}

		inode = atoi(&address[14]);
		printf("Data at Address: [%s, %d]\n", address, inode);
	}
	return 0;
}

int test3() {
	int ret, fd;
	char* file = "/testfile";

	char address[1024];
	char data[1024];
	memset(data, 'A', sizeof(data));

	if ((ret = rd_creat(file)) < 0) {
		printf("Error creating file\n");
		return -1;
	}

	if ((fd = rd_open(file)) < 0) {
		printf("Error opening file\n");
		return -1;
	}

	printf("fd: %d\n", fd);

	if ((ret = rd_write(fd, data, sizeof(data))) < 0) {
		printf("Error writing to file");
		return -1;
	}

	if ((ret = rd_lseek(fd, 0)) < 0) {
		printf("Error seeking file position\n");
		return -1;
	}

	if ((ret = rd_read(fd, address, sizeof(data))) < 0) {
		printf("Error reading file\n");
		return -1;
	}

	printf("DataA: %s\n", address);

	if ((ret = rd_close(fd)) < 0) {
		printf("Error closing file\n");
		return -1;
	}

	printf("Test 3 PASS\n");

	return 0;
}

int test4() {
	int ret, fd;
	char* dir = "/foo";
	char* file = "/foo/bar";

	char address[1024];
	char data[1024];
	memset(data, 'B', sizeof(data));

	if ((ret = rd_mkdir(dir)) < 0) {
		printf("Error creating directory\n");
		return -1;
	}

	if ((ret = rd_creat(file)) < 0) {
		printf("Error creating file\n");
		return -1;
	}

	if ((fd = rd_open(file)) < 0) {
		printf("Error opening directory\n");
		return -1;
	}

	printf("fd: %d\n", fd);

	if ((ret = rd_write(fd, data, sizeof(data))) < 0) {
		printf("Error writing to file");
		return -1;
	}

	if ((ret = rd_lseek(fd, 0)) < 0) {
		printf("Error seeking file position\n");
		return -1;
	}

	if ((ret = rd_read(fd, address, sizeof(data))) < 0) {
		printf("Error reading file\n");
		return -1;
	}

	printf("DataB: %s\n", address);

	if ((ret = rd_close(fd)) < 0) {
		printf("Error closing file\n");
		return -1;
	}

	printf("Test 4 PASS\n");

	return 0;
}

int main(int argc, char const *argv[]) {
	test1();
	// test2();
	// test3();
	// test4();
	return 0;
}
