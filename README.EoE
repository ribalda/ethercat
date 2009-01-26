-------------------------------------------------------------------------------

$Id$

vim: spelllang=en spell tw=78

-------------------------------------------------------------------------------

This file shall give additional information on how to set up a network
environment with Ethernet over EtherCAT devices.

A virtual network interface will appear for every EoE-capable slave. The
interface naming scheme is either eoeXsY, where X is the master index and Y is
the slave's ring position, or (if the slave has an alias set) eoeXaZ, where Z
is the (decimal) alias address. Please provide a network configuration file for
any of these interfaces. On SUSE systems, these can look like the following:

/etc/sysconfig/network/ifcfg-eoe0s14:
IPADDRESS=10.0.0.14/8
STARTMODE=auto

This will tell the ifupd to configure and open the device after it appears.

If the interfaces shall be part of a network bridge, the IPADDRESS line is not
necessary. Instead, copy the file script/ifup-eoe.sh to your systems if-up.d
directory (on SUSE, this is /etc/sysconfig/network/if-up.d), and customize the
included variables.

-------------------------------------------------------------------------------
