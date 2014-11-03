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
#  The license mentioned above concerns the source code only. Using the
#  EtherCAT technology and brand is only permitted in compliance with the
#  industrial property and similar rights of Beckhoff Automation GmbH.
#
#------------------------------------------------------------------------------

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = \
	include \
	script

if ENABLE_KERNEL
SUBDIRS += devices
SUBDIRS += master
endif

if BUILD_TOOL
SUBDIRS += tool
endif

if ENABLE_USERLIB
SUBDIRS += lib
endif

if ENABLE_TTY
SUBDIRS += tty
endif

# userspace example depends on lib/
SUBDIRS += examples

DIST_SUBDIRS = \
	devices \
	examples \
	include \
	lib \
	m4 \
	master \
	script \
	tool \
	tty

noinst_HEADERS = \
	globals.h

EXTRA_DIST = \
	Doxyfile.in \
	FEATURES \
	Kbuild.in \
	README.EoE \
	ethercat.spec \
	ethercat.spec.in

BUILT_SOURCES = \
	Doxyfile \
	Kbuild \
	ethercat.spec

modules:
	$(MAKE) -C "$(LINUX_SOURCE_DIR)" M="@abs_srcdir@" modules

modules_install:
	$(MAKE) -C "$(LINUX_SOURCE_DIR)" M="@abs_srcdir@" \
		INSTALL_MOD_DIR="$(INSTALL_MOD_DIR)" modules_install

clean-local:
	$(MAKE) -C "$(LINUX_SOURCE_DIR)" M="@abs_srcdir@" clean

mydist:
	hg log --style=changelog $(srcdir) > ChangeLog
	@REV=`hg id -i $(srcdir)` && \
		$(MAKE) dist-bzip2 distdir=$(PACKAGE)-$(VERSION)-$${REV}

dist-hook:
	if which hg >/dev/null 2>&1; then \
		hg id -i $(srcdir) 2>/dev/null >$(distdir)/revision; \
	fi

mrproper: clean cleandoc
	rm -rf \
		*~ \
		ChangeLog \
		Doxyfile \
		Kbuild \
		Makefile \
		Makefile.in \
		TAGS \
		aclocal.m4 \
		autoconf \
		autom4te.cache \
		config.h \
		config.h.in \
		config.log \
		config.status \
		configure \
		configure.in \
		libtool \
		stamp-h1

doc:
	doxygen Doxyfile

cleandoc:
	@rm -rf doxygen-output

#------------------------------------------------------------------------------
