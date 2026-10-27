# Kbuild makefile for the Process Information Driver (procinfo.ko)
# Owner: M1. Three source files are linked into one loadable module.

obj-m := procinfo.o
procinfo-y := procinfo_main.o procinfo_tasks.o procinfo_ctl.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
