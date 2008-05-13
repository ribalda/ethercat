#------------------------------------------------------------------------------
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

SUBDIRS = e1000

DIST_SUBDIRS = e1000

EXTRA_DIST = \
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
	Kbuild.in \
	e100-2.6.18-ethercat.c \
	e100-2.6.18-orig.c \
	ecdev.h \
	forcedeth-2.6.17-ethercat.c \
	forcedeth-2.6.17-orig.c \
	forcedeth-2.6.19-ethercat.c \
	forcedeth-2.6.19-orig.c

modules:
	$(MAKE) -C "@abs_top_srcdir@" modules

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
if ENABLE_FORCEDETH
	cp $(srcdir)/ec_forcedeth.ko $(DESTDIR)$(LINUX_MOD_PATH)
endif
if ENABLE_R8169
	cp $(srcdir)/ec_r8169.ko $(DESTDIR)$(LINUX_MOD_PATH)
endif

clean-local:
	$(MAKE) -C "$(LINUX_SOURCE_DIR)" M="@abs_srcdir@" clean

#------------------------------------------------------------------------------
