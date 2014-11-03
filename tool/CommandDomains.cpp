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

#include "CommandDomains.h"
#include "MasterDevice.h"

/*****************************************************************************/

CommandDomains::CommandDomains():
    Command("domains", "Show configured domains.")
{
}

/*****************************************************************************/

string CommandDomains::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName() << " [OPTIONS]" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "Without the --verbose option, the domains are displayed" << endl
        << "one-per-line. Example:" << endl
        << endl
        << "Domain0: LogBaseAddr 0x00000000, Size   6, WorkingCounter 0/1"
        << endl << endl
        << "The domain's base address for the logical datagram" << endl
        << "(LRD/LWR/LRW) is displayed followed by the domain's" << endl
        << "process data size in byte. The last values are the current" << endl
        << "datagram working counter sum and the expected working" << endl
        << "counter sum. If the values are equal, all PDOs were" << endl
        << "exchanged during the last cycle." << endl
        << endl
        << "If the --verbose option is given, the participating slave" << endl
        << "configurations/FMMUs and the current process data are" << endl
        << "additionally displayed:" << endl
        << endl
        << "Domain1: LogBaseAddr 0x00000006, Size   6, WorkingCounter 0/1"
        << endl
        << "  SlaveConfig 1001:0, SM3 ( Input), LogAddr 0x00000006, Size 6"
        << endl
        << "    00 00 00 00 00 00" << endl
        << endl
        << "The process data are displayed as hexadecimal bytes." << endl
        << endl
        << "Command-specific options:" << endl
        << "  --domain  -d <index>  Positive numerical domain index." << endl
        << "                        If ommitted, all domains are" << endl
        << "                        displayed." << endl
        << endl
        << "  --verbose -v          Show FMMUs and process data" << endl
        << "                        in addition." << endl
        << endl
        << numericInfo();

    return str.str();
}

/****************************************************************************/

void CommandDomains::execute(const StringVector &args)
{
	MasterIndexList masterIndices;
    bool doIndent;
    DomainList domains;
    DomainList::const_iterator di;

    if (args.size()) {
        stringstream err;
        err << "'" << getName() << "' takes no arguments!";
        throwInvalidUsageException(err);
    }

	masterIndices = getMasterIndices();
    doIndent = masterIndices.size() > 1;
    MasterIndexList::const_iterator mi;
    for (mi = masterIndices.begin();
            mi != masterIndices.end(); mi++) {
        ec_ioctl_master_t io;
        MasterDevice m(*mi);
        m.open(MasterDevice::Read);
        m.getMaster(&io);
        domains = selectedDomains(m, io);

        if (domains.size() && doIndent) {
            cout << "Master" << dec << *mi << endl;
        }

        for (di = domains.begin(); di != domains.end(); di++) {
            showDomain(m, io, *di, doIndent);
        }
    }
}

/****************************************************************************/

void CommandDomains::showDomain(
        MasterDevice &m,
        const ec_ioctl_master_t &master,
        const ec_ioctl_domain_t &domain,
        bool doIndent
        )
{
    unsigned char *processData;
    ec_ioctl_domain_data_t data;
    unsigned int i, j;
    ec_ioctl_domain_fmmu_t fmmu;
    unsigned int dataOffset;
    string indent(doIndent ? "  " : "");
    unsigned int wc_sum = 0, dev_idx;

    for (dev_idx = EC_DEVICE_MAIN; dev_idx < master.num_devices; dev_idx++) {
        wc_sum += domain.working_counter[dev_idx];
    }

    cout << indent << "Domain" << dec << domain.index << ":"
        << " LogBaseAddr 0x"
        << hex << setfill('0')
        << setw(8) << domain.logical_base_address
        << ", Size " << dec << setfill(' ')
        << setw(3) << domain.data_size
        << ", WorkingCounter "
        << wc_sum << "/"
        << domain.expected_working_counter;
    if (master.num_devices > 1) {
        cout << " (";
        for (dev_idx = EC_DEVICE_MAIN; dev_idx < master.num_devices;
                dev_idx++) {
            cout << domain.working_counter[dev_idx];
            if (dev_idx + 1 < master.num_devices) {
                cout << "+";
            }
        }
        cout << ")";
    }
    cout << endl;

    if (!domain.data_size || getVerbosity() != Verbose)
        return;

    processData = new unsigned char[domain.data_size];

    try {
        m.getData(&data, domain.index, domain.data_size, processData);
    } catch (MasterDeviceException &e) {
        delete [] processData;
        throw e;
    }

    for (i = 0; i < domain.fmmu_count; i++) {
        m.getFmmu(&fmmu, domain.index, i);

        cout << indent << "  SlaveConfig "
            << dec << fmmu.slave_config_alias
            << ":" << fmmu.slave_config_position
            << ", SM" << (unsigned int) fmmu.sync_index << " ("
            << setfill(' ') << setw(6)
            << (fmmu.dir == EC_DIR_INPUT ? "Input" : "Output")
            << "), LogAddr 0x"
            << hex << setfill('0')
            << setw(8) << fmmu.logical_address
            << ", Size " << dec << fmmu.data_size << endl;

        dataOffset = fmmu.logical_address - domain.logical_base_address;
        if (dataOffset + fmmu.data_size > domain.data_size) {
            stringstream err;
            delete [] processData;
            err << "Fmmu information corrupted!";
            throwCommandException(err);
        }

        cout << indent << "    " << hex << setfill('0');
        for (j = 0; j < fmmu.data_size; j++) {
            if (j && !(j % BreakAfterBytes))
                cout << endl << indent << "    ";
            cout << setw(2)
                << (unsigned int) *(processData + dataOffset + j) << " ";
        }
        cout << endl;
    }

    delete [] processData;
}

/*****************************************************************************/
