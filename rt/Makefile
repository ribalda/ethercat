#------------------------------------------------------------------------------
#
#  Makefile
#
#  $Id$
#
#  Copyright (C) 2006  Florian Pose, Ingenieurgemeinschaft IgH
#
#  This file is part of the IgH EtherCAT Master.
#
#  The IgH EtherCAT Master is free software; you can redistribute it
#  and/or modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; version 2 of the License.
#
#  The IgH EtherCAT Master is distributed in the hope that it will be
#  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with the IgH EtherCAT Master; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
#------------------------------------------------------------------------------

ifneq ($(KERNELRELEASE),)

#------------------------------------------------------------------------------
#  kbuild section
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
#  default section
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
