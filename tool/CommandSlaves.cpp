/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <iostream>
#include <iomanip>
#include <list>
using namespace std;

#include "CommandSlaves.h"

/*****************************************************************************/

CommandSlaves::CommandSlaves():
    Command("slaves", "Display slaves on the bus.")
{
}

/*****************************************************************************/

string CommandSlaves::helpString() const
{
    stringstream str;

    str << getName() << " [OPTIONS]" << endl
        << endl
        << getBriefDescription() << endl
        << endl
        << "If the --verbose option is not given, the slaves are" << endl
        << "displayed one-per-line. Example:" << endl
        << endl
        << "1  5555:0  PREOP  +  EL3162 2C. Ana. Input 0-10V" << endl
        << "|  |    |  |      |  |" << endl
        << "|  |    |  |      |  \\- Name from the SII if avaliable," << endl
        << "|  |    |  |      |     otherwise vendor ID and product" << endl
        << "|  |    |  |      |     code (both hexadecimal)." << endl
        << "|  |    |  |      \\- Error flag. '+' means no error," << endl
        << "|  |    |  |         'E' means that scan or" << endl
        << "|  |    |  |         configuration failed." << endl
        << "|  |    |  \\- Current application-layer state." << endl
        << "|  |    \\- Relative position (decimal) after the last" << endl
        << "|  |       slave with an alias address set." << endl
        << "|  \\- Alias address of the slave (if set), or the alias" << endl
        << "|     of the last slave with an alias, or zero if not" << endl
        << "|     applicable" << endl
        << "\\- Absolute ring position in the bus (use this with any" << endl
        << "   --slave option)." << endl
        << endl
        << "If the --verbose option is given, a detailed (multi-line)" << endl
        << "description is output for each slave." << endl
        << endl
        << "Command-specific options:" << endl
        << "  --slave   -s <index>  Positive numerical ring position," << endl
        << "                        or 'all' for all slaves (default)." << endl
        << "  --verbose -v          Show detailed slave information." << endl
        << endl
        << numericInfo();

    return str.str();
}

/****************************************************************************/

void CommandSlaves::execute(MasterDevice &m, const StringVector &args)
{
    m.open(MasterDevice::Read);

    if (getVerbosity() == Verbose) {
        if (slavePosition == -1) {
            unsigned int numSlaves = m.slaveCount(), i;

            for (i = 0; i < numSlaves; i++) {
                showSlave(m, i);
            }
        } else {
            showSlave(m, slavePosition);
        }
    } else {
        listSlaves(m, slavePosition);
    }
}

/****************************************************************************/

void CommandSlaves::listSlaves(
        MasterDevice &m,
        int slavePosition
        )
{
    unsigned int numSlaves, i;
    ec_ioctl_slave_t slave;
    uint16_t lastAlias, aliasIndex;
    Info info;
    typedef list<Info> InfoList;
    InfoList infoList;
    InfoList::const_iterator iter;
    stringstream str;
    unsigned int maxPosWidth = 0, maxAliasWidth = 0,
                 maxRelPosWidth = 0, maxStateWidth = 0;
    
    numSlaves = m.slaveCount();

    lastAlias = 0;
    aliasIndex = 0;
    for (i = 0; i < numSlaves; i++) {
        m.getSlave(&slave, i);
        
        if (slave.alias) {
            lastAlias = slave.alias;
            aliasIndex = 0;
        }

        if (slavePosition == -1 || i == (unsigned int) slavePosition) {
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

            info.state = alStateString(slave.state);
            info.flag = (slave.error_flag ? 'E' : '+');

            if (strlen(slave.name)) {
                info.name = slave.name;
            } else {
                str << "0x" << hex << setfill('0')
                    << setw(8) << slave.vendor_id << ":0x"
                    << setw(8) << slave.product_code;
                info.name = str.str();
                str.str("");
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
        }

        aliasIndex++;
    }

    for (iter = infoList.begin(); iter != infoList.end(); iter++) {
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

void CommandSlaves::showSlave(
        MasterDevice &m,
        uint16_t slavePosition
        )
{
    ec_ioctl_slave_t slave;
    list<string> protoList;
    list<string>::const_iterator protoIter;
    
    m.getSlave(&slave, slavePosition);
        
    cout << "=== Slave " << dec << slavePosition << " ===" << endl;
    
    if (slave.alias)
        cout << "Alias: " << slave.alias << endl;

    cout
        << "State: " << alStateString(slave.state) << endl
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
