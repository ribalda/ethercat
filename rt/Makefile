#------------------------------------------------------------------------------
#
#  Makefile Echtzeitmodule
#
#  $Id$
#
#------------------------------------------------------------------------------

ifneq ($(KERNELRELEASE),)

#------------------------------------------------------------------------------
#  Kbuild-Abschnitt
#------------------------------------------------------------------------------

ifneq ($(wildcard $(src)/rt.conf),)
include $(src)/rt.conf
else
MODULENAME := rt
endif

obj-m := $(MODULENAME).o

$(MODULENAME)-objs := msr_rt.o \
			rt_lib/msr-core/msr_lists.o \
			rt_lib/msr-core/msr_main.o \
			rt_lib/msr-core/msr_charbuf.o \
			rt_lib/msr-core/msr_reg.o \
			rt_lib/msr-core/msr_interpreter.o \
			rt_lib/msr-core/msr_messages.o \
			rt_lib/msr-core/msr_proc.o \
			rt_lib/msr-core/msr_error_reg.o \
			rt_lib/msr-utils/msr_utils.o \
			rt_lib/msr-utils/msr_time.o \
			rt_lib/msr-math/msr_base64.o \
			rt_lib/msr-math/msr_hex_bin.o \
			libm.o

EXTRA_CFLAGS := -I$(src)/rt_lib/msr-include -D_SIMULATION \
		-I/usr/include -mhard-float \
		-DSVNREV=$(shell svnversion $(src)) -DUSER=$(USER)

#------------------------------------------------------------------------------

else

#------------------------------------------------------------------------------
#  Default-Abschnitt
#------------------------------------------------------------------------------

ifneq ($(wildcard rt.conf),)
include rt.conf
else
MODULENAME := msr_rt
KERNEL := $(shell uname -r)
endif

KERNELDIR := /lib/modules/$(KERNEL)/build

modules:
	$(MAKE) -C $(KERNELDIR) M=`pwd`

clean:
	$(MAKE) -C $(KERNELDIR) M=`pwd` clean

install:
	@./install.sh $(MODULENAME) $(KERNEL)

#------------------------------------------------------------------------------

endif
