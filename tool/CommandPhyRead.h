/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#ifndef __COMMANDPHYREAD_H__
#define __COMMANDPHYREAD_H__

#include "Command.h"

/****************************************************************************/

class CommandPhyRead:
    public Command
{
    public:
        CommandPhyRead();

        string helpString() const;
        void execute(MasterDevice &, const StringVector &);
};

/****************************************************************************/

#endif
