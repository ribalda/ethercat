#!/bin/sh

#------------------------------------------------------------------------------
#
#  Realtime module install script
#
#  $Id: install.sh 5 2006-04-07 13:49:10Z fp $
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

if [ $# -ne 2 ]; then
    echo "Usage: $0 <MODULENAME> <KERNEL>"
    exit 1
fi

MODULENAME=$1
KERNEL=$2

MODULESDIR=/lib/modules/$KERNEL/kernel/drivers/rt

echo "Realtime installer"
echo "  target: $MODULENAME"
echo "  kernel: $KERNEL"

# Create target directory

if [ ! -d $MODULESDIR ]; then
    echo "  creating $MODULESDIR..."
    mkdir $MODULESDIR || exit 1
fi

# Install files

echo "  installing $MODULENAME..."
if ! cp $MODULENAME.ko $MODULESDIR/$MODULENAME.ko; then exit 1; fi

# Calculate dependencies

echo "  building module dependencies..."
depmod

# Finish

echo "done."
exit 0

#------------------------------------------------------------------------------
