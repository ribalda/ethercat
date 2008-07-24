/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#ifndef __COMMANDDOWNLOAD_H__
#define __COMMANDDOWNLOAD_H__

#include "Command.h"

/****************************************************************************/

class CommandDownload:
    public Command
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
