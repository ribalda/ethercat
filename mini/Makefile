#----------------------------------------------------------------
#
#  Makefile
#
#  Minimales EtherCAT-Modul
#
#  $Id$
#
#----------------------------------------------------------------

MSRDIR = /vol/projekte/msr_messen_steuern_regeln
KERNELDIR = $(MSRDIR)/linux/kernel/2.4.20/include/linux-2.4.20.CX1100-rthal5
CFLAGS = -O2 -Wall -Wuninitialized -D__KERNEL__ -DMODULE -I$(KERNELDIR)/include

MODULE = ec_mini_mod.o

SRC = ec_mini.c 
OBJ = $(SRC:.c=.o)

#----------------------------------------------------------------

all: .depend Makefile $(MODULE)

$(MODULE): $(OBJ)
	$(LD) -r $(OBJ) -o $@

#----------------------------------------------------------------

depend .depend dep:
	$(CC) $(CFLAGS) -M $(SRC) > .depend

ifeq (.depend,$(wildcard .depend))
include .depend
endif

#----------------------------------------------------------------

clean:
	rm -f *.o *~ core .depend

#----------------------------------------------------------------
