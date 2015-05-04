#define RD_CREAT  _IOR(155, 0, char *)
#define RD_OPEN	  _IOR(155, 2, char *)
#define RD_CLOSE  _IOR(155, 3, int)
#define RD_WRITE  _IOWR(22, 3, struct IOParameter)
#define RD_UNLINK _IOR(155, 7, char *)


struct IOParameter {
	int fd; 
	char *address;
	int numBytes; 
};

int rd_creat(char *pathname);
int rd_unlink(char *pathname);
int rd_open(char *pathname); 
int rd_close(int fd);
int rd_write(int fd, char *address, int num_bytes);