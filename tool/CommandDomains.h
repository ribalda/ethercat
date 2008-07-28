/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#ifndef __COMMANDDOMAINS_H__
#define __COMMANDDOMAINS_H__

#include "Command.h"

/****************************************************************************/

class CommandDomains:
    public Command
{
    public:
        CommandDomains();

        string helpString() const;
        void execute(MasterDevice &, const StringVector &);

    protected:
		void showDomain(MasterDevice &, const ec_ioctl_domain_t &);
};

/****************************************************************************/

#endif
