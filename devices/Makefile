#------------------------------------------------------------------------------
#
#  Makefile
#
#  IgH EtherCAT-Treiber - EtherCAT Geraete
#
#  $Id$
#
#------------------------------------------------------------------------------

ifneq ($(KERNELRELEASE),)

#------------------------------------------------------------------------------
# Kbuild-Abschnitt

obj-m := ec_8139too.o

ec_8139too-objs := 8139too.o

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

clean:
	$(MAKE) -C $(KERNELDIR) M=`pwd` clean

#------------------------------------------------------------------------------

endif
