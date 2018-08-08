#pragma once;

#include <Common.h>
#include "MicroManager.h"

namespace UAlbertaBot
{
class StaticDefenseManager : public MicroManager
{
public:

	StaticDefenseManager();
	void executeMicro(const BWAPI::Unitset & targets);

	std::unordered_map<BWAPI::Unit, int> targeted;

	int getAttackPriority(BWAPI::Unit, BWAPI::Unit);
	BWAPI::Unit getTarget(BWAPI::Unit, const BWAPI::Unitset &);

    void assignTargets(const BWAPI::Unitset & targets);
};
}