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

obj-m := drivers/ rt/ mini/

#------------------------------------------------------------------------------

else

#------------------------------------------------------------------------------
# Default-Abschnitt

include ethercat.conf

modules:
	$(MAKE) -C $(KERNELDIR) M=`pwd` modules

clean:
	$(MAKE) -C $(KERNELDIR) M=`pwd` clean
	@rm -rf */.tmp_versions

doc docs:
	doxygen Doxyfile

config conf $(CONFIG_FILE):
	@echo "# EtherCAT Konfigurationsdatei Kernel 2.6" > $(CONFIG_FILE)
	@echo >> $(CONFIG_FILE)
	@echo "KERNELDIR = /usr/src/linux" >> $(CONFIG_FILE)
	@echo "RTAIDIR   =" >> $(CONFIG_FILE)
	@echo "RTLIBDIR  =" >> $(CONFIG_FILE)
	@echo >> $(CONFIG_FILE)
	@echo "MAKE_RT    = yes" >> $(CONFIG_FILE)
	@echo >> $(CONFIG_FILE)
	@echo "$(CONFIG_FILE) erstellt."

#----------------------------------------------------------------

endif
