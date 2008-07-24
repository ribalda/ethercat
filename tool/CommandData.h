/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#ifndef __COMMANDDATA_H__
#define __COMMANDDATA_H__

#include "Command.h"

/****************************************************************************/

class CommandData:
    public Command
{
    public:
        CommandData();

        string helpString() const;
        void execute(MasterDevice &, const StringVector &);

    protected:
		void outputDomainData(MasterDevice &, unsigned int);
};

/****************************************************************************/

#endif
