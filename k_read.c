#include <asm/uaccess.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>

#include "ramdisk.h"

extern struct Ramdisk *ramdisk;
extern struct FileDescriptor *fdTable[1024];

int readFile(short iIndex, int filePos, unsigned char *data, int size) {
	unsigned char *newData;
	int newSize, possibleSize, maxSize;
	newData = (unsigned char *) kmalloc(MAXFSZ, GFP_KERNEL);
	if ((possibleSize = adjustPosition(iIndex, newData)) < 0) {
		return 0;
	}
	newSize = filePos + size;
	if (newSize > possibleSize) {
		/* Overflow File Size Case */
		maxSize = size - filePos;
		/* Copy the target into data to return */
		memcpy(data, newData + filePos, maxSize);
		/* Update the file position of the file */
		fdTable[iIndex]->filePos = size;
		return maxSize;
	} else {
		/* Fit File Size Case */
		/* Copy the target into data to return */
		memcpy(data, newData + filePos, size);
		/* Update the file position of the file */
		fdTable[iIndex]->filePos = newSize;
		return size;
	}
}

int k_read(int fd, char* address, int numBytes) {
	unsigned char *tempData;
	int dataSize, position, numBytesWritten, totalBytes;
	/* File descriptor refers to non-existent file */
	if (fdTable[fd] == NULL) {
		printk("k_write() Error : File does not exist or file descriptor is not valid\n");
		return -1;
	}
	/* Check that file is a regular file */
	if (strncmp(ramdisk->ib[fd].type, "reg", 3)) {
		printk("k_write() Error : File is not a regular file\n");
		return -1;
	}
	tempData = (unsigned char *) kmalloc(DBLKSZ, GFP_KERNEL);
	dataSize = 0; position = 0; numBytesWritten = 0; totalBytes = 0;
	while (position < numBytes) {
		dataSize = numBytes - position;
		if (dataSize > DBLKSZ) {
			dataSize = DBLKSZ;
		}
		if ((numBytesWritten = readFile(fd, fdTable[fd]->filePos, tempData, dataSize)) < 0) {
			printk("k_write() Error : Could not compute number of bytes written to file\n");
			return -1;
		}
		memcpy(address + position, tempData, dataSize);
		position += dataSize;
		totalBytes += numBytesWritten;
	}
	return totalBytes;
}

EXPORT_SYMBOL(readFile);
EXPORT_SYMBOL(k_read);
