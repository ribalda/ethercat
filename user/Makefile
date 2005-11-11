#----------------------------------------------------------------
#
#  M a k e f i l e
#
#  $Id$
#
#----------------------------------------------------------------

LIBNET_DIR = ../../soft/libnet-install
LIBPCAP_DIR = ../../soft/libpcap-install
FLTK_DIR = ../../soft/fltk-2.0-install

CC = g++
CFLAGS = -Wall -g -I$(LIBNET_DIR)/include -I$(LIBPCAP_DIR)/include \
	`$(FLTK_DIR)/bin/fltk-config --cflags`

TEST_EXE = ethercat-test
TEST_OBJ = main.o ec_master.o ec_command.o ec_slave.o
TEST_LDFLAGS = -L$(LIBNET_DIR)/lib -lnet -lpcap -lpthread

GUI_EXE = ethercat-gui
GUI_OBJ = main_gui.o ec_master.o ec_command.o ec_slave.o
GUI_LDFLAGS = -L$(LIBNET_DIR)/lib -lnet -lpcap -lpthread `$(FLTK_DIR)/bin/fltk-config --ldflags`

#----------------------------------------------------------------

first: $(TEST_EXE) $(GUI_EXE)

$(TEST_EXE): $(TEST_OBJ)
	$(CC) $(TEST_OBJ) $(TEST_LDFLAGS) -o $@

$(GUI_EXE): $(GUI_OBJ)
	$(CC) $(GUI_OBJ) $(GUI_LDFLAGS) -o $@

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

.cpp.o:
	$(CC) $(CFLAGS) -c $< -o $@

#----------------------------------------------------------------

main.o: main.c \
	ec_globals.h ec_master.h ec_command.h ec_slave.h

main_gui.o: main_gui.cpp \
	ec_globals.h ec_master.h ec_command.h ec_slave.h

ec_command.o: ec_command.c ec_command.h

ec_master.o: ec_master.c ec_master.h \
	ec_globals.h ec_command.h ec_slave.h

ec_slave.o: ec_slave.c ec_slave.h \
	ec_globals.h

#----------------------------------------------------------------

clean:
	rm -f *.o $(TEST_EXE) $(GUI_EXE) *~

#----------------------------------------------------------------

