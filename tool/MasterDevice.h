/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#ifndef __EC_MASTER_H__
#define __EC_MASTER_H__

#include <stdexcept>
#include <string>
#include <vector>
#include <list>
using namespace std;

#include "../include/ecrt.h"
#include "../master/ioctl.h"

/****************************************************************************/

class MasterDeviceException:
    public runtime_error
{
    public:
        /** Constructor with std::string parameter. */
        MasterDeviceException(
                const string &s /**< Message. */
                ): runtime_error(s) {}
};

/****************************************************************************/

class MasterDevice
{
    public:
        MasterDevice();
        ~MasterDevice();

        void setIndex(unsigned int);

        enum Permissions {Read, ReadWrite};
        void open(Permissions);
        void close();

        unsigned int slaveCount();

        void getMaster(ec_ioctl_master_t *);
        void getConfig(ec_ioctl_config_t *, unsigned int);
        void getConfigPdo(ec_ioctl_config_pdo_t *, unsigned int, uint8_t,
                uint16_t);
        void getConfigPdoEntry(ec_ioctl_config_pdo_entry_t *, unsigned int,
                uint8_t, uint16_t, uint8_t);
        void getConfigSdo(ec_ioctl_config_sdo_t *, unsigned int, unsigned int);
        void getDomain(ec_ioctl_domain_t *, unsigned int);
        void getFmmu(ec_ioctl_domain_fmmu_t *, unsigned int, unsigned int);
        void getData(ec_ioctl_domain_data_t *, unsigned int, unsigned int,
                unsigned char *);
        void getSlave(ec_ioctl_slave_t *, uint16_t);
        void getSync(ec_ioctl_slave_sync_t *, uint16_t, uint8_t);
        void getPdo(ec_ioctl_slave_sync_pdo_t *, uint16_t, uint8_t, uint8_t);
        void getPdoEntry(ec_ioctl_slave_sync_pdo_entry_t *, uint16_t, uint8_t,
                uint8_t, uint8_t);
        void getSdo(ec_ioctl_slave_sdo_t *, uint16_t, uint16_t);
        void getSdoEntry(ec_ioctl_slave_sdo_entry_t *, uint16_t, int, uint8_t);
        void readSii(ec_ioctl_slave_sii_t *);
        void writeSii(ec_ioctl_slave_sii_t *);

    protected:
#if 0
        void outputDomainData(unsigned int);
        enum {BreakAfterBytes = 16};
        void showDomain(unsigned int);
        void listSlavePdos(uint16_t, bool = false);
        void listSlaveSdos(uint16_t, bool = false);
        void listSlaves(int);
        void showSlave(uint16_t);
        void generateSlaveXml(uint16_t);
        void requestState(uint16_t, uint8_t);

        static string slaveState(uint8_t);
        static void printRawData(const uint8_t *, unsigned int);
        static uint8_t calcSiiCrc(const uint8_t *, unsigned int);
#endif
        
    private:
        //enum {DefaultBufferSize = 1024};

        unsigned int index;
        int fd;
};

/****************************************************************************/

#endif
