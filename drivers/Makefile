#----------------------------------------------------------------
#
#  Makefile
#
#  IgH EtherCAT-Treiber
#
#  $Id$
#
#----------------------------------------------------------------

ifneq ($(KERNELRELEASE),)

#----------------------------------------------------------------
# Kbuild-Abschnitt

obj-m := 8139too-ecat.o

8139too-ecat-objs := 8139too.o ec_device.o ec_master.o \
					ec_slave.o ec_command.o ec_types.o

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
