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
	offset = ~(0x01 << (7 - (bIndex % 8)));
	/* Clear the bit */
	ramdisk->bb.byte[bIndex / 8] = byte & offset; 
	/* Increment number of free blocks available in Super Block*/
	ramdisk->sb.numFreeBlocks++;
	return;
}

int removeDirEntry(short index, char* targetFilename) {
	int i, j, k, l; 
	struct DirEntry entry; 

	targetFilename++;

	if (!strncmp(ramdisk->ib[index].type, "reg", 3)) {
		printk("removeDirEntry() Error : File is not a directory file\n");
		return -1;
	}
	for (i = 0; i < NUMPTRS; i++) {
		switch (i) {
			case DPTR1: 
			case DPTR2: 
			case DPTR3: 
			case DPTR4: 
			case DPTR5: 
			case DPTR6: 
			case DPTR7: 
			case DPTR8: { 
				if (ramdisk->ib[index].location[i] == 0) { continue; }
				for (j = 0; j < NUMEPB; j++) {
					entry = ramdisk->ib[index].location[i]->dir.entry[j];
					// printk("CompareDPTR: %s AND %s\n", entry.filename, targetFilename);
					if (!strncmp(entry.filename, targetFilename, 14)) {
						memset(&entry, 0, sizeof(struct DirEntry));
						if (isEmpty(ramdisk->ib[index].location[i])) {
							cleanupDirLocation(ramdisk->ib[index].location[i]);
							ramdisk->ib[index].location[i] = NULL;
						}
						return 0;
					}
				}
			}

			case RPTR: {
				if (ramdisk->ib[index].location[8] == 0) { continue; }

				for (j = 0; j < NODESZ; j++) {
					if (ramdisk->ib[index].location[i]->ptr.location[j] == 0) { continue; }

					for (k = 0; k < NUMEPB; k++) {
						entry = ramdisk->ib[index].location[8]->ptr.location[j]->dir.entry[k];
						// printk("CompareRPTR: %s AND %s\n", entry.filename, targetFilename);
						if (!strncmp(entry.filename, targetFilename, 14)) {
							memset(&entry, 0, sizeof(struct DirEntry));
							if (isEmpty(ramdisk->ib[index].location[8]->ptr.location[j])) {
								cleanupDirLocation(ramdisk->ib[index].location[8]->ptr.location[j]);
								ramdisk->ib[index].location[8]->ptr.location[j] = NULL;
							}
						}
					}
					if (isEmpty(ramdisk->ib[index].location[8])) {
						cleanupDirLocation(ramdisk->ib[index].location[8]);
						ramdisk->ib[index].location[8] = NULL;
					}
					return 0; 
				}
			}

			case RRPTR: {
				if (ramdisk->ib[index].location[9] == 0) { continue; }

				for (j = 0; j < NODESZ; j++) {
					if (ramdisk->ib[index].location[9]->ptr.location[j] == 0) { continue; }

					for (k = 0; k < NODESZ; k++) {
						if (ramdisk->ib[index].location[9]->ptr.location[j]->ptr.location[k] == 0) { continue; }

						for (l = 0; l < NUMEPB; l++) {
							entry = ramdisk->ib[index].location[9]->ptr.location[j]->ptr.location[k]->dir.entry[l];
							// printk("CompareRRPTR: %s AND %s\n", entry.filename, targetFilename);
							if (!strncmp(entry.filename, targetFilename, 14)) {
								memset(&entry, 0, sizeof(struct DirEntry));
								if (isEmpty(ramdisk->ib[index].location[9]->ptr.location[j]->ptr.location[k])) {
									cleanupDirLocation(ramdisk->ib[index].location[9]->ptr.location[j]->ptr.location[k]);
									ramdisk->ib[index].location[9]->ptr.location[j]->ptr.location[k] = NULL;
								}
								// break;
							}
						}
						if (isEmpty(ramdisk->ib[index].location[9]->ptr.location[j])) {
							cleanupDirLocation(ramdisk->ib[index].location[9]->ptr.location[j]);
							ramdisk->ib[index].location[9]->ptr.location[j] = NULL;
						}
					}
					if (isEmpty(ramdisk->ib[index].location[9])) {
						cleanupDirLocation(ramdisk->ib[index].location[9]);
						ramdisk->ib[index].location[9] = NULL;
					}
					return 0;
				}
			}
		}
	}
	return -1;
}

void cleanupRegLocation(union Block *location) {
	unsigned int bIndex;
	unsigned char byte, offset;

	/* Retrieve element index in free block */
	bIndex = (location - ramdisk->fb) / BLKSZ;
	/* Figure out corresponding byte number and offset in Bitmap Block */
	byte = ramdisk->bb.byte[bIndex / 8];
	offset = ~(0x01 << (7 - (bIndex % 8)));
	/* Clear the bit */
	ramdisk->bb.byte[bIndex / 8] = byte & offset; 
	/* Increment number of free blocks available in Super Block */
	ramdisk->sb.numFreeBlocks++;
	/* Set the block bytes to zero */
	memset(location->reg.data, 0, BLKSZ);
	return;
}

int removeRegEntry(short index) {
	int i, j, k;

	for (i = 0; i < NUMPTRS; i++) {
		if (ramdisk->ib[index].location[i] == 0) { return 0; }
		switch (i) {
			case DPTR1: 
			case DPTR2: 
			case DPTR3: 
			case DPTR4: 
			case DPTR5: 
			case DPTR6: 
			case DPTR7: 
			case DPTR8: {
				cleanupRegLocation(ramdisk->ib[index].location[i]);
				continue;
			}

			case RPTR: {
				for (j = 0; j < NODESZ; j++) {
					if (ramdisk->ib[index].location[8]->ptr.location[j] == 0) { break; }
					cleanupRegLocation(ramdisk->ib[index].location[8]->ptr.location[j]);
				}
				cleanupRegLocation(ramdisk->ib[index].location[8]);
				continue;
			}

			case RRPTR: {
				for (j = 0; j < NODESZ; j++) {
					if (ramdisk->ib[index].location[9]->ptr.location[j] == 0) { break; }

					for (k = 0; k < NUMEPB; k++) {
						if (ramdisk->ib[index].location[9]->ptr.location[j]->ptr.location[k] == 0) { break; }
						cleanupRegLocation(ramdisk->ib[index].location[9]->ptr.location[j]->ptr.location[k]);
					}
					cleanupRegLocation(ramdisk->ib[index].location[9]->ptr.location[j]);
				}
				cleanupRegLocation(ramdisk->ib[index].location[9]);
				continue;
			}
		}
	}
	memset(&ramdisk->ib[index], 0, sizeof(struct Inode));
	return 0;
}

int minusParentInodeSize(char* pathName, char* lastPath, int* currentInode, int blkSize) {
	int index;
	unsigned int pathSize;
	char *tempPath, *subPath; 

	index = 0; pathSize = 0; pathName++;
	tempPath = (char *) kmalloc(14, GFP_KERNEL);

	/* Walk through the absolute pathName split up with / character */
	while ((subPath = strchr(pathName, '/')) != NULL) {
		pathSize = subPath - pathName;

		if (1 > pathSize || pathSize > 14) {
			printk("minusParentInodeSize() Error: Pathname is too small or too big (14 character max)\n");
			return -1;
		} else {
			strncpy(tempPath, pathName, pathSize);
			tempPath[pathSize] = '\0';
			pathName = subPath + 1;

			if ((index = getInode(index, tempPath)) < 0) {
				printk("minusParentInodeSize() Error: Could not get index node from pathName\n");
				return -1;
			}

			*currentInode = index;
			/* Reduce the size of current index node by blkSize */
			ramdisk->ib[index].size = ramdisk->ib[index].size - blkSize;
		}
	}

	strncpy(lastPath, pathName, pathSize);
	return 0;
}

int modifyParentInodeMinus(char *pathName, int blkSize) {
	int ret, index, pathSize;
	char* lastPath;

	/* Check that the file is not the root */
	if (pathName[0] != '/') {
		printk("fileExists() Error : Path requires the root character\n");
		return -1;
	}
	/* Check that path name is valid */
	if ((pathSize = strlen(pathName)) < 2) {
		printk("fileExists() Error : Path requires a file name following the root character\n");
		return -1;
	}

	index = 0; 
	lastPath = (char *) kmalloc(14, GFP_KERNEL);

	/* Reduce the size of inode in root by blkSize */
	ramdisk->ib[index].size = ramdisk->ib[index].size - blkSize;

	/* Recursively go through each parent and reduce each inode by blkSize */
	if ((ret = minusParentInodeSize(pathName, lastPath, &index, blkSize)) < 0) {
		printk("modifyParentInode() Error : Could not properly decrease parent inode size\n");
		return -1;
	}

	if (pathName[0] == '\0') {
		printk("fileExists() Error : Last character of path name is /\n");
		return -1;
	}

	strncpy(lastPath, pathName, pathSize);
	return removeDirEntry(index, lastPath);
}

int k_unlink(char* pathName) {
	int index, ret;
	short parentInode;
	char* filename;

	parentInode = 0; 
	filename = (char *) kmalloc(14, GFP_KERNEL);

	/* Check that the file is not the root */
	if (!strncmp(pathName, "/\0", 2)) {
		printk("k_unlink() Error : You can not delete the root\n");
		return -1;
	}

	/* Check that the file exists */
	if ((index = fileExists(pathName, filename, &parentInode)) <= 0) {
		printk("k_unlink() Error : File does not exist\n");
		return -1;
	}
	/* Check that the file is not open */
	if (fdTable[index] != NULL) {
		printk("k_unlink() Error : You can not delete a file that is open\n");
		return -1;
	}

	/* Directory File */
	if (!strncmp(ramdisk->ib[index].type, "dir", 3)) {
		/* Check that the directory is empty */
		if (ramdisk->ib[index].size > 0) {
			printk("k_unlink() Error : You can not remove a non-empty directory");
			return -1;
		}
		/* Remove the directory file */
		if ((ret = removeDirEntry(parentInode, filename)) != 0) {
			printk("k_unlink() Error : Could not remove directory entry\n");
			return -1;
		}
		/* Set the index node to zero */
		memset(&ramdisk->ib[index], 0, sizeof(struct Inode));
		/* Increment number of free index node */
		ramdisk->sb.numFreeInodes++;
		return 0;
	}
	/* Regular File */
	else if (!strncmp(ramdisk->ib[index].type, "reg", 3)) {
		/* Remove file */
		if ((ret = removeRegEntry(index)) != 0) {
			printk("k_unlink() Error : Could not remove regular file\n");
			return -1;
		}
		/* Modify/Remove index node entries in parent directory of file */
		if ((ret = modifyParentInodeMinus(pathName, ramdisk->ib[index].size)) != 0) {
			printk("k_unlink() Error : Could not modify the information of parent index nodes\n");
			return -1;
		}
		/* Set the index node to zero */
		memset(&ramdisk->ib[index], 0, sizeof(struct Inode));
		/* Increment number of available free index node*/
		ramdisk->sb.numFreeInodes++;
		return 0;
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
