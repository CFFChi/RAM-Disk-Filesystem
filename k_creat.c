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

	/* Retrieve last directory entry in pathname and store in parentInode */
	if ((ret = fileExists(pathname, fileName, &parentInode)) != 0) {
		printk("k_creat() Error : File already exists or Error in fileExists()\n");
		return -1;
	}

	printk("Creating %s...\n", fileName);

	if ((freeInode = getFreeInode()) < 0) {
		printk("k_creat() Error : Could not find free index node\n");
		return -1;
	}
	// printk("availability in k_creat: %d\n", ramdisk->sb.numFreeInodes);
	// printk("freeInode in creat: %d\n", freeInode);
	setRegInode(freeInode, 0);
	// printk("parentInode : %d\n", parentInode);
	if ((ret = assignInode(parentInode, freeInode, fileName, 0)) < 0) {
		printk("kcreat() Error: Could not assign freeInode to parentInode\n");
		return -1;
	}
	return 0;
}

EXPORT_SYMBOL(setRegInode);
EXPORT_SYMBOL(k_creat);
