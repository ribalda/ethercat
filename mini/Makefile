#----------------------------------------------------------------
#
#  Makefile
#
#  Minimales EtherCAT-Modul
#
#  $Id$
#
#----------------------------------------------------------------

ifneq ($(KERNELRELEASE),)

#----------------------------------------------------------------
# Kbuild-Abschnitt

obj-m := ec_mini.o

ec_mini-objs := mini.o

#----------------------------------------------------------------

else

#----------------------------------------------------------------
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

#----------------------------------------------------------------

endif
