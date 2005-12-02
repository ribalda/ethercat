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

obj-m := ec_mini_mod.o

ec_mini_mod-objs := ec_mini.o

#----------------------------------------------------------------

else

#----------------------------------------------------------------
# Default-Abschnitt

CONFIG_FILE = ../ethercat.conf
PWD = $(shell pwd)


include $(CONFIG_FILE) # Für KERNELDIR

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

#----------------------------------------------------------------

endif
