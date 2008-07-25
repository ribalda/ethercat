/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#ifndef __COMMANDSLAVES_H__
#define __COMMANDSLAVES_H__

#include "Command.h"

/****************************************************************************/

class CommandSlaves:
    public Command
{
    public:
        CommandSlaves();

        string helpString() const;
        void execute(MasterDevice &, const StringVector &);

    protected:
        struct Info {
            string pos;
            string alias;
            string relPos;
            string state;
            string flag;
            string name;
        };

        void listSlaves(MasterDevice &, int);
        void showSlave(MasterDevice &, uint16_t);
};

/****************************************************************************/

#endif
