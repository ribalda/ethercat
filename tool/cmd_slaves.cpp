/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <iostream>
#include <iomanip>
#include <list>
using namespace std;

#include "globals.h"

/****************************************************************************/

const char *help_slaves =
    "[OPTIONS]\n"
    "\n"
    "\n"
    "Command-specific options:\n";

void listSlaves(int);
void showSlave(uint16_t);

/****************************************************************************/

void command_slaves()
{
    masterDev.open(MasterDevice::Read);

    if (verbosity == Verbose) {
        if (slavePosition == -1) {
            unsigned int numSlaves = masterDev.slaveCount(), i;

            for (i = 0; i < numSlaves; i++) {
                showSlave(i);
            }
        } else {
            showSlave(slavePosition);
        }
    } else {
        listSlaves(slavePosition);
    }
}

/****************************************************************************/

string slaveState(uint8_t state)
{
    switch (state) {
        case 1: return "INIT";
        case 2: return "PREOP";
        case 4: return "SAFEOP";
        case 8: return "OP";
        default: return "???";
    }
}

/****************************************************************************/

struct SlaveInfo {
    string pos;
    string alias;
    string relPos;
    string state;
    string flag;
    string name;
};

void listSlaves(int slavePosition)
{
    unsigned int numSlaves, i;
    ec_ioctl_slave_t slave;
    uint16_t lastAlias, aliasIndex;
    SlaveInfo slaveInfo;
    typedef list<SlaveInfo> SlaveInfoList;
    SlaveInfoList slaveInfoList;
    SlaveInfoList::const_iterator iter;
    stringstream str;
    unsigned int maxPosWidth = 0, maxAliasWidth = 0,
                 maxRelPosWidth = 0, maxStateWidth = 0;
    
    numSlaves = masterDev.slaveCount();

    lastAlias = 0;
    aliasIndex = 0;
    for (i = 0; i < numSlaves; i++) {
        masterDev.getSlave(&slave, i);
        
        if (slave.alias) {
            lastAlias = slave.alias;
            aliasIndex = 0;
        }

        if (slavePosition == -1 || i == (unsigned int) slavePosition) {
            str << dec << i;
            slaveInfo.pos = str.str();
            str.clear();
            str.str("");

            str << lastAlias;
            slaveInfo.alias = str.str();
            str.str("");

            str << aliasIndex;
            slaveInfo.relPos = str.str();
            str.str("");

            slaveInfo.state = slaveState(slave.state);
            slaveInfo.flag = (slave.error_flag ? 'E' : '+');

            if (strlen(slave.name)) {
                slaveInfo.name = slave.name;
            } else {
                str << "0x" << hex << setfill('0')
                    << setw(8) << slave.vendor_id << ":0x"
                    << setw(8) << slave.product_code;
                slaveInfo.name = str.str();
                str.str("");
            }


            slaveInfoList.push_back(slaveInfo);

            if (slaveInfo.pos.length() > maxPosWidth)
                maxPosWidth = slaveInfo.pos.length();
            if (slaveInfo.alias.length() > maxAliasWidth)
                maxAliasWidth = slaveInfo.alias.length();
            if (slaveInfo.relPos.length() > maxRelPosWidth)
                maxRelPosWidth = slaveInfo.relPos.length();
            if (slaveInfo.state.length() > maxStateWidth)
                maxStateWidth = slaveInfo.state.length();
        }

        aliasIndex++;
    }

    for (iter = slaveInfoList.begin(); iter != slaveInfoList.end(); iter++) {
        cout << setfill(' ') << right
            << setw(maxPosWidth) << iter->pos << "  "
            << setw(maxAliasWidth) << iter->alias
            << ":" << left
            << setw(maxRelPosWidth) << iter->relPos << "  "
            << setw(maxStateWidth) << iter->state << "  "
            << iter->flag << "  "
            << iter->name << endl;
    }
}

/****************************************************************************/

void showSlave(uint16_t slavePosition)
{
    ec_ioctl_slave_t slave;
    list<string> protoList;
    list<string>::const_iterator protoIter;
    
    masterDev.getSlave(&slave, slavePosition);
        
    cout << "=== Slave " << dec << slavePosition << " ===" << endl;
    
    if (slave.alias)
        cout << "Alias: " << slave.alias << endl;

    cout
        << "State: " << slaveState(slave.state) << endl
        << "Flag: " << (slave.error_flag ? 'E' : '+') << endl
        << "Identity:" << endl
        << "  Vendor Id:       0x"
        << hex << setfill('0')
        << setw(8) << slave.vendor_id << endl
        << "  Product code:    0x"
        << setw(8) << slave.product_code << endl
        << "  Revision number: 0x"
        << setw(8) << slave.revision_number << endl
        << "  Serial number:   0x"
        << setw(8) << slave.serial_number << endl;

    if (slave.mailbox_protocols) {
        cout << "Mailboxes:" << endl
        << "  RX: 0x"
        << hex << setw(4) << slave.rx_mailbox_offset << "/"
        << dec << slave.rx_mailbox_size
        << ", TX: 0x"
        << hex << setw(4) << slave.tx_mailbox_offset << "/"
        << dec << slave.tx_mailbox_size << endl
        << "  Supported protocols: ";

        if (slave.mailbox_protocols & EC_MBOX_AOE) {
            protoList.push_back("AoE");
        }
        if (slave.mailbox_protocols & EC_MBOX_EOE) {
            protoList.push_back("EoE");
        }
        if (slave.mailbox_protocols & EC_MBOX_COE) {
            protoList.push_back("CoE");
        }
        if (slave.mailbox_protocols & EC_MBOX_FOE) {
            protoList.push_back("FoE");
        }
        if (slave.mailbox_protocols & EC_MBOX_SOE) {
            protoList.push_back("SoE");
        }
        if (slave.mailbox_protocols & EC_MBOX_VOE) {
            protoList.push_back("VoE");
        }

        for (protoIter = protoList.begin(); protoIter != protoList.end();
                protoIter++) {
            if (protoIter != protoList.begin())
                cout << ", ";
            cout << *protoIter;
        }
        cout << endl;
    }

    if (slave.has_general_category) {
        cout << "General:" << endl
            << "  Group: " << slave.group << endl
            << "  Image name: " << slave.image << endl
            << "  Order number: " << slave.order << endl
            << "  Device name: " << slave.name << endl;

        if (slave.mailbox_protocols & EC_MBOX_COE) {
            cout << "  CoE details:" << endl
                << "    Enable Sdo: "
                << (slave.coe_details.enable_sdo ? "yes" : "no") << endl
                << "    Enable Sdo Info: "
                << (slave.coe_details.enable_sdo_info ? "yes" : "no") << endl
                << "    Enable Pdo Assign: "
                << (slave.coe_details.enable_pdo_assign
                        ? "yes" : "no") << endl
                << "    Enable Pdo Configuration: "
                << (slave.coe_details.enable_pdo_configuration
                        ? "yes" : "no") << endl
                << "    Enable Upload at startup: "
                << (slave.coe_details.enable_upload_at_startup
                        ? "yes" : "no") << endl
                << "    Enable Sdo complete access: "
                << (slave.coe_details.enable_sdo_complete_access
                        ? "yes" : "no") << endl;
        }

        cout << "  Flags:" << endl
            << "    Enable SafeOp: "
            << (slave.general_flags.enable_safeop ? "yes" : "no") << endl
            << "    Enable notLRW: "
            << (slave.general_flags.enable_not_lrw ? "yes" : "no") << endl
            << "  Current consumption: "
            << dec << slave.current_on_ebus << " mA" << endl;
    }
}

/*****************************************************************************/
