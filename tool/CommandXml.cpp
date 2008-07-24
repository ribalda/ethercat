/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <iostream>
#include <iomanip>
using namespace std;

#include "CommandXml.h"

/*****************************************************************************/

CommandXml::CommandXml():
    Command("xml", "Generate slave information XML.")
{
}

/*****************************************************************************/

string CommandXml::helpString() const
{
    stringstream str;

    str << getName() << " [OPTIONS]"
        << endl
        << getBriefDescription() << endl
        << endl
        << "Note that the Pdo information can either originate" << endl
        << "from the SII or from the CoE communication area. For" << endl
        << "some slaves, this is dependant on the last slave" << endl
        << "configuration." << endl
        << endl
        << "Command-specific options:" << endl
        << "  --slave -s <index>  Positive numerical ring position," << endl
        << "                      or 'all' for all slaves (default)." << endl
        << endl
        << numericInfo();

    return str.str();
}

/****************************************************************************/

void CommandXml::execute(MasterDevice &m, const StringVector &args)
{
    m.open(MasterDevice::Read);

    if (slavePosition == -1) {
        unsigned int numSlaves = m.slaveCount(), i;

        for (i = 0; i < numSlaves; i++) {
            generateSlaveXml(m, i);
        }
    } else {
        generateSlaveXml(m, slavePosition);
    }
}

/****************************************************************************/

void CommandXml::generateSlaveXml(MasterDevice &m, uint16_t slavePosition)
{
    ec_ioctl_slave_t slave;
    ec_ioctl_slave_sync_t sync;
    ec_ioctl_slave_sync_pdo_t pdo;
    string pdoType;
    ec_ioctl_slave_sync_pdo_entry_t entry;
    unsigned int i, j, k;
    
    m.getSlave(&slave, slavePosition);

    cout
        << "<?xml version=\"1.0\" ?>" << endl
        << "  <EtherCATInfo>" << endl
        << "    <!-- Slave " << slave.position << " -->" << endl
        << "    <Vendor>" << endl
        << "      <Id>" << slave.vendor_id << "</Id>" << endl
        << "    </Vendor>" << endl
        << "    <Descriptions>" << endl
        << "      <Devices>" << endl
        << "        <Device>" << endl
        << "          <Type ProductCode=\"#x"
        << hex << setfill('0') << setw(8) << slave.product_code
        << "\" RevisionNo=\"#x"
        << hex << setfill('0') << setw(8) << slave.revision_number
        << "\">" << slave.order << "</Type>" << endl;

    if (strlen(slave.name)) {
        cout
            << "          <Name><![CDATA["
            << slave.name
            << "]]></Name>" << endl;
    }

    for (i = 0; i < slave.sync_count; i++) {
        m.getSync(&sync, slavePosition, i);

        cout
            << "          <Sm Enable=\"" << dec << (unsigned int) sync.enable
            << "\" StartAddress=\"" << sync.physical_start_address
            << "\" ControlByte=\"" << (unsigned int) sync.control_register
            << "\" DefaultSize=\"" << sync.default_size
            << "\" />" << endl;
    }

    for (i = 0; i < slave.sync_count; i++) {
        m.getSync(&sync, slavePosition, i);

        for (j = 0; j < sync.pdo_count; j++) {
            m.getPdo(&pdo, slavePosition, i, j);
            pdoType = (sync.control_register & 0x04 ? "R" : "T");
            pdoType += "xPdo";

            cout
                << "          <" << pdoType
                << " Sm=\"" << i << "\" Fixed=\"1\" Mandatory=\"1\">" << endl
                << "            <Index>#x"
                << hex << setfill('0') << setw(4) << pdo.index
                << "</Index>" << endl
                << "            <Name>" << pdo.name << "</Name>" << endl;

            for (k = 0; k < pdo.entry_count; k++) {
                m.getPdoEntry(&entry, slavePosition, i, j, k);

                cout
                    << "            <Entry>" << endl
                    << "              <Index>#x"
                    << hex << setfill('0') << setw(4) << entry.index
                    << "</Index>" << endl;
                if (entry.index)
                    cout
                        << "              <SubIndex>"
                        << dec << (unsigned int) entry.subindex
                        << "</SubIndex>" << endl;
                
                cout
                    << "              <BitLen>"
                    << dec << (unsigned int) entry.bit_length
                    << "</BitLen>" << endl;

                if (entry.index) {
                    cout
                        << "              <Name>" << entry.name
                        << "</Name>" << endl
                        << "              <DataType>";

                    if (entry.bit_length == 1) {
                        cout << "BOOL";
                    } else if (!(entry.bit_length % 8)) {
                        if (entry.bit_length <= 64)
                            cout << "UINT" << (unsigned int) entry.bit_length;
                        else
                            cout << "STRING("
                                << (unsigned int) (entry.bit_length / 8)
                                << ")";
                    } else {
                        cerr << "Invalid bit length "
                            << (unsigned int) entry.bit_length << endl;
                    }

                        cout << "</DataType>" << endl;
                }

                cout << "            </Entry>" << endl;
            }

            cout
                << "          </" << pdoType << ">" << endl;
        }
    }

    cout
        << "        </Device>" << endl
        << "     </Devices>" << endl
        << "  </Descriptions>" << endl
        << "</EtherCATInfo>" << endl;
}
/*****************************************************************************/
