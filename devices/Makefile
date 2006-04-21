#------------------------------------------------------------------------------
#
#  kbuild Makefile
#
#  IgH EtherCAT master device modules
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

obj-m := ec_8139too.o

ec_8139too-objs := 8139too.o

REV := $(shell svnversion $(src) 2>/dev/null)

EXTRA_CFLAGS = -DEC_REV=$(REV) -DEC_USER=$(USER)

#------------------------------------------------------------------------------

else

#------------------------------------------------------------------------------
#  default section

ifneq ($(wildcard ../ethercat.conf),)
include ../ethercat.conf
else
KERNEL := $(shell uname -r)
endif

KERNELDIR := /lib/modules/$(KERNEL)/build

modules:
	$(MAKE) -C $(KERNELDIR) M=`pwd`

clean:
	$(MAKE) -C $(KERNELDIR) M=`pwd` clean

#------------------------------------------------------------------------------

endif
