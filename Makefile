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

#----------------------------------------------------------------

all: .rs232dbg .drivers .rt .mini

doc docs:
	doxygen Doxyfile

.drivers:
	$(MAKE) -C drivers

ifeq ($(MAKE_RT),yes)
.rt:
	$(MAKE) -C rt
else
.rt:
	@echo "Skipping Real-Time."
endif

ifeq ($(MAKE_RS232),yes)
.rs232dbg:
	$(MAKE) -C rs232dbg
else
.rs232dbg:
	@echo "Skipping rs232dbg."
endif

.mini:
	$(MAKE) -C mini

config $(CONFIG_FILE):
	@echo "# EtherCAT Konfigurationsdatei" > $(CONFIG_FILE)
	@echo >> $(CONFIG_FILE)
	@echo "KERNELDIR = /vol/projekte/msr_messen_steuern_regeln/linux/kernel/2.4.20/include/linux-2.4.20.CX1100-rthal5" >> $(CONFIG_FILE)
	@echo "RTAIDIR   = /vol/projekte/msr_messen_steuern_regeln/linux/kernel/2.4.20/include/rtai-24.1.13" >> $(CONFIG_FILE)
	@echo "RTLIBDIR  = rt_lib" >> $(CONFIG_FILE)
	@echo >> $(CONFIG_FILE)
	@echo "GCC_SYSTEMDIR = /usr/lib/gcc-lib/i486-suse-linux/3.3/include" >> $(CONFIG_FILE)
	@echo >> $(CONFIG_FILE)
	@echo "MAKE_RT    = yes" >> $(CONFIG_FILE)
	@echo "MAKE_RS232 = yes" >> $(CONFIG_FILE)
	@echo >> $(CONFIG_FILE)
	@echo "$(CONFIG_FILE) erstellt."

clean:
	$(MAKE) -C rt clean
	$(MAKE) -C drivers clean
	$(MAKE) -C rs232dbg clean
	$(MAKE) -C mini clean

#----------------------------------------------------------------
