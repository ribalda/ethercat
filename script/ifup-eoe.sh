#!/bin/bash

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
