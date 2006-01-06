#------------------------------------------------------------------------------
#
# Makefile Echtzeitmodule
#
# $Id$
#
#------------------------------------------------------------------------------

ifneq ($(KERNELRELEASE),)

#------------------------------------------------------------------------------
# Kbuild-Abschnitt

obj-m	:= msr_modul.o

msr_modul-objs := msr_module.o \
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

EXTRA_CFLAGS := -I $(src)/rt_lib/msr-include -D_SIMULATION \
		-I/usr/include -mhard-float

#------------------------------------------------------------------------------

else

#------------------------------------------------------------------------------
# Default-Abschnitt

ifneq ($(wildcard ethercat.conf),)
include ethercat.conf
else
KERNELDIR = /lib/modules/`uname -r`/build
endif

modules:
	$(MAKE) -C $(KERNELDIR) M=`pwd` modules

clean:
	$(MAKE) -C $(KERNELDIR) M=`pwd` clean

#------------------------------------------------------------------------------

endif
