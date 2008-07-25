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
    SlaveList slaves;
    
    m.open(MasterDevice::Read);
    slaves = selectedSlaves(m);

    if (getVerbosity() == Verbose) {
        showSlaves(m, slaves);
    } else {
        listSlaves(m, slaves);
    }
}

/****************************************************************************/

void CommandSlaves::listSlaves(
        MasterDevice &m,
        const SlaveList &slaves
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

        if (slaveInList(slave, slaves)) {
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

void CommandSlaves::showSlaves(
        MasterDevice &m,
        const SlaveList &slaves
        )
{
    SlaveList::const_iterator si;
    list<string> protoList;
    list<string>::const_iterator protoIter;

    for (si = slaves.begin(); si != slaves.end(); si++) {
        cout << "=== Slave " << dec << si->position << " ===" << endl;

        if (si->alias)
            cout << "Alias: " << si->alias << endl;

        cout
            << "State: " << alStateString(si->al_state) << endl
            << "Flag: " << (si->error_flag ? 'E' : '+') << endl
            << "Identity:" << endl
            << "  Vendor Id:       0x"
            << hex << setfill('0')
            << setw(8) << si->vendor_id << endl
            << "  Product code:    0x"
            << setw(8) << si->product_code << endl
            << "  Revision number: 0x"
            << setw(8) << si->revision_number << endl
            << "  Serial number:   0x"
            << setw(8) << si->serial_number << endl;

        if (si->mailbox_protocols) {
            cout << "Mailboxes:" << endl
                << "  RX: 0x"
                << hex << setw(4) << si->rx_mailbox_offset << "/"
                << dec << si->rx_mailbox_size
                << ", TX: 0x"
                << hex << setw(4) << si->tx_mailbox_offset << "/"
                << dec << si->tx_mailbox_size << endl
                << "  Supported protocols: ";

            if (si->mailbox_protocols & EC_MBOX_AOE) {
                protoList.push_back("AoE");
            }
            if (si->mailbox_protocols & EC_MBOX_EOE) {
                protoList.push_back("EoE");
            }
            if (si->mailbox_protocols & EC_MBOX_COE) {
                protoList.push_back("CoE");
            }
            if (si->mailbox_protocols & EC_MBOX_FOE) {
                protoList.push_back("FoE");
            }
            if (si->mailbox_protocols & EC_MBOX_SOE) {
                protoList.push_back("SoE");
            }
            if (si->mailbox_protocols & EC_MBOX_VOE) {
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

        if (si->has_general_category) {
            cout << "General:" << endl
                << "  Group: " << si->group << endl
                << "  Image name: " << si->image << endl
                << "  Order number: " << si->order << endl
                << "  Device name: " << si->name << endl;

            if (si->mailbox_protocols & EC_MBOX_COE) {
                cout << "  CoE details:" << endl
                    << "    Enable Sdo: "
                    << (si->coe_details.enable_sdo ? "yes" : "no") << endl
                    << "    Enable Sdo Info: "
                    << (si->coe_details.enable_sdo_info ? "yes" : "no") << endl
                    << "    Enable Pdo Assign: "
                    << (si->coe_details.enable_pdo_assign
                            ? "yes" : "no") << endl
                    << "    Enable Pdo Configuration: "
                    << (si->coe_details.enable_pdo_configuration
                            ? "yes" : "no") << endl
                    << "    Enable Upload at startup: "
                    << (si->coe_details.enable_upload_at_startup
                            ? "yes" : "no") << endl
                    << "    Enable Sdo complete access: "
                    << (si->coe_details.enable_sdo_complete_access
                            ? "yes" : "no") << endl;
            }

            cout << "  Flags:" << endl
                << "    Enable SafeOp: "
                << (si->general_flags.enable_safeop ? "yes" : "no") << endl
                << "    Enable notLRW: "
                << (si->general_flags.enable_not_lrw ? "yes" : "no") << endl
                << "  Current consumption: "
                << dec << si->current_on_ebus << " mA" << endl;
        }
    }
}

/****************************************************************************/

bool CommandSlaves::slaveInList(
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

/*****************************************************************************/
