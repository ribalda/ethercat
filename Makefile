#------------------------------------------------------------------------------
#
#  Globales Makefile
#
#  IgH EtherCAT-Treiber
#
#  $Id$
#
#------------------------------------------------------------------------------

ifneq ($(KERNELRELEASE),)

#------------------------------------------------------------------------------
# Kbuild-Abschnitt

obj-m := master/ devices/

#------------------------------------------------------------------------------

else

#------------------------------------------------------------------------------
# Default-Abschnitt

ifneq ($(wildcard ethercat.conf),)
include ethercat.conf
else
KERNELDIR := /usr/src/linux
endif

modules:
	$(MAKE) -C $(KERNELDIR) M=`pwd`

clean:
	$(MAKE) -C $(KERNELDIR) M=`pwd` clean

config conf $(CONFIG_FILE):
	@echo "# EtherCAT Konfigurationsdatei Kernel 2.6" > $(CONFIG_FILE)
	@echo >> $(CONFIG_FILE)
	@echo "KERNELDIR = /usr/src/linux" >> $(CONFIG_FILE)
	@echo >> $(CONFIG_FILE)
	@echo "$(CONFIG_FILE) erstellt."

#------------------------------------------------------------------------------

endif
