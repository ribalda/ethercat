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
    devices \
    include \
    master \
    script

if BUILD_TOOL
SUBDIRS += tool
endif

if ENABLE_USERLIB
SUBDIRS += lib
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
    tool

noinst_HEADERS = \
	globals.h

EXTRA_DIST = \
	Doxyfile.in \
	FEATURES \
	Kbuild.in \
	README.EoE

BUILT_SOURCES = \
	Doxyfile \
	Kbuild

modules:
	$(MAKE) -C "$(LINUX_SOURCE_DIR)" M="@abs_srcdir@" modules

modules_install:
	$(MAKE) -C master modules_install
	$(MAKE) -C devices modules_install

clean-local:
	$(MAKE) -C "$(LINUX_SOURCE_DIR)" M="@abs_srcdir@" clean
	@rm -f Modules.symvers

mydist:
	svn2cl $(srcdir)
	@SVNREV=`svnversion $(srcdir)` && \
	  $(MAKE) dist-bzip2 \
	  distdir=$(PACKAGE)-$(VERSION)-r$${SVNREV}

dist-hook:
	if which svnversion >/dev/null 2>&1; then \
		svnversion $(srcdir) 2>/dev/null >$(distdir)/svnrevision; \
	fi

mrproper: clean cleandoc
	rm -rf \
		*~ \
		aclocal.m4 \
		autoconf \
		autom4te.cache \
		ChangeLog \
		config.h \
		config.h.in \
		config.log \
		config.status \
		configure \
		configure.in \
		Doxyfile \
		Kbuild \
		libtool \
		Makefile \
		Makefile.in \
		Module.symvers \
		stamp-h1 \
		TAGS

doc:
	doxygen Doxyfile

cleandoc:
	@rm -rf doxygen-output

#------------------------------------------------------------------------------
