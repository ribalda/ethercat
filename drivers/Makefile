#------------------------------------------------------------------------------
#
#  Makefile
#
#  IgH EtherCAT-Treiber
#
#  $Id$
#
#------------------------------------------------------------------------------

ifneq ($(KERNELRELEASE),)

#------------------------------------------------------------------------------
# Kbuild-Abschnitt

obj-m := 8139too-ecat.o ecat-master.o

8139too-ecat-objs := 8139too.o

ecat-master-objs := ec_module.o ec_master.o ec_device.o ec_slave.o \
			ec_command.o ec_types.o ec_domain.o

REV = `svnversion $(src)`
DATE = `date`

EXTRA_CFLAGS = -DEC_REV="$(REV)" -DEC_USER="$(USER)" -DEC_DATE="$(DATE)"

#------------------------------------------------------------------------------

else

#------------------------------------------------------------------------------
# Default-Abschnitt

modules:
	$(MAKE) -C ..

clean:
	$(MAKE) -C .. clean

#------------------------------------------------------------------------------

endif
