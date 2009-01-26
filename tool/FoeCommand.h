/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#ifndef __FOECOMMAND_H__
#define __FOECOMMAND_H__

#include "Command.h"

/****************************************************************************/

class FoeCommand:
    public Command
{
    public:
        FoeCommand(const string &, const string &);

    protected:
		static std::string resultText(int);
		static std::string errorText(int);
};

/****************************************************************************/

#endif
