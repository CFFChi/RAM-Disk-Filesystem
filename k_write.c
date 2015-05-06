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

int plusParentInodeSize(short targetIndex, short* parentInodes) {
	int ret, size;
	short root;

	// printk("target in plusParent: %d\n", targetIndex);

	root = 0; size = 1;
	/* Get the directories to the file from root and store it and its size into array */
	if ((ret = searchParentInodes(root, targetIndex, &size, parentInodes)) < 0) {
		printk("plusParentInodeSize() Error : Could not get parent directories\n");
		return -1;
	}

	return size;
}

int modifyParentInodePlus(short iIndex, int fileSize) {
	int i, oldSize, ret;
	short index;
	short parentInodes[256];

	printk("index: %d\n", iIndex);
	printk("fileSize : %d\n", fileSize);

	/* Increase the parent directories by the size of the file */
	if ((ret = plusParentInodeSize(iIndex, parentInodes)) < 0) {
		printk("modifyParentInodePlus() Error : Could not increase parent inode size\n");
		return -1;
	}

	if (ret <= 0) {
		printk("modifyParentInodePlus() Error : Size of parent node array is 0\n");
		return -1;
	}

	// printk("numParents: %d\n", ret);

	/* Go through the parent directory and increase file size */
	for (i = 0; i < ret; i++) {
		index = parentInodes[i];
		oldSize = ramdisk->ib[index].size;
		ramdisk->ib[index].size = oldSize + fileSize;
	}

	/* Finally, increase the size at the index of the file */
	ramdisk->ib[iIndex].size = fileSize;
	return 0;
}


int write(short iIndex, unsigned char *data, int size) {

	int i, j, k;
	int newSize, position, bytesToWrite;
	int freeBlock, fbSingle, fbDouble;
	union Block *location, *singlePtr, *doublePtr;

	/* Initialize the position as 0 */
	position = 0;
	/* Set the number of btyes we need to write */
	bytesToWrite = size;

	for (i = 0; i < NUMPTRS; i++) {
		location = ramdisk->ib[iIndex].location[i];
		/* Check if there is anything allocated in this location */
		if (location == 0) {
			/* Find and get a free block */
			if ((freeBlock = getFreeBlock()) < 0) {
				printk("write() Error : Could not find free block\n");
				return -1;
			}
			/* Assign this free block to this location */
			ramdisk->ib[iIndex].location[i] = &ramdisk->fb[freeBlock];
			/* Direct Pointer : location[0] to location[7] */
			if (0 <= i && i <= 7) {
				/* Compute the remaining data size to write */
				newSize = size - position;
				/* Size of remaining is larger than 256 bytes */
				if (newSize > BLKSZ) { newSize = BLKSZ; }
				/* Offset the data by the data size */
				data = data + newSize;
				/* Write the data into the regular block of this location */
				memcpy((*ramdisk->ib[iIndex].location[i]).reg.data, data, newSize);
				/* Reposition by the size of data */
				position += newSize;
				/* Compute the remaining bytes we have to write */
				bytesToWrite -= position;
				/* Check if the data is written to  */
				if (bytesToWrite < BLKSZ) {
					return 0;
				}
			}
			/* Redirection Block : location[8] and location[9] */
			else {
				for (j = 0; j < NODESZ; j++) {
					singlePtr = (*ramdisk->ib[iIndex].location[i]).ptr.location[j];
					/* Check if there is anything allocated in this redirection block */
					if (singlePtr == 0) {
						/* Find and get a free block */
						if ((fbSingle = getFreeBlock()) < 0) {
							printk("write() Error : Could not find free block\n");
							return -1;
						}
						/* Assign this free block to this redirection block */
						ramdisk->ib[iIndex].location[i]->ptr.location[j] = &ramdisk->fb[fbSingle];
					}
					/* Single Redirection Block case */
					if (i == 8) {
						newSize = size - position;
						if (newSize > BLKSZ) {
							newSize = BLKSZ;
						}
						data = data + newSize;
						/* Write the data into the regular block of this single redirection block */
						memcpy((*(*ramdisk->ib[iIndex].location[8]).ptr.location[j]).reg.data, data, newSize);
						position += newSize;
						if ((size - position) < BLKSZ) {
							return 0;
						}
					}
					/* Double Redirection Block case */
					if (i == 9) {
						for (k = 0; k < NODESZ; k++) {
							doublePtr = ((*(*ramdisk->ib[iIndex].location[9]).ptr.location[j]).ptr.location[k]);
							/* Check if there is anything allocated in the double redirection block */
							if (doublePtr == 0) {
								/* Find and get a free block */
								if ((fbDouble = getFreeBlock()) < 0) {
									printk("write() Error : Could not find free block\n");
									return -1;
								}
								/* Assign this free block to the double redirection block */
								ramdisk->ib[iIndex].location[9]->ptr.location[j]->ptr.location[k] = &ramdisk->fb[fbDouble];
								/* Compute the size of the data */
								newSize = size - position;
								/* Size of data is larger than 256 bytes */
								if (newSize > BLKSZ) {
									newSize = BLKSZ;
								}
								/* Offset the data by the data size */
								data = data + newSize;
								/* Write the data into the regular block of this double redirection block */
								memcpy((*(*(*ramdisk->ib[iIndex].location[9]).ptr.location[j]).ptr.location[k]).reg.data, data, newSize);
								/* Reposition by the size of the data */
								position += newSize;
								/* Check if this is the last block of the location */
								if ((size - position) < BLKSZ) {
									return 0;
								}
							}
						}
					}
				}
			}
		}
	}
	/* Error : Could not write the data */
	printk("write() Error : Could not write the data");
	return -1;
}

int writeFile(short iIndex, int filePos, unsigned char *data, int dataSize) {
	int position, ret, newSize;
	unsigned char *newData;

	// printk("Data in writeFile(): %s\n", data);

	/* Initialize these values  */
	position = 0;
	newData = (unsigned char *) kmalloc(dataSize, GFP_KERNEL);

	/* Copies the contents of the ib[iIndex] into data */
	adjustPosition(iIndex, newData);
	/* Compute the size of the data */
	newSize = filePos + dataSize;

	/* File position exceeds the maximum allowed size */
	if (newSize > MAXFSZ) {
		/* Write to temporary data container displaced by filePos whatever is possible */
		memcpy(newData + filePos, data, (MAXFSZ - filePos));
		/* Perform the actual write into ramdisk */
		if ((ret = write(iIndex, newData, MAXFSZ)) < 0) {
			printk("writeFile() Error : Could not write file\n");
			return -1;
		}
		/* Traverse through the parent directories and increase their size by the data size */
		if ((ret = modifyParentInodePlus(iIndex, MAXFSZ)) < 0) {
			printk(" writeFile() Error : Could not modify parent inode\n");
			return -1;
		}
		/* Set the file position to the end */
		fdTable[iIndex]->filePos = MAXFSZ;
		return (MAXFSZ - filePos);
	}

	/* Data fits into the data block */
	else {
		/* Write to data container displaced by filePos the data */
		memcpy(newData + filePos, data, dataSize);
		/* Perform the actual write into ramdisk */
		if ((ret = write(iIndex, newData, newSize)) < 0) {
			printk("writeFile() Error : Could not write file\n");
			return -1;
		}
		/* Traverse through the parent directories and increase their size by the data size */
		if ((ret = modifyParentInodePlus(iIndex, newSize)) < 0) {
			printk(" writeFile() Error : Could not modify parent inode\n");
			return -1;
		}
		/* Set the file position to the size of the data */
		fdTable[iIndex]->filePos = newSize;
		return dataSize;
	}
	return -1;
}

int k_write(int fd, char* address, int numBytes) {
	int dataSize, position;
	int numBytesWritten, totalBytes;
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
	/* Initialize these values */
	dataSize = 0; position = 0; totalBytes = 0;
	data = (unsigned char *) kmalloc(DBLKSZ, GFP_KERNEL);
	/* Write data until position reaches the size of the data */
	while (position < numBytes) {
		/* Compute the remaining data size to write */
		dataSize = numBytes - position;
		/* Set data size as size of direct pointer if too big */
		if (dataSize > DBLKSZ) {
			dataSize = DBLKSZ;
		}
		/* Write the data in address displaced by position into temporary data container */
		memcpy(data, address + position, dataSize);
		/* Set and adjust the size of file into the file descriptor table */
		if ((numBytesWritten = writeFile(fd, fdTable[fd]->filePos, data, dataSize)) < 0) {
			printk("k_write() Error : Could not compute number of bytes written to file\n");
			return -1;
		}

		// printk("numBytesWritten: %s\n", numBytesWritten);
		/* Add the position by the data size written to data container */
		position += dataSize;
		/* Add the number of bytes written to address */
		totalBytes += numBytesWritten;
		/* Reset the temporary data container */
		memset(data, 0, dataSize);
	}
	printk("Total Bytes Written: %d\n", totalBytes);
	return totalBytes;
}

EXPORT_SYMBOL(plusParentInodeSize);
EXPORT_SYMBOL(modifyParentInodePlus);
EXPORT_SYMBOL(write);
EXPORT_SYMBOL(writeFile);
EXPORT_SYMBOL(k_write);