/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <list>
#include <iostream>
#include <iomanip>
#include <sstream>
using namespace std;

#include "CommandConfig.h"
#include "byteorder.h"

/*****************************************************************************/

CommandConfig::CommandConfig():
    Command("config", "Show slave configurations.")
{
}

/*****************************************************************************/

string CommandConfig::helpString() const
{
    stringstream str;

    str << getName() << " [OPTIONS]" << endl
    	<< endl
    	<< getBriefDescription() << endl
    	<< endl
    	<< "Without the --verbose option, slave configurations are" << endl
    	<< "output one-per-line. Example:" << endl
    	<< endl
    	<< "1001:0  0x0000003b/0x02010000  3  OP" << endl
    	<< "|       |                      |  |" << endl
    	<< "|       |                      |  \\- Application-layer" << endl
    	<< "|       |                      |     state of the attached" << endl
    	<< "|       |                      |     slave, or '-', if no" << endl
    	<< "|       |                      |     slave is attached." << endl
    	<< "|       |                      \\- Absolute decimal ring" << endl
    	<< "|       |                         position of the attached" << endl
    	<< "|       |                         slave, or '-' if none" << endl
    	<< "|       |                         attached." << endl
    	<< "|       \\- Vendor ID and product code (both" << endl
    	<< "|          hexadecimal)." << endl
    	<< "\\- Alias and relative position (both decimal)." << endl
    	<< endl
    	<< "With the --verbose option given, the configured Pdos and" << endl
    	<< "Sdos are output in addition." << endl
    	<< endl
    	<< "Command-specific options:" << endl
    	<< "  --verbose -v  Show detailed configurations." << endl;

	return str.str();
}

/*****************************************************************************/

bool operator<(
        const ec_ioctl_config_t &a,
        const ec_ioctl_config_t &b
        )
{
    return a.alias < b.alias
        || (a.alias == b.alias && a.position < b.position);
}

/*****************************************************************************/

/** Lists the bus configuration.
 */
void CommandConfig::execute(MasterDevice &m, const StringVector &args)
{
    ec_ioctl_master_t master;
    unsigned int i;
    ec_ioctl_config_t config;
    ConfigList configList;

    m.open(MasterDevice::Read);
    m.getMaster(&master);

    for (i = 0; i < master.config_count; i++) {
        m.getConfig(&config, i);
        configList.push_back(config);
    }

    configList.sort();

    if (getVerbosity() == Verbose) {
        showDetailedConfigs(m, configList);
    } else {
        listConfigs(m, configList);
    }
}

/*****************************************************************************/

/** Lists the complete bus configuration.
 */
void CommandConfig::showDetailedConfigs(
		MasterDevice &m,
		const ConfigList &configList
		)
{
    ConfigList::const_iterator configIter;
    unsigned int j, k, l;
    ec_ioctl_slave_t slave;
    ec_ioctl_config_pdo_t pdo;
    ec_ioctl_config_pdo_entry_t entry;
    ec_ioctl_config_sdo_t sdo;

    for (configIter = configList.begin();
            configIter != configList.end();
            configIter++) {

        cout << "Alias: "
            << dec << configIter->alias << endl
            << "Position: " << configIter->position << endl
            << "Vendor Id: 0x"
            << hex << setfill('0')
            << setw(8) << configIter->vendor_id << endl
            << "Product code: 0x"
            << setw(8) << configIter->product_code << endl
            << "Attached slave: ";
        
        if (configIter->slave_position != -1) {
            m.getSlave(&slave, configIter->slave_position);
            cout << configIter->slave_position
                << " (" << alStateString(slave.state) << ")" << endl;
        } else {
            cout << "none" << endl;
        }

        for (j = 0; j < EC_MAX_SYNC_MANAGERS; j++) {
            if (configIter->syncs[j].pdo_count) {
                cout << "SM" << dec << j << " ("
                    << (configIter->syncs[j].dir == EC_DIR_INPUT
                            ? "Input" : "Output") << ")" << endl;
                for (k = 0; k < configIter->syncs[j].pdo_count; k++) {
                    m.getConfigPdo(&pdo, configIter->config_index, j, k);

                    cout << "  Pdo 0x" << hex
                        << setw(4) << pdo.index
                        << " \"" << pdo.name << "\"" << endl;

                    for (l = 0; l < pdo.entry_count; l++) {
                        m.getConfigPdoEntry(&entry,
                                configIter->config_index, j, k, l);

                        cout << "    Pdo entry 0x" << hex
                            << setw(4) << entry.index << ":"
                            << setw(2) << (unsigned int) entry.subindex
                            << ", " << dec << (unsigned int) entry.bit_length
                            << " bit, \"" << entry.name << "\"" << endl;
                    }
                }
            }
        }

        cout << "Sdo configuration:" << endl;
        if (configIter->sdo_count) {
            for (j = 0; j < configIter->sdo_count; j++) {
                m.getConfigSdo(&sdo, configIter->config_index, j);

                cout << "  0x"
                    << hex << setfill('0')
                    << setw(4) << sdo.index << ":"
                    << setw(2) << (unsigned int) sdo.subindex
                    << ", " << dec << sdo.size << " byte: " << hex;

                switch (sdo.size) {
                    case 1:
                        cout << "0x" << setw(2)
                            << (unsigned int) *(uint8_t *) &sdo.data;
                        break;
                    case 2:
                        cout << "0x" << setw(4)
                            << le16tocpu(*(uint16_t *) &sdo.data);
                        break;
                    case 4:
                        cout << "0x" << setw(8)
                            << le32tocpu(*(uint32_t *) &sdo.data);
                        break;
                    default:
                        cout << "???";
                }

                cout << endl;
            }
        } else {
            cout << "  None." << endl;
        }

        cout << endl;
    }
}

/*****************************************************************************/

/** Lists the bus configuration.
 */
void CommandConfig::listConfigs(
        MasterDevice &m,
        const ConfigList &configList
        )
{
    ConfigList::const_iterator configIter;
    stringstream str;
    Info info;
    typedef list<Info> InfoList;
    InfoList list;
    InfoList::const_iterator iter;
    unsigned int maxAliasWidth = 0, maxPosWidth = 0,
                 maxSlavePosWidth = 0, maxStateWidth = 0;
    ec_ioctl_slave_t slave;

    for (configIter = configList.begin();
            configIter != configList.end();
            configIter++) {

        str << dec << configIter->alias;
        info.alias = str.str();
        str.clear();
        str.str("");

        str << configIter->position;
        info.pos = str.str();
        str.clear();
        str.str("");

        str << hex << setfill('0')
            << "0x" << setw(8) << configIter->vendor_id
            << "/0x" << setw(8) << configIter->product_code;
        info.ident = str.str();
        str.clear();
        str.str("");

        if (configIter->slave_position != -1) {
            m.getSlave(&slave, configIter->slave_position);

            str << configIter->slave_position;
            info.slavePos = str.str();
            str.clear();
            str.str("");

            str << alStateString(slave.state);
            info.state = str.str();
            str.clear();
            str.str("");
        } else {
            str << "-";
            info.slavePos = str.str();
            str.clear();
            str.str("");

            str << "-";
            info.state = str.str();
            str.clear();
            str.str("");
        }

        list.push_back(info);

        if (info.alias.length() > maxAliasWidth)
            maxAliasWidth = info.alias.length();
        if (info.pos.length() > maxPosWidth)
            maxPosWidth = info.pos.length();
        if (info.slavePos.length() > maxSlavePosWidth)
            maxSlavePosWidth = info.slavePos.length();
        if (info.state.length() > maxStateWidth)
            maxStateWidth = info.state.length();
    }

    for (iter = list.begin(); iter != list.end(); iter++) {
        cout << setfill(' ') << right
            << setw(maxAliasWidth) << iter->alias
            << ":" << left
            << setw(maxPosWidth) << iter->pos
            << "  "
            << iter->ident
            << "  "
            << setw(maxSlavePosWidth) << iter->slavePos << "  "
            << setw(maxStateWidth) << iter->state << "  "
            << endl;
    }
}

/*****************************************************************************/
