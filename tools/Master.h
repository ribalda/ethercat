/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#ifndef __EC_MASTER_H__
#define __EC_MASTER_H__

#include <stdexcept>
using namespace std;

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

        unsigned int slaveCount();
        void listSlaves();

    protected:
        string slaveState(uint8_t) const;
        
    private:
        unsigned int index;
        int fd;
};

/****************************************************************************/

#endif
