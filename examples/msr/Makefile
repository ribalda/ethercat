#------------------------------------------------------------------------------
#
#  Makefile
#
#  Sample module for use with IgH MSR library.
#
#  $Id$
#
#  Copyright (C) 2006  Florian Pose, Ingenieurgemeinschaft IgH
#
#  This file is part of the IgH EtherCAT Master.
#
#  The IgH EtherCAT Master is free software; you can redistribute it
#  and/or modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2 of the
#  License, or (at your option) any later version.
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
#  The right to use EtherCAT Technology is granted and comes free of
#  charge under condition of compatibility of product made by
#  Licensee. People intending to distribute/sell products based on the
#  code, have to sign an agreement to guarantee that products using
#  software based on IgH EtherCAT master stay compatible with the actual
#  EtherCAT specification (which are released themselves as an open
#  standard) as the (only) precondition to have the right to use EtherCAT
#  Technology, IP and trade marks.
#
#------------------------------------------------------------------------------

MODULE := ec_msr_sample

#------------------------------------------------------------------------------
#  kbuild section
#------------------------------------------------------------------------------

ifneq ($(KERNELRELEASE),)

obj-m := $(MODULE).o

$(MODULE)-objs := msr_sample.o \
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

EXTRA_CFLAGS := -I$(src)/rt_lib/msr-include -I/usr/realtime/include \
		-D_SIMULATION -mhard-float

#------------------------------------------------------------------------------
#  default section
#------------------------------------------------------------------------------

else

ifneq ($(wildcard kernel.conf),)
include kernel.conf
else
KERNEL := $(shell uname -r)
endif

KERNELDIR := /lib/modules/$(KERNEL)/build

modules:
	$(MAKE) -C $(KERNELDIR) M=`pwd`

clean:
	$(MAKE) -C $(KERNELDIR) M=`pwd` clean

install:
	@./install.sh $(MODULE) $(KERNEL)

endif

#------------------------------------------------------------------------------
