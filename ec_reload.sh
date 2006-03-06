#!/bin/sh
###############################################################################
#
#  Shell-Script zum Laden des EtherCAT-Masters
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

copy_to_tmp()
{
    if ! cp $1 /tmp/
	then
	echo "Fehler beim Kopieren von $1 nach /tmp..."
	exit -1
    fi
}

insert_module()
{
    name=`basename $1`
    echo "Lade Modul \"$name\"..."
    if ! insmod $*
	then
	echo "Fehler beim Laden!"
	exit -1
    fi
}

# Parameter abfragen
if [ $# -eq 0 ]
then
echo "$0: Parameter <ec_device_index> fehlt!"
exit 1
fi

echo "Lade EtherCAT..."

# Aktuelle Versionen nach /tmp kopieren...
copy_to_tmp master/ec_master.ko
copy_to_tmp devices/ec_8139too.ko

# Module entfernen...
remove_module 8139too
remove_module 8139cp
remove_module ec_8139too
remove_module ec_master

# Neue Versionen laden
insert_module /tmp/ec_master.ko
insert_module /tmp/ec_8139too.ko ec_device_index=$1

echo "EtherCAT neu geladen."

exit 0