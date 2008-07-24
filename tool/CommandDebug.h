/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#ifndef __COMMANDDEBUG_H__
#define __COMMANDDEBUG_H__

#include "Command.h"

/****************************************************************************/

class CommandDebug:
    public Command
{
    public:
        CommandDebug();

        string helpString() const;
        void execute(MasterDevice &, const StringVector &);
};

/****************************************************************************/

#endif
