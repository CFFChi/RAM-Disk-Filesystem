struct IOParameter {
	int fd; 
	char *address;
	int numBytes; 
};

int rd_creat(char *pathname);
int rd_mkdir(char *pathname);
int rd_unlink(char *pathname);
int rd_open(char *pathname); 
int rd_close(int fd);
int rd_read(int fd, char *address, int numBytes);
int rd_write(int fd, char *address, int numBytes);
int rd_lseek(int fd, int offset);