/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#include <sys/types.h>

#include <string>
#include <sstream>
#include <vector>
using namespace std;

#include "MasterDevice.h"

/*****************************************************************************/

enum Verbosity {
    Quiet,
    Normal,
    Verbose
};

extern string commandName;
extern unsigned int masterIndex;
extern int slavePosition;
extern int domainIndex;
extern vector<string> commandArgs;
extern Verbosity verbosity;
extern string dataTypeStr;
extern bool force;

extern MasterDevice masterDev;

/****************************************************************************/

class InvalidUsageException:
    public runtime_error
{
    public:
        /** Constructor with std::string parameter. */
        InvalidUsageException(
                const stringstream &s /**< Message. */
                ): runtime_error(s.str()) {}
};

/****************************************************************************/

class ExecutionFailureException:
    public runtime_error
{
    public:
        /** Constructor with std::string parameter. */
        ExecutionFailureException(
                const stringstream &s /**< Message. */
                ): runtime_error(s.str()) {}
};

/*****************************************************************************/

#define swap16(x) \
        ((uint16_t)( \
        (((uint16_t)(x) & 0x00ffU) << 8) | \
        (((uint16_t)(x) & 0xff00U) >> 8) ))
#define swap32(x) \
        ((uint32_t)( \
        (((uint32_t)(x) & 0x000000ffUL) << 24) | \
        (((uint32_t)(x) & 0x0000ff00UL) <<  8) | \
        (((uint32_t)(x) & 0x00ff0000UL) >>  8) | \
        (((uint32_t)(x) & 0xff000000UL) >> 24) ))

#if __BYTE_ORDER == __LITTLE_ENDIAN

#define le16tocpu(x) x
#define le32tocpu(x) x

#define cputole16(x) x
#define cputole32(x) x

#elif __BYTE_ORDER == __BIG_ENDIAN

#define le16tocpu(x) swap16(x)
#define le32tocpu(x) swap32(x)

#define cputole16(x) swap16(x)
#define cputole32(x) swap32(x)

#endif

/****************************************************************************/

enum {BreakAfterBytes = 16};
enum {DefaultBufferSize = 1024};

void printRawData(const uint8_t *, unsigned int);

/****************************************************************************/
