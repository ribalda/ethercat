#################################################################
#
#  Makefile
#
#  IgH EtherCAT-Treiber
#
#  $Date$
#  $Author$
#
#################################################################

MSR_DIR = /vol/projekte/msr_messen_steuern_regeln
EC_DIR = $(MSR_DIR)/ethercat

#KERNELDIR = $(EC_DIR)/linux-2.4.20.CX1100-rthal5-kdb
KERNELDIR = $(EC_DIR)/linux-2.4.20-kdb
#KERNELDIR = /usr/src/linux

ECAT_8139_OBJ = drv_8139too.o ec_device.o ec_master.o \
	ec_slave.o ec_command.o ec_types.o

CFLAGS = -O2 -Wall -Wuninitialized -D__KERNEL__ \
	-DMODULE -I$(KERNELDIR)/include

#################################################################

all: .depend Makefile ecat_8139too.o

ecat_8139too.o: $(ECAT_8139_OBJ)
	$(LD) -r $(ECAT_8139_OBJ) -o $@

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

doc docs:
	$(MAKE) -C .. doc

#################################################################

.depend depend dep:
	$(CC) $(CFLAGS) -M *.c > .depend

ifeq (.depend,$(wildcard .depend))
include .depend
endif

#################################################################

clean:
	rm -f *.o *~ core .depend

#################################################################

