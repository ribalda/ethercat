#------------------------------------------------------------------------------
#
#  $Id$
#
#  Copyright (C) 2019  Florian Pose, Ingenieurgemeinschaft IgH
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
#  ---
#
#  vim: syntax=automake
#
#------------------------------------------------------------------------------

EXTRA_DIST =

bin_PROGRAMS = ethercat_mbg

ethercat_mbg_SOURCES = \
	CommandMbg.cpp \
	MasterDevice.cpp \
	NumberListParser.cpp \
	main.cpp

noinst_HEADERS = \
	CommandMbg.h \
	MasterDevice.h \
	NumberListParser.h

REV = `if test -s $(top_srcdir)/revision; then \
		cat $(top_srcdir)/revision; \
	else \
		hg id -i $(top_srcdir) 2>/dev/null || echo "unknown"; \
	fi`

ethercat_mbg_CXXFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/master \
	-Wall -DREV=$(REV) \
	-fno-strict-aliasing

#------------------------------------------------------------------------------
