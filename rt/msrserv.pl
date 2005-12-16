#!/usr/bin/perl -w
#------------------------------------------------------------
#
# (C) Copyright
#     Diese Software ist geistiges Eigentum der 
#     Ingenieurgemeinschaft IgH. Sie darf von 
#     Toyota Motorsport GmbH
#     beliebig kopiert und veraendert werden. 
#     Die Weitergabe an Dritte ist untersagt.
#     Dieser Urhebrrechtshinweis muss erhalten
#     bleiben.
#
#     Ingenieurgemeinschaft IgH
#     Heinz-Baecker-Strasse 34
#     D-45356 Essen
#     Tel.:  +49-201/61 99 31
#     Fax.:  +49-201/61 98 36
#     WWW:   http://www.igh-essen.com
#     Email: msr@igh-essen.com
#
#------------------------------------------------------------
#
# Multithreaded Server 
# according to the example from "Programming Perl"
# this code is improved according to the example from 
# perldoc perlipc, so now safely being usable under Perl 5.8 
# (see note (*))
#
# works with read/write on a device-file  
#
# $Revision: 1.1 $
# $Date: 2004/10/01 16:00:42 $
# $RCSfile: msrserv.pl,v $
#
#------------------------------------------------------------

require 5.002;
use strict;
BEGIN { $ENV{PATH} = '/opt/msr/bin:/usr/bin:/bin' }
use Socket;
use Carp;
use FileHandle;
use Getopt::Std; 

use Sys::Syslog qw(:DEFAULT setlogsock); 

use vars qw (
	     $self $pid $dolog $port $dev %opts $selfbase
	     $len $offset $stream $written $read $log $blksize
	     $instdir
	     $authfile %authhosts
	     );


# Do logging to local syslogd by unix-domain socket instead of inetd
setlogsock("unix");  

# Prototypes and some little Tools
sub spawn;
sub logmsg { 
  my ($level, $debug, @text) = @_;
  syslog("daemon|$level", @text) if $debug > $dolog;
#  print STDERR "daemon|$level", @text, "\n" if $dolog;
}
sub out {
  my $waitpid = wait; 
  logmsg("notice", 2, "$waitpid exited");
  unlink "$selfbase.pid";
  exit 0;
}

sub help {
  print "\n  usage: $0 [-l og] [-h elp] [-p port] [-d device]\n"; 
  exit; 
}

# Process Options
%opts = (
	 "l" => 1,
	 "h" => 0,
	 "p" => 2345,
	 "d" => "/dev/msr"
	 );
  
getopts("lhp:d:", \%opts);

help if $opts{"h"};

( $self =  $0 ) =~ s+.*/++ ;
( $selfbase = $self ) =~ s/\..*//;
$log = "$selfbase.log";
$dolog = $opts{"l"};
$port = $opts{"p"};
$dev = $opts{"d"};
$blksize = 1024; # try to write as much bytes
$instdir = "/opt/msr";
$authfile = "$instdir/etc/hosts.auth"; 

# Start logging
openlog($self, 'pid');

# Flush Output, dont buffer
$| = 1;

# first fork and run in background
if ($pid = fork) {
#  open LOG, ">$log" if $dolog;
#  close LOG;
  logmsg("notice", 2, "forked process: $pid\n");
  exit 0;
}

# Server tells about startup success
open (PID, ">/$instdir/var/run/$selfbase.pid");
print PID "$$\n";
close PID;

# Cleanup on exit (due to kill -TERM signal)
$SIG{TERM} = \&out;

# We use streams
my $proto = getprotobyname('tcp');

# Open Server socket
socket(Server, PF_INET, SOCK_STREAM, $proto) or die "socket: $!";
setsockopt(Server, SOL_SOCKET, SO_REUSEADDR, pack("l", 1))
  or die "setsocketopt: $!";
bind (Server, sockaddr_in($port, INADDR_ANY))
  or die "bind: $!";
listen (Server, SOMAXCONN) 
  or die "listen: $!";

%authhosts = ();
# get authorized hosts
open (AUTH, $authfile) 
  or logmsg ("notice", 2, "Could not read allowed hosts file: $authfile");
while (<AUTH>) {
    chomp;
    my $host = lc $_;
     if ($host =~ /^[\d\w]/) {
	 $authhosts{$_} = 1;
	 logmsg ("notice", 2, "Authorized host: >$host<");
     }
}
close (AUTH);

# tell about open server socket
logmsg ("notice", 2, "Server started at port $port");

my $waitedpid = 0;
my $paddr;

# wait for children to return, thus avoiding zombies
# improvement (*)
use POSIX ":sys_wait_h";
sub REAPER {
  my $child;
  while (($waitedpid = waitpid(-1,WNOHANG)) > 0) {
    logmsg ("notice", 2, "reaped $waitedpid", ($? ? " with exit $?" : ""));
  }
  $SIG{CHLD} = \&REAPER;  # loathe sysV
}

# also all sub-processes should wait for their children
$SIG{CHLD} = \&REAPER;

# start a new server for every incoming request
# improvement (*) -- loop forever

while ( 1 ) {
  for ( $waitedpid = 0;
	($paddr = accept(Client,Server)) || $waitedpid;
	$waitedpid = 0, close Client ) {
    next if $waitedpid and not $paddr;
    my ($port, $iaddr) = sockaddr_in($paddr);
    my $name = lc gethostbyaddr($iaddr, AF_INET);
    my $ipaddr = inet_ntoa($iaddr);
    my $n = 0;
    
# tell about the requesting client
    logmsg ("info", 2, "Connection from >$ipaddr< ($name) at port $port");
    
    spawn sub {
      my ($head, $hlen, $pos, $pegel, $typ, $siz, $nch, $nrec, $dat, $i, $j, $n, $llen); 
      my ($watchpegel, $shmpegel);
      my ($rin, $rout, $in, $line, $data_requested, $oversample);
      my (@channels);
      
#   to use stdio on writing to Client
      Client->autoflush();
      
#   Open Device 
      sysopen (DEV, "$dev", O_RDWR | O_NONBLOCK, 0666) or die("can't open $dev");
      
#   Bitmask to check for input on stdin
      $rin = "";
      vec($rin, fileno(Client), 1) = 1; 
      
#   check for authorized hosts
      my $access = 'deny';
      $access = 'allow' if $authhosts{$ipaddr};
      $line = "<remote_host host=\"$ipaddr\" access=\"$access\">\n";
      logmsg ("info", 2, $line);
      $len = length $line;
      $offset = 0;
      while ($len) {
	$written = syswrite (DEV, $line, $len, $offset);
	$len -= $written;
	$offset += $written;
      }
      
      while ( 1 ) {
	$in = select ($rout=$rin, undef, undef, 0.0); # poll client
#     look for any Input from Client
	if ($in) {
#       exit on EOF
	  $len = sysread (Client, $line, $blksize) or exit;
	  logmsg("info", 0, "got $len bytes: \"$line\""); 
	  $offset = 0;
#       copy request to device
	  while ($len) {
	    $written = syswrite (DEV, $line, $len, $offset);
	    $len -= $written;
	    $offset += $written;
	  }
	}
#     look for some output from device
	if ($len = sysread DEV, $stream, $blksize) {
	  print Client $stream;
	} else {
	  select undef, undef, undef, 0.1; # calm down if nothing on device
	}
      }
    };
    logmsg("info", 2, "spawned\n");
  }
  logmsg("info", 2, "server loop\n");
}

sub spawn {
  my $coderef = shift;
  
  unless (@_ == 0 && $coderef && ref($coderef) eq 'CODE') {
    confess "usage: spawn CODEREF";
  }
  my $pid; 
  if (!defined($pid = fork)) {
    logmsg ("notice", 2, "fork failed: $!");
    return;
  } elsif ($pid) {
    logmsg ("notice", 2, "Request $pid");
    return; # Parent
  }

# do not use fdup as in the original example
# open (STDIN, "<&Client") or die "Can't dup client to stdin";
# open (STDOUT, ">&Client") or die "Can't dup client to stdout";
# STDOUT->autoflush();
  exit &$coderef();
}












