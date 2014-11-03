#!/bin/bash

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
#  The license mentioned above concerns the source code only. Using the EtherCAT
#  technology and brand is only permitted in compliance with the industrial
#  property and similar rights of Beckhoff Automation GmbH.
#
#  vim: expandtab
#
#------------------------------------------------------------------------------

# this ifup.d script adds special network interfaces to a network bridge

CFGNAME=${1}
IFNAME=${2}

# customize here
BRNAME="eoebr0"
INTERFACES=""
BRCTL="/sbin/brctl"
LOGGER="logger -t ifup-eoe"

# if the current interface in the list of interfaces to bridge?
if ! echo ${INTERFACES} | grep -qw ${IFNAME}; then
    exit 0;
fi

# does the EoE bridge already exist?
if ! ${BRCTL} show | grep -q "^${BRNAME}"; then
    ${LOGGER} Creating ${BRNAME}
    ${BRCTL} addbr ${BRNAME} # create it
fi

${LOGGER} Adding ${IFNAME} to ${BRNAME}
ip link set ${IFNAME} down
ip addr flush dev ${IFNAME}
${BRCTL} addif ${BRNAME} ${IFNAME}
ip link set ${IFNAME} up

#------------------------------------------------------------------------------
