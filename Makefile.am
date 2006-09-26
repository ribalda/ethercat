#------------------------------------------------------------------------------
#
#  Makefile.am
#
#  IgH EtherCAT master
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

SUBDIRS = master/ devices/

initdir = $(sysconfdir)/init.d
sysdir = $(sysconfdir)/sysconfig

include_HEADERS = include/ecdb.h include/ecrt.h
noinst_SCRIPTS = script/lsec.pl script/ethercat.sh script/sysconfig

MINI_FILES = \
	examples/mini/Kbuild \
	examples/mini/Makefile.am \
	examples/mini/Makefile.in \
	examples/mini/mini.c

RTAI_FILES = \
	examples/rtai/Kbuild \
	examples/rtai/Makefile.am \
	examples/rtai/Makefile.in \
	examples/rtai/rtai_sample.c

MSR_FILES = \
	examples/msr/Kbuild \
	examples/msr/libm.o_shipped \
	examples/msr/Makefile.am \
	examples/msr/Makefile.in \
	examples/msr/msr_load \
	examples/msr/msr_param.h \
	examples/msr/msr_sample.c \
	examples/msr/msrserv.pl \
	examples/msr/msr_unload

EXTRA_DIST = $(noinst_SCRIPTS) documentation/ethercat_doc.pdf \
	$(MINI_FILES) $(RTAI_FILES) $(MSR_FILES)

mydist:
	@SVNREV=`svnversion $(srcdir)` && \
		$(MAKE) dist-bzip2 distdir=$(PACKAGE)-$(VERSION)-r$${SVNREV}

dist-hook:
	if which svnversion >/dev/null 2>&1; then \
		svnversion $(srcdir) 2>/dev/null >$(distdir)/svnrevision; \
	fi

install-data-local:
	$(mkinstalldirs) $(bindir)
	$(mkinstalldirs) $(initdir)
	$(mkinstalldirs) $(sysdir)
	$(INSTALL_SCRIPT) $(srcdir)/script/lsec.pl $(bindir)/lsec
	$(INSTALL_SCRIPT) $(srcdir)/script/ethercat.sh $(initdir)/ethercat
	$(INSTALL_DATA) $(srcdir)/script/sysconfig $(sysdir)/ethercat
	$(DEPMOD) $(LINUX_KERNEL_VERSION)

uninstall-local:
	rm -rf $(LINUX_MODULES_DIR)/ethercat
	rm $(bindir)/lsec
	rm $(initdir)/ethercat
	rm $(sysdir)/ethercat

doc:
	doxygen Doxyfile

cleandoc:
	@rm -rf doxygen-output

#------------------------------------------------------------------------------
