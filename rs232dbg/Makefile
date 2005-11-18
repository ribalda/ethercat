#----------------------------------------------------------------
#
#  Makefile
#
#  Kernel-Debugging über die serielle Schnittstelle
#
#  $Id$
#
#----------------------------------------------------------------

CONFIG_FILE = ../kerneldirs.mk

ifeq ($(CONFIG_FILE),$(wildcard $(CONFIG_FILE)))
include $(CONFIG_FILE)
else
KERNELDIR = /vol/projekte/msr_messen_steuern_regeln/linux/kernel/2.4.20/include/linux-2.4.20.CX1100-rthal5
RTAIDIR   = /vol/projekte/msr_messen_steuern_regeln/linux/kernel/2.4.20/include/rtai-24.1.13
RTLIBDIR  = rt_lib
endif

CFLAGS = -O2 -Wall -Wuninitialized -D__KERNEL__ -DMODULE \
	-I$(KERNELDIR)/include -D_RTAI -I$(RTAIDIR)/include -I$(RTLIBDIR)/msr-include

MODULE = sdbg.o
SRC = rs232dbg.c aip_com.c
OBJ = $(SRC:.c=.o)

#----------------------------------------------------------------

all: .output_dirs .depend $(MODULE) Makefile

$(MODULE): $(OBJ)
	$(LD) -r $(OBJ) -o $@

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

.output_dirs:
	@echo "x-- Directories -------------"
	@echo "| Kernel $(KERNELDIR)"
	@echo "| RTAI   $(RTAIDIR)"
	@echo "| RT_lib $(RTLIBDIR)"
	@echo "x----------------------------"

depend .depend dep:
	$(CC) $(CFLAGS) -M $(SRC) > .depend

clean:
	rm -f *.o *~ core .depend

#----------------------------------------------------------------

ifeq (.depend,$(wildcard .depend))
include .depend
endif

#----------------------------------------------------------------
