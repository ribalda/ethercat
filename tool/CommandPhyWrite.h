/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#ifndef __COMMANDPHYWRITE_H__
#define __COMMANDPHYWRITE_H__

#include "Command.h"

/****************************************************************************/

class CommandPhyWrite:
    public Command
{
    public:
        CommandPhyWrite();

        string helpString() const;
        void execute(MasterDevice &, const StringVector &);

    private:
        void loadPhyData(ec_ioctl_slave_phy_t *, const istream &);
};

/****************************************************************************/

#endif
