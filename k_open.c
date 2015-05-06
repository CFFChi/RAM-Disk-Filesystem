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

	/* Initialize file descriptor for this file */
	fd->filePos = 0;
	fd->inodePtr = &ramdisk->ib[index];
	fdTable[index] = fd;

	// printk("filePos: %d\n ", fdTable[index]->filePos);
	// printk("index in k_open: %d\n", index);

	return index;
}

EXPORT_SYMBOL(k_open);
