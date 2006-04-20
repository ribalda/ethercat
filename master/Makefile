#------------------------------------------------------------------------------
#
#  Makefile
#
#  IgH EtherCAT master module
#
#  $Id$
#
#------------------------------------------------------------------------------

ifneq ($(KERNELRELEASE),)

#------------------------------------------------------------------------------
#  kbuild section

obj-m := ec_master.o

ec_master-objs := module.o master.o device.o slave.o command.o types.o \
			domain.o mailbox.o canopen.o ethernet.o

REV := $(shell svnversion $(src) 2>/dev/null)

EXTRA_CFLAGS := -DSVNREV=$(REV) -DUSER=$(USER)

#------------------------------------------------------------------------------

else

#------------------------------------------------------------------------------
#  default section

ifneq ($(wildcard ../ethercat.conf),)
include ../ethercat.conf
else
KERNEL := $(shell uname -r)
endif

KERNELDIR := /lib/modules/$(KERNEL)/build

modules:
	$(MAKE) -C $(KERNELDIR) M=`pwd`

clean:
	$(MAKE) -C $(KERNELDIR) M=`pwd` clean

#------------------------------------------------------------------------------

endif
