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

RTLIBDIR=rt_lib

#include $(KERNELDIR)/.config

#CFLAGS = -DRTAI -D__KERNEL__ -DMODULE -I$(KERNELDIR)/include -I$(RTAIDIR)/include -I$(RTLIBDIR)/msr-include \
#	-Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -fomit-frame-pointer \
#	-pipe -mpreferred-stack-boundary=2 -march=i686 -nostdinc -iwithprefix include

CFLAGS = -O2 -Wall -Wuninitialized -D__KERNEL__ -DMODULE -DSERIALDEBUG -DMSR_NO_PROC -I$(KERNELDIR)/include -D_RTAI -I$(RTAIDIR)/include -I$(RTLIBDIR)/msr-include

#CFLAGS += -DSIMULATION
#LDFLAGS = 

#VPATH = $(RTLIBDIR)/

TARGET = msr_modul
MODULE = $(TARGET).o

SRC = msr_io.c 

#Suchpfad für die Dateien aus dem RT-Lib-Verzeichnis
VPATH = $(RTLIBDIR)/msr-core:$(RTLIBDIR)/msr-control:$(RTLIBDIR)/msr-math:$(RTLIBDIR)/msr-misc:$(RTLIBDIR)/msr-utils

#Datei aus dem RT-Libverzeichnis für dies Projekt
RTSRC = msr_main.c msr_lists.c msr_charbuf.c msr_reg.c msr_interpreter.c msr_utils.c msr_messages.c msr_base64.c msr_proc.c msr_error_reg.c

ALLSRC = $(SRC) $(RTSRC)

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


#all: msr_module.o
#
#msr_io.o: msr_io.c msr_io.h
#	$(CC) $(CFLAGS) -c -o $@ $<
#
#msr_module.o: msr_io.o
#	$(LD) -r -o $@ $^
#

#                  $(CC) -c $(CFLAGS) $(CPPFLAGS) -o $@ $<


#clean:
#	rm -f *.o *~ core

