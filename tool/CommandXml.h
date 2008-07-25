/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#ifndef __COMMANDXML_H__
#define __COMMANDXML_H__

#include "Command.h"

/****************************************************************************/

class CommandXml:
    public Command
{
    public:
        CommandXml();

        string helpString() const;
        void execute(MasterDevice &, const StringVector &);

    protected:
        void generateSlaveXml(MasterDevice &, const ec_ioctl_slave_t &);
};

/****************************************************************************/

#endif
