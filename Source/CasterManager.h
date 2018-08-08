#pragma once;

#include <Common.h>
#include "MicroManager.h"

namespace UAlbertaBot
{
	class MicroManager;

	class CasterManager : public MicroManager
	{

		BWAPI::Player _self = BWAPI::Broodwar->self();
		BWAPI::Unit unitClosestToEnemy;
		BWAPI::Position mainBasePosition;

		bool runToAlly(BWAPI::Unit, BWAPI::Unit);
		bool runToBase(BWAPI::Unit);
		void cautiouslyTravel(BWAPI::Unit, BWAPI::Position);


		//void assignTargets(const BWAPI::Unitset & targets);

		BWAPI::Unitset broodlingTargeted;
		void assignQueenTargets(const BWAPI::Unitset & units, const BWAPI::Unitset & targets);
		int getBroodlingPriority(BWAPI::Unit unit, BWAPI::Unit target);
		bool canBroodling(BWAPI::Unit target);

		int infestedValue = BWAPI::UnitTypes::Zerg_Infested_Terran.mineralPrice() + BWAPI::UnitTypes::Zerg_Infested_Terran.gasPrice();
		void assignInfestedTargets(const BWAPI::Unitset & units, const BWAPI::Unitset & targets);
		int getExplosionPriority(BWAPI::Unit unit, BWAPI::Unit target);
		int computeExplosionDamage(BWAPI::Position location);
		int computeSwarmValue(BWAPI::Position p);

		//std::unordered_map <BWAPI::Unit, BWAPI::Unitset> consuming;
		int swarmCastTime;
		void assignDefilerTargets(const BWAPI::Unitset &, const BWAPI::Unitset &);
		bool consume(BWAPI::Unit);
		BWAPI::Position selectDarkSwarmLocation(BWAPI::Unit);
		int computePlagueDamage(BWAPI::Position);


	public:

		void setUnitClosestToEnemy(BWAPI::Unit unit) { unitClosestToEnemy = unit; }

		void executeMicro(const BWAPI::Unitset & targets);

		CasterManager();
		~CasterManager() {}
	};
}