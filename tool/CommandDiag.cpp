/*****************************************************************************
 *
 *  $Id$
 *
 *  Copyright (C) 2016  Ralf Roesch, Roesch & Walter Industrie-Elektronik GmbH
 *  Copyright (C) 2006-2012  Florian Pose, Ingenieurgemeinschaft IgH
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
#include <stdio.h>
#include <iostream>
#include <iomanip>
#include <string.h>
using namespace std;

#include "CommandDiag.h"
#include "MasterDevice.h"

/*****************************************************************************/

CommandDiag::CommandDiag():
    Command("diag", "Output slave(s) ESC error registers.")
{
}

/*****************************************************************************/

string CommandDiag::helpString(const string &binaryBaseName) const
{
    stringstream str;

    str << binaryBaseName << " " << getName()
        << " [OPTIONS]" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "Command-specific options:" << endl
        << "  --alias    -a <alias>" << endl
        << "  --position -p <pos>    Slave selection." << endl
        << "  --reset    -r          Resets all error registers in ESC." << endl
        << "  --verbose  -v          Verbose output error ESC registers of selected slave(s)." << endl
        << endl;

    return str.str();
}

/****************************************************************************/

void CommandDiag::execute(const StringVector &args)
{
    MasterIndexList masterIndices;
    SlaveList slaves;
    bool doIndent;

    if (args.size() > 1) {
        stringstream err;
        err << "'" << getName() << "' takes max one argument!";
        throwInvalidUsageException(err);
    }

    masterIndices = getMasterIndices();
    doIndent = masterIndices.size() > 1;
    MasterIndexList::const_iterator mi;
    for (mi = masterIndices.begin();
            mi != masterIndices.end(); mi++) {
        MasterDevice m(*mi);
        m.open(MasterDevice::ReadWrite);
        slaves = selectedSlaves(m);

        CheckallSlaves(m, slaves, doIndent);
    }
}

void CommandDiag::EscRegRead(MasterDevice &m, uint16_t slave_position, uint16_t address, uint8_t *data, size_t byteSize)
{
    ec_ioctl_slave_reg_t io;

    io.slave_position = slave_position;
    io.address = address;
    io.size = byteSize;
    io.data = data;
    io.emergency = false;

    memset(data, 0, byteSize);
    try {
        m.readReg(&io);
    } catch (MasterDeviceException &e) {
        fprintf(stderr, "EscRegRead %s slave = %i, address = %x, size = %zu\n", e.what(), io.slave_position, io.address, io.size);
    }
}

void CommandDiag::EscRegReadWrite(MasterDevice &m, uint16_t slave_position, uint16_t address, uint8_t *data, size_t byteSize)
{
    ec_ioctl_slave_reg_t io;

    io.slave_position = slave_position;
    io.address = address;
    io.size = byteSize;
    io.data = data;
    io.emergency = false;

    try {
        m.readWriteReg(&io);
    } catch (MasterDeviceException &e) {
        fprintf(stderr, "EscRegReadWrite %s slave = %i, address = %x, size = %zu\n", e.what(), io.slave_position, io.address, io.size);
    }
}

/****************************************************************************/

void CommandDiag::CheckallSlaves(
        MasterDevice &m,
        const SlaveList &slaves,
        bool doIndent
        )
{
    ec_ioctl_master_t master;
    unsigned int i, lastDevice;
    ec_ioctl_slave_t slave;
    uint16_t lastAlias, aliasIndex;
    Info info;
    typedef list<Info> InfoList;
    InfoList infoList;
    InfoList::const_iterator iter;
    stringstream str;
    unsigned int maxPosWidth = 0, maxAliasWidth = 0,
                 maxRelPosWidth = 0, maxStateWidth = 0,
                 maxESCerrorsWidth = 0;
    string indent(doIndent ? "  " : "");

    m.getMaster(&master);

    lastAlias = 0;
    aliasIndex = 0;
    for (i = 0; i < master.slave_count; i++) {
        m.getSlave(&slave, i);

        if (slave.alias) {
            lastAlias = slave.alias;
            aliasIndex = 0;
        }

        if (slaveInList(slave, slaves)) {
            int slave_position = i;
            uint8_t dl_status[2];
            uint8_t ecat_errors[0x14];

            str << dec << i;
            info.pos = str.str();
            str.clear();
            str.str("");

            str << lastAlias;
            info.alias = str.str();
            str.str("");

            str << aliasIndex;
            info.relPos = str.str();
            str.str("");

            info.state = alStateString(slave.al_state);
            info.flag = (slave.error_flag ? 'E' : '+');
            info.device = slave.device_index;

            if (strlen(slave.name)) {
                info.name = slave.name;
            } else {
                str << "0x" << hex << setfill('0')
                    << setw(8) << slave.vendor_id << ":0x"
                    << setw(8) << slave.product_code;
                info.name = str.str();
                str.str("");
            }

            EscRegRead(m, slave_position, 0x110, dl_status, sizeof(dl_status));
            if (getReset()) {
                memset(ecat_errors, 0, sizeof(ecat_errors));
                EscRegReadWrite(m, slave_position, 0x300, ecat_errors, sizeof(ecat_errors));
            } else {
                EscRegRead(m, slave_position, 0x300, ecat_errors, sizeof(ecat_errors));
            }

            info.ESC_DL_Status = EC_READ_U16(dl_status);
            info.ESCerrors = 0;

            for (int i = 0; i < EC_MAX_PORTS; i++) {
                int check_port;
                /* check port only if: loop is open, and Communication established */
                check_port = (((info.ESC_DL_Status >>  (8 + i * 2)) & 0x3) == 2) ? 0x1 : 0x0;
                /* some error registers are only availble when MII or EBUS port is present */
                check_port += (slave.ports[i].desc == EC_PORT_MII) ? 1 : 0;
                check_port += (slave.ports[i].desc == EC_PORT_EBUS) ? 1 : 0;
                info.Invalid_Frame_Counter[i] = (check_port > 0) ? EC_READ_U8(&ecat_errors[i * 2]) : 0;
                info.RX_Error_Counter[i] = (check_port > 0) ? EC_READ_U8(&ecat_errors[0x1 + (i * 2)]) : 0;
                info.Forwarded_RX_Error_Counter[i] = (check_port > 1) ? EC_READ_U8(&ecat_errors[0x8 + i]) : 0;
                info.ECAT_Processing_Unit_Error_Counter = (check_port > 1) && (i == 0) ? EC_READ_U8(&ecat_errors[0xC]) : 0;
                info.Lost_Link_Counter[i] = (check_port > 1) ? EC_READ_U8(&ecat_errors[0x10 + i]) : 0;

                info.ESCerrors |= info.Invalid_Frame_Counter[i] ? 0x01 : 0x00;
                info.ESCerrors |= info.RX_Error_Counter[i] ? 0x02 : 0x00;
                info.ESCerrors |= info.Forwarded_RX_Error_Counter[i] ? 0x04 : 0x00;
                info.ESCerrors |= info.ECAT_Processing_Unit_Error_Counter ? 0x08 : 0x00;
                info.ESCerrors |= info.Lost_Link_Counter[i] ? 0x10 : 0x00;
            }

            if (info.ESCerrors) {
                info.sESCerrors = "";
                if (info.ESCerrors & 0x01) {
                    info.sESCerrors += "E_IFC ";
                }
                if (info.ESCerrors & 0x02) {
                    info.sESCerrors += "E_REC ";
                }
                if (info.ESCerrors & 0x04) {
                    info.sESCerrors += "E_FREC ";
                }
                if (info.ESCerrors & 0x08) {
                    info.sESCerrors += "E_PUEC ";
                }
                if (info.ESCerrors & 0x10) {
                    info.sESCerrors += "E_LLC ";
                }
            } else {
                info.sESCerrors = "ESC no errors. ";
            }

            infoList.push_back(info);

            if (info.pos.length() > maxPosWidth)
                maxPosWidth = info.pos.length();
            if (info.alias.length() > maxAliasWidth)
                maxAliasWidth = info.alias.length();
            if (info.relPos.length() > maxRelPosWidth)
                maxRelPosWidth = info.relPos.length();
            if (info.state.length() > maxStateWidth)
                maxStateWidth = info.state.length();
            if (info.sESCerrors.length() > maxESCerrorsWidth)
                maxESCerrorsWidth = info.sESCerrors.length();
        }

        aliasIndex++;
    }

    if (infoList.size() && doIndent) {
        cout << "Master" << dec << m.getIndex() << endl;
    }

    lastDevice = EC_DEVICE_MAIN;
    for (iter = infoList.begin(); iter != infoList.end(); iter++) {
        if (iter->device != lastDevice) {
            lastDevice = iter->device;
            cout << "xxx LINK FAILURE xxx" << endl;
        }
        cout << indent << setfill(' ') << right
             << setw(maxPosWidth) << iter->pos << "  "
             << setw(maxAliasWidth) << iter->alias
             << ":" << left
             << setw(maxRelPosWidth) << iter->relPos << "  "
             << setw(maxStateWidth) << iter->state << "  "
             << iter->flag << "  "
             << setw(maxESCerrorsWidth) << iter->sESCerrors << " "
             << iter->name << endl;
        if (getVerbosity() == Verbose) {
            string indent("    ");
            if (iter->ESCerrors & 0x01) {
                cout << indent << "Invalid Frame Counter -";
                for (int i = 0; i < EC_MAX_PORTS; i++) {
                    if (iter->Invalid_Frame_Counter[i]) {
                        cout << dec << " P[" << i << "]: " << iter->Invalid_Frame_Counter[i];
                    }
                }
                cout << endl;
            }
            if (iter->ESCerrors & 0x02) {
                cout << indent << "RX Error counter -";
                for (int i = 0; i < EC_MAX_PORTS; i++) {
                    if (iter->RX_Error_Counter[i]) {
                        cout << dec << " P[" << i << "]: " << iter->RX_Error_Counter[i];
                    }
                }
                cout << endl;
            }
            if (iter->ESCerrors & 0x04) {
                cout << indent << "Forwarded RX Error Counter -";
                for (int i = 0; i < EC_MAX_PORTS; i++) {
                    if (iter->Forwarded_RX_Error_Counter[i]) {
                        cout << dec << " P[" << i << "]: " << iter->Forwarded_RX_Error_Counter[i];
                    }
                }
                cout << endl;
            }
            if (iter->ESCerrors & 0x08) {
                cout << indent << "ECAT Processing Unit Error Counter - ";
                cout << dec << " << iter->ECAT_Processing_Unit_Error_Counter" << endl;
            }
            if (iter->ESCerrors & 0x10) {
                cout << indent << "Lost Link Counter -";
                for (int i = 0; i < EC_MAX_PORTS; i++) {
                    if (iter->Lost_Link_Counter[i]) {
                        cout << dec << " P[" << i << "]: " << iter->Lost_Link_Counter[i];
                    }
                }
                cout << endl;
            }
        }
    }
}

bool CommandDiag::slaveInList(
        const ec_ioctl_slave_t &slave,
        const SlaveList &slaves
        )
{
    SlaveList::const_iterator si;

    for (si = slaves.begin(); si != slaves.end(); si++) {
        if (si->position == slave.position) {
            return true;
        }
    }

    return false;
}
