/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#ifndef __COMMANDSIIWRITE_H__
#define __COMMANDSIIWRITE_H__

#include "Command.h"

/****************************************************************************/

class CommandSiiWrite:
    public Command
{
    public:
        CommandSiiWrite();

        string helpString() const;
        void execute(MasterDevice &, const StringVector &);
};

/****************************************************************************/

#endif
