#------------------------------------------------------------------------------
#
#  $Id: Makefile 545 2006-09-19 13:28:40Z fp $
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
	canopen.c canopen.h \
	datagram.c datagram.h \
	debug.c	debug.h \
	device.c device.h \
	domain.c domain.h \
	doxygen.c \
	ethernet.c ethernet.h \
	fsm_sii.c fsm_sii.h \
	fsm_change.c fsm_change.h \
	fsm_coe.c fsm_coe.h \
	fsm.c fsm.h \
	globals.h \
	mailbox.c mailbox.h \
	master.c master.h \
	module.c \
	slave.c slave.h \
	xmldev.c xmldev.h

all-local:
	$(MAKE) -C "$(LINUX_SOURCE_DIR)" \
		M="@abs_srcdir@" EC_DBG_IF="$(EC_DBG_IF)" modules

clean-local:
	$(MAKE) -C "$(LINUX_SOURCE_DIR)" M="@abs_srcdir@" clean

install-data-local:
	$(MAKE) -C "$(LINUX_SOURCE_DIR)" M="@abs_srcdir@" \
		INSTALL_MOD_PATH="$(DESTDIR)" \
		INSTALL_MOD_DIR="ethercat" modules_install

#------------------------------------------------------------------------------
