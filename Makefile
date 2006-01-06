#----------------------------------------------------------------
#
#  Globales Makefile
#
#  IgH EtherCAT-Treiber
#
#  $Id$
#
#----------------------------------------------------------------

CONFIG_FILE = ethercat.conf

ifeq ($(CONFIG_FILE),$(wildcard $(CONFIG_FILE)))
include $(CONFIG_FILE)
endif

obj-m := drivers/ mini/

ifeq ($(MAKE_RT),yes)
obj-m += rt/
endif

#----------------------------------------------------------------

all:
	$(MAKE) -C $(KERNELDIR) M=`pwd` modules

clean:
	$(MAKE) -C $(KERNELDIR) M=`pwd` clean

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
