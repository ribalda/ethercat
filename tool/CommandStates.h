/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#ifndef __COMMANDSTATES_H__
#define __COMMANDSTATES_H__

#include "Command.h"

/****************************************************************************/

class CommandStates:
    public Command
{
    public:
        CommandStates();

        string helpString() const;
        void execute(MasterDevice &, const StringVector &);
};

/****************************************************************************/

#endif
