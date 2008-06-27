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
using namespace std;

#include "../include/ecrt.h"
#include "../master/ioctl.h"

/****************************************************************************/

class MasterException:
    public runtime_error
{
    public:
        /** Constructor with std::string parameter. */
        MasterException(
                const string &s /**< Message. */
                ): runtime_error(s) {}

        /** Constructor with const char pointer parameter. */
        MasterException(
                const char *s /**< Message. */
                ): runtime_error(s) {}
};

/****************************************************************************/

class Master
{
    public:
        Master();
        ~Master();

        void setIndex(unsigned int);

        void writeAlias(int, bool, const vector<string> &);
        void showConfig();
        void outputData(int);
        void setDebug(const vector<string> &);
        void showDomains(int);
        void showMaster();
        void listPdos(int, bool = false);
        void listSdos(int, bool = false);
        void sdoDownload(int, const string &, const vector<string> &);
        void sdoUpload(int, const string &, const vector<string> &);
        void showSlaves(int, bool);
        void siiRead(int);
        void siiWrite(int, bool, const vector<string> &);
        void requestStates(int, const vector<string> &);
        void generateXml(int);

    protected:
        enum Permissions {Read, ReadWrite};
        void open(Permissions);
        void close();

        void writeSlaveAlias(uint16_t, uint16_t);
        void outputDomainData(unsigned int);
        void showDomain(unsigned int);
        void listSlavePdos(uint16_t, bool = false, bool = false);
        void listSlaveSdos(uint16_t, bool = false, bool = false);
        void listSlaves(int);
        void showSlave(uint16_t);
        void generateSlaveXml(uint16_t);
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
        void getData(ec_ioctl_data_t *, unsigned int, unsigned int,
                unsigned char *);
        void getSlave(ec_ioctl_slave_t *, uint16_t);
        void getSync(ec_ioctl_sync_t *, uint16_t, uint8_t);
        void getPdo(ec_ioctl_pdo_t *, uint16_t, uint8_t, uint8_t);
        void getPdoEntry(ec_ioctl_pdo_entry_t *, uint16_t, uint8_t, uint8_t,
                uint8_t);
        void getSdo(ec_ioctl_sdo_t *, uint16_t, uint16_t);
        void getSdoEntry(ec_ioctl_sdo_entry_t *, uint16_t, int, uint8_t);
        void requestState(uint16_t, uint8_t);

        static string slaveState(uint8_t);
        static void printRawData(const uint8_t *, unsigned int);
        static uint8_t calcSiiCrc(const uint8_t *, unsigned int);
        
    private:
        enum {DefaultBufferSize = 1024};

        unsigned int index;
        int fd;
};

/****************************************************************************/

#endif
