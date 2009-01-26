/*****************************************************************************
 *
 * $Id:$
 *
 ****************************************************************************/

#ifndef __COMMANDFOEWRITE_H__
#define __COMMANDFOEWRITE_H__

#include "FoeCommand.h"

/****************************************************************************/

class CommandFoeWrite:
    public FoeCommand
{
    public:
        CommandFoeWrite();

        string helpString() const;
        void execute(MasterDevice &, const StringVector &);

    protected:
        void loadFoeData(ec_ioctl_slave_foe_t *, const istream &);
};

/****************************************************************************/

#endif
