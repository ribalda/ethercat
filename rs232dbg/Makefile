
MSRDIR = /vol/projekte/msr_messen_steuern_regeln
ECATDIR = $(MSRDIR)/ethercat

KERNELDIR = $(ECATDIR)/linux-2.4.20.CX1100-rthal5-kdb
RTAIDIR   = $(MSRDIR)/linux/kernel/2.4.20/include/rtai-24.1.13

CFLAGS = -O2 -Wall -Wuninitialized -D__KERNEL__ -DMODULE \
	-I$(KERNELDIR)/include -D_RTAI -I$(RTAIDIR)/include -I$(RTLIBDIR)/msr-include

TARGET = sdbg
MODULE = $(TARGET).o

SRC = rs232dbg.c aip_com.c

ALLSRC = $(SRC)

OBJS = $(ALLSRC:.c=.o)  

all: .depend $(TARGET).o Makefile

$(TARGET).o: $(SRC:.c=.o) $(RTSRC:.c=.o)
	$(LD) -r $(OBJS)  -o $@ $(LDFLAGS)

install: msr_modul.o
	lsmod | grep cif-rtai >/dev/null 2>&1 && sudo rmmod msr_modul || true
	sudo insmod msr_modul.o

clean:
	rm -f *.o *~ core .depend

depend .depend dep:
	$(CC) $(CFLAGS) -M *.c > $@

ifeq (.depend,$(wildcard .depend))
include .depend
endif
