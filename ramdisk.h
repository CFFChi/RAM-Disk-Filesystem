#define RDSKSZ 	2097152 // 2MB
#define BLKSZ	256
#define BTMPSZ 	(4 * BLKSZ) // Size of Bitmap Block
#define NODESZ 	64			// Size of Pointer

#define NUMEPB 		16  // Max entries per directory block
#define NUMPTRS 	10  // Number of pointers in block
#define NUMDPTRS	8   // Number of direct pointers

#define FNAMESZ 14 					// Max file name length
#define DBLKSZ 	(NUMDPTRS * BLKSZ)  // Size of Direct Pointer

#define IBARR 	1024  						   // Number nodes in Inode array
#define FBARR 	(RDSKSZ/BLKSZ - (1 + 256 + 4)) // Number of blocks in File array
#define IBARRSZ (IBARR * NODESZ)			   // Size of Index Node Partition
#define FBARRSZ (FBARR * BLKSZ)				   // Size of Free Block Partition

#define MAXFSZ	((NUMDPTRS*BLKSZ) + (NODESZ*BLKSZ) + (NODESZ*NODESZ*BLKSZ))  // Max file size
#define MAXFCT	1024 														 // Max file count

#define DPTR1 0
#define DPTR2 1
#define DPTR3 2
#define DPTR4 3
#define DPTR5 4
#define DPTR6 5
#define DPTR7 6
#define DPTR8 7
#define RPTR  8
#define RRPTR 9

/* ioctl Macro Definition */
#define MAGIC_NUMBER 222
#define RD_CREAT     _IOR(MAGIC_NUMBER, 0, char *)
#define RD_MKDIR	 _IOR(MAGIC_NUMBER, 1, char *)
#define RD_OPEN	     _IOR(MAGIC_NUMBER, 2, char *)
#define RD_CLOSE	 _IOR(MAGIC_NUMBER, 3, int)
#define RD_READ		_IOWR(MAGIC_NUMBER, 4, struct IOParameter)
#define RD_WRITE	_IOWR(MAGIC_NUMBER, 5, struct IOParameter)
#define RD_LSEEK	 _IOR(MAGIC_NUMBER, 6, struct IOParameter)
#define RD_UNLINK 	 _IOR(MAGIC_NUMBER, 7, char *)
#define RD_READDIR 	_IOWR(MAGIC_NUMBER, 8, struct IOParameter)

/* Bitmap Operation */
#define SET_BIT_MASK1(x) (0x01 << (7-x))		// Sets bit x to 1 and remaining bits to 0
#define SET_BIT_MASK0(x) (~SET_BIT_MASK1(x))	// Sets bit x to 0 and remaining bits to 1

/* FileDescriptor - Keeps track of opened files on a per-process basis
 *		rPosition : Current read position of a files
 *		wPosition : Current write position of a file
 *		inodePtr : Pointer to the index node of a file
 */
 struct FileDescriptor {
 	int filePos;
 	struct Inode *inodePtr;
 };

/* ioctlInfo - Container to store parameters from user-space
 * 		fd : File Descriptor
 *		size : Size of the file
 *		pathname : Pathname of the file
 *		address : Address of the file
 *		param : Arguments
 */
struct IoctlInfo {
	char *pathname;
	unsigned int size;
};

/*
 * IOParam
 *		fd : File descriptor
 *		address : Address of Data
 *		numBytees : Size of file
 */
struct IOParameter {
	int fd;
	char *address;
	int numBytes;
};

/* Superblock (Size: 256 bytes)
 * 		numFreeInodes : Number of free index nodes in ramdisk
 * 		numFreeBlocks : Number of free blocks in ramdisk
 * 		emptiness : Empty byte sequence to align 256 bytes block size
 */
struct SuperBlock {
	int numFreeInodes;
	int numFreeBlocks;
	char emptiness[BLKSZ - 8];
};

/* Inode (Size: 64 bytes)
 * 		type : Type of the file (regular="reg" or directory="dir")
 * 		size : Size of the file in bytes
 * 		*blocks : Pointer to the file
 *			Pointer to File
 *			Pointer to Directory
 *			Pointer to Pointer to Block for Larger Files
 *		emptiness : Empty byte sequence to align 256 bytes block size
 */
struct Inode {
	char type[4];
	int size;
	union Block *location[NUMPTRS];
	char emptiness[NODESZ - 48];
};

/* Regular File Block
 * 		data : Data content of the file
 */
struct RegBlock {
	char data[BLKSZ];
};

/* Directory Entry
 *		filename : Name of the file (Maximum 14 characters)
 *		inodeNumber : Number for indexing into Index Node Array
 */
struct DirEntry {
	char filename[FNAMESZ];
	short inode;
};

/* Directory File Block
 *		entry : Array of directory entries containing filename and inode information
 *    size of DFB is sizeof(dirEntry) (16) * sizeof(NUMEPB) (16) = 256, or BLKSZ
 *
 */
struct DirBlock {
	struct DirEntry entry[NUMEPB];
};

/* Pointer to File Block
 *		*blocks : Pointer to the file
 *    size is ptr size (4B) * NODESZ (64) = 256, or BLKSZ
 */
struct PtrBlock {
	union Block *location[NODESZ];
};

/* Bitmap
 * 		 byte : Keep track of free and allocated blocks in the rest of the partition
 */
struct Bitmap {
	unsigned char byte[BLKSZ * BTMPSZ];
};

/*  Block (Max Size : 256 bytes)
 *		regBlock : Regular file
 *		dirBlock : Directory file
 *		ptrBlock : Pointer to Block file
 */
union Block {
	struct RegBlock reg;
	struct DirBlock dir;
	struct PtrBlock ptr;
};

/*
 * RamDisk (Size: 2MB) contains
 *		SuperBlock 	(Size: 1 block)
 *		Inode 	(Size: 256 Inodes)
 *		Bitmap (Size: 4 blocks)
 *		FreeBlock	(Size: (2MB - 256(1+256+4)))
 */
struct Ramdisk {
	struct SuperBlock sb;
	struct Inode ib[IBARR];
	struct Bitmap	bb;
	union Block fb[FBARR];
};

int rd_creat(char* pathname);
int rd_mkdir(char* pathname);
int rd_open(char* pathname);
int rd_close(int fd);
int rd_read(int fd, char* address, int num_bytes);
int rd_write(int fd, char* address, int num_bytes);
int rd_lseek(int fd, int offset);
int rd_unlink(char* pathname);
int rd_readdir(int fd, char* address);

/* ioctl Functions */
int initializeRAMDISK(void);
int k_creat(char* pathname);
int k_mkdir(char* pathname);
int k_open(char* pathname);
int k_close(int fd);
int k_read(int fd, char* address, int numBytes);
int k_write(int fd, char* address, int numBytes);
int k_unlink(char* pathname);
int k_lseek(int fd, int offset);
int k_readdir(int fd, char* address);

/* Helper Functions */
void setDirEntryFree(short fIndex, int eIndex, char* filename, short newInode);
void setDirEntryAlloc(short fIndex, int lIndex, int eIndex, char* filename, short newInode);
void setDirEntryRPTR(int index, int i, int j, char *filename, short newInode);
void setDirEntryRRPTR(int index, int i, int j, int k, char *filename, short newInode);
void assignFreeBlockDPTR(int index, int i, int fbIndex);
void assignFreeBlockRPTR(int index, int pIndex, int fbIndex);
void assignFreeBlockRRPTR1(int index, int pIndex, int fbIndex);
void assignFreeBlockRRPTR2(int index, int p1, int p2, int fbIndex);
void setRootInode(int iIndex, int size);
int isEmpty(union Block *location);
int getFreeInode(void);
int getFreeBlock(void);
int getInode(int index, char* targetFilename);
int fileExists(char *pathName, char* lastPath, short* parentInode);
int assignInode(short index, short newInode, char *filename, int dirFlag);
int searchParentInodes(short index, short targetInode, int *pIndex, short* parentInodes);
int adjustPosition(short index, unsigned char* data);
