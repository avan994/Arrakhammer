#pragma once;

#include <Common.h>
#include "MicroManager.h"

namespace UAlbertaBot
{
	class LurkerManager : public MicroManager
	{
	private:
		int lurkerRange = BWAPI::UnitTypes::Zerg_Lurker.groundWeapon().maxRange();

		int AMBUSH_TIME = 10 * 20;
		std::unordered_map<BWAPI::Unit, int> ambushStatus;
		BWAPI::Unitset BFFs;
		void startAmbush(BWAPI::Unit);

		bool ambush;
		int _lastAmbushSwitchTime;

		void switchAmbush(bool);

	public:

		LurkerManager();
		void executeMicro(const BWAPI::Unitset & targets);

		int getAttackPriority(BWAPI::Unit rangedUnit, BWAPI::Unit target);
		BWAPI::Unit getTarget(BWAPI::Unit rangedUnit, const BWAPI::Unitset & targets);

		int getLineSplashPriority(BWAPI::Unit rangedUnit, BWAPI::Unit target, BWAPI::Unitset & targets);
	};
}