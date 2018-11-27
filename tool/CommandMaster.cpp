/*****************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2006-2009  Florian Pose, Ingenieurgemeinschaft IgH
 *
 *  This file is part of the IgH EtherCAT Master.
 *
 *  The IgH EtherCAT Master is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License version 2, as
 *  published by the Free Software Foundation.
 *
 *  The IgH EtherCAT Master is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 *  Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with the IgH EtherCAT Master; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  ---
 *
 *  The license mentioned above concerns the source code only. Using the
 *  EtherCAT technology and brand is only permitted in compliance with the
 *  industrial property and similar rights of Beckhoff Automation GmbH.
 *
 ****************************************************************************/

#include <iostream>
#include <iomanip>
using namespace std;

#include "CommandMaster.h"
#include "MasterDevice.h"

#define MAX_TIME_STR_SIZE 50

/*****************************************************************************/

CommandMaster::CommandMaster():
    Command("master", "Show master and Ethernet device information.")
{
}

/****************************************************************************/

string CommandMaster::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName() << " [OPTIONS]" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "Command-specific options:" << endl
        << "  --master -m <indices>  Master indices. A comma-separated" << endl
        << "                         list with ranges is supported." << endl
        << "                         Example: 1,4,5,7-9. Default: - (all)."
        << endl << endl
        << numericInfo();

    return str.str();
}

/****************************************************************************/

void CommandMaster::execute(const StringVector &args)
{
	MasterIndexList masterIndices;
    ec_ioctl_master_t data;
    stringstream err;
    unsigned int dev_idx, j;
    time_t epoch;
    char time_str[MAX_TIME_STR_SIZE + 1];
    size_t time_str_size;

    if (args.size()) {
        err << "'" << getName() << "' takes no arguments!";
        throwInvalidUsageException(err);
    }

	masterIndices = getMasterIndices();
    MasterIndexList::const_iterator mi;
    for (mi = masterIndices.begin();
            mi != masterIndices.end(); mi++) {
        MasterDevice m(*mi);
        m.open(MasterDevice::Read);
        m.getMaster(&data);

        cout
            << "Master" << m.getIndex() << endl
            << "  Phase: ";

        switch (data.phase) {
            case 0:  cout << "Waiting for device(s)..."; break;
            case 1:  cout << "Idle"; break;
            case 2:  cout << "Operation"; break;
            default: cout << "???";
        }

        cout << endl
            << "  Active: " << (data.active ? "yes" : "no") << endl
            << "  Slaves: " << data.slave_count << endl
            << "  Ethernet devices:" << endl;

        for (dev_idx = EC_DEVICE_MAIN; dev_idx < data.num_devices;
                dev_idx++) {
            cout << "    " << (dev_idx == EC_DEVICE_MAIN ? "Main" : "Backup")
                << ": ";
            cout << hex << setfill('0')
                << setw(2) << (unsigned int) data.devices[dev_idx].address[0]
                << ":"
                << setw(2) << (unsigned int) data.devices[dev_idx].address[1]
                << ":"
                << setw(2) << (unsigned int) data.devices[dev_idx].address[2]
                << ":"
                << setw(2) << (unsigned int) data.devices[dev_idx].address[3]
                << ":"
                << setw(2) << (unsigned int) data.devices[dev_idx].address[4]
                << ":"
                << setw(2) << (unsigned int) data.devices[dev_idx].address[5]
                << " ("
                << (data.devices[dev_idx].attached ?
                        "attached" : "waiting...")
                << ")" << endl << dec
                << "      Link: "
                << (data.devices[dev_idx].link_state ? "UP" : "DOWN") << endl
                << "      Tx frames:   "
                << data.devices[dev_idx].tx_count << endl
                << "      Tx bytes:    "
                << data.devices[dev_idx].tx_bytes << endl
                << "      Rx frames:   "
                << data.devices[dev_idx].rx_count << endl
                << "      Rx bytes:    "
                << data.devices[dev_idx].rx_bytes << endl
                << "      Tx errors:   "
                << data.devices[dev_idx].tx_errors << endl
                << "      Tx frame rate [1/s]: "
                << setfill(' ') << setprecision(0) << fixed;
            for (j = 0; j < EC_RATE_COUNT; j++) {
                cout << setw(ColWidth)
                    << data.devices[dev_idx].tx_frame_rates[j] / 1000.0;
                if (j < EC_RATE_COUNT - 1) {
                    cout << " ";
                }
            }
            cout << endl
                << "      Tx rate [KByte/s]:   "
                << setprecision(1) << fixed;
            for (j = 0; j < EC_RATE_COUNT; j++) {
                cout << setw(ColWidth)
                    << data.devices[dev_idx].tx_byte_rates[j] / 1024.0;
                if (j < EC_RATE_COUNT - 1) {
                    cout << " ";
                }
            }
            cout << endl
                << "      Rx frame rate [1/s]: "
                << setfill(' ') << setprecision(0) << fixed;
            for (j = 0; j < EC_RATE_COUNT; j++) {
                cout << setw(ColWidth)
                    << data.devices[dev_idx].rx_frame_rates[j] / 1000.0;
                if (j < EC_RATE_COUNT - 1) {
                    cout << " ";
                }
            }
            cout << endl
                << "      Rx rate [KByte/s]:   "
                << setprecision(1) << fixed;
            for (j = 0; j < EC_RATE_COUNT; j++) {
                cout << setw(ColWidth)
                    << data.devices[dev_idx].rx_byte_rates[j] / 1024.0;
                if (j < EC_RATE_COUNT - 1) {
                    cout << " ";
                }
            }
            cout << setprecision(0) << endl;
        }
        unsigned int lost = data.tx_count - data.rx_count;
        if (lost == 1) {
            // allow one frame travelling
            lost = 0;
        }
        cout << "    Common:" << endl
            << "      Tx frames:   "
            << data.tx_count << endl
            << "      Tx bytes:    "
            << data.tx_bytes << endl
            << "      Rx frames:   "
            << data.rx_count << endl
            << "      Rx bytes:    "
            << data.rx_bytes << endl
            << "      Lost frames: " << lost << endl
            << "      Tx frame rate [1/s]: "
            << setfill(' ') << setprecision(0) << fixed;
        for (j = 0; j < EC_RATE_COUNT; j++) {
            cout << setw(ColWidth)
                << data.tx_frame_rates[j] / 1000.0;
            if (j < EC_RATE_COUNT - 1) {
                cout << " ";
            }
        }
        cout << endl
            << "      Tx rate [KByte/s]:   "
            << setprecision(1) << fixed;
        for (j = 0; j < EC_RATE_COUNT; j++) {
            cout << setw(ColWidth)
                << data.tx_byte_rates[j] / 1024.0;
            if (j < EC_RATE_COUNT - 1) {
                cout << " ";
            }
        }
        cout << endl
            << "      Rx frame rate [1/s]: "
            << setfill(' ') << setprecision(0) << fixed;
        for (j = 0; j < EC_RATE_COUNT; j++) {
            cout << setw(ColWidth)
                << data.rx_frame_rates[j] / 1000.0;
            if (j < EC_RATE_COUNT - 1) {
                cout << " ";
            }
        }
        cout << endl
            << "      Rx rate [KByte/s]:   "
            << setprecision(1) << fixed;
        for (j = 0; j < EC_RATE_COUNT; j++) {
            cout << setw(ColWidth)
                << data.rx_byte_rates[j] / 1024.0;
            if (j < EC_RATE_COUNT - 1) {
                cout << " ";
            }
        }
        cout << endl
            << "      Loss rate [1/s]:     "
            << setprecision(0) << fixed;
        for (j = 0; j < EC_RATE_COUNT; j++) {
            cout << setw(ColWidth)
                << data.loss_rates[j] / 1000.0;
            if (j < EC_RATE_COUNT - 1) {
                cout << " ";
            }
        }
        cout << endl
            << "      Frame loss [%]:      "
            << setprecision(1) << fixed;
        for (j = 0; j < EC_RATE_COUNT; j++) {
            double perc = 0.0;
            if (data.tx_frame_rates[j]) {
                perc = 100.0 * data.loss_rates[j] / data.tx_frame_rates[j];
            }
            cout << setw(ColWidth) << perc;
            if (j < EC_RATE_COUNT - 1) {
                cout << " ";
            }
        }
        cout << setprecision(0) << endl;

        cout << "  Distributed clocks:" << endl
            << "    Reference clock:   ";
        if (data.ref_clock != 0xffff) {
            cout << "Slave " << dec << data.ref_clock;
        } else {
            cout << "None";
        }
        cout << endl
            << "    DC reference time: " << data.dc_ref_time << endl
            << "    Application time:  " << data.app_time << endl
            << "                       ";

        epoch = data.app_time / 1000000000 + 946684800ULL;
        time_str_size = strftime(time_str, MAX_TIME_STR_SIZE,
                "%Y-%m-%d %H:%M:%S", gmtime(&epoch));
        cout << string(time_str, time_str_size) << "."
            << setfill('0') << setw(9) << data.app_time % 1000000000 << endl;
    }
}

/*****************************************************************************/
