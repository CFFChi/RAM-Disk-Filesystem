int k_readdir(int fd, char* address) {
	struct FileDescriptor* desc;
	struct DirEntry temp;
	
	if ((desc = fdTable[fd]) == NULL) {
		printk("k_readdir() Error : File Descriptor is not valid\n");
		return -1;
	}
	
	if (strncmp(ramdisk->ib[fd].type, "dir", 3) != 0) {
		printk("k_readdir() Error : File is not a directory\n");
		return -1;
	} 
	
	if(isEmpty(desc->inodePtr->location)) {
		printk("k_readdir() Error : File is empty\n");
		return 0;
	}
	
	
	//UNFINISHED!!
	if(islast?(desc->filePos)) {
		printk("k_readdir() Error : Reached end of directory\n");
		return 0;
	}
	
	temp.filename = (*ramdisk->ib[fd].location[0]).dir.entry[desc->filePos].filename;
	temp.inode = (*ramdisk->ib[fd].location[0]).dir.entry[desc->filePos].inode;
	
	//UNFINISHED!!
	write temp to address;
	
	desc->filePos++;
	
}
	
