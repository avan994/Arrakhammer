#pragma once;

#include <Common.h>
#include "MicroManager.h"
#include <unordered_map>

namespace UAlbertaBot
{
class MicroManager;

class MeleeManager : public MicroManager
{

public:

	MeleeManager();

	bool runby_done;
	std::unordered_map<BWAPI::Unit, BWAPI::Position> runby_triggered;
	std::unordered_map<BWAPI::Unit, bool> runby_location_reached;

	std::unordered_map<BWAPI::Unit, BWAPI::Position> randomMovementBias;

	void executeMicro(const BWAPI::Unitset & targets);
	void assignTargets(const BWAPI::Unitset & targets);

	int getAttackPriority(BWAPI::Unit attacker, BWAPI::Unit unit);
	BWAPI::Unit getTarget(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets);
    bool meleeUnitShouldRetreat(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets);
	//void filterMeleeTargets(const BWAPI::Unit & unit, const BWAPI::Unitset & targets, BWAPI::Unitset & meleeUnitTargets, bool individualCheck);
};
}