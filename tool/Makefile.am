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
#  ---
#
#  vim: syntax=make
#
#------------------------------------------------------------------------------

bin_PROGRAMS = ethercat

ethercat_SOURCES = \
	Command.cpp \
	CommandAlias.cpp \
	CommandConfig.cpp \
	CommandData.cpp \
	CommandDebug.cpp \
	CommandDomains.cpp \
	CommandDownload.cpp \
	CommandFoeRead.cpp \
	CommandFoeWrite.cpp \
	CommandGraph.cpp \
	CommandMaster.cpp \
	CommandPdos.cpp \
	CommandReg.cpp \
	CommandRegRead.cpp \
	CommandRegWrite.cpp \
	CommandSdos.cpp \
	CommandSiiRead.cpp \
	CommandSiiWrite.cpp \
	CommandSlaves.cpp \
	CommandStates.cpp \
	CommandUpload.cpp \
	CommandVersion.cpp \
	CommandXml.cpp \
	FoeCommand.cpp \
	MasterDevice.cpp \
	SdoCommand.cpp \
	main.cpp \
	sii_crc.cpp

noinst_HEADERS = \
	Command.h \
	CommandAlias.h \
	CommandConfig.h \
	CommandData.h \
	CommandDebug.h \
	CommandDomains.h \
	CommandDownload.h \
	CommandFoeRead.h \
	CommandFoeWrite.h \
	CommandGraph.h \
	CommandMaster.h \
	CommandPdos.h \
	CommandReg.h \
	CommandRegRead.h \
	CommandRegWrite.h \
	CommandSdos.h \
	CommandSiiRead.h \
	CommandSiiWrite.h \
	CommandSlaves.h \
	CommandStates.h \
	CommandUpload.h \
	CommandVersion.h \
	CommandXml.h \
	FoeCommand.h \
	MasterDevice.h \
	SdoCommand.h \
	sii_crc.h

REV = `if test -s $(top_srcdir)/svnrevision; then \
		cat $(top_srcdir)/svnrevision; \
	else \
		svnversion $(srcdir)/.. 2>/dev/null || echo "unknown"; \
	fi`

ethercat_CXXFLAGS = -I$(top_srcdir)/master -Wall -DSVNREV=$(REV)

#------------------------------------------------------------------------------
