#------------------------------------------------------------------------------
#
#  kbuild Makefile
#
#  IgH EtherCAT master device modules
#
#  $Id$
#
#------------------------------------------------------------------------------

ifneq ($(KERNELRELEASE),)

#------------------------------------------------------------------------------
#  kbuild section

obj-m := ec_8139too.o

ec_8139too-objs := 8139too.o

REV := $(shell svnversion $(src) 2>/dev/null)

EXTRA_CFLAGS = -DEC_REV=$(REV) -DEC_USER=$(USER)

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
