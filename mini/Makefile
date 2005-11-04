#----------------------------------------------------------------
#
#  Makefile
#
#  Minimales EtherCAT-Modul
#
#  $Id$
#
#----------------------------------------------------------------

EC_DIR = /vol/projekte/msr_messen_steuern_regeln/ethercat

#KERNELDIR = $(EC_DIR)/linux-2.4.20.CX1100-rthal5-kdb
KERNELDIR = $(EC_DIR)/linux-2.4.20-kdb
#KERNELDIR = /usr/src/linux

CFLAGS = -O2 -Wall -Wuninitialized -D__KERNEL__ \
	-DMODULE -I$(KERNELDIR)/include

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
