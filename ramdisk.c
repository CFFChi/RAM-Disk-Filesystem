#include <asm/uaccess.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>

#include "ramdisk.h"
#include "ramdisk_ioctl.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Masaya Ando (mando@bu.edu)");

struct Ramdisk *ramdisk; 
struct FileDescriptor *fdTable[1024];

static struct file_operations proc_operations; 
static struct proc_dir_entry *proc_entry;
static int ramdisk_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);

/* Helper Functions for Kernel level IOCTL Functions*/
int isEmpty(union Block *location) {
	struct RegBlock file = location->reg;
	int i = 0; 
	while (i < SIZEOF_BLOCK) {
		if (file.data[i++] != 0) {
			return 0; 
		}
	}
	return 1; 
}

void setDirInode(int iIndex, int size) {
	char dir[4] = "dir";
	memcpy(ramdisk->ib[iIndex].type, dir, 4);
	ramdisk->ib[iIndex].size = size;
	return;
}

void setRegInode(int iIndex, int size) {
	char reg[4] = "reg";
	memcpy(ramdisk->ib[iIndex].type, reg, sizeof(int));
	ramdisk->ib[iIndex].size = size;
	return;
}

void setDirEntry(short fIndex, int eIndex, char* filename, short newInode) {
	memcpy(ramdisk->fb[fIndex].dir.entry[eIndex].filename, filename, 14);
	ramdisk->fb[fIndex].dir.entry[eIndex].inode = newInode; 
	return;
}

void cleanupDirLocation(union Block *location, union Block *locationToNull) {
	unsigned int bIndex;
	unsigned char byte, offset; 
	/* Retrieve element index in free block */
	bIndex = (location - ramdisk->fb) / SIZEOF_BLOCK;
	/* Figure out corresponding byte number and offset in Bitmap Block */
	byte = ramdisk->bb.byte[bIndex / 8]; 
	offset = SET_BIT_MASK0(bIndex % 8);
	/* Clear the bit specified by bIndex/8 */
	ramdisk->bb.byte[bIndex / 8] = byte & offset;
	/* Increment number of free blocks available in Super Block*/
	ramdisk->sb.numFreeBlocks++;
	/* Nullify the current location */
	locationToNull = NULL;

	return;
}

int removeDirEntry(short iIndex, char* filename) {

	int i, j, k, l;
	struct InodeBlock *inode; 
	struct DirBlock *dirBlock; 
	struct PtrBlock *ptrBlock;
	union Block *location, *singlePtr, *doublePtr;

	inode = &(ramdisk->ib[iIndex]);
	if (strncmp(inode->type, "reg", 3) == 0) {
		printk("removeDirEntry() Error : File is not a directory file\n");
		return -1;
	} 

	for (i = 0; i < SIZEOF_LOCATION; i++) {
		location = inode->location[i];

		if (0 <= i && i <= 7) {
			if (location == 0) { continue ;	}

			dirBlock = &(location->dir);

			for (j = 0; j < SIZEOF_DIR_BLOCK; j++) {
				if (strncmp(dirBlock->entry[j].filename, filename, 14) == 0) {
					memset(&(dirBlock->entry[j]), 0, sizeof(struct DirEntry));
					if (isEmpty(location)) {
						cleanupDirLocation(location, inode->location[i]);
					}
					return 0;
				}
			}	
		} else if (8 <= i && i <= 9) {
			if (location == 0) { continue; }

			ptrBlock = &(location->ptr);

			for (j = 0; j < SIZEOF_PTR; j++) {
				singlePtr = ptrBlock->location[j];
				if (singlePtr == 0) { continue; }

				dirBlock = &(singlePtr->dir);

				/* Single Redirection Block */
				if (i == 8) {
					for (k = 0; k < SIZEOF_DIR_BLOCK; k++) {
						/* Depth One */
						if ((strncmp(dirBlock->entry[k].filename, filename, 14)) == 0) {
							memset(&(dirBlock->entry[k]), 0, sizeof(struct DirEntry));
							if (isEmpty(singlePtr)) {
								cleanupDirLocation(singlePtr, ptrBlock->location[j]);
							}
							break; 
						}
					}
					/* Depth Zero */
					if (isEmpty(location)) {
						cleanupDirLocation(location, inode->location[i]);
					}
					return 0;
				}
				/* Double Redirection Block */
				if (i == 9) {
					for (k = 0; k < SIZEOF_PTR; k++) {
						doublePtr = singlePtr->ptr.location[j];
						if (doublePtr == 0) { continue; }

						dirBlock = &(doublePtr->dir);

						/* Depth Two */
						for (l = 0; l < SIZEOF_DIR_BLOCK; l++) {
							if (strncmp(dirBlock->entry[l].filename, filename, 14) == 0) {
								memset(&(dirBlock->entry[l]), 0, sizeof(struct DirEntry));
								if (isEmpty(doublePtr)) {
									cleanupDirLocation(doublePtr, singlePtr->ptr.location[k]);
								}							
							}
						}
						/* Depth One */
						if (isEmpty(singlePtr)) {
							cleanupDirLocation(singlePtr, ptrBlock->location[j]);
						}
						/* Depth Zero */
						if (isEmpty(location)) {
							cleanupDirLocation(location, inode->location[i]);	
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

	/* Set the block bytes to zero */
	memset(location->reg.data, 0, SIZEOF_BLOCK);
	/* Retrieve element index in free block */
	bIndex = (location - ramdisk->fb) / SIZEOF_BLOCK; 
	/* Figure out corresponding byte number and offset in Bitmap Block */
	byte = ramdisk->bb.byte[bIndex / 8]; 
	offset = SET_BIT_MASK0(bIndex % 8);
	/* Clear bit */
	ramdisk->bb.byte[bIndex / 8] = byte & offset; 
	/* Increment number of free blocks available in Super Block */
	ramdisk->sb.numFreeBlocks++;
	return; 
}

int removeRegEntry(struct InodeBlock *inode) {

	int i, j, k;
	union Block* location, *singlePtr, *doublePtr; 


	for (i = 0; i < SIZEOF_LOCATION; i++) {
		location = inode->location[i];

		/* There is nothing inside this block so just return */
		if (location == 0) { return 0; }

		if (0 <= i && i <= 7) { 
			cleanupRegLocation(location); 
		} else {
			if (i == 8) {
				/*　Remove file blocks inside Single Redirection Block */
				for (j = 0; j < SIZEOF_PTR; j++) {
						singlePtr = ((*inode->location[8]).ptr.location[j]);
						if (singlePtr == 0) { break; }				
						cleanupRegLocation(singlePtr);
				}
				/* Remove Single Redirection Block */
				cleanupRegLocation(location); 
			}

			if (i == 9) {
				/* Depth One of Double Redirection Block */
				for (j = 0; j < SIZEOF_PTR; j++) {
					singlePtr = ((*inode->location[9]).ptr.location[j]); 
					if (singlePtr == 0) { break; }
					/* Remove all files in Second pointer of Double Redirection Blocks */
					for (k = 0; k < SIZEOF_PTR; k++) {
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
	memset(inode, 0, sizeof(struct InodeBlock));
	return 0; 
}

void setDirEntryLocation(short iIndex, int bIndex, int eIndex, char*filename, short newInode) {
	memcpy((*ramdisk->ib[iIndex].location[bIndex]).dir.entry[eIndex].filename, filename, 14);
	(*ramdisk->ib[iIndex].location[bIndex]).dir.entry[eIndex].inode = newInode; 
	return;
}

void setSinglePtrLocation(short iIndex, int ptrIndex, short fIndex) {
	ramdisk->ib[iIndex].location[8]->ptr.location[ptrIndex] = &ramdisk->fb[fIndex];
	return;
}

void setDirEntrySinglePtrLocationEntry(short iIndex, int ptrIndex, int eIndex, char* filename, short newInode) {
	memcpy(((*(*ramdisk->ib[iIndex].location[8]).ptr.location[ptrIndex]).dir.entry[eIndex].filename), filename, 14);
	((*(*ramdisk->ib[iIndex].location[8]).ptr.location[ptrIndex]).dir.entry[eIndex].inode) = newInode; 
	return;
}

void setDoublePtr1Location(short iIndex, int ptrIndex, short fIndex) {
	ramdisk->ib[iIndex].location[9]->ptr.location[ptrIndex] = &ramdisk->fb[fIndex];
	return;
}

void setDoublePtr2Location(short iIndex, int ptrIndex1, int ptrIndex2, short fIndex) {
	ramdisk->ib[iIndex].location[9]->ptr.location[ptrIndex1]->ptr.location[ptrIndex2] = &ramdisk->fb[fIndex];
	return;
}

void setDirEntryDoublePtrLocation(short iIndex, int ptrIndex1, int ptrIndex2, int eIndex, char* filename, short newInode) {
	memcpy(((*(*(*ramdisk->ib[iIndex].location[9]).ptr.location[ptrIndex1]).ptr.location[ptrIndex2]).dir.entry[eIndex].filename), filename, 14);
	((*(*(*ramdisk->ib[iIndex].location[9]).ptr.location[ptrIndex1]).ptr.location[ptrIndex2]).dir.entry[eIndex].inode) = newInode; 
	return;
}

int getInode(int index, char* pathname) {
	int i, j, k, l;
	char* file; 
	union Block *location, *singlePtr, *doublePtr; 
	struct InodeBlock *inode; 

	i=0; j=0; k=0; l=0;

	// printk("index1: %d\n", index);

	inode = &(ramdisk->ib[index]);

	// printk("filetype: %s\n", inode->type);

	if (memcmp(inode->type, "dir", 3)) {
		printk("getInode() Error : inode->type is not a directory file\n");
		return -1; 
	}

	for (i = 0; i < SIZEOF_LOCATION; i++) {
		location = inode->location[i];
		/* Loop through DIRECT Block pointers (location[0 ~ 7]) */
		if (0 <= i && i <= 7) {
			if (location != 0) {
				file = location->reg.data;
				for (j = 0; k < SIZEOF_DIR_BLOCK; k++) {
					if (file != 0) {
						// printk("Comparing1 %s AND %s\n", location->dir.entry[k].filename, pathname);
						if (strncmp(location->dir.entry[k].filename, pathname, 14) == 0) {
							return location->dir.entry[k].inode;
						}
					}
				}
			}
		}
		/* Loop through Single Indirect & Double Indirect Block pointers (location[8 ~ 9]) */
		else {

			// printk("here\n");
			if (location == 0) { continue; }

			for (j = 0; j < SIZEOF_PTR; j++) {
				singlePtr = location->ptr.location[j];
				if (singlePtr != 0) { continue; }

				/* Single-Indirect Block Pointer */
				if (i == 8) {
					for (k = 0; k < SIZEOF_DIR_BLOCK; k++) {
						if (singlePtr->reg.data != 0) {
							// printk("Comparing2  %s AND %s\n", singlePtr->dir.entry[j].filename, pathname);
							if (strncmp(singlePtr->dir.entry[j].filename, pathname, 14) == 0) {
								return singlePtr->dir.entry[j].inode;
							}
						}
					}
				} 
				/* Double-Indirect Block Pointer */
				if (i == 9) {
					for (k = 0; k < SIZEOF_PTR; k++) {
						doublePtr = singlePtr->ptr.location[k];
						if (doublePtr != 0) { continue; }

						for (l = 0; l < SIZEOF_DIR_BLOCK; l++) {
							if (doublePtr->reg.data != 0) {
								// printk("Comparing3  %s AND %s\n", doublePtr->dir.entry[l].filename, pathname);
								if (strncmp(doublePtr->dir.entry[l].filename, pathname, 14) == 0) {
									return doublePtr->dir.entry[l].inode;
								}
							}
						}
					}
				}
			}
		}
	}
	/* Inode Not Found */
	return 0; 
}

int fileExists(char *pathname, char* lastPath, short* parentInode) {
	unsigned int size; 
	char *path, *subpath;  
	int index, currentIndex; 

	if (pathname[0] != '/' || strlen(pathname) < 2) {
		printk("fileExist() Error: Invalid pathname\n");
		return -1;
	}

	index = 0; 
	path = (char *) kmalloc(14, GFP_KERNEL);
	pathname++;

	// printk("pathname before while: %s\n", pathname);

	while ((subpath = strchr(pathname, '/')) != NULL) {
		size = subpath - pathname; 

		if (1 > size || size > 14) {
			printk("fileExists() Error : Could not parse pathname / invalid input\n");
			return -1;
		} else {
			strncpy(path, pathname, size);
			path[size] = '\0';
			pathname = subpath + 1; 
			if ((index = getInode(index, path)) < 0) {
				printk("fileExist() Error : Could not get index node from pathname\n");
				return -1; 
			}
		}
	}

	// printk("pathname after while: %s\n", pathname);

	if (*pathname == '\0') {
		printk("fileExists() Error : Last character of path is /\n");
		return -1; 
	} else {
		strncpy(path, pathname, 14);

		if ((currentIndex = getInode(index, path)) < 0) {
			printk("fileExists() Error : Could not get inode from pathname\n");
			return -1; 
		}
		strncpy(lastPath, path, 14);
		*parentInode = index;
		if (currentIndex > 0) {
			return currentIndex;
		}
		/* File does not exist on system */
		printk("Not found\n");
		return 0;
	}
}

int getFreeInode(void) {
	int i;
	/* Check free index node availability */
	if (ramdisk->sb.numFreeInodes == 0) {
		printk("getFreeInode() Error : There is no more free inode in ramdisk\n");
		return -1;
	}

	/* Return index node  */
	for (i = 0; i < MAX_FILE_COUNT; i++) {
		// printk("IB[%d].type : %s\n", i, ramdisk->ib[i].type);
		if (ramdisk->ib[i].type[0] == 0) {
			ramdisk->sb.numFreeInodes--;
			return i; 
		}
	}
	/* No Free Inode Found */
	return -1;
}

int getFreeBlock(void) {
	int i; 
	unsigned char byte; 
	unsigned char offset; 

	if (ramdisk->sb.numFreeBlocks == 0) {
		printk("getFreeBlock() Error : There is no more free block in ramdisk\n");
		return -1; 
	} 

	for (i = 0; i < FB_PARTITION; i++) {
		byte = ramdisk->bb.byte[(i / 8)];
		offset = 7 - (i % 8); 

		if (((byte >> offset) & 0x1) == 0) {
			byte = (byte | SET_BIT_MASK1(i % 8)); 
			ramdisk->bb.byte[(i / 8)] = byte;
			ramdisk->sb.numFreeBlocks--;
			return i;
		}		
	}
	return -1;
}

int assignInode(short k_inode, short newInode, char *filename) {
	int i, j, k, l;
	int freeBlock, fbSingle, fbDouble; 

	/* Loop through block locations[10] */
	for (i = 0; i < SIZEOF_LOCATION; i++) {
		/* Index Node Block is Fully Available*/
		if (ramdisk->ib[k_inode].location[i] == 0) {
			/* Search for free block to allocate for block */
			if ((freeBlock = getFreeBlock()) < 0) {
				printk("assignInode() Error : Could not find free block in ramdisk\n");
				return -1; 
			}

			/* Assign free block to block location i */
			ramdisk->ib[k_inode].location[i] = &ramdisk->fb[freeBlock];

			/* Direct Block Pointers locatoin[0 ~ 7] */
			if (0 <= i && i <= 7) {
				/* Insert an entry into the FreeBlock of Parent Directory */
				setDirEntry(freeBlock, 0, filename, newInode);
				return 0;
			} else {
				/* Single Indirect Block Pointer location[8] */
				if (i == 8) {
					/* Search for another free block for a dirBlock */
					if ((fbSingle = getFreeBlock()) < 0) {
						printk("assignInode() Error : Could not find free block in ramdisk\n");
						return -1; 
					}
					/* Assign Free Block to Index Node List */
					/* Insert a Directory entry into the FreeBlock of Parent Directory */
					setSinglePtrLocation(k_inode, 0, fbSingle);
					setDirEntry(fbSingle, 0, filename, newInode);
					return 0; 
				}

				/* Double Indirect Block Pointer location[9] */
				if (i == 9) {
					/* Get a free dir block for the first pointer */
					if ((fbSingle = getFreeBlock()) < 0) {
						printk("assignInode() Error : Could not find free block in ramdisk\n");
						return -1; 
					}

					/* Assign Pointer to Free Block to the First Redirection */
					setDoublePtr1Location(k_inode, 0, fbSingle);

					/* Get another free dir block for the second pointer */
					if ((fbDouble = getFreeBlock()) < 0) {
						printk("assignInode() Error : Could not find free block in ramdisk\n");
						return -1; 
					}

					/* Assign Pointer to Free Block to Second Redirection */
					/* Insert an Directory Entry into the FreeBlock of Parent Directory */
					setDoublePtr2Location(k_inode, 0, 0, fbDouble);
					setDirEntry(fbDouble, 0, filename, newInode);
					return 0; 
				} 
			}
		} 
		/* Index Node Block is Already Allocated. Need to look for empty entry slot*/
		else {
			if (0 <= i && i <= 7) {
				/* DirBlock has 16 entries */
				for (j = 0; j < SIZEOF_DIR_BLOCK; j++) {
					/* Look for a free location to store a directory entry */
					if ((*ramdisk->ib[k_inode].location[i]).dir.entry[j].inode == 0) {
						/* Insert index node into ib[k_inode].location[j] */
						setDirEntryLocation(k_inode, i, j, filename, newInode);
						return 0; 
					}
				}
			} else {
				if (i == 8) {
					for (j = 0; j < SIZEOF_PTR; j++) {
						/* We found an empty pointer location */
						if ((*ramdisk->ib[k_inode].location[8]).ptr.location[j] == 0) {
							/* Get a Free Block to Assign a Singe Pointer */
							if ((freeBlock = getFreeBlock()) < 0) {
								printk("assignInode() Error : Could not find free block in ramdisk\n");
								return -1; 
							}
							setSinglePtrLocation(k_inode, j, freeBlock);
							setDirEntry(freeBlock, 0, filename, newInode);
							return 0; 
						} 
						/* Loop through the pointer location directory entries to find a free slot */
						else {
							for (k = 0; k < SIZEOF_DIR_BLOCK; k++) {
								if ((*(*ramdisk->ib[k_inode].location[8]).ptr.location[j]).dir.entry[k].inode == 0) {
									setDirEntrySinglePtrLocationEntry(k_inode, j, k, filename, newInode);
									return 0;
								}
							}
						}
					}
				} 
				if (i == 9) {
					for (j = 0; j < SIZEOF_PTR; j++) {
						/* There is no second redirection block */
						if (((*ramdisk->ib[k_inode].location[9]).ptr.location[j]) == 0) {
							if ((fbSingle = getFreeBlock()) < 0) {
								printk("assignInode() Error : Could not find free block1 in ramdisk (i=9)\n");
								return -1; 
							}
							setDoublePtr1Location(k_inode, j, fbSingle);

							if ((fbDouble = getFreeBlock()) < 0) {
								printk("assignInode() Error : Could not find free block2 in ramdisk (i=9)\n");
								return -1; 
							}
							setDoublePtr2Location(k_inode, j, 0, fbDouble);
							
							setDirEntryDoublePtrLocation(k_inode, j, 0, 0, filename, newInode);
							return 0;
						} 
						/* Probe into second redirection block as it is present*/
						else {
							for (k = 0; k < SIZEOF_PTR; k++) {
								/* No Directory Block inside this redirection block */
								if ((*(*ramdisk->ib[k_inode].location[9]).ptr.location[j]).ptr.location[k] == 0) {
									if ((fbSingle = getFreeBlock()) < 0) {
										printk("assignInode() Error : Could not find free block in ramdisk\n");
										return -1; 
									}
									setDoublePtr2Location(k_inode, j, k, fbSingle);
									setDirEntryDoublePtrLocation(k_inode, j, k, 0, filename, newInode);
									return 0;
								} 
								/* Directory Block is inside this redirection block */
								else {
									for (l = 0; l < SIZEOF_DIR_BLOCK; j++) {
										if (((*(*(*ramdisk->ib[k_inode].location[9]).ptr.location[j]).ptr.location[k]).dir.entry[l].inode) == 0) {
											setDirEntryDoublePtrLocation(k_inode, j, k, l, filename, newInode);
											return 0; 
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	/* Unable to find a free directory entry */
	return -1; 
}

int minusParentInodeSize(char* pathname, char* path, int* currentInode, int blkSize) {
	unsigned int size; 
	char *subpath; 
	int index; 

	index = 0;

	printk("before while: %s\n", pathname);

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

	printk("pathname in modifyParentInodeMinus(): %s\n", pathname);

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

	if (*pathname == '\0') {
		return -1;
	} else {
		strncpy(path, pathname, 14);
		return removeDirEntry(index, path);
	}
}

int searchParentInodes(short iIndex, short targetInode, int *size, short* parentInodes) {
	int i, j, k, l, ret; 
	struct InodeBlock *inode; 
	struct DirBlock *dirBlock;
	struct PtrBlock *ptrBlock; 
	union Block *location, *singlePtr, *doublePtr; 

	printk("Inside searchParentInodes\n");

	inode = &(ramdisk->ib[iIndex]);
	if (strncmp(inode->type, "reg", 3) == 0) {
		printk("searchParentInodes() Error : File is not a directory file\n");
		return -1; 
	}

	for (i = 0; i < SIZEOF_LOCATION; i++) {
		location = inode->location[i];

		// printk("Here1\n");

		if (0 <= i && i <= 7) {
			if (location == 0) { continue; }

			// printk("Yep\n");

			dirBlock = &(location->dir); 

			for (k = 0; k < SIZEOF_DIR_BLOCK; k++) {
				if (dirBlock->entry[k].inode == targetInode) { return 1; }

				// printk("entry inode: %d\n", dirBlock->entry[k].inode);

				parentInodes[*size] = dirBlock->entry[k].inode; 
				*size = *size + 1;
				if ((ret = searchParentInodes(dirBlock->entry[k].inode, targetInode, size, parentInodes)) == 1) {
					return 1; 
				} 
				*size = *size - 1;
				parentInodes[*size] = 0;
			}
		}

		if (8 <= i && i <= 9) {

			// printk("here\n");

			if (location == 0) { continue; }

			ptrBlock = &(location->ptr);

			for (j = 0; j < SIZEOF_PTR; j++) {
				singlePtr = ptrBlock->location[j];
				if (singlePtr == 0) { continue; }

				dirBlock = &(singlePtr->dir);

				if (i == 8) {
					for (k = 0; k < SIZEOF_DIR_BLOCK; k++) {
						if (dirBlock->entry[k].inode == targetInode) { return 1; }

						parentInodes[*size] = dirBlock->entry[k].inode;
						*size = *size + 1; 
						if ((ret = searchParentInodes(dirBlock->entry[k].inode, targetInode, size, parentInodes)) == 1) {
							return 1; 
						}
						*size = *size - 1;
						parentInodes[*size] = 0; 
					}
				}

				if (i == 9) {
					for (k = 0; k < SIZEOF_PTR; k++) {

						doublePtr = singlePtr->ptr.location[k];
						if (doublePtr == 0) { continue;	}

						dirBlock = &(doublePtr->dir);

						for (l = 0; l < SIZEOF_DIR_BLOCK; k++) {
							if (dirBlock->entry[l].inode == targetInode) { return 1; }

							parentInodes[*size] = dirBlock->entry[l].inode;
							*size = *size + 1;
							if ((ret = searchParentInodes(dirBlock->entry[l].inode, targetInode, size, parentInodes)) == 1) {
								return 1;
							}
							*size = *size - 1;
							parentInodes[*size] = 0; 
						}
					}
				}
			}
		}
	}
	/* Could not find target index node */
	return 0;
}

int plusParentInodeSize(short targetIndex, short* parentInodes) {
	int ret, size; 
	short root;

	printk("target in plusParent: %d\n", targetIndex);

	root = 0; size = 1; 
	/* Get the directories to the file from root and store it and its size into array */
	if ((ret = searchParentInodes(root, targetIndex, &size, parentInodes)) < 0) {
		printk("plusParentInodeSize() Error : Could not get parent directories\n");
		return -1;
	}

	return size; 
}

int modifyParentInodePlus(short iIndex, int fileSize) {
	int i, oldSize, ret;
	short index; 
	short parentInodes[256];

	printk("index: %d\n", iIndex);
	printk("fileSize : %d\n", fileSize);

	/* Increase the parent directories by the size of the file */
	if ((ret = plusParentInodeSize(iIndex, parentInodes)) < 0) {
		printk("modifyParentInodePlus() Error : Could not increase parent inode size\n");
		return -1; 
	}

	if (ret <= 0) {
		printk("modifyParentInodePlus() Error : Size of parent node array is 0\n");
		return -1; 
	}
	/* Go through the parent directory and increase file size */
	for (i = 0; i < ret; i++) {
		index = parentInodes[i];
		oldSize = ramdisk->ib[index].size;
		ramdisk->ib[index].size = oldSize + fileSize; 
	}	

	/* Finally, increase the size at the index of the file */
	ramdisk->ib[iIndex].size = fileSize; 
	return 0; 
}

int k_unlink(char* pathname) {

	int index, size, ret; 
	short parentInode;
	char* filename; 

	if (strncmp(pathname, "/\0", 2) == 0) {
		printk("k_unlink() Error : You can not delete the root\n");
		return -1;
	}

	filename = (char *) kmalloc(14, GFP_KERNEL);

	printk("pathname: %s\n", pathname);

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
			memset(&ramdisk->ib[index], 0, sizeof(struct InodeBlock));
			/* Increment number of free index node */
			ramdisk->sb.numFreeInodes++;

			if ((ret = removeDirEntry(parentInode, filename)) != 0) {
				printk("k_unlink() Error : Could not remove directory entry\n");
				return -1;
			}

			return 0;
		} 
		/* Regular File */
		else if (strncmp(ramdisk->ib[index].type, "reg", 3) == 0) {
			size = ramdisk->ib[index].size; 

			/* Remove file */
			if ((ret = removeRegEntry(&(ramdisk->ib[index]))) != 0) { 
				printk("k_unlink() Error : Could not remove regular file\n");
				return -1; 
			} 

			/* Modify/Remove index node entries in parent directory of file */
			if ((ret = modifyParentInodeMinus(pathname, size)) != 0) {
				printk("k_unlink() Error : Could not modify the information of parent index nodes\n");
				return -1; 
			}

			/* Increment number of available free inodes*/
			ramdisk->sb.numFreeInodes++;
			return 0;
		}
	} else {
		printk("k_unlink() Error : File to delete is currently open\n");
		return -1;
	}
	return -1;
}

int k_creat(char* pathname) {
	short parentInode; 
	int ret, freeInode; 
	char* filename;

	filename = (char *) kmalloc(14, GFP_KERNEL);

	/* Retrieve last directory entry in pathname and store in parentInode */
	if ((ret = fileExists(pathname, filename, &parentInode)) != 0) {
		printk("k_creat() Error : File already exists or Error in fileExists()\n");
		return -1;
	}

	/* File Creation 
	 * 1) Search for free index node 
	 * 2) Set inode.type=REG and inode.size=0
	 * 3) Assign inode to parentInode   */
	if ((freeInode = getFreeInode()) < 0) {
		printk("k_creat() Error : Could not find free index node\n");
		return -1; 
	}

	printk("freeInode: %d\n", freeInode);
	setRegInode(freeInode, 0);

	printk("parentInode : %d\n", parentInode);

	if ((ret = assignInode(parentInode, freeInode, filename)) < 0) {
		printk("kcreat() Error: Could not assign freeInode to parentInode\n");
		return -1;
	} 
	return 0;
}

int k_open(char *pathname) {
	int index; 
	short parentInode; 
	char *filename; 
	struct FileDescriptor *fd;

	filename = (char *) kmalloc(14, GFP_KERNEL);

	if ((index = fileExists(pathname, filename, &parentInode)) <= 0) {
		printk("k_open() Error : File does not exist\n");
		return -1; 
	}

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

	return index; 
}

int k_close(int index) {
	if (fdTable[index] == NULL) {
		return -1; 
	} else {
		fdTable[index] = NULL;
		kfree(fdTable[index]);
	}
	return 0; 
}

void constructInode(short iIndex, unsigned char* data) {
	int i, j, k, position; 
	position = 0;


	for (i = 0; i < SIZEOF_LOCATION; i++) {
		/* Check if the block is empty or allocated */
		/* Block is not allocated yet so return */
		if (ramdisk->ib[iIndex].location[i] == 0) {
			return; 
		} 
		/* Block is already allocated so loop through and find position */
		else {
			switch (i) {
				case DIRECT_PTR1: 
				case DIRECT_PTR2: 
				case DIRECT_PTR3: 
				case DIRECT_PTR4: 
				case DIRECT_PTR5: 
				case DIRECT_PTR6: 
				case DIRECT_PTR7: 
				case DIRECT_PTR8: 
					memcpy(data + position, ramdisk->ib[iIndex].location[i], SIZEOF_BLOCK);	
					position += SIZEOF_BLOCK;
					continue; 
					 
				case SINGLE_REDIRECT_PTR:
					for (j = 0; j < SIZEOF_PTR; j++) {
						if (((*ramdisk->ib[iIndex].location[8]).ptr.location[j]) == 0) {
							return; 
						} 
						memcpy(data + position, (*ramdisk->ib[iIndex].location[8]).ptr.location[j], SIZEOF_BLOCK);
						position += SIZEOF_BLOCK;
					}
					continue; 

				case DOUBLE_REDIRECT_PTR:
					for (j = 0; j < SIZEOF_PTR; j++) {
						if (((*ramdisk->ib[iIndex].location[9]).ptr.location[j]) == 0) {
							return; 
						}

						for (k = 0; k < SIZEOF_DIR_BLOCK; k++) {
							if (((*(*ramdisk->ib[iIndex].location[9]).ptr.location[j]).ptr.location[k]) == 0) {
								return; 
							}
							memcpy(data + position, ((*(*ramdisk->ib[iIndex].location[9]).ptr.location[j]).ptr.location[k]), SIZEOF_BLOCK);
							position += SIZEOF_BLOCK;
						}
					}
					continue;
			}
		}
	} 
	return;
}

int write(short iIndex, unsigned char *data, int size) {

	int i, j, k;
	int newSize, position; 
	int freeBlock, fbSingle, fbDouble; 

	position = 0; 
	for (i = 0; i < SIZEOF_LOCATION; i++) {
		if (ramdisk->ib[iIndex].location[i] == 0) {
			if ((freeBlock = getFreeBlock()) < 0) {
				printk("write() Error : Could not find free block in ramdisk\n");
				return -1; 
			}

			ramdisk->ib[iIndex].location[i] = &ramdisk->fb[freeBlock];
		}

		switch (i) {
			case DIRECT_PTR1:
			case DIRECT_PTR2:
			case DIRECT_PTR3:
			case DIRECT_PTR4:
			case DIRECT_PTR5:
			case DIRECT_PTR6:
			case DIRECT_PTR7:
			case DIRECT_PTR8:
				newSize = size - position; 
				if (newSize > MAX_FILE_SIZE) {
					size = MAX_FILE_SIZE;
				}
				memcpy((*ramdisk->ib[iIndex].location[i]).reg.data, data + position, size);
				position += newSize;
				if ((newSize = size - position) < SIZEOF_BLOCK) { return 0; }
				continue;

			case SINGLE_REDIRECT_PTR: 
			case DOUBLE_REDIRECT_PTR:
				for (j = 0; j < SIZEOF_PTR; j++) {
					if (((*ramdisk->ib[iIndex].location[8]).ptr.location[j]) == 0) {
						if ((fbSingle = getFreeBlock()) < 0) {
							printk("write() Error : Could not find free block in ramdisk\n");
							return -1; 
						}					

						/* Assign free block to this index node*/
						ramdisk->ib[iIndex].location[9]->ptr.location[j] = &ramdisk->fb[fbSingle];
					}

					/* Single Indirect Block */
					if (i == 8) {
						newSize = size - position;
						if (newSize > MAX_FILE_SIZE) {
							size = MAX_FILE_SIZE;
						}

						memcpy((*(*ramdisk->ib[iIndex].location[9]).ptr.location[j]).reg.data, data + position, size);
						position += newSize; 
						if ((newSize = size - position) < SIZEOF_BLOCK) {
							return 0; 
						}
					}

					/* Double Indirect Block */
					if (i == 9) {
						for (k = 0; k < SIZEOF_PTR; k++) {
							if (((*(*ramdisk->ib[iIndex].location[9]).ptr.location[j]).ptr.location[k]) == 0) {
								if ((fbDouble = getFreeBlock()) < 0) {
									printk("write() Error : Could not find a free block in ramdisk\n");
									return -1;
								}

								/* Assign this free block to this double redirect pointer */
								ramdisk->ib[iIndex].location[9]->ptr.location[j]->ptr.location[k] = &ramdisk->fb[fbDouble];

								newSize = size - position; 
								if (newSize > MAX_FILE_SIZE) {
									size = MAX_FILE_SIZE;
								}
								memcpy((*(*(*ramdisk->ib[iIndex].location[9]).ptr.location[j]).ptr.location[k]).reg.data, data + position, size);
								position += newSize; 
								if ((newSize = size - position) < SIZEOF_BLOCK) {
									return 0;
								}
							}
						}
					}
				}
			}
	}
	return -1; 
}

int writeFile(short iIndex, int filePos, unsigned char *data, int dataSize) {
	int position, ret, newSize; 
	unsigned char *newData; 

	position = 0; 

	newData = (unsigned char *) kmalloc(dataSize, GFP_KERNEL);

	constructInode(iIndex, newData);

	newSize = filePos + dataSize; 

	/* Overflow */
	if (newSize > MAX_FILE_SIZE) {

		printk("Overflow case in writeFile()");

		memcpy(newData + filePos, data, (MAX_FILE_SIZE - filePos));
		if ((ret = write(iIndex, newData, MAX_FILE_SIZE)) < 0) {
			printk("writeFile() Error : Could not write file\n");
			return -1;
		}

		if ((ret = modifyParentInodePlus(iIndex, MAX_FILE_SIZE)) < 0) {
			printk(" writeFile() Error : Could not modify parent inode\n");
			return -1; 
		}

		fdTable[iIndex]->filePos = MAX_FILE_SIZE;
		return (MAX_FILE_SIZE - filePos);
	} 
	/* Data fits into block */
	else {

		printk("Fit case in writeFile()\n");

		memcpy(newData + filePos, data, dataSize); 

		if ((ret = write(iIndex, newData, newSize)) < 0) {
			printk("writeFile() Error : Could not write file\n");
			return -1;
		}
		if ((ret = modifyParentInodePlus(iIndex, newSize)) < 0) {
			printk(" writeFile() Error : Could not modify parent inode\n");
			return -1; 
		}

		fdTable[iIndex]->filePos = newSize; 
		return dataSize; 
	}
	return -1; 
}

int k_write(int fd, char* address, int size) {
	unsigned char *data; 
	int dataSize, position;
	int numBytes, totalBytes;  

	if (fdTable[fd] == NULL) {
		printk("k_write() Error : File does not exist or file descriptor is not valid\n");
		return -1;
	}

	if (strncmp(ramdisk->ib[fd].type, "reg", 3) != 0) {
		printk("k_write() Error : File is not a regular file\n");
		return -1;
	}

	position = 0; 
	numBytes = 0; 
	totalBytes = 0; 

	data = (unsigned char *) kmalloc(SIZEOF_DIRECT_PTR, GFP_KERNEL);

	while (position < size) {
		dataSize = size - position; 
		if (dataSize > SIZEOF_DIRECT_PTR) {
			dataSize = SIZEOF_DIRECT_PTR;
		}

		memcpy(data, address + position, dataSize);

		if ((numBytes = writeFile(fd, fdTable[fd]->filePos, data, dataSize)) < 0) {
			printk("k_write() Error : Could not compute number of bytes written to file\n");
			return -1;
		}
		position += numBytes; 	
		totalBytes += numBytes; 
	}
	return totalBytes; 
}

void cleanInfo(struct IoctlInfo *info) {
	info->size = 0; 
	info->pathname = 0;
	info = NULL;
	return;
}

void cleanParam(struct IOParameter *param) {
	param->fd = 0; 
	param->address = 0;
	param->numBytes = 0;
	param = NULL;
	return;
}

/* Ramdisk Entry Point */
static int ramdisk_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg) {
	int ret, fd;
	struct IoctlInfo info;
	struct IOParameter param; 

	printk("\n\n");	

	switch (cmd) {
		case RD_CREAT:
			printk("<1> Case: RD_CREAT()...\n");
			info.size = strlen_user((char *) arg);
			info.pathname = (char *) kmalloc(info.size, GFP_KERNEL);
			copy_from_user(info.pathname, (char *) arg, info.size);

			printk("<1> info->size : %u\n", info.size);
			printk("<1> info->pathname: %s\n", info.pathname);

			ret = k_creat(info.pathname);
			cleanInfo(&info);
			return ret; 
			break; 

		case RD_OPEN:
			printk("Case: RD_OPEN()...\n");

			info.size = strlen_user((char *) arg);
			info.pathname = (char *) kmalloc(info.size, GFP_KERNEL);

			copy_from_user(info.pathname, (char *) arg, info.size);

			printk("<1> info->size : %d\n", info.size);
			printk("<1> info->pathname: %s\n", info.pathname);

			ret = k_open(info.pathname);
			cleanInfo(&info);
			return ret;
			break;

		case RD_CLOSE: 
			printk("Case : RD_CLOSE()...\n");
			copy_from_user(&fd, (int *)arg, sizeof(int));

			printk("<1> fd : %d\n", fd);

			ret = k_close(fd);
			return ret; 
			break;

		case RD_WRITE:
			printk("<1> Case: RD_WRITE()...\n");
			copy_from_user(&param, (struct IOParameter *) arg, sizeof(struct IOParameter));

			printk("param->fd : %d\n", param.fd);
			printk("param->address : %x\n", param.address);
			printk("param->numBytes : %d\n", param.numBytes);

			ret = k_write(param.fd, param.address, param.numBytes);
			cleanParam(&param);
			return ret;
			break;

		case RD_UNLINK:
			printk("<1> Case: RD_UNLINK()...\n");
			info.size = strlen_user((char *) arg);
			info.pathname = (char *) kmalloc(info.size, GFP_KERNEL);
			copy_from_user(info.pathname, (char*) arg, info.size);

			printk("<1> info->size : %u\n", info.size);
			printk("<1> info->pathname: %s\n", info.pathname);

			ret = k_unlink(info.pathname);
			cleanInfo(&info);
			return ret; 
			break;
	}
}

int initializeRAMDISK(void) {
	/* Create the ramdisk skeleton */
	if (!(ramdisk = (struct Ramdisk *) vmalloc(sizeof(struct Ramdisk)))) {
		printk("initializeRAMDISK() Error: Could not vmalloc ramdisk\n");
		return 1; 
	}

	/* Initialize the ramdisk with 0 */
	memset(ramdisk, 0, sizeof(struct Ramdisk));

	/* Initialize the root at ramdisk[0] */
	setDirInode(0, 0);
	
	/* Initialize Index Node Array */
	ramdisk->sb.numFreeInodes = IB_PARTITION;  // Decrement for root
	ramdisk->sb.numFreeBlocks = FB_PARTITION; 
	return 0;
}



/* ioctl Entry Point */
static int __init initialiaze_routine(void) {
	int ret; 

	printk("<1> Loading Ramdisk File System Module...\n");
	/* Inject ramdisk_ioctl into kernel-level */
	proc_operations.ioctl = ramdisk_ioctl; 

	if (!(proc_entry = create_proc_entry("ramdisk", 0666, NULL))) {
		printk("<1> initializeIOCTL() Error: Could not create /proc entry\n");
		return 1; 
	}

	proc_entry->proc_fops = &proc_operations; 

	/* Initialize the entire Ramdisk File System */
	if ((ret = initializeRAMDISK()) != 0) {
		printk("initialize_routine() Error : Could not initialize RAMDISK\n");
		return 1;
	}

	printk("Ramdisk Initialization Complete...\n");

	return 0;
}

static void __exit cleanup_routine(void) {
	printk("<1> Dumping module\n");
	vfree(ramdisk);
	remove_proc_entry("ramdisk", NULL);
	return;
}
 
module_init(initialiaze_routine);
module_exit(cleanup_routine);

