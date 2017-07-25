#------------------------------------------------------------------------------
#
#  Copyright (C) 2013  Graeme Foot, Kinetic Engineering Design Ltd.
#                                   <graeme@touchcut.com>
#
#  This file is subject to the terms and conditions of the GNU General Public
#  License version 2, as published by the Free Software Foundation.
# 
#  This file is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
#  Public License for more details.
#
#  This file has been added to the IgH EtherCAT Master.
#
#  The IgH EtherCAT Master is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License version 2, as
#  published by the Free Software Foundation.
#
#  The IgH EtherCAT Master is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
#  Public License for more details.
#
#  You should have received a copy of the GNU General Public License along
#  with the IgH EtherCAT Master; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
#  ---
#
#  The license mentioned above concerns the source code only. Using the
#  EtherCAT technology and brand is only permitted in compliance with the
#  industrial property and similar rights of Beckhoff Automation GmbH.
#
#------------------------------------------------------------------------------

EXTRA_DIST = \
	Kbuild.in \
	LICENSE \
	cx2100-2.6.32-ethercat.c \
	cx2100-4.9-ethercat.c

BUILT_SOURCES = \
	Kbuild

modules:
	$(MAKE) -C "$(LINUX_SOURCE_DIR)" M="@abs_srcdir@" modules

modules_install:
	$(MAKE) -C "$(LINUX_SOURCE_DIR)" M="@abs_srcdir@" \
		INSTALL_MOD_DIR="$(INSTALL_MOD_DIR)" modules_install

clean-local:
	$(MAKE) -C "$(LINUX_SOURCE_DIR)" M="@abs_srcdir@" clean

#------------------------------------------------------------------------------
