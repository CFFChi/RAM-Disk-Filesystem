#include <asm/uaccess.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/semaphore.h>

#include "ramdisk.h"

MODULE_LICENSE("GPL");

/* Global Variable Declaration */
struct Ramdisk *ramdisk;
struct FileDescriptor *fdTable[1024];
EXPORT_SYMBOL(ramdisk);
EXPORT_SYMBOL(fdTable);

/* Declare and Initialize the mutex */
DECLARE_MUTEX(RAMutex);

/* Declare functions for ioctl operation */
static struct file_operations proc_operations;
static struct proc_dir_entry *proc_entry;
static int ramdisk_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);

/* Clean up information to prepare for next command */
void cleanInfo(struct IoctlInfo *info) {
	info->size = 0;
	info->pathname = 0;
	info = NULL;
	return;
}

/* Clean up parameters to prepare for next command */
void cleanParameter(struct IOParameter *param) {
	param->fd = 0;
	param->address = 0;
	param->numBytes = 0;
	param = NULL;
	return;
}

/***** Ramdisk Entry Point *****/
static int ramdisk_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg) {
	int ret, fd; 
	struct IoctlInfo info;
	struct IOParameter param;
	char* retAddress; 

	switch (cmd) {
		case RD_CREAT:
			down_interruptible(&RAMutex);
			// printk("\nCase: RD_CREAT()...\n");
			info.size = strlen_user((char *) arg);
			info.pathname = (char *) kmalloc(info.size, GFP_KERNEL);
			copy_from_user(info.pathname, (char *) arg, info.size);
			ret = k_creat(info.pathname);
			cleanInfo(&info);
			up(&RAMutex);
			return ret;
			break;

		case RD_MKDIR:
			down_interruptible(&RAMutex);
			// printk("\nCase: RD_MKDIR()...\n");
			info.size = strlen_user((char *) arg);
			info.pathname = (char *) kmalloc(info.size, GFP_KERNEL);
			copy_from_user(info.pathname, (char *) arg, info.size);
			ret = k_mkdir(info.pathname);
			cleanInfo(&info);
			up(&RAMutex);
			return ret;
			break;

		case RD_OPEN:
			down_interruptible(&RAMutex);
			// printk("\nCase: RD_OPEN()...\n");
			info.size = strlen_user((char *) arg);
			info.pathname = (char *) kmalloc(info.size, GFP_KERNEL);
			copy_from_user(info.pathname, (char *) arg, info.size);
			ret = k_open(info.pathname);
			cleanInfo(&info);
			up(&RAMutex);
			return ret;
			break;

		case RD_CLOSE:
			down_interruptible(&RAMutex);
			// printk("\nCase : RD_CLOSE()...\n");
			copy_from_user(&fd, (int *)arg, sizeof(int));
			ret = k_close(fd);
			up(&RAMutex);
			return ret;
			break;

		case RD_READ:
			down_interruptible(&RAMutex);
			// printk("\nCase: RD_READ()...\n");
			copy_from_user(&param, (struct IOParameter *) arg, sizeof(struct IOParameter));
			ret = k_read(param.fd, param.address, param.numBytes);
			cleanParameter(&param);
			up(&RAMutex);
			return ret;
			break;

		case RD_WRITE:
			down_interruptible(&RAMutex);
			// printk("\nCase: RD_WRITE()...\n");
			copy_from_user(&param, (struct IOParameter *) arg, sizeof(struct IOParameter));
			ret = k_write(param.fd, param.address, param.numBytes);
			cleanParameter(&param);
			up(&RAMutex);
			return ret;
			break;

		case RD_LSEEK:
			down_interruptible(&RAMutex);
			// printk("\nCase: RD_LSEEK()...\n");
			copy_from_user(&param, (struct IOParameter *) arg, sizeof(struct IOParameter));
			ret = k_lseek(param.fd, param.numBytes);
			cleanParameter(&param);
			up(&RAMutex);
			return ret;
			break;

		case RD_UNLINK:
			down_interruptible(&RAMutex);
			// printk("\nCase: RD_UNLINK()...\n");
			info.size = strlen_user((char *) arg);
			info.pathname = (char *) kmalloc(info.size, GFP_KERNEL);
			copy_from_user(info.pathname, (char*) arg, info.size);
			ret = k_unlink(info.pathname);
			cleanInfo(&info);
			up(&RAMutex);
			return ret;
			break;

		case RD_READDIR: 
			down_interruptible(&RAMutex);
			// printk("\nCase: RD_READDIR()...\n");
			copy_from_user(&param, (struct IOParameter *) arg, sizeof(struct IOParameter));
			retAddress = (char *) kmalloc(NUMEPB, GFP_KERNEL);
			ret = k_readdir(param.fd, retAddress);
			copy_to_user(param.address, retAddress, NUMEPB);
			cleanParameter(&param);
			up(&RAMutex);
			return ret; 
			break;

		default:
			printk("\nUnknown ioctl command...\n");
			return -EINVAL;
			break;
	}
}

int initializeRAMDISK(void) {
	/* Create the Ramdisk skeleton */
	if (!(ramdisk = (struct Ramdisk *) vmalloc(sizeof(struct Ramdisk)))) {
		printk("initializeRAMDISK() Error: Could not vmalloc ramdisk\n");
		return 1;
	}
	/* Initialize the Ramdisk with Zeros*/
	memset(ramdisk, 0, sizeof(struct Ramdisk));
	/* Initialize the root at ramdisk[0] with size 0 */
	setRootInode(0, 0);
	/* Initialize Number of Available Free Inodes */
	ramdisk->sb.numFreeInodes = IBARR - 1;
	/* Initialize Number of Available Free Blocks */
	ramdisk->sb.numFreeBlocks = FBARR;
	return 0;
}

/***** Ioctl Entry Point *****/
static int __init initialiaze_routine(void) {
	int ret;
	printk("<1> Loading Ramdisk File System Module...\n");
	/* Ioctl Operation Injection */
	proc_operations.ioctl = ramdisk_ioctl;
	if (!(proc_entry = create_proc_entry("ramdisk", 0666, NULL))) {
		printk("<1> initializeIOCTL() Error: Could not create /proc entry\n");
		return 1;
	}

	proc_entry->proc_fops = &proc_operations;
	/* Ramdisk Initialization */
	if ((ret = initializeRAMDISK()) != 0) {
		printk("initialize_routine() Error : Could not initialize RAMDISK\n");
		return 1;
	} else {
		printk("<1> Ramdisk Initialization Complete...\n");
		return 0;
	}
}

static void __exit cleanup_routine(void) {
	printk("<1> Dumping Ramdisk File System Module...\n");
	vfree(ramdisk);
	remove_proc_entry("ramdisk", NULL);
	return;
}

module_init(initialiaze_routine);
module_exit(cleanup_routine);
