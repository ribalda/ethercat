#------------------------------------------------------------------------------
#
#  EtherCAT Makefile
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

obj-m := master/ devices/

#------------------------------------------------------------------------------

else

#------------------------------------------------------------------------------
#  default section

ifneq ($(wildcard ethercat.conf),)
include ethercat.conf
else
KERNEL := `uname -r`
DEVICEINDEX := 99
endif

KERNELDIR := /lib/modules/$(KERNEL)/build

modules:
	$(MAKE) -C $(KERNELDIR) M=`pwd`

clean: cleandoc
	$(MAKE) -C $(KERNELDIR) M=`pwd` clean

doc:
	doxygen Doxyfile

cleandoc:
	@rm -rf doc


install:
	@./install.sh $(KERNEL) $(DEVICEINDEX)

#------------------------------------------------------------------------------

endif
