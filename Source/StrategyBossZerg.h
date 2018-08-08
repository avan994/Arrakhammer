#pragma once

#include "BuildOrder.h"
#include "BuildOrderQueue.h"

namespace UAlbertaBot
{

// Next tech target to work toward (not necessarily quickly).
enum class TechTarget
	{ None
	, Hydralisks
	, Mutalisks
	, Ultralisks
	, Lurkers
	, Defilers
	, Guardians
};

class StrategyBossZerg
{
	StrategyBossZerg::StrategyBossZerg();

	// Values that cannot or should not be exceeded.
	const int absoluteMaxSupply = 400;
	const int absoluteMaxDrones = 75;

	BWAPI::Player _self;
	BWAPI::Player _enemy;
	BWAPI::Race _enemyRace;

	// The target unit mix. If nothing can or should be made, None.
	BWAPI::UnitType _mineralUnit;
	BWAPI::UnitType _gasUnit;
	TechTarget _techTarget;

	// Target proportion of larvas spent on drones versus combat units.
	double _economyRatio;

	// Larva use counts to maintain the economy ratio.
	// These get reset when _economyRatio changes.
	int _economyDrones;
	int _economyTotal;

	// The most recent build order created by freshProductionPlan().
	// Empty while we're in the opening book.
	BuildOrder _latestBuildOrder;

	// Recognize problems.
	bool _emergencyGroundDefense;
	int _emergencyStartFrame;

	int _produceArmyTime = 0;
	bool _produceArmy = false;
	bool _produceDrones = false;
	bool _dropStrategy = false;
	bool _cheeseDetected = false;

	int _existingSupply;
	int _pendingSupply;
	int _supplyUsed;

	int _lastUpdateFrame;
	int minerals;
	int gas;

	// Unit stuff, including uncompleted units.
	int nLairs;
	int nHives;
	int nHatches;
	int nCompletedHatches;
	int nSpores;

	int nGas;          // taken geysers
	int nFreeGas;      // untaken geysers at our completed bases

	int nDrones;
	int nMineralDrones;
	int nGasDrones;
	int nLarvas;

	int nLings;
	int nHydras;
	int nMutas;
	int nLurkers;
	int nUltras;
	int nGuardians;
	int nDevourers;
	int nScourge;
	int nQueens;
	int nDefilers;
	int nInfested;

	int lurkerPreference;
	int lingPreference;
	int devourerPreference;
	int guardianPreference;
	int queenPreference;

	// Tech stuff. It has to be completed for the tech to be available.
	int nEvo;
	bool hasPool;
	bool hasDen;
	bool hasHydraSpeed;
	bool hasSpire;
	bool hasQueensNest;
	bool hasUltra;
	bool hasUltraUps; 
	bool hasGreaterSpire;
	bool hasASpire;
	bool hasDefiler;
	bool hasInfested;

	bool hasLurkerAspect;

	// hasLairTech means "can research stuff in the lair" (not "can research stuff that needs lair").
	bool hasHiveTech;
	bool hasLair;
	bool hasLairTech;
	int techLevel;

	bool outOfBook;
	int nBases;           // our bases
	int nFreeBases;       // untaken non-island bases
	int nMineralPatches;  // mineral patches at all our bases
	int maxDrones;        // maximum reasonable number given nMineralPatches

	bool droningMode;
	void assessSituation();

	// Update the resources, unit counts, and related stuff above.
	void updateSupply();
	void updateGameState();

	int numInEgg(BWAPI::UnitType) const;
	bool isBeingBuilt(const BWAPI::UnitType unitType) const;
	int numBeingBuilt(const BWAPI::UnitType unitType) const;

	void produceCheck(const MacroAct & act);

	void cancelStuff(int mineralsNeeded);
	bool nextInQueueIsUseless(BuildOrderQueue & queue) const;

	void produce(const MacroAct & act);
	bool needDroneNext();
	
	void makeOverlords(BuildOrderQueue & queue);

	bool takeUrgentAction(BuildOrderQueue & queue);
	void makeUrgentReaction(BuildOrderQueue & queue);
	void checkGroundDefenses(BuildOrderQueue & queue);

	bool vProtossDenOverSpire();
	bool vTerranDenOverSpire();
	bool chooseDenOverSpire();
	void chooseUnitMix(bool denOverSpire);
	void chooseTechTarget(bool denOverSpire);
	void chooseEconomyRatio();
	void chooseStrategy();
	
	std::string techTargetToString(TechTarget target);
	void drawStrategySketch();
	void drawStrategyBossInformation();

	//convenience functions
	bool upping(const MacroAct &) const;
	bool shouldGet(const MacroAct & t) const;
	bool canGet(const MacroAct &) const;
	int has(const BWAPI::UnitType &, bool = false) const;
	bool busy(const BWAPI::UnitType &, bool = false) const ;

public:
	static StrategyBossZerg & Instance();

	void setUnitMix(BWAPI::UnitType minUnit, BWAPI::UnitType gasUnit);
	void setEconomyRatio(double ratio);

	// Called once per frame for emergencies and other urgent needs.
	void handleUrgentProductionIssues(BuildOrderQueue & queue);

	// Called when the production queue is empty.
	BuildOrder & freshProductionPlan();
};

};
