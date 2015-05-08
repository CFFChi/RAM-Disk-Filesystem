obj-m += ramdisk_module.o
ramdisk_module-objs := ramdisk.o k_read.o k_write.o k_open.o k_close.o k_creat.o k_unlink.o k_mkdir.o k_readdir.o k_lseek.o helper.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	gcc -g -o ramdisk_test ramdisk_test.c ramdisk_ioctl.c
	gcc -g -o k_test k_test.c ramdisk_ioctl.c
	insmod ramdisk_module.ko

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm ramdisk_test k_test
	rmmod ramdisk_module

pull:
	scp mando@csa2.bu.edu:/home/ugrad/mando/CS552/ramdisk/* .
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	gcc -g -o ramdisk_test ramdisk_test.c ramdisk_ioctl.c
	gcc -g -o k_test k_test.c ramdisk_ioctl.c
	insmod ramdisk_module.ko

update:
	rmmod ramdisk_module
	rm ramdisk_test k_test
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	scp mando@csa2.bu.edu:/home/ugrad/mando/CS552/ramdisk/* .
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	gcc -g -o ramdisk_test ramdisk_test.c ramdisk_ioctl.c
	gcc -g -o k_test k_test.c ramdisk_ioctl.c
	insmod ramdisk_module.ko
