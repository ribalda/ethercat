#----------------------------------------------------------------
#
#  Makefile
#
#  EtherCAT-RT-Modul
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

CFLAGS = -O2 -Wall -Wuninitialized -D__KERNEL__ -DMODULE -DSERIALDEBUG -DMSR_NO_PROC -I$(KERNELDIR)/include -D_RTAI -I$(RTAIDIR)/include -I$(RTLIBDIR)/msr-include

#Suchpfad für die Dateien aus dem RT-Lib-Verzeichnis
VPATH = $(RTLIBDIR)/msr-core:$(RTLIBDIR)/msr-control:$(RTLIBDIR)/msr-math:$(RTLIBDIR)/msr-misc:$(RTLIBDIR)/msr-utils

MODULE = msr_modul.o
SRC = msr_io.c
RTSRC = msr_main.c msr_lists.c msr_charbuf.c msr_reg.c msr_interpreter.c \
	msr_utils.c msr_messages.c msr_base64.c msr_proc.c msr_error_reg.c
ALLSRC = $(SRC) $(RTSRC)
OBJ = $(ALLSRC:.c=.o)

#----------------------------------------------------------------

all: .output_dirs .depend $(MODULE) Makefile

$(MODULE): $(OBJ)
	@echo "Making module"
	$(LD) -r $(OBJ) -o $@

.c.o:
	@echo "Making obj $@"
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
