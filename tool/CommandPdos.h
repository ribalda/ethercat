/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#ifndef __COMMANDPDOS_H__
#define __COMMANDPDOS_H__

#include "Command.h"

/****************************************************************************/

class CommandPdos:
    public Command
{
    public:
        CommandPdos();

        string helpString() const;
        void execute(MasterDevice &, const StringVector &);

    protected:
		void listSlavePdos(MasterDevice &, const ec_ioctl_slave_t &, bool);
};

/****************************************************************************/

#endif
