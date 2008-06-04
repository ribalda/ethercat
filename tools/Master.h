/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#ifndef __EC_MASTER_H__
#define __EC_MASTER_H__

#include <stdexcept>
using namespace std;

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

        void open(unsigned int);
        void close();

        void outputData(int);
        void showDomains(int);
        void listSlaves();
        void listPdos(int);
        void generateXml(int);

    protected:
        void outputDomainData(unsigned int);
        void showDomain(unsigned int);
        void listSlavePdos(uint16_t, bool = false);
        void generateSlaveXml(uint16_t);
        unsigned int domainCount();
        unsigned int slaveCount();
        void slaveSyncs(uint16_t);
        void getDomain(ec_ioctl_domain_t *, unsigned int);
        void getFmmu(ec_ioctl_domain_fmmu_t *, unsigned int, unsigned int);
        void getData(ec_ioctl_data_t *, unsigned int, unsigned int,
                unsigned char *);
        void getSlave(ec_ioctl_slave_t *, uint16_t);
        void getSync(ec_ioctl_sync_t *, uint16_t, uint8_t);
        void getPdo(ec_ioctl_pdo_t *, uint16_t, uint8_t, uint8_t);
        void getPdoEntry(ec_ioctl_pdo_entry_t *, uint16_t, uint8_t, uint8_t,
                uint8_t);

        static string slaveState(uint8_t);
        
    private:
        unsigned int index;
        int fd;
};

/****************************************************************************/

#endif
