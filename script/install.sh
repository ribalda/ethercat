#!/bin/sh

#------------------------------------------------------------------------------
#
#  EtherCAT install script
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

# Fetch parameters

if [ $# -ne 1 ]; then
    echo "This script is called by \"make\". Run \"make install\" instead."
    exit 1
fi

KERNEL=$1

if [ ! -d /lib/modules/$KERNEL ]; then
    echo "Kernel \"$KERNEL\" does not exist in /lib/modules!"
    exit 1
fi

#------------------------------------------------------------------------------

# Copy files

INSTALLDIR=/lib/modules/$KERNEL/kernel/drivers/net
MODULES=(master/ec_master.ko devices/ec_8139too.ko)

echo "EtherCAT installer - Kernel: $KERNEL"
echo "  Installing modules"

for mod in ${MODULES[*]}; do
    echo "    $mod"
    cp $mod $INSTALLDIR || exit 1
done

#------------------------------------------------------------------------------

# Update dependencies

echo "  Building module dependencies"
depmod

#------------------------------------------------------------------------------

# Create configuration file

CONFIGFILE=/etc/sysconfig/ethercat

if [ -s $CONFIGFILE ]; then
    echo "  Note: Using existing configuration file."
else
    echo "  Creating $CONFIGFILE"
    cp script/sysconfig $CONFIGFILE || exit 1
    echo "  Note: Please edit DEVICE_INDEX in $CONFIGFILE!"
fi

#------------------------------------------------------------------------------

# Install rc script

echo "  Installing startup script"
cp script/ethercat.sh /etc/init.d/ethercat || exit 1
chmod +x /etc/init.d/ethercat || exit 1
if [ ! -L /usr/sbin/rcethercat ]; then
    ln -s /etc/init.d/ethercat /usr/sbin/rcethercat || exit 1
fi

#------------------------------------------------------------------------------

# Install tools

echo "  Installing tools"
cp script/ec_list.pl /usr/local/bin/ec_list || exit 1
chmod +x /usr/local/bin/ec_list || exit 1

#------------------------------------------------------------------------------

# Finish

echo "Done"
exit 0

#------------------------------------------------------------------------------
