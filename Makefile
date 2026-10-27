# Top-level Makefile - builds the kernel module and the user programs.
# Owner: M1.   make | make load | make unload | make clean
all:
	$(MAKE) -C kernel
	$(MAKE) -C user

load: all
	sudo insmod kernel/procinfo.ko
	sudo dmesg | tail -n 3

unload:
	sudo rmmod procinfo
	sudo dmesg | tail -n 2

clean:
	$(MAKE) -C kernel clean
	$(MAKE) -C user clean

.PHONY: all load unload clean
