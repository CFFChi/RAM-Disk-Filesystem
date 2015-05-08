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

void setRegInode(int iIndex, int size) {
	char reg[4] = "reg";
	memcpy(ramdisk->ib[iIndex].type, reg, 4);
	ramdisk->ib[iIndex].size = size;
	return;
}

int k_creat(char* pathname) {
	short parentInode;
	int ret, freeInode;
	char* fileName;

	parentInode = 0;
	fileName = (char *) kmalloc(14, GFP_KERNEL);

	/* Retrieve last directory entry in pathname and store parent index in parentInode */
	if ((ret = fileExists(pathname, fileName, &parentInode)) != 0) {
		printk("k_creat() Error : File already exists or Error in fileExists()\n");
		kfree(fileName);
		return -1;
	}
	/* Find free index node */
	if ((freeInode = getFreeInode()) < 0) {
		printk("k_creat() Error : Could not find free index node\n");
		return -1;
	} else {
		/* Set the type and size of the retrieved index node */
		setRegInode(freeInode, 0);
	}

	/* Assign the file name to the index node */
	if ((ret = assignInode(parentInode, freeInode, fileName, 0)) < 0) {
		printk("kcreat() Error: Could not assign freeInode to parentInode\n");
		kfree(fileName);
		return -1;
	}
	kfree(fileName);
	return 0;
}

EXPORT_SYMBOL(setRegInode);
EXPORT_SYMBOL(k_creat);
