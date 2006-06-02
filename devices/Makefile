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

ifneq ($(KERNELRELEASE),)

#------------------------------------------------------------------------------
#  kbuild section

obj-m := ec_8139too.o

ec_8139too-objs := 8139too.o

REV := $(shell svnversion $(src) 2>/dev/null || echo "unknown")

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

KERNEL_DIR := /lib/modules/$(KERNEL)/build
CURRENT_DIR := $(shell pwd)

modules:
	$(MAKE) -C $(KERNEL_DIR) M=$(CURRENT_DIR)

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(CURRENT_DIR) clean

#------------------------------------------------------------------------------

endif
