#------------------------------------------------------------------------------
#
#  Makefile
#
#  IgH EtherCAT-Treiber
#
#  $Id$
#
#------------------------------------------------------------------------------

ifneq ($(KERNELRELEASE),)

#------------------------------------------------------------------------------
# Kbuild-Abschnitt

obj-m := ec_master.o

ec_master-objs := module.o master.o device.o slave.o command.o types.o domain.o

REV = `svnversion $(src)`
DATE = `date`

EXTRA_CFLAGS = -DEC_REV="$(REV)" -DEC_USER="$(USER)" -DEC_DATE="$(DATE)"

#------------------------------------------------------------------------------

else

#------------------------------------------------------------------------------
# Default-Abschnitt

ifneq ($(wildcard ethercat.conf),)
include ethercat.conf
else
KERNELDIR = /lib/modules/`uname -r`/build
endif

modules:
	$(MAKE) -C $(KERNELDIR) M=`pwd` modules

clean: cleandoc
	$(MAKE) -C $(KERNELDIR) M=`pwd` clean

doc:
	doxygen Doxyfile

cleandoc:
	@rm -rf doc

#------------------------------------------------------------------------------

endif
