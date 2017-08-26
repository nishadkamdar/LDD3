Codes from Linux Device Drivers, 3rd Edition by Jonathan Corbet, Greg Kroah-Hartman, Alessandro Rubini

To compile the modules, create a makefile and write

obj-m := <module_name>.o

Now, in the terminal, type the following command to compile the module

make -C /usr/src/linux-2.6.32 M=$PWD modules

To clean the files created by the module, type

make -C /usr/src/linux-2.6.32 M=$PWD clean

The path /usr/src/linux-2.6.32 is the kernel source compiled manually.

sudo insmod <module_name>.ko

a proc file, /proc/scullmem will be created.
cat the scullmem file

type dmesg: You will see the following debug messages:

I was assigned major number 249. To talk to
the driver, create a dev file with
mknod /dev/scull c 249 0

run: 
mknod /dev/scull c 249 0

compile and run sculltest.c

cat the /dev/scull device
