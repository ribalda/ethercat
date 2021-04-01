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

#include "CommandJson.h"
#include "MasterDevice.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#define MAX_TIME_STR_SIZE 50

/*****************************************************************************/

CommandJson::CommandJson():
    Command("json", "Show master and Ethernet device information in json format.")
{
}

/****************************************************************************/

string CommandJson::helpString(const string &binaryBaseName) const
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

void CommandJson::execute(const StringVector &args)
{
	MasterIndexList masterIndices;
    ec_ioctl_master_t data;
    stringstream err;
    unsigned int dev_idx;

    if (args.size()) {
        err << "'" << getName() << "' takes no arguments!";
        throwInvalidUsageException(err);
    }

    // create a JSON object
    json jsonOutput = {};

    masterIndices = getMasterIndices();
    jsonOutput["masterList"] = masterIndices;

    MasterIndexList::const_iterator mi;
    for (mi = masterIndices.begin();
            mi != masterIndices.end(); mi++) {
        MasterDevice m(*mi);
        m.open(MasterDevice::Read);
        m.getMaster(&data);

        auto masterID = "master" + std::to_string(m.getIndex());
        jsonOutput[masterID]["id"] = m.getIndex();
        jsonOutput[masterID]["phase"] = data.phase;
        jsonOutput[masterID]["active"]=data.active ? "yes" : "no";
        jsonOutput[masterID]["slaveCount"]=data.slave_count;
        for (dev_idx = EC_DEVICE_MAIN; dev_idx < data.num_devices; dev_idx++) {
          auto etherNetDev = dev_idx == EC_DEVICE_MAIN ? "Main" : "Backup";
          // jsonOutput[masterID][etherNetDev]["MAC"]="todo";
          jsonOutput[masterID][etherNetDev]["NicState"]=data.devices[dev_idx].attached ? "attached" : "waiting...";
          jsonOutput[masterID][etherNetDev]["linkState"]=data.devices[dev_idx].link_state ? "UP" : "DOWN";
          jsonOutput[masterID][etherNetDev]["frames"]["TX"]=data.devices[dev_idx].tx_count;
          jsonOutput[masterID][etherNetDev]["frames"]["RX"]=data.devices[dev_idx].rx_count;
          jsonOutput[masterID][etherNetDev]["bytes"]["TX"]=data.devices[dev_idx].tx_bytes;
          jsonOutput[masterID][etherNetDev]["bytes"]["RX"]=data.devices[dev_idx].rx_bytes;
          jsonOutput[masterID][etherNetDev]["errors"]["TX"]=data.devices[dev_idx].tx_errors;
          jsonOutput[masterID][etherNetDev]["frameRate"]["TX"]=data.devices[dev_idx].tx_frame_rates[0] / 1000.0;
          jsonOutput[masterID][etherNetDev]["frameRate"]["RX"]=data.devices[dev_idx].rx_frame_rates[0] / 1000.0;
          jsonOutput[masterID][etherNetDev]["kBytesRate"]["TX"]=data.devices[dev_idx].tx_byte_rates[0] / 1024.0;
          jsonOutput[masterID][etherNetDev]["kBytesRate"]["RX"]=data.devices[dev_idx].rx_byte_rates[0] / 1024.0;
        }
        jsonOutput[masterID]["Common"]["frames"]["TX"]=data.tx_count;
        jsonOutput[masterID]["Common"]["frames"]["RX"]=data.rx_count;
        jsonOutput[masterID]["Common"]["frames"]["lost"]= data.tx_count - data.rx_count;
        jsonOutput[masterID]["Common"]["bytes"]["TX"]=data.tx_bytes;
        jsonOutput[masterID]["Common"]["bytes"]["RX"]=data.rx_bytes;
        jsonOutput[masterID]["Common"]["frameRate"]["TX"]=data.tx_frame_rates[0] / 1000.0;
        jsonOutput[masterID]["Common"]["frameRate"]["RX"]=data.rx_frame_rates[0] / 1000.0;
        jsonOutput[masterID]["Common"]["kBytesRate"]["TX"]=data.tx_byte_rates[0] / 1024.0;
        jsonOutput[masterID]["Common"]["kBytesRate"]["RX"]=data.rx_byte_rates[0] / 1024.0;

        jsonOutput[masterID]["DC"]["refClockSlaveID"]=data.ref_clock;
        jsonOutput[masterID]["DC"]["refTime"]=data.dc_ref_time;
        jsonOutput[masterID]["DC"]["appTime"]=data.app_time;
    }
// pretty print with indent of 4 spaces
// std::cout << std::setw(4) << jsonOutput << '\n';
std::cout << jsonOutput << '\n';
}

/*****************************************************************************/
