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

int isEmpty(union Block *location) {
	int i;
	for (i = 0; i < BLKSZ; i++) {
		if (location->reg.data[i] != 0) {
			return 0;
		}
	}
	return 1;
}


void setRootInode(int iIndex, int size) {
	char dir[4] = "dir";
	memcpy(ramdisk->ib[iIndex].type, dir, 4);
	ramdisk->ib[iIndex].size = size;
	return;
}

int getInode(int index, char* targetFilename) {
	int i, j, k, l;
	struct DirEntry entry; 

	if (strncmp(ramdisk->ib[index].type, "dir", 3)) {
		printk("getinode() Error : Inode type is not a directory file\n");
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
				if (ramdisk->ib[index].location[i] != 0) {
					for (j = 0; j < NUMEPB; j++) {
						if (ramdisk->ib[index].location[i]->reg.data != 0) {
							entry = ramdisk->ib[index].location[i]->dir.entry[j];
							if (!strncmp(entry.filename, targetFilename, 14)) {
								return entry.inode;
							}
						}
					}
				}
			}
			continue;

			case RPTR: {
				if (ramdisk->ib[index].location[i] == 0) { continue; }
				for (j = 0; j < NODESZ; j++) {
					if (ramdisk->ib[index].location[i]->ptr.location[j] == 0) { continue; } 
					for (k = 0; k < NODESZ; k++) {
						if (ramdisk->ib[index].location[i]->ptr.location[j]->reg.data != 0) {
							entry = ramdisk->ib[index].location[i]->ptr.location[j]->dir.entry[k];
							if (!strncmp(entry.filename, targetFilename, 14)) {
								return entry.inode;
							}
						}
					}
				}
			}
			continue;

			case RRPTR: {
				if (ramdisk->ib[index].location[i] == 0) { continue; }
				for (j = 0; j < NODESZ; j++) {
					if (ramdisk->ib[index].location[i]->ptr.location[j] == 0) { continue; }
					for (k = 0; k < NODESZ; k++) {
						if (ramdisk->ib[index].location[i]->ptr.location[j]->ptr.location[k] == 0) { continue; }
						for (l = 0; l < NUMEPB; l++) {
							if (ramdisk->ib[index].location[i]->ptr.location[j]->ptr.location[k]->reg.data != 0) {
								entry = ramdisk->ib[index].location[i]->ptr.location[j]->ptr.location[k]->dir.entry[l];
								if (!strncmp(entry.filename, targetFilename, 14)) {
									return entry.inode;
								}
							}
						} 
					}
				}
			}
			continue;
		}
	}
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


void setDirEntryFree(short fIndex, int eIndex, char* filename, short newInode) {
	memcpy(ramdisk->fb[fIndex].dir.entry[eIndex].filename, filename, 14);
	ramdisk->fb[fIndex].dir.entry[eIndex].inode = newInode;
	return;
}

void setDirEntryAlloc(short iIndex, int lIndex, int eIndex, char* filename, short newInode) {
	memcpy((*ramdisk->ib[iIndex].location[lIndex]).dir.entry[eIndex].filename, filename, 14);
	(*ramdisk->ib[iIndex].location[lIndex]).dir.entry[eIndex].inode = newInode;
	return;
}

void setDirEntryRPTR(int index, int i, int j, char *filename, short newInode) {
	memcpy((*(*ramdisk->ib[index].location[8]).ptr.location[i]).dir.entry[j].filename, filename, 14);
	(*(*ramdisk->ib[index].location[8]).ptr.location[i]).dir.entry[j].inode = newInode;
	return;
}

void setDirEntryRRPTR(int index, int i, int j, int k, char *filename, short newInode) {
	memcpy((*(*(*ramdisk->ib[index].location[9]).ptr.location[i]).ptr.location[j]).dir.entry[k].filename, filename, 14);
	(*(*(*ramdisk->ib[index].location[9]).ptr.location[i]).ptr.location[j]).dir.entry[k].inode = newInode;
	return;
}

void assignFreeBlockDPTR(int index, int i, int fbIndex) {
	ramdisk->ib[index].location[i] = &ramdisk->fb[fbIndex];
	return;
}

void assignFreeBlockRPTR(int index, int pIndex, int fbIndex) {
	ramdisk->ib[index].location[8]->ptr.location[pIndex] = &ramdisk->fb[fbIndex];
	return;
}

void assignFreeBlockRRPTR1(int index, int pIndex, int fbIndex) {
	ramdisk->ib[index].location[9]->ptr.location[pIndex] = &ramdisk->fb[fbIndex];
	return;
}

void assignFreeBlockRRPTR2(int index, int p1, int p2, int fbIndex) {
	ramdisk->ib[index].location[9]->ptr.location[p1]->ptr.location[p2] = &ramdisk->fb[fbIndex];
	return;
}

int assignInode(short index, short newInode, char *filename, int dirFlag) {
	int i, j, k, l;
	int fbDirect, fbSingle, fbDouble1, fbDouble2;
	short inode; 

	for (i = 0; i < NUMPTRS; i++) {
		if (ramdisk->ib[index].location[i] == 0) {
			if ((fbDirect = getFreeBlock()) < 0) {
				printk("assignInode() Error : Could not find free block in ramdisk\n");
				return -1;
			}
			assignFreeBlockDPTR(index, i, fbDirect);

			switch (i) {
				case DPTR1: 
				case DPTR2: 
				case DPTR3: 
				case DPTR4: 
				case DPTR5: 
				case DPTR6: 
				case DPTR7: 
				case DPTR8: {
					setDirEntryFree(fbDirect, 0, filename, newInode);
					return 0; 
				}
				continue;

				case RPTR: {
					if ((fbSingle = getFreeBlock()) < 0) {
						printk("assignInode() Error : Could not find free block in ramdisk\n");
						return -1;
					}
					assignFreeBlockRPTR(index, 0, fbSingle);
					setDirEntryFree(fbSingle, 0, filename, newInode);
					return 0;
				}
				continue;

				case RRPTR: {
					if ((fbDouble1 = getFreeBlock()) < 0) {
						printk("assignInode() Error : Could not find free block in ramdisk\n");
						return -1;
					}
					assignFreeBlockRRPTR1(index, 0, fbDouble1);

					if ((fbDouble2 = getFreeBlock()) < 0) {
						printk("assignInode() Error : Could not find free block in ramdisk\n");
						return -1;
					}
					assignFreeBlockRRPTR2(index, 0, 0, fbDouble2);
					setDirEntryFree(fbDouble2, 0, filename, newInode);
					return 0;
				}
			}
		} else {
			switch (i) {
				case DPTR1: 
				case DPTR2: 
				case DPTR3: 
				case DPTR4: 
				case DPTR5: 
				case DPTR6: 
				case DPTR7: 
				case DPTR8: {
					for (j = 0; j < NUMEPB; j++) {
						inode = (*ramdisk->ib[index].location[i]).dir.entry[j].inode; 
						if (inode == 0) {
							setDirEntryAlloc(index, i, j, filename, newInode);
							return 0;
						}
					}
					continue;
				}

				case RPTR: {
					for (j = 0; j < NODESZ; j++) {
						if ((*ramdisk->ib[index].location[8]).ptr.location[j] == 0)  {
							if ((fbSingle = getFreeBlock()) < 0) {
								printk("assignInode() Error : Could not find free block in ramdisk\n");
								return -1;
							}	
							assignFreeBlockRPTR(index, j, fbSingle);
							setDirEntryFree(fbSingle, 0, filename, newInode);
							return 0;
						} else {
							for (k = 0; k < NUMEPB; k++) {
								inode = (*(*ramdisk->ib[index].location[8]).ptr.location[j]).dir.entry[k].inode;
								if (inode == 0) {
									setDirEntryRPTR(index, j, k, filename, newInode);
									return 0;
								}
							}
						}
					}
					continue;
				}

				case RRPTR: {
					for (j = 0; j < NODESZ; j++) {
						if ((*ramdisk->ib[index].location[9]).ptr.location[j] == 0) {
							if ((fbDouble1 = getFreeBlock()) < 0) {
								printk("assignInode() Error : Could not find free block in ramdisk\n");
								return -1;
							}
							assignFreeBlockRRPTR1(index, j, fbDouble1);

							if ((fbDouble2 = getFreeBlock()) < 0) {
								printk("assignInode() Error : Could not find free block in ramdisk\n");
								return -1;
							}
							assignFreeBlockRRPTR2(index, j, 0, fbDouble2);
							setDirEntryRRPTR(index, j, 0, 0, filename, newInode);
							return 0;
						} else {
							for (l = 0; l < NUMEPB; l++) {
								inode = (*(*(*ramdisk->ib[index].location[9]).ptr.location[j]).ptr.location[k]).dir.entry[l].inode;
								if (inode == 0) {
									setDirEntryRRPTR(index, j, k, l, filename, newInode);
									return 0;
								}
							}
						}
					}
					continue;
				}
			}
		}
	}
	return -1;
}


int searchParentInodes(short index, short targetInode, int *pIndex, short* parentInodes) {
	short entryInode;
	int i, j, k, l;

	if (strncmp(ramdisk->ib[index].type, "reg", 3) == 0) {
		printk("searchParentInodes() Error : File is not a directory file\n");
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
					entryInode = (*ramdisk->ib[index].location[i]).dir.entry[j].inode;
					if (entryInode == targetInode) { return 1; }
					parentInodes[*pIndex] = entryInode;
					*pIndex += 1;
					if (searchParentInodes(entryInode, targetInode, pIndex, parentInodes)) { return 1; }
					*pIndex -= 1;
					parentInodes[*pIndex] = 0;
				}
				continue;
			}
			
			case RPTR: {
				if (ramdisk->ib[index].location[i] == 0) { continue; }
				for (j = 0; j < NODESZ; j++) {
					if ((*ramdisk->ib[index].location[i]).ptr.location[j] == 0) { continue; }	

					for (k = 0; k < NUMEPB; k++) {
						entryInode = (*(*ramdisk->ib[index].location[i]).ptr.location[j]).dir.entry[k].inode;
						if (entryInode == targetInode) { return 1; }
						parentInodes[*pIndex] = entryInode;
						*pIndex += 1;
						if (searchParentInodes(entryInode, targetInode, pIndex, parentInodes)) { return 1; }
						*pIndex = *pIndex - 1;
						parentInodes[*pIndex] = 0;
					}
				}
				continue;
			}

			case RRPTR: {
				if (ramdisk->ib[index].location[i] == 0) { continue; }
				for (j = 0; j < NODESZ; j++) {
					if ((*ramdisk->ib[index].location[i]).ptr.location[j] == 0) { continue; }	

					for (k = 0; k < NODESZ; k++) {
						if ((*(*ramdisk->ib[index].location[i]).ptr.location[j]).ptr.location[k] == 0) { continue;}

						for (l = 0; l < NUMEPB; l++) {
							entryInode = (*(*ramdisk->ib[index].location[i]).ptr.location[j]).ptr.location[k]->dir.entry[l].inode;
							if (entryInode == targetInode) { return 1; }
							parentInodes[*pIndex] = entryInode;
							*pIndex += 1;
							if (searchParentInodes(entryInode, targetInode, pIndex, parentInodes)) { return 1; }
							*pIndex = *pIndex - 1;
							parentInodes[*pIndex] = 0;
						}
					}
				}
				continue;
			}
		}
	}
	return 0; 
}

int adjustPosition(short index, unsigned char* data) {

	int i, j, k, possibleSize; 

	possibleSize = 0; 
	for (i = 0; i < NUMPTRS; i++) {
		if (ramdisk->ib[index].location[i] == 0) {
			return possibleSize; 
		}

		switch (i) {
			case DPTR1: 
			case DPTR2: 
			case DPTR3: 
			case DPTR4: 
			case DPTR5: 
			case DPTR6: 
			case DPTR7: 
			case DPTR8: {
				data = data + possibleSize; 
				memcpy(data, ramdisk->ib[index].location[i], BLKSZ);
				possibleSize += BLKSZ;
				continue;
			}

			case RPTR: {
				for (j = 0; j < NODESZ; j++) {
					if ((*ramdisk->ib[8].location[i]).ptr.location[j] == 0) {
						return possibleSize; 
					}
					data = data + possibleSize; 
					memcpy(data, ramdisk->ib[index].location[i], BLKSZ);
					possibleSize += BLKSZ;
				}
				continue;
			}

			case RRPTR: {
				for (j = 0; j < NODESZ; j++) {
					if ((*ramdisk->ib[index].location[9]).ptr.location[j] == 0) {
						return possibleSize;
					}

					for (k = 0; k < NODESZ; k++) {
						if ((*(*ramdisk->ib[index].location[9]).ptr.location[j]).ptr.location[k] == 0) {
							return possibleSize; 
						}
						data = data + possibleSize; 
						memcpy(data, ramdisk->ib[index].location[i], BLKSZ);
						possibleSize += BLKSZ;
					}
				}
				continue;
			}
		}
	}
	return possibleSize;
}

EXPORT_SYMBOL(isEmpty);
EXPORT_SYMBOL(setDirEntryFree);
EXPORT_SYMBOL(setDirEntryAlloc);
EXPORT_SYMBOL(assignFreeBlockDPTR);
EXPORT_SYMBOL(assignFreeBlockRPTR);
EXPORT_SYMBOL(assignFreeBlockRRPTR1);
EXPORT_SYMBOL(assignFreeBlockRRPTR2);
EXPORT_SYMBOL(setDirEntryRPTR);
EXPORT_SYMBOL(setRootInode);
EXPORT_SYMBOL(getInode);
EXPORT_SYMBOL(fileExists);
EXPORT_SYMBOL(getFreeInode);
EXPORT_SYMBOL(getFreeBlock);
EXPORT_SYMBOL(assignInode);
EXPORT_SYMBOL(searchParentInodes);
EXPORT_SYMBOL(adjustPosition);