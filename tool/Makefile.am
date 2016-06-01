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
#  vim: syntax=automake
#
#------------------------------------------------------------------------------

EXTRA_DIST =

bin_PROGRAMS = ethercat

ethercat_SOURCES = \
	../master/soe_errors.c \
	Command.cpp \
	CommandAlias.cpp \
	CommandCrc.cpp \
	CommandCStruct.cpp \
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
	CommandRegRead.cpp \
	CommandRegWrite.cpp \
	CommandRegReadWrite.cpp \
	CommandReboot.cpp \
	CommandRescan.cpp \
	CommandSdos.cpp \
	CommandSiiRead.cpp \
	CommandSiiWrite.cpp \
	CommandSlaves.cpp \
	CommandSoeRead.cpp \
	CommandSoeWrite.cpp \
	CommandStates.cpp \
	CommandUpload.cpp \
	CommandVersion.cpp \
	CommandXml.cpp \
	DataTypeHandler.cpp \
	FoeCommand.cpp \
	MasterDevice.cpp \
	NumberListParser.cpp \
	SdoCommand.cpp \
	SoeCommand.cpp \
	main.cpp \
	sii_crc.cpp

noinst_HEADERS = \
	Command.h \
	CommandAlias.h \
	CommandCrc.h \
	CommandCStruct.h \
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
	CommandRegRead.h \
	CommandRegWrite.h \
	CommandRegReadWrite.h \
	CommandReboot.h \
	CommandRescan.h \
	CommandSdos.h \
	CommandSiiRead.h \
	CommandSiiWrite.h \
	CommandSlaves.h \
	CommandSoeRead.h \
	CommandSoeWrite.h \
	CommandStates.h \
	CommandUpload.h \
	CommandVersion.h \
	CommandXml.h \
	DataTypeHandler.h \
	FoeCommand.h \
	MasterDevice.h \
	NumberListParser.h \
	SdoCommand.h \
	SoeCommand.h \
	sii_crc.h

if ENABLE_EOE
ethercat_SOURCES += \
	CommandEoe.cpp \
	CommandIp.cpp
noinst_HEADERS += \
	CommandEoe.h \
	CommandIp.h
else
EXTRA_DIST += \
	CommandEoe.cpp \
	CommandIp.cpp \
	CommandEoe.h \
	CommandIp.h
endif

REV = `if test -s $(top_srcdir)/revision; then \
		cat $(top_srcdir)/revision; \
	else \
		hg id -i $(top_srcdir) 2>/dev/null || echo "unknown"; \
	fi`

ethercat_CXXFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/master \
	-Wall -DREV=$(REV) \
	-fno-strict-aliasing

#------------------------------------------------------------------------------
