#------------------------------------------------------------------------------
#
#  $Id$
#
#  Copyright (C) 2006-2008  Florian Pose, Ingenieurgemeinschaft IgH
#
#  This file is part of the IgH EtherCAT Master.
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
	82571-2.6.34-ethercat.c \
	82571-2.6.34-orig.c \
	defines-2.6.34-ethercat.h \
	defines-2.6.34-orig.h \
	e1000-2.6.34-ethercat.h \
	e1000-2.6.34-orig.h \
	es2lan-2.6.34-ethercat.c \
	es2lan-2.6.34-orig.c \
	ethtool-2.6.34-ethercat.c \
	ethtool-2.6.34-orig.c \
	hw-2.6.34-ethercat.h \
	hw-2.6.34-orig.h \
	ich8lan-2.6.34-ethercat.c \
	ich8lan-2.6.34-orig.c \
	lib-2.6.34-ethercat.c \
	lib-2.6.34-orig.c \
	netdev-2.6.34-ethercat.c \
	netdev-2.6.34-orig.c \
	param-2.6.34-ethercat.c \
	param-2.6.34-orig.c \
	phy-2.6.34-ethercat.c \
	phy-2.6.34-orig.c

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
