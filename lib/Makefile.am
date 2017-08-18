#------------------------------------------------------------------------------
#
#  $Id$
#
#  Copyright (C) 2006-2012  Florian Pose, Ingenieurgemeinschaft IgH
#
#  This file is part of the IgH EtherCAT master userspace library.
#
#  The IgH EtherCAT master userspace library is free software; you can
#  redistribute it and/or modify it under the terms of the GNU Lesser General
#  Public License as published by the Free Software Foundation; version 2.1 of
#  the License.
#
#  The IgH EtherCAT master userspace library is distributed in the hope that
#  it will be useful, but WITHOUT ANY WARRANTY; without even the implied
#  warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public License
#  along with the IgH EtherCAT master userspace library. If not, see
#  <http://www.gnu.org/licenses/>.
#
#  ---
#
#  The license mentioned above concerns the source code only. Using the
#  EtherCAT technology and brand is only permitted in compliance with the
#  industrial property and similar rights of Beckhoff Automation GmbH.
#
#------------------------------------------------------------------------------

lib_LTLIBRARIES = libethercat.la

#------------------------------------------------------------------------------

libethercat_la_SOURCES = \
	common.c \
	domain.c \
	master.c \
	reg_request.c \
	sdo_request.c \
	slave_config.c \
	voe_handler.c

noinst_HEADERS = \
	domain.h \
	ioctl.h \
	master.h \
	reg_request.h \
	sdo_request.h \
	slave_config.h \
	voe_handler.h

libethercat_la_CFLAGS = -fno-strict-aliasing -Wall -I$(top_srcdir)
libethercat_la_LDFLAGS = -version-info 1:0:0

#------------------------------------------------------------------------------

if ENABLE_RTDM

lib_LTLIBRARIES += libethercat_rtdm.la

libethercat_rtdm_la_SOURCES = $(libethercat_la_SOURCES)
libethercat_rtdm_la_CFLAGS = $(libethercat_la_CFLAGS) -DUSE_RTDM
libethercat_rtdm_la_LDFLAGS = $(libethercat_la_LDFLAGS)

if ENABLE_XENOMAI
libethercat_rtdm_la_CFLAGS += $(XENOMAI_RTDM_CFLAGS)
libethercat_rtdm_la_LDFLAGS += $(XENOMAI_RTDM_LDFLAGS)
endif

if ENABLE_RTAI
libethercat_rtdm_la_CFLAGS += $(RTAI_LXRT_CFLAGS)
libethercat_rtdm_la_LDFLAGS += $(RTAI_LXRT_LDFLAGS)
endif

endif

#------------------------------------------------------------------------------
