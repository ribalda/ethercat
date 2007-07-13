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

EXTRA_DIST = \
	Kbuild \
	e1000_ethtool-2.6.18-ethercat.c \
	e1000_ethtool-2.6.18-orig.c \
	e1000.h \
	e1000_hw-2.6.18-ethercat.c \
	e1000_hw-2.6.18-orig.c \
	e1000_hw.h \
	e1000_main-2.6.18-ethercat.c \
	e1000_main-2.6.18-orig.c \
	e1000_osdep.h \
	e1000_param-2.6.18-ethercat.c \
	e1000_param-2.6.18-orig.c \
	LICENSE

modules:
	$(MAKE) -C "$(LINUX_SOURCE_DIR)" M="@abs_top_srcdir@" modules

modules_install:
	mkdir -p $(DESTDIR)$(LINUX_MOD_PATH)
if ENABLE_E1000
	cp $(srcdir)/ec_e1000.ko $(DESTDIR)$(LINUX_MOD_PATH)
endif

clean-local:
	$(MAKE) -C "$(LINUX_SOURCE_DIR)" M="@abs_srcdir@" clean

#------------------------------------------------------------------------------
