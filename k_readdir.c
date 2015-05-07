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

void addInodeStr(char* address){
	short inode;
	unsigned char tens, ones; 
	/* Shift bits to obtain the byte */
    inode = ((struct DirEntry *)address)->inode;
    ones = inode;
    tens = inode << 8;
    /* Convert the character byte into ASCII */
    *((unsigned char *)address + 14) = (unsigned char)(48 + tens);
    *((unsigned char *)address + 15) = (unsigned char)(48 + ones);
	return;
}

int readDirEntry(short index, int filePos, struct DirEntry *dirEntry) {
	int possibleSize;
	unsigned char *data; 
	struct DirEntry *tempDirEntry; 

	data = (unsigned char *) kmalloc(BLKSZ, GFP_KERNEL);

	/* Get the maximum possible file size for this data at index */
	if ((possibleSize = adjustPosition(index, data)) < 0) {
		/* If it returns -1, that means the size is 0 */
		return 0; 
	} 
	/* Check that the max possible file size is less than the current file position */
	if (filePos >= possibleSize) {
		printk("readDirEntry() Error : File position is greater than max possible size\n");
		return -1; 
	}

	while ((filePos + NUMEPB) <= possibleSize) {
		tempDirEntry = (struct DirEntry *) kmalloc(sizeof(struct DirEntry), GFP_KERNEL);
		memcpy(tempDirEntry, data + filePos, NUMEPB); 

		if (tempDirEntry->inode == 0) {
			filePos += NUMEPB;
		} else {
			memcpy(dirEntry, tempDirEntry, NUMEPB);
			fdTable[index]->filePos = filePos + NUMEPB;
			kfree(data);
			kfree(tempDirEntry);
			return 1;
		}
	}
	return 0;
}

int k_readdir(int fd, char* address) {
	int ret; 
	struct DirEntry *dirEntry; 

	dirEntry = (struct DirEntry *) kmalloc(sizeof(struct DirEntry), GFP_KERNEL);

	/* Check that the file is not open */
	if (fdTable[fd] == NULL) {
		printk("k_readdir() Error : File descriptor is not valid\n");
		return -1;
	}

	/* Check that the file is a directory file */
	if (strncmp(ramdisk->ib[fd].type, "dir", 3)) {
		printk("k_readdir() Error : File at given address is not a directory file\n");
		return -1; 
	}

	/* Read the filename from file descriptor and store direcotry into dirEntry */
	if ((ret = readDirEntry(fd, fdTable[fd]->filePos, dirEntry)) < 0) {
		printk("k_readdir() Error : Could not find read directory entry\n");
		return -1;
	} else if (ret == 0) {
		return 0;
	}

	/* String manipulation to return to the user */
	memcpy(address, dirEntry, 16);
	addInodeStr(address);
	return 1;
}

EXPORT_SYMBOL(addInodeStr);
EXPORT_SYMBOL(readDirEntry);
EXPORT_SYMBOL(k_readdir);