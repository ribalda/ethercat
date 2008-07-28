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

    protected:
        void loadSiiData(ec_ioctl_slave_sii_t *, const istream &);
        void checkSiiData(const ec_ioctl_slave_sii_t *data);
};

/****************************************************************************/

#endif
