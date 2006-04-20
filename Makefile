#------------------------------------------------------------------------------
#
#  EtherCAT Makefile
#
#  $Id$
#
#------------------------------------------------------------------------------

ifneq ($(KERNELRELEASE),)

#------------------------------------------------------------------------------
#  kbuild section

obj-m := master/ devices/

#------------------------------------------------------------------------------

else

#------------------------------------------------------------------------------
#  default section

ifneq ($(wildcard ethercat.conf),)
include ethercat.conf
else
KERNEL := `uname -r`
DEVICEINDEX := 99
endif

KERNELDIR := /lib/modules/$(KERNEL)/build

modules:
	$(MAKE) -C $(KERNELDIR) M=`pwd`

clean: cleandoc
	$(MAKE) -C $(KERNELDIR) M=`pwd` clean

doc:
	doxygen Doxyfile

cleandoc:
	@rm -rf doc


install:
	@./install.sh $(KERNEL) $(DEVICEINDEX)

#------------------------------------------------------------------------------

endif
