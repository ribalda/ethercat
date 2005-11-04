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

#KERNELDIR=/usr/src/linux
#KERNELDIR=/home/rich/linux-2.4.20.CX1100-rthal5
#KERNELDIR=./linux-2.4.20.CX1100-rthal5

#IgH
KERNELDIR = /vol/projekte/msr_messen_steuern_regeln/linux/kernel/2.4.20/include/linux-2.4.20.CX1100-rthal5
RTAIDIR   = /vol/projekte/msr_messen_steuern_regeln/linux/kernel/2.4.20/include/rtai-24.1.13
RTLIBDIR = rt_lib

#euler-nottuln
#KERNELDIR = /usr/src/linux
#RTAIDIR = /usr/src/rtai

#patra
#KERNELDIR = /usr/src/linux-2.4.20.CX1100-rthal5
#RTAIDIR   = /usr/src/rtai-24.1.13

#include $(KERNELDIR)/.config

ECAT_8139_OBJ = drv_8139too.o ec_device.o ec_master.o \
	ec_slave.o ec_command.o ec_types.o


CFLAGS = -O2 -Wall -Wuninitialized -D__KERNEL__ -DMODULE \
	-I$(KERNELDIR)/include -D_RTAI -I$(RTAIDIR)/include \
	-I$(RTLIBDIR)/msr-include

ifdef CONFIG_SMP
	CFLAGS += -D__SMP__ -DSMP
endif

#################################################################

all: .depend ecat_8139too.o

ecat_8139too.o: $(ECAT_8139_OBJ)
	$(LD) -r $(ECAT_8139_OBJ) -o $@

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

doc docs:
	$(MAKE) -C .. doc

#################################################################

.depend:
	$(CC) $(CFLAGS) -M *.c > .depend

ifeq (.depend,$(wildcard .depend))
include .depend
endif

#################################################################

clean:
	rm -f *.o *~ core .depend

#################################################################

