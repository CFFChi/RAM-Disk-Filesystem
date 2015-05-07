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

/* http://stackoverflow.com/questions/26688413/how-to-extract-digits-from-a-number-in-c-begining-from-the-most-significant-dig */
void addInodeStr(char* address){
	short inode;
	int tens, ones; 

    inode = ((struct DirEntry *)address)->inode;
    tens = inode / 10;
    ones = inode % 10;

    *((unsigned char *)address + 14) = (48 + tens);
    *((unsigned char *)address + 15) = (48 + ones);

	return;
}

int readDirEntry(short iIndex, int filePos, struct DirEntry *dirEntry) {
	int possibleSize;
	unsigned char *data; 
	struct DirEntry *tempDirEntry; 

	data = (unsigned char *) kmalloc(MAXFSZ, GFP_KERNEL);
	possibleSize = 0;

	possibleSize = adjustPosition(iIndex, data);
	
	printk("data after: %s\n", data);
	printk("possibleSize: %s\n", possibleSize);

	if (filePos >= possibleSize - 1) {
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
			fdTable[iIndex]->filePos = filePos + NUMEPB;
			// kfree(data);
			kfree(tempDirEntry);
			return 1;
		}
	}
	return 0;
}

int k_readdir(int fd, char* address) {
	int ret; 
	struct DirEntry *dirEntry; 

	dirEntry = (struct DirEntry *) kmalloc(16, GFP_KERNEL);

	if (fdTable[fd] == NULL) {
		printk("k_readdir() Error : File descriptor is not valid\n");
		return -1;
	}

	if (strncmp(ramdisk->ib[fd].type, "dir", 3)) {
		printk("k_readdir() Error : File at given address is not a directory file\n");
		return -1; 
	}

	if ((ret = readDirEntry(fd, fdTable[fd]->filePos, dirEntry)) < 0) {
		printk("k_readdir() Error : Could not find read directory entry\n");
		return -1;
	} else if (ret == 0) {
		return 0;
	}

	memcpy(address, dirEntry, 16);
	addInodeStr(address);

	printk("Final Address: %s\n", address);
	return 1;
}

EXPORT_SYMBOL(addInodeStr);
EXPORT_SYMBOL(readDirEntry);
EXPORT_SYMBOL(k_readdir);