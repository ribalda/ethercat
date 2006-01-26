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

obj-m := master/ devices/ rt/ mini/

#------------------------------------------------------------------------------

else

#------------------------------------------------------------------------------
# Default-Abschnitt

include ethercat.conf

modules:
	$(MAKE) -C $(KERNELDIR) M=`pwd`

clean:
	$(MAKE) -C $(KERNELDIR) M=`pwd` clean
	@rm -rf */.tmp_versions

config conf $(CONFIG_FILE):
	@echo "# EtherCAT Konfigurationsdatei Kernel 2.6" > $(CONFIG_FILE)
	@echo >> $(CONFIG_FILE)
	@echo "KERNELDIR = /usr/src/linux" >> $(CONFIG_FILE)
	@echo >> $(CONFIG_FILE)
	@echo "$(CONFIG_FILE) erstellt."

#----------------------------------------------------------------

endif
