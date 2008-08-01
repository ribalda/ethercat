/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#ifndef __COMMANDSDOS_H__
#define __COMMANDSDOS_H__

#include "SdoCommand.h"

/****************************************************************************/

class CommandSdos:
    public SdoCommand
{
    public:
        CommandSdos();

        string helpString() const;
        void execute(MasterDevice &, const StringVector &);

    protected:
		void listSlaveSdos(MasterDevice &, const ec_ioctl_slave_t &, bool);
};

/****************************************************************************/

#endif
