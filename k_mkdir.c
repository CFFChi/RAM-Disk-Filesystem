int k_mkdir(char* pathname) {
	short parentInode; 
	int ret, freeInode; 
	char* filename;

	filename = (char *) kmalloc(14, GFP_KERNEL);

	/* Retrieve last directory entry in pathname and store in parentInode */
	if ((ret = fileExists(pathname, filename, &parentInode)) != 0) {
		printk("k_creat() Error : Dir already exists or Error in fileExists()\n");
		return -1;
	}
	
	/* File Creation 
	 * 1) Search for free index node 
	 * 2) Set inode.type=DIR and inode.size=0
	 * 3) Assign inode to parentInode   */
	if ((freeInode = getFreeInode()) < 0) {
		printk("k_creat() Error : Could not find free index node\n");
		return -1; 
	}
	
	printk("freeInode: %d\n", freeInode);
	setDirInode(freeInode, 0);

	printk("parentInode : %d\n", parentInode);

	if ((ret = assignInode(parentInode, freeInode, filename)) < 0) {
		printk("kcreat() Error: Could not assign freeInode to parentInode\n");
		return -1;
	} 
	return 0;
}
