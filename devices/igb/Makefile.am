#------------------------------------------------------------------------------
#
#  $Id$
#
#  Copyright (C) 2006-2017  Florian Pose, Ingenieurgemeinschaft IgH
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
#------------------------------------------------------------------------------

EXTRA_DIST = \
	e1000_82575-3.18-ethercat.c \
	e1000_82575-3.18-ethercat.h \
	e1000_82575-3.18-orig.c \
	e1000_82575-3.18-orig.h \
	e1000_82575-4.4-ethercat.c \
	e1000_82575-4.4-ethercat.h \
	e1000_82575-4.4-orig.c \
	e1000_82575-4.4-orig.h \
	e1000_defines-3.18-ethercat.h \
	e1000_defines-3.18-orig.h \
	e1000_defines-4.4-ethercat.h \
	e1000_defines-4.4-orig.h \
	e1000_hw-3.18-ethercat.h \
	e1000_hw-3.18-orig.h \
	e1000_hw-4.4-ethercat.h \
	e1000_hw-4.4-orig.h \
	e1000_i210-3.18-ethercat.c \
	e1000_i210-3.18-ethercat.h \
	e1000_i210-3.18-orig.c \
	e1000_i210-3.18-orig.h \
	e1000_i210-4.4-ethercat.c \
	e1000_i210-4.4-ethercat.h \
	e1000_i210-4.4-orig.c \
	e1000_i210-4.4-orig.h \
	e1000_mac-3.18-ethercat.c \
	e1000_mac-3.18-ethercat.h \
	e1000_mac-3.18-orig.c \
	e1000_mac-3.18-orig.h \
	e1000_mac-4.4-ethercat.c \
	e1000_mac-4.4-ethercat.h \
	e1000_mac-4.4-orig.c \
	e1000_mac-4.4-orig.h \
	e1000_mbx-3.18-ethercat.c \
	e1000_mbx-3.18-ethercat.h \
	e1000_mbx-3.18-orig.c \
	e1000_mbx-3.18-orig.h \
	e1000_mbx-4.4-ethercat.c \
	e1000_mbx-4.4-ethercat.h \
	e1000_mbx-4.4-orig.c \
	e1000_mbx-4.4-orig.h \
	e1000_nvm-3.18-ethercat.c \
	e1000_nvm-3.18-ethercat.h \
	e1000_nvm-3.18-orig.c \
	e1000_nvm-3.18-orig.h \
	e1000_nvm-4.4-ethercat.c \
	e1000_nvm-4.4-ethercat.h \
	e1000_nvm-4.4-orig.c \
	e1000_nvm-4.4-orig.h \
	e1000_phy-3.18-ethercat.c \
	e1000_phy-3.18-ethercat.h \
	e1000_phy-3.18-orig.c \
	e1000_phy-3.18-orig.h \
	e1000_phy-4.4-ethercat.c \
	e1000_phy-4.4-ethercat.h \
	e1000_phy-4.4-orig.c \
	e1000_phy-4.4-orig.h \
	e1000_regs-3.18-ethercat.h \
	e1000_regs-3.18-orig.h \
	e1000_regs-4.4-ethercat.h \
	e1000_regs-4.4-orig.h \
	igb-3.18-ethercat.h \
	igb-3.18-orig.h \
	igb-4.4-ethercat.h \
	igb-4.4-orig.h \
	igb_ethtool-3.18-ethercat.c \
	igb_ethtool-3.18-orig.c \
	igb_ethtool-4.4-ethercat.c \
	igb_ethtool-4.4-orig.c \
	igb_hwmon-3.18-ethercat.c \
	igb_hwmon-3.18-orig.c \
	igb_hwmon-4.4-ethercat.c \
	igb_hwmon-4.4-orig.c \
	igb_main-3.18-ethercat.c \
	igb_main-3.18-orig.c \
	igb_main-4.4-ethercat.c \
	igb_main-4.4-orig.c \
	igb_ptp-3.18-ethercat.c \
	igb_ptp-3.18-orig.c \
	igb_ptp-4.4-ethercat.c \
	igb_ptp-4.4-orig.c \
	update.sh

BUILT_SOURCES = \
	Kbuild

modules:
	$(MAKE) -C "$(LINUX_SOURCE_DIR)" M="@abs_srcdir@" modules

modules_install:
	$(MAKE) -C "$(LINUX_SOURCE_DIR)" M="@abs_srcdir@" \
		INSTALL_MOD_DIR="$(INSTALL_MOD_DIR)" modules_install

clean-local:
	$(MAKE) -C "$(LINUX_SOURCE_DIR)" M="@abs_srcdir@" clean

#------------------------------------------------------------------------------
