#----------------------------------------------------------------
#
#  Makefile
#
#  IgH EtherCAT-Treiber
#
#  $Id$
#
#----------------------------------------------------------------

KERNEL_DIRS_FILE = ../kerneldirs.mk

ifeq ($(KERNEL_DIRS_FILE),$(wildcard $(KERNEL_DIRS_FILE)))
include $(KERNEL_DIRS_FILE)
else
KERNELDIR = /vol/projekte/msr_messen_steuern_regeln/linux/kernel/2.4.20/include/linux-2.4.20.CX1100-rthal5
RTAIDIR   = /vol/projekte/msr_messen_steuern_regeln/linux/kernel/2.4.20/include/rtai-24.1.13
RTLIBDIR = rt_lib
endif

CFLAGS = -O2 -Wall -Wuninitialized -D__KERNEL__ -DMODULE \
	-I$(KERNELDIR)/include -D_RTAI -I$(RTAIDIR)/include \
	-I$(RTLIBDIR)/msr-include

ifdef CONFIG_SMP
	CFLAGS += -D__SMP__ -DSMP
endif

MODULE = ecat_8139too.o
OBJ = ec_device.o ec_master.o ec_slave.o ec_command.o ec_types.o
SRC = $(OBJ:.o=.c)

#----------------------------------------------------------------

all: .output_dirs .depend $(MODULE)

$(MODULE): $(OBJ)
	$(LD) -r $(OBJ) -o $@

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

doc docs:
	$(MAKE) -C .. doc

.output_dirs:
	@echo "x-- Directories -------------"
	@echo "| Kernel $(KERNELDIR)"
	@echo "| RTAI   $(RTAIDIR)"
	@echo "| RT_lib $(RTLIBDIR)"
	@echo "x----------------------------"

.depend:
	$(CC) $(CFLAGS) -M $(SRC) > .depend

clean:
	rm -f *.o *~ core .depend

#----------------------------------------------------------------

ifeq (.depend,$(wildcard .depend))
include .depend
endif

#----------------------------------------------------------------
