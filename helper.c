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

/* Helper Functions for Kernel level IOCTL Functions*/
int isEmpty(union Block *location) {
	// printk("check empty\n");
	int i = 0;
	while (i < BLKSZ) {
		if (location->reg.data[i] != 0) {
			return 0;
		}
		i++;
	}
	return 1;
}

void setDirEntry(short fIndex, int eIndex, char* filename, short newInode) {
	printk("fIndex: %d\n", fIndex);
	printk("eIndex: %d\n", eIndex);
	memcpy(ramdisk->fb[fIndex].dir.entry[eIndex].filename, filename, 14);
	ramdisk->fb[fIndex].dir.entry[eIndex].inode = newInode;
	return;
}

void setDirEntryLocation(short iIndex, int bIndex, int eIndex, char*filename, short newInode) {
	printk("set\n");
	memcpy((*ramdisk->ib[iIndex].location[bIndex]).dir.entry[eIndex].filename, filename, 14);
	(*ramdisk->ib[iIndex].location[bIndex]).dir.entry[eIndex].inode = newInode;
	printk("setDir filename: %s\n", (*ramdisk->ib[iIndex].location[bIndex]).dir.entry[eIndex].filename);
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

void setDirEntryDoublePtrLocation(short iIndex, int ptrIndex1, int ptrIndex2, int eIndex, char* filename, short newInode) {
	memcpy(((*(*(*ramdisk->ib[iIndex].location[9]).ptr.location[ptrIndex1]).ptr.location[ptrIndex2]).dir.entry[eIndex].filename), filename, 14);
	((*(*(*ramdisk->ib[iIndex].location[9]).ptr.location[ptrIndex1]).ptr.location[ptrIndex2]).dir.entry[eIndex].inode) = newInode;
	return;
}

void setRootInode(int iIndex, int size) {
	char dir[4] = "dir";
	memcpy(ramdisk->ib[iIndex].type, dir, 4);
	ramdisk->ib[iIndex].size = size;
	return;
}

int getInode(int index, char* pathname) {
	int i, j, k, l;
	char* file;
	union Block *location, *singlePtr, *doublePtr;
	struct Inode *inode;

	printk("index1: %d\n", index);

	inode = &(ramdisk->ib[index]);

	// printk("blah: %s\n", (*ramdisk->ib[0].location[0]).dir.entry[0].filename);
	// printk("filetype: %s\n", inode->type);

	if (strncmp(inode->type, "dir", 3) != 0) {
		printk("getInode() Error : inode->type is not a directory file\n");
		return -1;
	}

	for (i = 0; i < NUMPTRS; i++) {


		location = inode->location[i];
		/* Loop through DIRECT Block pointers (location[0 ~ 7]) */
		if (0 <= i && i <= 7) {
			if (location != 0) {
				file = location->reg.data;
				for (j = 0; j < NUMEPB; j++) {
					// printk("i : %d, j: %d\n", i, j);
					if (file != 0) {
						// printk("Comparing1 %s AND %s\n", location->dir.entry[j].filename, pathname);
						if (strncmp(location->dir.entry[j].filename, pathname, 14) == 0) {
							return location->dir.entry[j].inode;
						}
					}
				}
			}
		}
		/* Loop through Single Indirect & Double Indirect Block pointers (location[8 ~ 9]) */
		else {

			// printk("i: %d\n", i);

			if (location == 0) { continue; }
			// printk("here\n");
			for (j = 0; j < NODESZ; j++) {
				singlePtr = location->ptr.location[j];
				if (singlePtr == 0) { continue; }

				// printk("i: %d\n", i);
				/* Single-Indirect Block Pointer */
				if (i == 8) {
					for (k = 0; k < NUMEPB; k++) {
						if (singlePtr->reg.data != 0) {
							// printk("Comparison2:  %s AND %s\n", singlePtr->dir.entry[k].filename, pathname);
							if (strncmp(singlePtr->dir.entry[k].filename, pathname, 14) == 0) {
								return singlePtr->dir.entry[k].inode;
							}
						}
					}
				}
				/* Double-Indirect Block Pointer */
				if (i == 9) {
					for (k = 0; k < NODESZ; k++) {
						doublePtr = singlePtr->ptr.location[k];
						if (doublePtr == 0) { continue; }

						printk("i: %d, j: %d, k: %d\n", i,j, k);
						for (l = 0; l < NUMEPB; l++) {
							if (doublePtr->reg.data != 0) {
								// printk("Comparison3:  %s AND %s\n", doublePtr->dir.entry[l].filename, pathname);
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

	size = strlen(pathname);

	if (pathname[0] != '/' || size < 2) {
		printk("fileExist() Error: Invalid pathname\n");
		return -1;
	}

	index = 0;
	currentIndex = 0;
	path = (char *) kmalloc(14, GFP_KERNEL);
	pathname++;

	// printk("parentInode before: %d\n", *parentInode);


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
		strncpy(path, pathname, size);
		printk("pathname: %s\n", path);
		if ((currentIndex = getInode(index, path)) < 0) {
			printk("fileExists() Error : Could not get inode from pathname\n");
			return -1;
		}
		strncpy(lastPath, path, size);
		*parentInode = index;
		printk("parentInode after: %d\n", *parentInode);
		if (currentIndex > 0) {
			// printk("currentIndex : %d\n", currentIndex);
			return currentIndex;
		}
		/* File does not exist on system */
		printk("File %s is not found\n", path);
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
	for (i = 0; i < MAXFCT; i++) {
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

	for (i = 0; i < FBARR; i++) {
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

void assignInodeDPTR(short pIndex, int iIndex, int fbIndex) {
	ramdisk->ib[pIndex].location[iIndex] = &ramdisk->fb[fbIndex];
	return;
}

void assignInodeRPTR(short pIndex, int ptrIndex, int fbIndex) {
	ramdisk->ib[pIndex].location[8]->ptr.location[ptrIndex] = &ramdisk->fb[fbIndex];
	return;
}

void assignInodeRRPTR1(short pIndex, int ptrIndex, int fbIndex) {
	ramdisk->ib[pIndex].location[9]->ptr.location[ptrIndex] = &ramdisk->fb[fbIndex];
	return;	
}

void assignInodeRRPTR2(short pIndex, int ptrIndex1, int ptrIndex2, int fbIndex) {
	ramdisk->ib[pIndex].location[9]->ptr.location[ptrIndex1]->ptr.location[ptrIndex2] = &ramdisk->fb[fbIndex];
	return;	
}

int assignInode(short pIndex, short newInode, char *filename, int dirFlag) {
	short entryInode;
	int i, j, k, l;
	int fbDirect, fbSingle, fbDouble1, fbDouble2;

	for (i = 0; i < NODESZ; i++) {
		if (ramdisk->ib[pIndex].location[i] == 0) {
			if ((fbDirect = getFreeBlock()) < 0) {
				printk("assignInode() Error : Could not find free block in ramdisk (fbDirect)\n");
				return -1;
			}
			ramdisk->ib[pIndex].location[i] = &ramdisk->fb[fbDirect];
			assignInodeDPTR(pIndex, i, fbDirect);
			printk("%x\n", ramdisk->ib[pIndex].location[i]);
			
			switch (i) {
				case DPTR: {	
					// printk("DPTR FREE\n");
					setDirEntry(fbDirect, 0, filename, newInode);
					if (!dirFlag) {
						setDirEntryLocation(pIndex, fbDirect, 0, filename, newInode);
					}
					return 0; 
				}
				
				case RPTR: {
					// printk("RPTR FREE\n");
					if ((fbSingle = getFreeBlock()) < 0) {
						printk("assignInode() Error : Could not find free block in ramdisk (fbSingle)\n");
						return -1; 
					}
					assignInodeRPTR(pIndex, 0, fbSingle);
					setDirEntry(fbSingle, 0, filename, newInode);
					return 0;
				}

				case RRPTR: {
					// printk("RRPTR FREE\n");
					if ((fbDouble1 = getFreeBlock()) < 0) {
						printk("assignInode() Error : Could not find free block in ramdisk (fbDouble1)\n");
						return -1;
					}
					assignInodeRRPTR1(pIndex, 0, fbDouble1);
					setDirEntry(fbDouble1, 0, filename, newInode);
					return 1;

					if ((fbDouble2 = getFreeBlock()) < 0) {
						printk("assignInode() Error : Could not find free block in ramdisk (fbDouble2)\n");
						return -1;
					}
					assignInodeRRPTR2(pIndex, 0, 0, fbDouble2);
					setDirEntry(fbDouble2, 0, filename, newInode);
					return 0; 
				}
			}
		} else {
			switch (i) {
				case DPTR: {
					// printk("DPTR ALLOC\n");
					for (j = 0; j < NUMEPB; j++) {
						entryInode = (*ramdisk->ib[pIndex].location[i]).dir.entry[j].inode;
						if (entryInode == 0) {
							setDirEntryLocation(pIndex, i, j, filename, newInode);
							return 0;
						}
					}
				}

				case RPTR: {
					// printk("RPTR ALLOC\n");
					for (j = 0; j < NODESZ; j++) {
						if ((*ramdisk->ib[pIndex].location[8]).ptr.location[j] == 0) {
							if ((fbSingle = getFreeBlock()) < 0) {
								printk("assignInode() Error : Could not find free block in ramdisk (fbSingle)\n");
								return -1;
							} 
							assignInodeRPTR(pIndex, j, fbSingle);
							setDirEntry(fbSingle, 0, filename, newInode);
							return 0;
						} else {
							for (k = 0; k < NUMEPB; k++) {
								entryInode = (*(*ramdisk->ib[pIndex].location[8]).ptr.location[j]).dir.entry[k].inode;
								if (entryInode == 0) {
									setDirEntrySinglePtrLocationEntry(pIndex, j, k, filename, newInode);
									return 0;
								}
							}
						}
					}
				}
				case RRPTR: {
					// printk("RRPTR ALLOC\n");
					for (j = 0; j < NODESZ; j++) {
						if ((*ramdisk->ib[pIndex].location[9]).ptr.location[j] == 0) {
							if ((fbDouble1 = getFreeBlock()) < 0) {
								printk("assignInode() Error : Could not find free block in ramdisk (fbDouble1)\n");
								return -1;
							}
							assignInodeRRPTR1(pIndex, j, fbDouble1);

							if ((fbDouble2 = getFreeBlock()) < 0) {
								printk("assignInode() Error : Could not find free block in ramdisk (fbDouble2)\n");
								return -1;
							}
							assignInodeRRPTR2(pIndex, j, 0, fbDouble2);
							setDirEntryDoublePtrLocation(pIndex, j, 0, 0, filename, newInode);
							return 0;
						} else {
							for (k = 0; k < NODESZ; k++) {
								if ((*(*ramdisk->ib[pIndex].location[9]).ptr.location[j]).ptr.location[k] == 0) {
									if ((fbDouble1 = getFreeBlock()) < 0) {
										printk("assignInode() Error : Could not find free block in ramdisk (fbDouble1)\n");
										return -1;
									}
									assignInodeRRPTR2(pIndex, j, k, fbDouble1);
									setDirEntryDoublePtrLocation(pIndex, j, k, 0, filename, newInode);
									return 0;
								} else {
									for (l = 0; l < NUMEPB; l++) {
										entryInode = (*(*(*ramdisk->ib[pIndex].location[9]).ptr.location[j]).ptr.location[k]).dir.entry[k].inode; 
										if (entryInode == 0) {
											setDirEntryDoublePtrLocation(pIndex, j, k, l, filename, newInode);
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
	return -1;
}

int searchParentInodes(short iIndex, short targetInode, int *pIndex, short* parentInodes) {
	short entryInode;
	int i, j, k, l;

	if (strncmp(ramdisk->ib[iIndex].type, "reg", 3) == 0) {
		printk("searchParentInodes() Error : File is not a directory file\n");
		return -1;
	}

	for (i = 0; i < NUMPTRS; i++) {
		switch (i) {
			case DPTR: {
				if (ramdisk->ib[iIndex].location[i] == 0) { continue; }

				for (j = 0; j < NUMEPB; j++) {
					entryInode = (*ramdisk->ib[iIndex].location[i]).dir.entry[j].inode;
					if (entryInode == targetInode) { return 1; }
					parentInodes[*pIndex] = entryInode;
					*pIndex += 1;
					if (searchParentInodes(entryInode, targetInode, pIndex, parentInodes)) { return 1; }
					*pIndex -= 1;
					parentInodes[*pIndex] = 0;
					continue;
				}
			}
			
			case RPTR: {
				if (ramdisk->ib[iIndex].location[i] == 0) { continue; }
				for (j = 0; j < NODESZ; j++) {
					if ((*ramdisk->ib[iIndex].location[i]).ptr.location[j] == 0) { continue; }	

					for (k = 0; k < NUMEPB; k++) {
						entryInode = (*(*ramdisk->ib[iIndex].location[i]).ptr.location[j]).dir.entry[k].inode;
						if (entryInode == targetInode) { return 1; }
						parentInodes[*pIndex] = entryInode;
						*pIndex += 1;
						if (searchParentInodes(entryInode, targetInode, pIndex, parentInodes)) { return 1; }
						*pIndex = *pIndex - 1;
						parentInodes[*pIndex] = 0;
						continue;
					}
				}
			}

			case RRPTR: {
				if (ramdisk->ib[iIndex].location[i] == 0) { continue; }
				for (j = 0; j < NODESZ; j++) {
					if ((*ramdisk->ib[iIndex].location[i]).ptr.location[j] == 0) { continue; }	

					for (k = 0; k < NODESZ; k++) {
						if ((*(*ramdisk->ib[iIndex].location[i]).ptr.location[j]).ptr.location[k] == 0) { continue;}

						for (l = 0; l < NUMEPB; l++) {
							entryInode = (*(*ramdisk->ib[iIndex].location[i]).ptr.location[j]).ptr.location[k]->dir.entry[l].inode;
							if (entryInode == targetInode) { return 1; }
							parentInodes[*pIndex] = entryInode;
							*pIndex += 1;
							if (searchParentInodes(entryInode, targetInode, pIndex, parentInodes)) { return 1; }
							*pIndex = *pIndex - 1;
							parentInodes[*pIndex] = 0;
							continue;
						}
					}
				}
			}
		}
	}
	return 0; 
}

int adjustPosition(short iIndex, unsigned char* data) {
	int i, j, k, size;
	union Block *location, *singlePtr, *doublePtr1, *doublePtr2;

	size = 0;
	for (i = 0; i < NUMPTRS; i++) {
		location = ramdisk->ib[iIndex].location[i];
		if (location == 0) { return size; }

		if (0 <= i && i <= 7) {
			data = data + size;
			memcpy(data, location, BLKSZ);
			size += BLKSZ;

		} else {
			for (j = 0; j < NODESZ; j++) {
				/* Single Redirection*/
				if (i == 8) {
					singlePtr = (*ramdisk->ib[8].location[i]).ptr.location[j];
					if (singlePtr == 0) { return size; }
					data = data + size;
					memcpy(data, singlePtr, BLKSZ);
					size += BLKSZ;
				}

				/* Double Redirection */
				else {
					doublePtr1 = (*ramdisk->ib[9].location[i]).ptr.location[j];
					if (doublePtr1 == 0) { return size; }

					for (k = 0; k < NODESZ; k++) {
						doublePtr2 = (*(*ramdisk->ib[9].location[i]).ptr.location[j]).ptr.location[k];
						if (doublePtr2 == 0) { return size; }

						data = data + size;
						memcpy(data, doublePtr2, BLKSZ);
						size += BLKSZ;
					}
				}
			}
		}
	}
	return size;
}


EXPORT_SYMBOL(isEmpty);
EXPORT_SYMBOL(setDirEntry);
EXPORT_SYMBOL(setDirEntryLocation);
EXPORT_SYMBOL(setSinglePtrLocation);
EXPORT_SYMBOL(setDirEntrySinglePtrLocationEntry);
EXPORT_SYMBOL(setDirEntryDoublePtrLocation);
EXPORT_SYMBOL(assignInodeDPTR);
EXPORT_SYMBOL(assignInodeRPTR);
EXPORT_SYMBOL(assignInodeRRPTR1);
EXPORT_SYMBOL(assignInodeRRPTR2);
EXPORT_SYMBOL(setRootInode);
EXPORT_SYMBOL(getInode);
EXPORT_SYMBOL(fileExists);
EXPORT_SYMBOL(getFreeInode);
EXPORT_SYMBOL(getFreeBlock);
EXPORT_SYMBOL(assignInode);
EXPORT_SYMBOL(searchParentInodes);
EXPORT_SYMBOL(adjustPosition);
