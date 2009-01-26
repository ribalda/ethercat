/*****************************************************************************
 *
 * $Id:$
 *
 ****************************************************************************/

#ifndef __COMMANDFOEREAD_H__
#define __COMMANDFOEREAD_H__

#include "FoeCommand.h"

/****************************************************************************/

class CommandFoeRead:
    public FoeCommand
{
    public:
        CommandFoeRead();

        string helpString() const;
        void execute(MasterDevice &, const StringVector &);
};

/****************************************************************************/

#endif
