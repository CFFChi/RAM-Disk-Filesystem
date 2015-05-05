#define SIZEOF_RAMDISK 	(2 * 1024 * 1024)
#define SIZEOF_BLOCK	256 

#define SIZEOF_SB	1
#define SIZEOF_IB	256
#define SIZEOF_BB	4
#define SIZEOF_FB	(SIZEOF_RAMDISK - SIZEOF_BLOCK*(SIZEOF_SB + SIZEOF_IB + SIZEOF_BB))

#define SIZEOF_PTR 			(SIZEOF_BLOCK / 4)
#define SIZEOF_DIR_BLOCK	(SIZEOF_PTR / 4)
#define SIZEOF_LOCATION		10 
#define SIZEOF_FILENAME		14
#define SIZEOF_DIRECT_PTR	(NUM_DIRECT_PTR * SIZEOF_BLOCK)

#define IB_PARTITION (SIZEOF_IB * SIZEOF_IB / SIZEOF_PTR)
#define FB_PARTITION (SIZEOF_FB / SIZEOF_BLOCK)


#define NUM_DIRECT_PTR	8

#define MAX_FILE_SIZE	((NUM_DIRECT_PTR*SIZEOF_BLOCK) + (SIZEOF_PTR*SIZEOF_BLOCK) + (SIZEOF_PTR*SIZEOF_PTR*SIZEOF_BLOCK))
#define MAX_FILE_COUNT	1024

#define DIRECT_PTR1 0
#define DIRECT_PTR2 1 
#define DIRECT_PTR3 2
#define DIRECT_PTR4 3
#define DIRECT_PTR5 4
#define DIRECT_PTR6 5
#define DIRECT_PTR7 6 
#define DIRECT_PTR8 7 
#define SINGLE_REDIRECT_PTR 8
#define DOUBLE_REDIRECT_PTR 9

/* Bitmap Operation */
#define SET_BIT_MASK1(x) (0x01 << (7-x))		// Sets bit x to 1 and remaining bits to 0
#define SET_BIT_MASK0(x) (~SET_BIT_MASK1(x))	// Sets bit x to 0 and remaining bits to 1


/* ioctl Macro Definition */
#define MAGIC_NUMBER 155
#define RD_CREAT    _IOR(MAGIC_NUMBER, 0, char *)
#define RD_OPEN	    _IOR(MAGIC_NUMBER, 2, char *)
#define RD_CLOSE	_IOR(MAGIC_NUMBER, 3, int)
#define RD_READ		_IOR(MAGIC_NUMBER, 4, struct IOParameter)
#define RD_WRITE	_IOWR(MAGIC_NUMBER, 5, struct IOParameter)
#define RD_LSEEK	_IOR(MAGIC_NUMBER, 6, struct IOParameter)
#define RD_UNLINK 	_IOR(MAGIC_NUMBER, 7, char *)


/* ioctl Functions */
int initializeRAMDISK(void);
int k_creat(char* pathname);
int k_open(char* pathname);
int k_close(int fd);
int k_read(int fd, char* address, int numBytes);
int k_write(int fd, char* address, int numBytes);
int k_unlink(char* pathname);
int k_lseek(int fd, int offset);

/* FileDescriptor - Keeps track of opened files on a per-process basis
 *		rPosition : Current read position of a file
 *		wPosition : Current write position of a file
 *		inodePtr : Pointer to the index node of a file
 */
 struct FileDescriptor {
 	int filePos;
 	struct InodeBlock *inodePtr;
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

/* Superblock (Size: 256 bytes)
 * 		numFreeInodes : Number of free index nodes in ramdisk
 * 		numFreeBlocks : Number of free blocks in ramdisk 
 * 		emptiness : Empty byte sequence to align 256 bytes block size
 */
struct SuperBlock {
	int numFreeInodes;
	int numFreeBlocks;
	int createIndex; 
	int everySixteen; 
	char emptiness[SIZEOF_BLOCK - 12];
};
	
/* InodeBlock (Size: 64 bytes) 
 * 		type : Type of the file (regular="reg" or directory="dir")
 * 		size : Size of the file in bytes
 * 		*blocks : Pointer to the file 
 *			Pointer to File
 *			Pointer to Directory
 *			Pointer to Pointer to Block for Larger Files
 *		emptiness : Empty byte sequence to align 256 bytes block size
 */
struct InodeBlock {
	char type[4]; 
	int size; 
	union Block *location[SIZEOF_LOCATION]; 
	char emptiness[SIZEOF_PTR - 48];
};

/* Regular File Block
 * 		data : Data content of the file
 */
struct RegBlock {
	char data[SIZEOF_BLOCK];
};

/* Directory File Block 
 *		filename : Name of the file (Maximum 14 characters)
 *		inodeNumber : Number for indexing into Index Node Array
 */
struct DirEntry {
	char  filename[SIZEOF_FILENAME];
	short inode;
};

/* Directory Entry 
 *		entry : Array of directory entries containing filename and inode information
 *
 */
struct DirBlock {
	struct DirEntry entry[SIZEOF_DIR_BLOCK];
};

/* Pointer to File Block 
 *		*blocks : Pointer to the file
 */
struct PtrBlock {
	union Block *location[SIZEOF_PTR];
};

/* Bitmap Block
 * 		 byte : Keep track of free and allocated blocks in the rest of the partition
 */
struct BitmapBlock {
	unsigned char byte[SIZEOF_BLOCK * SIZEOF_BB];
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
 *		InodeBlock 	(Size: 256 InodeBlocks)
 *		BitmapBlock (Size: 4 blocks)
 *		FreeBlock	(Size: (2MB - 256(1+256+4)))
 */
struct Ramdisk {
	struct SuperBlock 	sb;
	struct InodeBlock 	ib[IB_PARTITION];
	struct BitmapBlock	bb;
	union  Block		fb[FB_PARTITION];
};