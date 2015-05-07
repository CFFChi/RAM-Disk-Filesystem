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

void writeToRegDataDPTR(int index, int i, unsigned char* dataChunk, int size) {
	memcpy((*ramdisk->ib[index].location[i]).reg.data, dataChunk, size);
	return;
}

void writeToRegDataRPTR(int index, int pIndex, unsigned char* dataChunk, int size) {
	memcpy((*(*ramdisk->ib[index].location[8]).ptr.location[pIndex]).reg.data, dataChunk, size);
	return;
}

void writeToRegDataRRPTR(int index, int p1, int p2, unsigned char* dataChunk, int size) {
	memcpy((*(*(*ramdisk->ib[index].location[9]).ptr.location[p1]).ptr.location[p2]).reg.data, dataChunk, size);
	return;
}

int plusParentInodeSize(short targetIndex, short* parentInodes) {
	int ret, size, root;
	root = 0; size = 1;
	/* Get the directories to the file from root and store it  into dst array */
	if ((ret = searchParentInodes(root, targetIndex, &size, parentInodes)) < 0) {
		printk("plusParentInodeSize() Error : Could not get parent directories\n");
		return -1;
	}
	return size;
}

int modifyParentInodePlus(short index, int fileSize) {
	int i, oldSize, ret;
	short parentInodes[256];

	/* Build an array of parent directories */
	if ((ret = plusParentInodeSize(index, parentInodes)) < 0) {
		printk("modifyParentInodePlus() Error : Could not increase parent inode size\n");
		return -1;
	}
	/* Go through the parent directory and increase file size */
	for (i = 0; i < ret; i++) {
		oldSize = ramdisk->ib[parentInodes[i]].size;
		ramdisk->ib[index].size = oldSize + fileSize;
	}
	/* Finally, increase the size at the index of the file */
	ramdisk->ib[index].size = fileSize;
	return 0;
}

int write(short index, unsigned char *data, int size) {
	int i, j, k;
	int newSize, position, remainingBytes; 
	int fbDirect, fbSingle, fbDouble1, fbDouble2; 

	position = 0; remainingBytes = size; 
	for (i = 0; i < NUMPTRS; i++) {
		if (ramdisk->ib[index].location[i] == 0) {
			if ((fbDirect = getFreeBlock()) < 0) {
				printk("write() Error : Could find a free block to allocate\n");
				return -1;
			}
			assignFreeBlockDPTR(index, i, fbDirect);

			switch (i) {
				case DPTR1: 
				case DPTR2: 
				case DPTR3: 
				case DPTR4: 
				case DPTR5: 
				case DPTR6: 
				case DPTR7: 
				case DPTR8: {
					if ((newSize = size - position) > BLKSZ) { 
						newSize = BLKSZ;
					}
					data = data + newSize; 
					writeToRegDataDPTR(index, i, data, newSize);
					position += newSize;
					if ((remainingBytes -= position) < BLKSZ) { return 0; }
					continue;
				}

				case RPTR: {
					for (j = 0; j < NODESZ; j++) {
						if ((*ramdisk->ib[index].location[8]).ptr.location[j] == 0) {
							if ((fbSingle = getFreeBlock()) < 0) {
								printk("write() Error : Could not find a free block to allocate\n");
								return -1;
							}
							assignFreeBlockRPTR(index, j, fbSingle);

							if ((newSize = size - position) > BLKSZ) {
								newSize = BLKSZ;
							}
							data = data + newSize;
							writeToRegDataRPTR(index, j, data, newSize);
							position += newSize;
							if ((remainingBytes -= position) < BLKSZ) { return 0; }
						}
					}
					continue;
				}

				case RRPTR: {
					for (j = 0; j < NODESZ; j++) {
						if ((*ramdisk->ib[index].location[9]).ptr.location[j] == 0) {
							if ((fbDouble1 = getFreeBlock()) < 0) {
								printk("write() Error : Could not find a free block to allocate\n");
								return -1;
							}
							assignFreeBlockRRPTR1(index, j, fbDouble1);

							for (k = 0; k < NODESZ; k++) {
								if (((*(*ramdisk->ib[index].location[9]).ptr.location[j]).ptr.location[k]) == 0) {
									if ((fbDouble2 = getFreeBlock()) < 0) {
										printk("write() Error : Could not find a free block to allocate\n");
										return -1;
									}
									assignFreeBlockRRPTR2(index, j, k, fbDouble2);

									if ((newSize = size - position) > BLKSZ) {
										newSize = BLKSZ;
									}
									data = data + newSize;
									writeToRegDataRRPTR(index, j, k, data, newSize);
									position += newSize; 
									if ((remainingBytes -= position) < BLKSZ) { return 0; }
								}
							}
						}
					}
					continue;
				}
			}
		}
	}
	printk("write() Error : Could not write the data into ramdisk\n");
	return -1;
}

int writeFile(short index, int filePos, unsigned char *data, int dataSize) {
	int position, ret, newSize;
	unsigned char *newData;

	/* Initialize these values  */
	position = 0;
	newData = (unsigned char *) kmalloc(dataSize, GFP_KERNEL);
	/* Copies the contents of the ib[iIndex] into data */
	if ((ret = adjustPosition(index, newData)) < -1) {
		printk("writeFile() Error : Could not get the contents of index node\n");
		return -1;
	} 
	/* Compute the size of the data */
	newSize = filePos + dataSize;
	if (newSize > MAXFSZ) {
		/* Data size exceeds the maximum allowed size */
		/* Write to temporary data container displaced by filePos whatever we can */
		memcpy(newData + filePos, data, (MAXFSZ - filePos));
		/* Perform the actual write into ramdisk */
		if ((ret = write(index, newData, MAXFSZ)) < 0) {
			printk("writeFile() Error : Could not write file\n");
			return -1;
		}
		/* Traverse through the parent directories and increase their size by the data size */
		if ((ret = modifyParentInodePlus(index, MAXFSZ)) < 0) {
			printk(" writeFile() Error : Could not modify parent inode\n");
			return -1;
		}
		/* Set the file position to the end */
		fdTable[index]->filePos = MAXFSZ;
		return (MAXFSZ - filePos);
	} else {
		/* Data size fits into the block */
		/* Write to data container displaced by filePos the data */
		memcpy(newData + filePos, data, dataSize);
		/* Perform the actual write into ramdisk */
		if ((ret = write(index, newData, newSize)) < 0) {
			printk("writeFile() Error : Could not write file\n");
			return -1;
		}
		/* Traverse through the parent directories and increase their size by the data size */
		if ((ret = modifyParentInodePlus(index, newSize)) < 0) {
			printk(" writeFile() Error : Could not modify parent inode\n");
			return -1;
		}
		/* Set the file position to the size of the data */
		fdTable[index]->filePos = newSize;
		return dataSize;
	}
	return -1;
}

int k_write(int fd, char* address, int numBytes) {
	unsigned char *tempData;
	int dataSize, position, numBytesWritten, totalBytes;

	/* File descriptor refers to non-existent file */
	if (fdTable[fd] == NULL) {
		printk("k_write() Error : File does not exist or file descriptor is not valid\n");
		return -1;
	}

	/* File descriptor refers to a directory file */
	if (strncmp(ramdisk->ib[fd].type, "reg", 3)) {
		printk("k_write() Error : File is not a regular file\n");
		return -1;
	}
	/* Initialize these values */
	tempData = (unsigned char *) kmalloc(DBLKSZ, GFP_KERNEL);
	dataSize = 0; position = 0; totalBytes = 0;
	/* Write data until position reaches the size of the data */
	while (position < numBytes) {
		/* Compute the remaining data size to write */
		dataSize = numBytes - position;
		/* Set data size as size of direct pointer if too big */
		if (dataSize > DBLKSZ) {
			dataSize = DBLKSZ;
		}
		/* Write the data in address displaced by position into temporary data container */
		memcpy(tempData, address + position, dataSize);
		/* Set and adjust the size of file into the file descriptor table */
		if ((numBytesWritten = writeFile(fd, fdTable[fd]->filePos, tempData, dataSize)) < 0) {
			printk("k_write() Error : Could not compute number of bytes written to file\n");
			return -1;
		}
		/* Add the position by the data size written to data container */
		position += dataSize;
		/* Add the number of bytes written to address */
		totalBytes += numBytesWritten;
		/* Reset the temporary data container */
		memset(tempData, 0, dataSize);
	}
	// printk("Total Bytes Written: %d\n", totalBytes);
	return totalBytes;
}

EXPORT_SYMBOL(writeToRegDataDPTR);
EXPORT_SYMBOL(writeToRegDataRPTR);
EXPORT_SYMBOL(writeToRegDataRRPTR);
EXPORT_SYMBOL(plusParentInodeSize);
EXPORT_SYMBOL(modifyParentInodePlus);
EXPORT_SYMBOL(write);
EXPORT_SYMBOL(writeFile);
EXPORT_SYMBOL(k_write);