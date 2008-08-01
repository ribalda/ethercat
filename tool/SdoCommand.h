/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#ifndef __SDOCOMMAND_H__
#define __SDOCOMMAND_H__

#include "Command.h"

/****************************************************************************/

class SdoCommand:
    public Command
{
    public:
        SdoCommand(const string &, const string &);

        struct DataType {
            const char *name;
            uint16_t coeCode;
            unsigned int byteSize;
        };
        static const DataType *findDataType(const string &);
        static const DataType *findDataType(uint16_t);
        static const char *abortText(uint32_t);

    private:
        struct AbortMessage {
            uint32_t code;
            const char *message;
        };

        static const DataType dataTypes[];
        static const AbortMessage abortMessages[];
};

/****************************************************************************/

#endif
