/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#ifndef __COMMANDMASTER_H__
#define __COMMANDMASTER_H__

#include "Command.h"

/****************************************************************************/

class CommandMaster:
    public Command
{
    public:
        CommandMaster();

        string helpString() const;
        void execute(MasterDevice &, const StringVector &);
};

/****************************************************************************/

#endif
