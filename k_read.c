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
	int newSize, possibleSize, maxSize;
	unsigned char *newData;

	newData = (unsigned char *) kmalloc(MAXFSZ, GFP_KERNEL);
	possibleSize = adjustPosition(iIndex, newData) - 1;
	newSize = filePos + size;

	// printk("data ; %s\n", data);

	if (newSize > possibleSize) {
		printk("Overflow Read Case\n");

		maxSize = size - filePos;
		/* Copy the target into data to return */
		memcpy(data, newData + filePos, maxSize);
		/* Update the file position of the file */
		fdTable[iIndex]->filePos = size;

		return maxSize;

	} else {
		printk("Fits Read Case\n");

		/* Copy the target into data to return */
		memcpy(data, newData + filePos, size);
		/* Update the file position of the file */
		fdTable[iIndex]->filePos = newSize;

		return size;
	}
}

int k_read(int fd, char* address, int size) {
	int dataSize, position;
	int numBytes, totalBytes;
	unsigned char *data;

	/* File descriptor refers to non-existent file */
	if (fdTable[fd] == NULL) {
		printk("k_write() Error : File does not exist or file descriptor is not valid\n");
		return -1;
	}

	/* File descriptor refers to a directory file */
	if (strncmp(ramdisk->ib[fd].type, "reg", 3) != 0) {
		printk("k_write() Error : File is not a regular file\n");
		return -1;
	}

	dataSize = 0;
	position = 0;
	numBytes = 0;
	totalBytes = 0;

	data = (unsigned char *) kmalloc(DBLKSZ, GFP_KERNEL);

	while (position < size) {
		dataSize = size - position;

		if (dataSize > DBLKSZ) {
			dataSize = DBLKSZ;
		}
		if ((numBytes = readFile(fd, fdTable[fd]->filePos, data, dataSize)) < 0) {
			printk("k_write() Error : Could not compute number of bytes written to file\n");
			return -1;
		}
		memcpy(address + position, data, dataSize);
		position += dataSize;
		totalBytes += numBytes;
		// printk("Read : %s\n", address + position);
	}
	// printk("Read After: %s\n", address);
	// printk("Total Bytes Read: %d\n", totalBytes);
	return totalBytes;
}

EXPORT_SYMBOL(readFile);
EXPORT_SYMBOL(k_read);
