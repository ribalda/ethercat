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

bin_PROGRAMS = ethercat

ethercat_SOURCES = \
    MasterDevice.cpp MasterDevice.h \
	coe_datatypes.cpp \
	sii_crc.cpp \
    cmd_alias.cpp \
    cmd_config.cpp \
    cmd_data.cpp \
    cmd_debug.cpp \
	cmd_domain.cpp \
	cmd_master.cpp \
	cmd_pdos.cpp \
	cmd_sdos.cpp \
	cmd_download.cpp \
	cmd_upload.cpp \
	cmd_slaves.cpp \
	cmd_sii_read.cpp \
	cmd_sii_write.cpp \
	cmd_states.cpp \
	cmd_xml.cpp \
    main.cpp

ethercat_CXXFLAGS = -I../master -Wall

#------------------------------------------------------------------------------
