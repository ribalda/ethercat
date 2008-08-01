/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#ifndef __COMMANDDOWNLOAD_H__
#define __COMMANDDOWNLOAD_H__

#include "SdoCommand.h"

/****************************************************************************/

class CommandDownload:
    public SdoCommand
{
    public:
        CommandDownload();

        string helpString() const;
        void execute(MasterDevice &, const StringVector &);

	protected:
		enum {DefaultBufferSize = 1024};
};

/****************************************************************************/

#endif
