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
	foe_request.c \
	reg_request.c \
	sdo_request.c \
	slave_config.c \
	voe_handler.c

noinst_HEADERS = \
	domain.h \
	ioctl.h \
	master.h \
	foe_request.h \
	reg_request.h \
	sdo_request.h \
	slave_config.h \
	voe_handler.h

libethercat_la_CFLAGS = -fno-strict-aliasing -Wall -I$(top_srcdir)

#
# http://www.gnu.org/software/libtool/manual/html_node/ ...
# Updating-version-info.html
#
# - If the library source code has changed at all since the last update, then
#   increment revision (‘c:r:a’ becomes ‘c:r+1:a’).
# - If any interfaces have been added, removed, or changed since the last
#   update, increment current, and set revision to 0.
# - If any interfaces have been added since the last public release, then
#   increment age.
# - If any interfaces have been removed or changed since the last public
#   release, then set age to 0.
#
# History
#
# 1:0.0
# 	ecrt_master_sync_reference_clock_to() added.
# 2:0:1
#
libethercat_la_LDFLAGS = -version-info 2:0:1

#------------------------------------------------------------------------------

if ENABLE_RTDM

lib_LTLIBRARIES += libethercat_rtdm.la

libethercat_rtdm_la_SOURCES = $(libethercat_la_SOURCES)
libethercat_rtdm_la_CFLAGS = $(libethercat_la_CFLAGS)
libethercat_rtdm_la_LDFLAGS = $(libethercat_la_LDFLAGS)

if ENABLE_XENOMAI
libethercat_rtdm_la_CFLAGS += $(XENOMAI_RTDM_CFLAGS)
libethercat_rtdm_la_LDFLAGS += $(XENOMAI_RTDM_LDFLAGS)
endif

if ENABLE_XENOMAI_V3
libethercat_rtdm_la_CFLAGS += $(XENOMAI_RTDM_CFLAGS)
libethercat_rtdm_la_LDFLAGS += $(XENOMAI_RTDM_LDFLAGS)
libethercat_rtdm_la_CFLAGS += -DUSE_RTDM_XENOMAI_V3
else
libethercat_rtdm_la_CFLAGS += -DUSE_RTDM
endif

if ENABLE_RTAI
libethercat_rtdm_la_CFLAGS += $(RTAI_LXRT_CFLAGS)
libethercat_rtdm_la_LDFLAGS += $(RTAI_LXRT_LDFLAGS)
endif

endif

#------------------------------------------------------------------------------
