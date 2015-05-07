#include <asm/uaccess.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>

#include "ramdisk.h"

MODULE_LICENSE("GPL");

struct Ramdisk *ramdisk;
EXPORT_SYMBOL(ramdisk);
struct FileDescriptor *fdTable[1024];
EXPORT_SYMBOL(fdTable);

static struct file_operations proc_operations;
static struct proc_dir_entry *proc_entry;
static int ramdisk_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);


void cleanInfo(struct IoctlInfo *info) {
	/* Clean up information to prepare for next command */
	info->size = 0;
	info->pathname = 0;
	info = NULL;
	return;
}

void cleanParam(struct IOParameter *param) {
	/* Clean up parameters to prepare for next command */
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
			printk("\nCase: RD_CREAT()...\n");

			info.size = strlen_user((char *) arg);
			info.pathname = (char *) kmalloc(info.size, GFP_KERNEL);

			copy_from_user(info.pathname, (char *) arg, info.size);
			// printk("<1> info->size : %u\n", info.size);
			// printk("<1> info->pathname: %s\n", info.pathname);

			ret = k_creat(info.pathname);
			cleanInfo(&info);
			return ret;
			break;

		case RD_MKDIR:
			printk("\nCase: RD_MKDIR()...\n");

			info.size = strlen_user((char *) arg);
			info.pathname = (char *) kmalloc(info.size, GFP_KERNEL);

			copy_from_user(info.pathname, (char *) arg, info.size);
			// printk("<1> info->size : %u\n", info.size);
			// printk("<1> info->pathname: %s\n", info.pathname);

			ret = k_mkdir(info.pathname);
			cleanInfo(&info);
			return ret;
			break;

		case RD_OPEN:
			printk("\nCase: RD_OPEN()...\n");

			info.size = strlen_user((char *) arg);
			info.pathname = (char *) kmalloc(info.size, GFP_KERNEL);

			copy_from_user(info.pathname, (char *) arg, info.size);
			// printk("<1> info->size : %d\n", info.size);
			// printk("<1> info->pathname: %s\n", info.pathname);

			ret = k_open(info.pathname);
			cleanInfo(&info);
			return ret;
			break;

		case RD_CLOSE:
			printk("\nCase : RD_CLOSE()...\n");

			copy_from_user(&fd, (int *)arg, sizeof(int));
			// printk("<1> fd : %d\n", fd);

			ret = k_close(fd);
			return ret;
			break;

		case RD_READ:
			printk("\nCase: RD_READ()...\n");

			copy_from_user(&param, (struct IOParameter *) arg, sizeof(struct IOParameter));
			// printk("<1> param->fd : %d\n", param.fd);
			// printk("<1> param->address : %x\n", param.address);
			// printk("<1> param->numBytes : %d\n", param.numBytes);

			printk("Data before k_read() : %s\n", param.address);
			ret = k_read(param.fd, param.address, param.numBytes);
			printk("Data after k_read() : %s\n", param.address);

			cleanParam(&param);
			return ret;
			break;

		case RD_WRITE:
			printk("\nCase: RD_WRITE()...\n");

			copy_from_user(&param, (struct IOParameter *) arg, sizeof(struct IOParameter));
			// printk("<1> param->fd : %d\n", param.fd);
			// printk("<1> param->address : %x\n", param.address);
			// printk("<1> param->numBytes : %d\n", param.numBytes);
			// printk("Data before k_write() : %s\n", param.address);
			// printk("Pos before write(): %d\n", fdTable[param.fd]->filePos);
			ret = k_write(param.fd, param.address, param.numBytes);
			// printk("Pos after write(): %d\n", fdTable[param.fd]->filePos);
			// printk("Data after k_write() : %s\n", param.address);

			cleanParam(&param);
			return ret;
			break;

		case RD_LSEEK:
			printk("\nCase: RD_LSEEK()...\n");

			copy_from_user(&param, (struct IOParameter *) arg, sizeof(struct IOParameter));
			// printk("<1> param->fd : %d\n", param.fd);
			// printk("<1> param->address : %x\n", param.address);
			// printk("<1> param->numBytes (offset) : %d\n", param.numBytes);

			// printk("<1> Pos before lseek(): %d\n", fdTable[param.fd]->filePos);
			ret = k_lseek(param.fd, param.numBytes);
			// printk("<1> Pos after lseek(): %d\n", fdTable[param.fd]->filePos);

			cleanParam(&param);
			return ret;
			break;

		case RD_UNLINK:
			printk("\nCase: RD_UNLINK()...\n");

			info.size = strlen_user((char *) arg);
			info.pathname = (char *) kmalloc(info.size, GFP_KERNEL);

			copy_from_user(info.pathname, (char*) arg, info.size);
			// printk("<1> info->size : %u\n", info.size);
			// printk("<1> info->pathname: %s\n", info.pathname);

			ret = k_unlink(info.pathname);
			cleanInfo(&info);
			return ret;
			break;

		case RD_READDIR: 
			printk("\nCase: RD_READDIR()...\n");
			copy_from_user(&param, (struct IOParameter *) arg, sizeof(struct IOParameter));
			printk("<1> param->fd : %d\n", param.fd);
			printk("<1> param->address : %x\n", param.address);
			printk("<1> param->numBytes (offset) : %d\n", param.numBytes);

			retAddress = (char *) kmalloc(NUMEPB, GFP_KERNEL);
			ret = k_readdir(param.fd, retAddress);

			copy_to_user(param.address, retAddress, NUMEPB);
			cleanParam(&param);
			return ret; 
			break;

		default:
			printk("\nUnknown ioctl() command. Exiting...\n");
			return -1;
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
	}

	printk("<1> Ramdisk Initialization Complete...\n");
	return 0;
}

static void __exit cleanup_routine(void) {
	printk("<1> Dumping Ramdisk File System Module...\n");
	vfree(ramdisk);
	remove_proc_entry("ramdisk", NULL);
	return;
}

module_init(initialiaze_routine);
module_exit(cleanup_routine);
