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

# HEADERS, because of tags target
nodist_noinst_HEADERS = \
	Kbuild.in \
	cdev.c cdev.h \
	datagram.c datagram.h \
	debug.c	debug.h \
	device.c device.h \
	domain.c domain.h \
	doxygen.c \
	ethernet.c ethernet.h \
	fmmu_config.c fmmu_config.h \
	fsm_change.c fsm_change.h \
	fsm_coe.c fsm_coe.h \
	fsm_master.c fsm_master.h \
	fsm_pdo.c fsm_pdo.h \
	fsm_pdo_entry.c fsm_pdo_entry.h \
	fsm_sii.c fsm_sii.h \
	fsm_slave_config.c fsm_slave_config.h \
	fsm_slave_scan.c fsm_slave_scan.h \
	globals.h \
	ioctl.h \
	mailbox.c mailbox.h \
	master.c master.h \
	module.c \
	pdo.c pdo.h \
	pdo_entry.c pdo_entry.h \
	pdo_list.c pdo_list.h \
	sdo.c sdo.h \
	sdo_entry.c sdo_entry.h \
	sdo_request.c sdo_request.h \
	slave.c slave.h \
	slave_config.c slave_config.h \
	sync.c sync.h \
	sync_config.c sync_config.h

BUILT_SOURCES = \
	Kbuild

modules:
	$(MAKE) -C "@abs_top_srcdir@" modules

modules_install:
	mkdir -p $(DESTDIR)$(LINUX_MOD_PATH)
	cp $(srcdir)/ec_master.ko $(DESTDIR)$(LINUX_MOD_PATH)

clean-local:
	$(MAKE) -C "$(LINUX_SOURCE_DIR)" M="@abs_srcdir@" clean

#------------------------------------------------------------------------------
