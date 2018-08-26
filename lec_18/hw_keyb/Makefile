ifneq ($(KERNELRELEASE),) # kbuild part of makefile
CFLAGS_hw3_keyb.o := -DDEBUG
obj-m := hw3_keyb.o
else # normal makefile
KDIR ?= /lib/modules/$(shell uname -r)/build

default: module app
module:
	$(MAKE) -C $(KDIR) M=$(PWD) C=1 modules
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) C=1 clean

CC = $(CROSS_COMPILE)gcc
CFLAGS = -O2 -Wall

.PHONY: module clean
endif
