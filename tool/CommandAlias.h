/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#ifndef __COMMANDALIAS_H__
#define __COMMANDALIAS_H__

#include "Command.h"

/****************************************************************************/

class CommandAlias:
    public Command
{
    public:
        CommandAlias();

        string helpString() const;
        void execute(MasterDevice &, const StringVector &);

    protected:
        void writeSlaveAlias(MasterDevice &, uint16_t, uint16_t);
};

/****************************************************************************/

#endif
