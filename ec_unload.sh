#!/bin/sh
###############################################################################
#
#  Shell-Script zum Entladen des EtherCAT-Masters
#
#  $Id$
#
###############################################################################

remove_module()
{
    if lsmod | grep ^$1 > /dev/null
	then
	echo "Entlade Modul \"$1\"..."
	rmmod $1 || exit -1
    fi
}

insert_module()
{
    name=`basename $1`
    echo "Lade Modul \"$name\"..."
    if ! modprobe $*
	then
	echo "Fehler beim Laden!"
	exit -1
    fi
}

###############################################################################

echo "Entlade EtherCAT..."

remove_module ec_8139too
remove_module ec_master

insert_module 8139too

echo "EtherCAT entladen."
exit 0

###############################################################################
