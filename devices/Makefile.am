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

SUBDIRS = \
	e1000 \
	e1000e

DIST_SUBDIRS = \
	e1000 \
	e1000e

# using HEADERS to enable tags target
noinst_HEADERS = \
	8139too-2.6.13-ethercat.c \
	8139too-2.6.13-orig.c \
	8139too-2.6.17-ethercat.c \
	8139too-2.6.17-orig.c \
	8139too-2.6.18-ethercat.c \
	8139too-2.6.18-orig.c \
	8139too-2.6.19-ethercat.c \
	8139too-2.6.19-orig.c \
	8139too-2.6.22-ethercat.c \
	8139too-2.6.22-orig.c \
	8139too-2.6.23-ethercat.c \
	8139too-2.6.23-orig.c \
	8139too-2.6.24-ethercat.c \
	8139too-2.6.24-orig.c \
	8139too-2.6.25-ethercat.c \
	8139too-2.6.25-orig.c \
	8139too-2.6.26-ethercat.c \
	8139too-2.6.26-orig.c \
	8139too-2.6.27-ethercat.c \
	8139too-2.6.27-orig.c \
	8139too-2.6.28-ethercat.c \
	8139too-2.6.28-orig.c \
	8139too-2.6.29-ethercat.c \
	8139too-2.6.29-orig.c \
	8139too-2.6.31-ethercat.c \
	8139too-2.6.31-orig.c \
	8139too-2.6.32-ethercat.c \
	8139too-2.6.32-orig.c \
	8139too-2.6.33-ethercat.c \
	8139too-2.6.33-orig.c \
	8139too-2.6.34-ethercat.c \
	8139too-2.6.34-orig.c \
	8139too-2.6.35-ethercat.c \
	8139too-2.6.35-orig.c \
	8139too-2.6.36-ethercat.c \
	8139too-2.6.36-orig.c \
	8139too-2.6.37-ethercat.c \
	8139too-2.6.37-orig.c \
	e100-2.6.20-ethercat.c \
	e100-2.6.20-orig.c \
	e100-2.6.24-ethercat.c \
	e100-2.6.24-orig.c \
	e100-2.6.26-ethercat.c \
	e100-2.6.26-orig.c \
	e100-2.6.27-ethercat.c \
	e100-2.6.27-orig.c \
	e100-2.6.28-ethercat.c \
	e100-2.6.28-orig.c \
	e100-2.6.29-ethercat.c \
	e100-2.6.29-orig.c \
	e100-2.6.31-ethercat.c \
	e100-2.6.31-orig.c \
	e100-2.6.32-ethercat.c \
	e100-2.6.32-orig.c \
	e100-2.6.33-ethercat.c \
	e100-2.6.33-orig.c \
	e100-2.6.35-ethercat.c \
	e100-2.6.35-orig.c \
	e100-2.6.37-ethercat.c \
	e100-2.6.37-orig.c \
	ecdev.h \
	generic.c \
	r8169-2.6.24-ethercat.c \
	r8169-2.6.24-orig.c \
	r8169-2.6.27-ethercat.c \
	r8169-2.6.27-orig.c \
	r8169-2.6.28-ethercat.c \
	r8169-2.6.28-orig.c \
	r8169-2.6.29-ethercat.c \
	r8169-2.6.29-orig.c \
	r8169-2.6.31-ethercat.c \
	r8169-2.6.31-orig.c \
	r8169-2.6.32-ethercat.c \
	r8169-2.6.32-orig.c \
	r8169-2.6.33-ethercat.c \
	r8169-2.6.33-orig.c \
	r8169-2.6.35-ethercat.c \
	r8169-2.6.35-orig.c \
	r8169-2.6.37-ethercat.c \
	r8169-2.6.37-orig.c

EXTRA_DIST = \
	Kbuild.in
	
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
