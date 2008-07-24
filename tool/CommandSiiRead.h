/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#ifndef __COMMANDSIIREAD_H__
#define __COMMANDSIIREAD_H__

#include "Command.h"

/****************************************************************************/

class CommandSiiRead:
    public Command
{
    public:
        CommandSiiRead();

        string helpString() const;
        void execute(MasterDevice &, const StringVector &);

    protected:
		struct CategoryName {
			uint16_t type;
			const char *name;
		};
		static const CategoryName categoryNames[];
		static const char *getCategoryName(uint16_t);
};

/****************************************************************************/

#endif
