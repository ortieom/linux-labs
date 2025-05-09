obj-m += int_stack.o

KERNELDIR ?= /lib/modules/$(shell uname -r)/build

CC = gcc
CFLAGS = -Wall -Wextra
USERSPACE_TARGET = kernel_stack

all: module userspace

module:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
	
userspace: $(USERSPACE_TARGET)

$(USERSPACE_TARGET): kernel_stack.c
	$(CC) $(CFLAGS) -o $@ $<


clean: clean-module clean-userspace

clean-module:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

clean-userspace:
	rm -f $(USERSPACE_TARGET)
