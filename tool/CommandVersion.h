/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#ifndef __COMMANDVERSION_H__
#define __COMMANDVERSION_H__

#include "Command.h"

/****************************************************************************/

class CommandVersion:
    public Command
{
    public:
        CommandVersion();

        string helpString() const;
        void execute(MasterDevice &, const StringVector &);
};

/****************************************************************************/

#endif
