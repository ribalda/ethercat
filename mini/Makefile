#----------------------------------------------------------------
#
#  Makefile
#
#  Minimales EtherCAT-Modul
#
#  $Id$
#
#----------------------------------------------------------------

CONFIG_FILE = ../ethercat.conf

ifeq ($(CONFIG_FILE),$(wildcard $(CONFIG_FILE)))
include $(CONFIG_FILE)
else
KERNELDIR = /vol/projekte/msr_messen_steuern_regeln/linux/kernel/2.4.20/include/linux-2.4.20.CX1100-rthal5
RTAIDIR   = /vol/projekte/msr_messen_steuern_regeln/linux/kernel/2.4.20/include/rtai-24.1.13
RTLIBDIR  = rt_lib
endif

CFLAGS = -O2 -Wall -Wuninitialized -D__KERNEL__ \
	-DMODULE -I$(KERNELDIR)/include

MODULE = ec_mini_mod.o
SRC = ec_mini.c
OBJ = $(SRC:.c=.o)

#----------------------------------------------------------------

all: .output_dirs .depend Makefile $(MODULE)

$(MODULE): $(OBJ)
	$(LD) -r $(OBJ) -o $@

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
