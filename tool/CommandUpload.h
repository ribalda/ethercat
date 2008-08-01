/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#ifndef __COMMANDUPLOAD_H__
#define __COMMANDUPLOAD_H__

#include "SdoCommand.h"

/****************************************************************************/

class CommandUpload:
    public SdoCommand
{
    public:
        CommandUpload();

        string helpString() const;
        void execute(MasterDevice &, const StringVector &);

    protected:
		enum {DefaultBufferSize = 1024};

        static void printRawData(const uint8_t *, unsigned int);
};

/****************************************************************************/

#endif
