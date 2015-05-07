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

void setDirInode(int iIndex, int size) {
	char dir[4] = "dir";
	memcpy(ramdisk->ib[iIndex].type, dir, 4);
	ramdisk->ib[iIndex].size = size;
	return;
}

int k_mkdir(char* pathname) {
	short parentInode;
	int ret, freeInode;
	char* dirName;

	dirName = (char *) kmalloc(14, GFP_KERNEL);

	/* Check if the directory already exists and also grab parent directory index */
	if ((ret = fileExists(pathname, dirName, &parentInode)) > 0) {
		printk("k_creat() Error : File already exists\n");
		return -1;
	} else if (ret < 0) {
		printk("k_creat() Error : Error inside fileExists()\n");
		return -1;
	}

	/* File does not exist so create the directory */
	printk("Creating directory : %s\n", dirName);

	/* Find a free index node */
	if ((freeInode = getFreeInode()) < 0) {
		printk("k_creat() Error : Could not find free index node\n");
		return -1;
	} else {
		/* Set the index node type and size */
		setDirInode(freeInode, 0);
	}

	if ((ret = assignInode(parentInode, freeInode, dirName, 1)) < 0) {
		printk("kcreat() Error: Could not assign freeInode to parentInode\n");
		return -1;
	}

	return 0;
}

EXPORT_SYMBOL(setDirInode);
EXPORT_SYMBOL(k_mkdir);
