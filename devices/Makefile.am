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

SUBDIRS = e1000

DIST_SUBDIRS = e1000

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
	e100-2.6.20-ethercat.c \
	e100-2.6.20-orig.c \
	e100-2.6.24-ethercat.c \
	e100-2.6.24-orig.c \
	e100-2.6.26-ethercat.c \
	e100-2.6.26-orig.c \
	ecdev.h \
	r8169-2.6.24-ethercat.c \
	r8169-2.6.24-orig.c

EXTRA_DIST = \
	Kbuild.in
	
BUILT_SOURCES = \
	Kbuild

modules:
	$(MAKE) -C "$(LINUX_SOURCE_DIR)" M="@abs_srcdir@" modules

modules_install:
	mkdir -p $(DESTDIR)$(LINUX_MOD_PATH)
if ENABLE_8139TOO
	cp $(srcdir)/ec_8139too.ko $(DESTDIR)$(LINUX_MOD_PATH)
endif
if ENABLE_E100
	cp $(srcdir)/ec_e100.ko $(DESTDIR)$(LINUX_MOD_PATH)
endif
if ENABLE_E1000
	$(MAKE) -C e1000 modules_install
endif
if ENABLE_R8169
	cp $(srcdir)/ec_r8169.ko $(DESTDIR)$(LINUX_MOD_PATH)
endif

clean-local:
	$(MAKE) -C "$(LINUX_SOURCE_DIR)" M="@abs_srcdir@" clean

#------------------------------------------------------------------------------
