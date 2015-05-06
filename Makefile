obj-m += ramdisk.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	gcc -g -o ramdisk_test ramdisk_test.c ramdisk_ioctl.c
	insmod ramdisk.ko

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rmmod ramdisk
	rm ramdisk_test

