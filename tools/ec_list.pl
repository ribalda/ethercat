#!/usr/bin/perl

#------------------------------------------------------------------------------
#
#  e c _ l i s t . p l
#
#  Userspace tool for listing EtherCAT slaves.
#
#  $Id: slave.c 340 2006-04-11 10:17:30Z fp $
#
#  Copyright (C) 2006  Florian Pose, Ingenieurgemeinschaft IgH
#
#  This file is part of the IgH EtherCAT Master.
#
#  The IgH EtherCAT Master is free software; you can redistribute it
#  and/or modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; version 2 of the License.
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
#------------------------------------------------------------------------------

use strict;
use Getopt::Std;

my $master_index;
my $master_dir;

#------------------------------------------------------------------------------

&get_options;
&query_master;
exit 0;

#------------------------------------------------------------------------------

sub query_master
{
    $master_dir = "/sys/ethercat" . $master_index;
    &query_slaves;
}

#------------------------------------------------------------------------------

sub query_slaves
{
    my $dirhandle;
    my $slave_dir;
    my $entry;
    my $slave_index;
    my $file_name;
    my $vendor_name;
    my @slaves;
    my $slave;
    my $abs;

    unless (opendir $dirhandle, $master_dir) {
	print "Failed to open directory \"$master_dir\".\n";
	exit 1;
    }

    while ($entry = readdir $dirhandle) {
        next unless $entry =~ /^slave(\d+)$/;
	$slave_dir = $master_dir . "/" . $entry;

	$slave = {};
	$slave->{'ring_position'} =
	    &read_integer("$slave_dir/ring_position");
	$slave->{'coupler_address'} =
	    &read_string("$slave_dir/coupler_address");
	$slave->{'vendor_name'} =
	    &read_string("$slave_dir/vendor_name");
	$slave->{'product_name'} =
	    &read_string("$slave_dir/product_name");
	$slave->{'product_desc'} =
	    &read_string("$slave_dir/product_desc");
	$slave->{'type'} =
	    &read_string("$slave_dir/type");

	push @slaves, $slave;
    }
    closedir $dirhandle;

    @slaves = sort { $a->{'ring_position'} <=> $b->{'ring_position'} } @slaves;

    print "EtherCAT bus listing for master $master_index:\n";
    for $slave (@slaves) {
	if ($slave->{'type'} eq "coupler") {
	    print "--------------------------------------------------------\n";
	}

	$abs = sprintf "%i", $slave->{'ring_position'};
	printf(" %3s %8s   %-12s %-10s %s\n", $abs,
	       $slave->{'coupler_address'}, $slave->{'vendor_name'},
	       $slave->{'product_name'}, $slave->{'product_desc'});
    }
}

#------------------------------------------------------------------------------

sub read_string
{
    (my $file_name) = @_;
    my $data;

    $data = `cat $file_name 2>/dev/null`;
    if ($?) {
	print "ERROR: Unable to read string $file_name!\n";
	exit 1;
    }

    chomp $data;
    return $data;
}

#------------------------------------------------------------------------------

sub read_integer
{
    (my $file_name) = @_;

    if (`cat $file_name 2>/dev/null` !~ /^(\d+)$/) {
	print "ERROR: Unable to read integer $file_name!\n";
	exit 1;
    }

    return int $1;
}

#------------------------------------------------------------------------------

sub get_options
{
    my %opt;
    my $optret;

    $optret = getopts "m:h", \%opt;

    &print_usage if defined $opt{'h'} or $#ARGV > -1;

    if (defined $opt{'m'}) {
	$master_index = $opt{'m'};
    }
    else {
	$master_index = 0;
    }
}

#------------------------------------------------------------------------------

sub print_usage
{
    print "Usage: ec_list [OPTIONS]\n";
    print "        -m <IDX>    Query master IDX.\n";
    print "        -h          Show this help.\n";
    exit 0;
}

#------------------------------------------------------------------------------
