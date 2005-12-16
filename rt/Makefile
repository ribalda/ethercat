# Comment/uncomment the following line to disable/enable debugging
#DEBUG = y


# Add your debugging flag (or not) to CFLAGS
ifeq ($(DEBUG),y)
  DEBFLAGS = -O -g -DSHORT_DEBUG # "-O" is needed to expand inlines
else
  DEBFLAGS = -O2
endif

CFLAGS += $(DEBFLAGS)
CFLAGS += -I.. 

RTLIB = /vol/projekte/msr_messen_steuern_regeln/linux/kernel_space/rt_lib-4.0.0-2.6krnl

#Suchpfad für die Dateien aus dem RT-Lib-Verzeichnis
VPATH := $(RTLIB)/msr-core:$(RTLIB)/msr-control:$(RTLIB)/msr-hwdriver:$(RTLIB)/msr-math:$(RTLIB)/msr-misc:$(RTLIB)/msr-utils

#Datei aus dem RT-Libverzeichnis für dies Projekt
RTSRC := msr_main.o msr_lists.o msr_charbuf.o msr_reg.o msr_interpreter.o msr_utils.o msr_messages.o msr_functiongen.o msr_base64.o msr_watchdog.o msr_proc.o msr_error_reg.o

ifneq ($(KERNELRELEASE),)
# call from kernel build system


EXTRA_CFLAGS := -I$(RTLIB)/msr-include -D_SIMULATION -I/usr/include -mhard-float

#EXTRA_LDFLAGS :=  -L/usr/lib -lm

msr_modul-y := msr_module.o \
	msr_jitter.o \
	rt_lib/msr-core/msr_lists.o \
	rt_lib/msr-core/msr_main.o \
	rt_lib/msr-core/msr_charbuf.o \
	rt_lib/msr-core/msr_reg.o \
	rt_lib/msr-core/msr_interpreter.o \
	rt_lib/msr-core/msr_messages.o \
	rt_lib/msr-core/msr_proc.o \
	rt_lib/msr-core/msr_error_reg.o \
	rt_lib/msr-utils/msr_utils.o \
	rt_lib/msr-math/msr_base64.o \
	libm.o 


obj-m	:= msr_modul.o



else


KERNELDIR := /lib/modules/$(shell uname -r)/build
PWD       := $(shell pwd)




default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

endif


clean:
	rm -f core .depend
	rm -rf .tmp_versions
	find -L -maxdepth 3 -name "*.o" -exec rm {} \;	
	find -L -maxdepth 3 -name "*~" -exec rm {} \;	
	find -L -maxdepth 3 -name "*.cmd" -exec rm {} \;	
	find -L -maxdepth 3 -name "*.ko" -exec rm {} \;	
	find -L -maxdepth 3 -name "*.mod.c" -exec rm {} \;	

depend .depend dep:
	$(CC) $(CFLAGS) -M *.c > .depend



ifeq (.depend,$(wildcard .depend))
include .depend
endif
