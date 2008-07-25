/*****************************************************************************
 *
 * $Id$
 *
 ****************************************************************************/

#ifndef __COMMANDCONFIG_H__
#define __COMMANDCONFIG_H__

#include <list>
using namespace std;

#include "Command.h"

/****************************************************************************/

class CommandConfig:
    public Command
{
    public:
        CommandConfig();

        string helpString() const;
        void execute(MasterDevice &, const StringVector &);

	protected:
		struct Info {
			string alias;
			string pos;
			string ident;
			string slavePos;
			string state;
		};

		typedef list<ec_ioctl_config_t> ConfigList;

		void showDetailedConfigs(MasterDevice &, const ConfigList &);
		void listConfigs(MasterDevice &m, const ConfigList &);
};

/****************************************************************************/

#endif
