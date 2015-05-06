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


void cleanupDirLocation(union Block *location) {
	unsigned int bIndex;
	unsigned char byte, offset;

	/* Retrieve element index in free block */
	bIndex = (location - ramdisk->fb) / BLKSZ;
	/* Figure out corresponding byte number and offset in Bitmap Block */
	byte = ramdisk->bb.byte[bIndex / 8];
	offset = SET_BIT_MASK0(bIndex % 8);
	/* Clear the bit specified by bIndex/8 */
	ramdisk->bb.byte[bIndex / 8] = byte & offset;
	/* Increment number of free blocks available in Super Block*/
	ramdisk->sb.numFreeBlocks++;

	/* Nullify the current location */
	// locationToNull = NULL;

	return;
}

int removeDirEntry(short iIndex, char* filename) {

	int i, j, k, l;
	struct Inode *inode;
	struct DirBlock *dirBlock;
	struct PtrBlock *ptrBlock;
	union Block *location, *singlePtr, *doublePtr;

	inode = &(ramdisk->ib[iIndex]);

	// printk("index in removeDir: %d\n", iIndex);

	if (strncmp(inode->type, "reg", 3) == 0) {
		printk("removeDirEntry() Error : File is not a directory file\n");
		return -1;
	}

	for (i = 0; i < NUMPTRS; i++) {
		location = inode->location[i];

		if (0 <= i && i <= 7) {
			if (location == 0) { continue ;	}

			// printk("here\n");
			dirBlock = &(location->dir);

			for (j = 0; j < NUMEPB; j++) {

				// printk("i: %d, j: %d\n", i , j);
				// printk("Comparison1: %s AND %s\n", dirBlock->entry[j].filename, filename);

				if (strncmp(dirBlock->entry[j].filename, filename, 14) == 0) {

					memset(&(dirBlock->entry[j]), 0, sizeof(struct DirEntry));
					if (isEmpty(location)) {
						cleanupDirLocation(location);
					}
					return 0;
				}
			}
		} else if (8 <= i && i <= 9) {
			if (location == 0) { continue; }

			ptrBlock = &(location->ptr);

			for (j = 0; j < NODESZ; j++) {

				singlePtr = ptrBlock->location[j];
				if (singlePtr == 0) { continue; }

				dirBlock = &(singlePtr->dir);

				/* Single Redirection Block */
				if (i == 8) {
					for (k = 0; k < NUMEPB; k++) {
						// printk("Comparison2: %s AND %s\n", dirBlock->entry[k].filename, filename);
						if ((strncmp(dirBlock->entry[k].filename, filename, 14)) == 0) {

							printk("here\n");

							memset(&(dirBlock->entry[k]), 0, sizeof(struct DirEntry));
							if (isEmpty(singlePtr)) {
								cleanupDirLocation(singlePtr);
							}
						}
					}
					/* Depth Zero */
					if (isEmpty(location)) {
						cleanupDirLocation(location);
					}
					return 0;
				}
				/* Double Redirection Block */
				if (i == 9) {
					for (k = 0; k < NODESZ; k++) {
						doublePtr = singlePtr->ptr.location[k];
						if (doublePtr == 0) { continue; }

						dirBlock = &(doublePtr->dir);

						/* Depth Two */
						for (l = 0; l < NUMEPB; l++) {
							printk("Comparison3: %s AND %s\n", dirBlock->entry[l].filename, filename);
							if (strncmp(dirBlock->entry[l].filename, filename, 14) == 0) {
								memset(&(dirBlock->entry[l]), 0, sizeof(struct DirEntry));
								if (isEmpty(doublePtr)) {
									cleanupDirLocation(doublePtr);
								}
							}
						}
						/* Depth One */
						if (isEmpty(singlePtr)) {
							cleanupDirLocation(singlePtr);
						}
						/* Depth Zero */
						if (isEmpty(location)) {
							cleanupDirLocation(location);
						}
						return 0;
					}
				}
			}
		}
	}
	/* Error : Could not find filename */
	printk("removeDirEntry() Error : Could not find a matching directory file\n");
	return -1;
}

void cleanupRegLocation(union Block *location) {


	unsigned int bIndex;
	unsigned char byte, offset;

	printk("inside cleanupRegLocation\n");

	/* Set the block bytes to zero */
	memset(location->reg.data, 0, BLKSZ);
	/* Retrieve element index in free block */
	bIndex = (location - ramdisk->fb) / BLKSZ;
	/* Figure out corresponding byte number and offset in Bitmap Block */
	byte = ramdisk->bb.byte[bIndex / 8];
	offset = SET_BIT_MASK0(bIndex % 8);
	/* Clear bit */
	ramdisk->bb.byte[bIndex / 8] = byte & offset;
	/* Increment number of free blocks available in Super Block */
	// printk(" b avilability in cleanupRegLocation: %d\n", ramdisk->sb.numFreeInodes);
	ramdisk->sb.numFreeBlocks++;
	// printk(" a avilability in cleanupRegLocation: %d\n", ramdisk->sb.numFreeInodes);
	return;
}

int removeRegEntry(short index) {

	struct Inode *inode = &(ramdisk->ib[index]);

	int i, j, k;
	union Block* location, *singlePtr, *doublePtr;


	for (i = 0; i < NUMPTRS; i++) {

		location = inode->location[i];

		/* There is nothing inside this block so just return */
		if (location == 0) {
			memset(ramdisk->ib[index].type, 0, 4);
			return 0;
		}

		// printk("in removeRegEntry\n");
		// printk("here\n");
			// printk("i: %d\n", i);

		if (0 <= i && i <= 7) {
			cleanupRegLocation(location);
		} else {
			if (i == 8) {
				/*　Remove file blocks inside Single Redirection Block */
				for (j = 0; j < NODESZ; j++) {
						singlePtr = ((*inode->location[8]).ptr.location[j]);
						if (singlePtr == 0) { break; }
						cleanupRegLocation(singlePtr);
				}
				/* Remove Single Redirection Block */
				cleanupRegLocation(location);
			}

			if (i == 9) {
				/* Depth One of Double Redirection Block */
				for (j = 0; j < NODESZ; j++) {
					singlePtr = ((*inode->location[9]).ptr.location[j]);
					if (singlePtr == 0) { break; }
					/* Remove all files in Second pointer of Double Redirection Blocks */
					for (k = 0; k < NODESZ; k++) {
						doublePtr = ((*(*inode->location[9]).ptr.location[j]).ptr.location[k]);
						if (doublePtr == 0) { break; }
						cleanupRegLocation(doublePtr);
					}
					/* Remove all files in first pointer of Double REdirection Blocks */
					cleanupRegLocation(singlePtr);
				}
				/* Remove Double Redirection Blocks */
				cleanupRegLocation(location);
			}
		}
	}

	/* Set the memory of index node to zero */
	memset(inode, 0, sizeof(struct Inode));
	return 0;
}

int minusParentInodeSize(char* pathname, char* path, int* currentInode, int blkSize) {
	unsigned int size;
	char *subpath;
	int index;

	index = 0;

	// printk("before while: %s\n", pathname);

	/* Disregard the root character '/' */
	pathname++;

	/* Walk through the absolute pathname split up with / character */
	while ((subpath = strchr(pathname, '/')) != NULL) {
		size = subpath - pathname;
		if (size < 1 || size > 14) {
			printk("minusParentInodeSize() Error: Pathname is too small or too big (14 character max)\n");
			return -1;
		} else {
			strncpy(path, pathname, size);
			path[size] = '\0';
			pathname = subpath;
			if ((index = getInode(index, subpath)) < 0) {
				printk("minusParentInodeSize() Error: Could not get inode from pathname\n");
				return -1;
			}
			memcpy(&currentInode, &index, sizeof(int));
			/* Reduce the size of current index node by blkSize */
			ramdisk->ib[index].size = ramdisk->ib[index].size - blkSize;
		}
	}
	return 0;
}

int modifyParentInodeMinus(char *pathname, int blkSize) {
	int ret;
	int index;
	char* path;

	// printk("pathname in modifyParentInodeMinus(): %s\n", pathname);

	/* Error checking for valid input  */
	if (pathname[0] != '/' || strlen(pathname) < 2) {
		printk("fileExist() Error: Invalid pathname\n");
		return -1;
	}

	index = 0;
	path = (char *) kmalloc(14, GFP_KERNEL);

	/* Reduce the size of inode in root by blkSize */
	ramdisk->ib[index].size = ramdisk->ib[index].size - blkSize;

	/* Recursively go through each parent and reduce each inode by blkSize */
	if ((ret = minusParentInodeSize(pathname, path, &index, blkSize)) < 0) {
		printk("modifyParentInode() Error : Could not properly decrease parent inode size\n");
		return -1;
	}

	// printk("ret in modifyParentInode: %d\n", ret);
	// printk("%s\n", (*ramdisk->ib[0].location[0]).dir.entry[0].filename);
	// printk("index %d\n", index);

	if (*pathname == '\0') {
		return -1;
	} else {
		strncpy(path, pathname, 14);
		printk("path  in modifyMinus: %s\n", path++);
		return removeDirEntry(index, path);
	}
}

int k_unlink(char* pathname) {

	int index, size, ret;
	short parentInode;
	char* filename;

	// printk("%s\n", (*ramdisk->ib[0].location[0]).dir.entry[0].filename);

	if (strncmp(pathname, "/\0", 2) == 0) {
		printk("k_unlink() Error : You can not delete the root\n");
		return -1;
	}

	filename = (char *) kmalloc(14, GFP_KERNEL);

	parentInode = 0;

	/* index is the index node number */
	if ((index = fileExists(pathname, filename, &parentInode)) <= 0) {
		printk("k_unlink() Error : File does not exist\n");
		return -1;
	}

	/* Check that the file is not open */
	if (fdTable[index] == NULL) {
		/* Directory File */
		if (strncmp(ramdisk->ib[index].type, "dir", 3) == 0) {
			/* Check that the directory is empty */
			if (ramdisk->ib[index].size != 0) {
				printk("k_unlink() Error : You can not remove a non-empty directory");
				return -1;
			}

			/* Set the index node to zero */
			memset(&ramdisk->ib[index], 0, sizeof(struct Inode));

			/* Increment number of free index node */
			ramdisk->sb.numFreeInodes++;
			// printk("availability: %d\n", ramdisk->sb.numFreeInodes);
			if ((ret = removeDirEntry(parentInode, filename)) != 0) {
				printk("k_unlink() Error : Could not remove directory entry\n");
				return -1;
			}

			return 0;
		}
		/* Regular File */
		else if (strncmp(ramdisk->ib[index].type, "reg", 3) == 0) {
			size = ramdisk->ib[index].size;
			// printk("index in k_unlink reg version: %d\n", index);
			// printk("parentInode in k_unlink: %d\n", parentInode);
			// printk("%s\n", (*ramdisk->ib[0].location[0]).dir.entry[0].filename);
			// memset(ramdisk->ib[index].type, 0, 4);

			/* Modify/Remove index node entries in parent directory of file */
			if ((ret = modifyParentInodeMinus(pathname, size)) != 0) {
				printk("k_unlink() Error : Could not modify the information of parent index nodes\n");
				return -1;
			}

			/* Remove file */
			if ((ret = removeRegEntry(index)) != 0) {
				printk("k_unlink() Error : Could not remove regular file\n");
				return -1;
			}
			
			/* Increment number of available free inodes*/
			ramdisk->sb.numFreeInodes++;
			// printk("availability in k_unlink: %d\n", ramdisk->sb.numFreeInodes);
			return 0;
		}
	} else {
		printk("k_unlink() Error : File to delete is currently open\n");
		return -1;
	}
	return -1;
}

EXPORT_SYMBOL(cleanupDirLocation);
EXPORT_SYMBOL(removeDirEntry);
EXPORT_SYMBOL(cleanupRegLocation);
EXPORT_SYMBOL(removeRegEntry);
EXPORT_SYMBOL(minusParentInodeSize);
EXPORT_SYMBOL(modifyParentInodeMinus);
EXPORT_SYMBOL(k_unlink);
