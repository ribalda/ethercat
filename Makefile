#################################################################
#
#  Globales Makefile
#
#  IgH EtherCAT-Treiber
#
#  $Id$
#
#################################################################

all: .rs232dbg .drivers .rt .mini

doc docs:
	doxygen Doxyfile

#################################################################

.drivers:
	$(MAKE) -C drivers

.rt:
	$(MAKE) -C rt

.rs232dbg:
	$(MAKE) -C rs232dbg

.mini:
	$(MAKE) -C mini

#################################################################

clean:
	$(MAKE) -C rt clean
	$(MAKE) -C drivers clean
	$(MAKE) -C rs232dbg clean
	$(MAKE) -C mini clean

#################################################################
