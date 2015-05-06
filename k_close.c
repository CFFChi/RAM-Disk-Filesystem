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

int k_close(int index) {
	if (fdTable[index] == NULL) {
		return -1;
	} else {
		fdTable[index] = NULL;
		kfree(fdTable[index]);
	}
	return 0;
}

EXPORT_SYMBOL(k_close);
