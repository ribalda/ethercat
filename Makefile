#----------------------------------------------------------------
#
#  Globales Makefile
#
#  IgH EtherCAT-Treiber
#
#  $Id$
#
#----------------------------------------------------------------

KERNEL_DIRS_FILE = kerneldirs.mk

#----------------------------------------------------------------

all: .rs232dbg .drivers .rt .mini

doc docs:
	doxygen Doxyfile

.drivers:
	$(MAKE) -C drivers

.rt:
	$(MAKE) -C rt

.rs232dbg:
	$(MAKE) -C rs232dbg

.mini:
	$(MAKE) -C mini

kerneldirs:
	@echo "# EtherCAT Standard-Kernel-Verzeichnisse" > $(KERNEL_DIRS_FILE)
	@echo >> $(KERNEL_DIRS_FILE)
	@echo "KERNELDIR = /vol/projekte/msr_messen_steuern_regeln/linux/kernel/2.4.20/include/linux-2.4.20.CX1100-rthal5" >> $(KERNEL_DIRS_FILE)
	@echo "RTAIDIR   = /vol/projekte/msr_messen_steuern_regeln/linux/kernel/2.4.20/include/rtai-24.1.13" >> $(KERNEL_DIRS_FILE)
	@echo "RTLIBDIR = rt_lib" >> $(KERNEL_DIRS_FILE)
	@echo >> $(KERNEL_DIRS_FILE)
	@echo "$(KERNEL_DIRS_FILE) erstellt."

clean:
	$(MAKE) -C rt clean
	$(MAKE) -C drivers clean
	$(MAKE) -C rs232dbg clean
	$(MAKE) -C mini clean

#----------------------------------------------------------------
