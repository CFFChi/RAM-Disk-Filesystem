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



int k_lseek(int fd, int offset) {
	/* Check if the file exists in the system */
	if (fdTable[fd] == NULL) {
		printk("k_lseek() Error : File Descriptor is not valid\n");
		return -1;
	}

	/* Check if the file is a regular file */
	if (strncmp(ramdisk->ib[fd].type, "reg", 3) != 0) {
		printk("k_lseek() Error : File is not a regular file\n");
		return -1;
	}

	/* Check if the offset is greater than the current file size */
	if (ramdisk->ib[fd].size < offset) {
		/* Return the end of the file position */
		fdTable[fd]->filePos = ramdisk->ib[fd].size;
	} else {
		/* Return the file position */
		fdTable[fd]->filePos = offset;
	}
	return 0;
}

EXPORT_SYMBOL(k_lseek);