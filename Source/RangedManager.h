#pragma once;

#include <Common.h>
#include "MicroManager.h"

namespace UAlbertaBot
{
class RangedManager : public MicroManager
{
public:

	RangedManager();
	void executeMicro(const BWAPI::Unitset & targets);

	bool scoutedBaseRecently; //for mutalisks
	int lastTimeScouted;

	BWAPI::Unitset targeted;

	int getAttackPriority(BWAPI::Unit rangedUnit, BWAPI::Unit target);
	BWAPI::Unit getTarget(BWAPI::Unit rangedUnit, const BWAPI::Unitset & targets);

    void assignTargets(const BWAPI::Unitset & targets);
};
}