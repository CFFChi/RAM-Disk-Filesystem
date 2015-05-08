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

void initializeFD(struct FileDescriptor *fd, int index, struct Inode *inode) {
	fd->filePos = 0;
	fd->inodePtr = inode; 
	fdTable[index] = fd;
	return; 
}

int k_open(char *pathname) {
	int index;
	short parentInode;
	char *filename;
	struct FileDescriptor *fd;

	parentInode = 0;
	filename = (char *) kmalloc(14, GFP_KERNEL);
	/* Check if the file exist in the system */
	if ((index = fileExists(pathname, filename, &parentInode)) <= 0) {
		printk("k_open() Error : File does not exist\n");
		return -1;
	}
	/* Check for invalid file descriptor */
	if (fdTable[index] != NULL) {
		printk("k_open() Error : Other process is accessing this file descriptor\n");
		return -1;
	}
	fd = (struct FileDescriptor *) kmalloc(sizeof(struct FileDescriptor), GFP_KERNEL);
	/* Build the file descriptor for this file */
	initializeFD(fd, index, &ramdisk->ib[index]);
	return index;
}

EXPORT_SYMBOL(k_open);
