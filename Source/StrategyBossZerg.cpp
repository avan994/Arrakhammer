#include "StrategyBossZerg.h"

#include "ProductionManager.h"
#include "ScoutManager.h"
#include "UnitUtil.h"

#include "ZSimcity.h"

using namespace UAlbertaBot;

using namespace BWAPI;
using namespace BWAPI::TechTypes;
using namespace BWAPI::UnitTypes;
using namespace BWAPI::UpgradeTypes;

StrategyBossZerg::StrategyBossZerg()
	: _self(BWAPI::Broodwar->self())
	, _enemy(BWAPI::Broodwar->enemy())
	, _enemyRace(_enemy->getRace())
	, _techTarget(TechTarget::None)
	, _latestBuildOrder(BWAPI::Races::Zerg)
	, _emergencyGroundDefense(false)
	, _emergencyStartFrame(-1)
	, _existingSupply(-1)
	, _pendingSupply(-1)
	, _lastUpdateFrame(-1)
{
	setUnitMix(Zerg_Drone, UnitTypes::None);

	_economyDrones = 0;
	_economyTotal = 0;
	chooseEconomyRatio();

	std::string messages[12];
	messages[0] = "GLHF";
	messages[1] = "Shoutout: Thanks to bftjoe for sunken placement and bugfixes!";
	messages[2] = "Shoutout: jaj22 and Ankmairdor helped with BWAPI/game mechanics!";
	messages[3] = "Shoutout: Many bugs discovered thanks to FawxOW's McRave!";
	messages[4] = "Shoutout: Thanks SSCAIT!";
	messages[5] = "Shoutout: tscmoo2's and Ankmairdor's advice/aid evolved the hydralisks!";
	messages[6] = "Shoutout: Thanks to Jay Scott for fantastic work on Steamhammer!";
	messages[7] = "Shoutout: PurpleWave is often in chat!";
	messages[8] = "Shoutout: Antiga provided amazing build order advice!";
	messages[9] = "Shoutout: Thanks to nepeta for constructive criticism!";
	messages[10] = "You wanna pizza me, boy?";
	messages[11] = "Infestation com-- oh, wait, I'm in a new game now.";
	
	srand(unsigned int(time(NULL)));
	int index = rand() % 12; //rand() % sizeof(messages)/ sizeof(*messages)
	BWAPI::Broodwar->sendText(messages[index].c_str());

	if (_enemyRace == BWAPI::Races::Zerg) {
		lurkerPreference = 0;
		lingPreference = 0;
		guardianPreference = -9999;
		devourerPreference = 1;
		queenPreference = 0;
	}
}

// -- -- -- -- -- -- -- -- -- -- --
// Private methods.

// Calculate supply existing, pending, and used.
// FOr pending supply, we need to know about overlords just hatching.
// For supply used, the BWAPI self->supplyUsed() can be slightly wrong,
// especially when a unit is just started or just died. 
void StrategyBossZerg::updateSupply()
{
	int existingSupply = 0;
	int pendingSupply = 0;
	int supplyUsed = 0;

	for (auto & unit : _self->getUnits())
	{
		if (unit->getType() == Zerg_Overlord)
		{
			if (unit->getOrder() == BWAPI::Orders::ZergBirth)
			{
				// Overlord is just hatching and doesn't provide supply yet.
				pendingSupply += 16;
			}
			else
			{
				existingSupply += 16;
			}
		}
		else if (unit->getType() == Zerg_Egg)
		{
			if (unit->getBuildType() == Zerg_Overlord) {
				pendingSupply += 16;
			}
			else if (unit->getBuildType().isTwoUnitsInOneEgg())
			{
				supplyUsed += 2 * unit->getBuildType().supplyRequired();
			}
			else
			{
				supplyUsed += unit->getBuildType().supplyRequired();
			}
		}
		else if (unit->getType() == Zerg_Hatchery && !unit->isCompleted())
		{
			// Don't count this. Hatcheries build too slowly and provide too little.
			// pendingSupply += 2;
		}
		else if (unit->getType().isResourceDepot())
		{
			// Only counts complete hatcheries because incomplete hatcheries are checked above.
			// Also counts lairs and hives whether complete or not, of course.
			existingSupply += 2;
		}
		else
		{
			supplyUsed += unit->getType().supplyRequired();
		}
	}

	_existingSupply = std::min(existingSupply, absoluteMaxSupply);
	_pendingSupply = pendingSupply;
	_supplyUsed = supplyUsed;

	// Note: _existingSupply is less than _self->supplyTotal() when an overlord
	// has just died. In other words, it recognizes the lost overlord sooner,
	// which is better for planning.

	//if (_self->supplyUsed() != _supplyUsed)
	//{
	//	BWAPI::Broodwar->printf("official supply used /= measured supply used %d /= %d", _self->supplyUsed(), supplyUsed);
	//}
}

// Called once per frame, possibly more.
// Includes screen drawing calls.
void StrategyBossZerg::updateGameState()
{
	if (_lastUpdateFrame == BWAPI::Broodwar->getFrameCount())
	{
		// No need to update more than once per frame.
		return;
	}
	_lastUpdateFrame = BWAPI::Broodwar->getFrameCount();

	if (_emergencyGroundDefense && _lastUpdateFrame >= _emergencyStartFrame + (25 * 24))
	{
		// Danger has been past for long enough. Call the end of the emergency.
		_emergencyGroundDefense = false;
	}

	//---draw chokepoints
	//ZSimcity::Instance().computeWall();
	//----------------------

	minerals = std::max(0, _self->minerals() - BuildingManager::Instance().getReservedMinerals());
	gas = std::max(0, _self->gas() - BuildingManager::Instance().getReservedGas());

	// Unit stuff, including uncompleted units.
	nLairs = UnitUtil::GetAllUnitCount(Zerg_Lair);
	nHives = UnitUtil::GetAllUnitCount(Zerg_Hive);
	nHatches = UnitUtil::GetAllUnitCount(Zerg_Hatchery)
		+ nLairs + nHives;
	nCompletedHatches = UnitUtil::GetCompletedUnitCount(Zerg_Hatchery)
		+ nLairs + nHives;
	nSpores = UnitUtil::GetAllUnitCount(Zerg_Spore_Colony);

	// nGas = number of geysers ready to mine (extractor must be complete)
	// nFreeGas = number of geysers free to be taken (no extractor, even uncompleted);
	InformationManager::Instance().getMyGasCounts(nGas, nFreeGas);

	nDrones = UnitUtil::GetAllUnitCount(Zerg_Drone);
	nMineralDrones = WorkerManager::Instance().getNumMineralWorkers();
	nGasDrones = WorkerManager::Instance().getNumGasWorkers();
	nLarvas = UnitUtil::GetAllUnitCount(Zerg_Larva);

	nLings = UnitUtil::GetAllUnitCount(Zerg_Zergling);
	nHydras = UnitUtil::GetCompletedUnitCount(Zerg_Hydralisk);
	nMutas = UnitUtil::GetCompletedUnitCount(Zerg_Mutalisk);
	nLurkers = UnitUtil::GetAllUnitCount(Zerg_Lurker);
	nUltras = UnitUtil::GetAllUnitCount(Zerg_Ultralisk);
	nGuardians = UnitUtil::GetAllUnitCount(Zerg_Guardian);
	nDevourers = UnitUtil::GetAllUnitCount(Zerg_Devourer);
	nScourge = UnitUtil::GetAllUnitCount(Zerg_Scourge);
	nQueens = UnitUtil::GetAllUnitCount(Zerg_Queen);
	nDefilers = UnitUtil::GetAllUnitCount(Zerg_Defiler);
	nInfested = UnitUtil::GetAllUnitCount(Zerg_Infested_Terran);

	// Tech stuff. It has to be completed for the tech to be available.
	nEvo = UnitUtil::GetCompletedUnitCount(Zerg_Evolution_Chamber);
	hasPool = UnitUtil::GetCompletedUnitCount(Zerg_Spawning_Pool) > 0;
	hasDen = UnitUtil::GetCompletedUnitCount(Zerg_Hydralisk_Den) > 0;
	hasHydraSpeed = _self->getUpgradeLevel(Muscular_Augments) != 0;
	hasSpire = UnitUtil::GetCompletedUnitCount(Zerg_Spire) > 0;
	hasQueensNest = UnitUtil::GetCompletedUnitCount(Zerg_Queens_Nest) > 0;
	hasUltra = UnitUtil::GetCompletedUnitCount(Zerg_Ultralisk_Cavern) > 0;
	// Enough upgrades that it is worth making ultras: Speed done, armor underway.
	hasUltraUps = _self->getUpgradeLevel(Anabolic_Synthesis) != 0 &&
		(_self->getUpgradeLevel(Chitinous_Plating) != 0 ||
		upping(Chitinous_Plating));

	hasLurkerAspect = _self->hasResearched(Lurker_Aspect);
	hasGreaterSpire = UnitUtil::GetCompletedUnitCount(Zerg_Greater_Spire) > 0;
	hasDefiler = UnitUtil::GetCompletedUnitCount(Zerg_Defiler_Mound) > 0;
	hasInfested = UnitUtil::GetCompletedUnitCount(Zerg_Infested_Command_Center) > 0;

	//part of a temporary quickfix to a bug...
	hasASpire = hasSpire || hasGreaterSpire;

	// hasLair means "can research stuff in the lair", like overlord speed.
	// hasLairTech means "can do stuff that needs lair", like research lurker aspect.
	// NOTE The two are different in game, but even more different in the bot because
	//      of a BWAPI 4.1.2 bug: You can't do lair research in a hive.
	//      This code reflects the bug so we can work around it as much as possible.
	hasHiveTech = UnitUtil::GetCompletedUnitCount(Zerg_Hive) > 0;
	hasLair = UnitUtil::GetCompletedUnitCount(Zerg_Lair) > 0;
	hasLairTech = hasLair || nHives > 0;
	techLevel = hasLairTech + hasHiveTech;
	
	outOfBook = ProductionManager::Instance().isOutOfBook();
	nBases = InformationManager::Instance().getNumBases(_self);
	nFreeBases = InformationManager::Instance().getNumFreeLandBases();
	nMineralPatches = InformationManager::Instance().getMyNumMineralPatches();
	maxDrones = std::min(absoluteMaxDrones, WorkerManager::Instance().getMaxWorkers());

	updateSupply();

	drawStrategySketch();
	drawStrategyBossInformation();

	assessSituation();
}

/* Work in Progress: intended to be used to calculate whether the zerg bot should 
 * use all hatcheries to produce drones, or produce units, in bursts. */
void StrategyBossZerg::assessSituation() {
	int minIncome = 65 * WorkerManager::Instance().getNumMineralWorkers();
	int gasIncome = 103 * WorkerManager::Instance().getNumGasWorkers();
	BWAPI::Broodwar->drawTextScreen(240, 10, "Income: %d %d / minute", minIncome, gasIncome);
	BWAPI::Broodwar->drawTextScreen(240, 20, "Army Production Mode: %d %d [broken]", _produceArmy, _produceDrones);

	//BWAPI::Broodwar->drawTextScreen(400, 20, "SpecOps Status (unimplemented): %d", 0);
	//BWAPI::Broodwar->drawTextScreen(400, 30, "Drop Assessment: %d (eventually)", _dropStrategy);

	int frameCount = BWAPI::Broodwar->getFrameCount();
	//if (frameCount < 5 * 60 * 20) return;
	//if (frameCount < 5*60*20 || frameCount % (20 * 20) != 0) return; //only assess every several frames; don't assess until we've scouted

	auto enemyBase = InformationManager::Instance().getEnemyMainBaseLocation();
	if (!enemyBase) return;

	int ourBases = InformationManager::Instance().getNumBases(_self);
	int enemyBases = InformationManager::Instance().getNumBases(_enemy);

	int enemySupply = 0;
	int enemyWorkerCount = 0;
	int enemyDefensePower = 0;

	BWAPI::Position closestPosition = enemyBase->getPosition();
	BWAPI::Position mainBasePosition = InformationManager::Instance().getMyMainBaseLocation()->getPosition();
	for (const auto & kv : InformationManager::Instance().getUnitData(_enemy).getUnits()) {
		const UnitInfo & ui(kv.second);
		if (ui.type.isWorker()) {
			enemyWorkerCount++;
		}
		else {
			if (ui.type.groundWeapon() != BWAPI::WeaponTypes::None ||
				ui.type.airWeapon() != BWAPI::WeaponTypes::None) {
				enemySupply += ui.type.supplyRequired();

				if (ui.type.isBuilding()) {
					enemyDefensePower += 8;
				}

				if (ui.lastPosition.getDistance(mainBasePosition) < closestPosition.getDistance(mainBasePosition)) {
					closestPosition = ui.lastPosition;
				}
			}
			else if (ui.type == Terran_Bunker) {
				enemyDefensePower += 20;
			}
		}
	}

	int ourSupply = 0;
	int ourWorkerCount = 0;
	int defensePower = 0;
	for (const auto & unit : _self->getUnits()) {
		BWAPI::UnitType type = unit->getType();
		if (type.isWorker()) {
			ourWorkerCount++;
		}
		else {
			if (type.groundWeapon() != BWAPI::WeaponTypes::None ||
				type.airWeapon() != BWAPI::WeaponTypes::None) {
				ourSupply += unit->getType().supplyRequired();

				if (unit->getType().isBuilding()) {
					defensePower += 5;
				}
			} 
		}
	}

	/* Conditions for nonstop drone spam until maxed out */
	if (enemyDefensePower >= 35 && _economyRatio < 0.43) {
		setEconomyRatio(0.43); //increase the economy ratio if we detect many static defnses
	}
	_produceDrones = (enemyDefensePower > 8 * enemyBases + 8 * 6 && enemySupply < ourSupply && enemyBases <= 3) || //the enemy has holed themselves in -- max out drones!
		(defensePower >= 15 && nDrones < std::min(3*9, nMineralPatches)); //we have a lot of sunkens but not nearly enough drones; saturate bases

	if (!_produceDrones && _enemyRace != BWAPI::Races::Zerg && nCompletedHatches >= 3 && !_emergencyGroundDefense && nDrones < std::min(7*nCompletedHatches, maxDrones)) {
		_produceDrones = true;
	}
	/* --- */

	BWAPI::Broodwar->drawTextScreen(240, 30, "Base Count: %d vs. %d", ourBases, enemyBases);
	BWAPI::Broodwar->drawTextScreen(240, 40, "Supply: %d vs. %d", ourSupply, enemySupply);
	BWAPI::Broodwar->drawTextScreen(240, 50, "Workers: %d vs. %d", ourWorkerCount, enemyWorkerCount);
	BWAPI::Broodwar->drawTextScreen(240, 60, "Defenses: %d vs. %d", defensePower, enemyDefensePower);
	
	bool timeHasPassed = _produceArmyTime - 60 * 20 < frameCount;

	//in reality, for defenses we should assess per-base and decide how many units to produce to destroy a given base
	bool enemyWentGreedy = (ourBases < 2 * enemyBases || ourWorkerCount < enemyWorkerCount) && enemyDefensePower < 10*enemyBases;
	bool wayAhead = ourBases > 2 * enemyBases && ourWorkerCount > ourBases*5 && nBases >= 5 &&
		nMineralPatches > nBases*5;
	bool enemyIsDead = (ourSupply > 1.5*enemySupply + enemyDefensePower);

	bool enemyMightAttack = closestPosition.getDistance(mainBasePosition) < 20 * 32 ||
		timeHasPassed; //about a minute slack
	bool enemyIsStrong = ourSupply + defensePower < enemySupply && enemyMightAttack;

	bool canSupportProduction = ourWorkerCount > 8 * nCompletedHatches;
	bool _newProduceArmy = _enemyRace != BWAPI::Races::Zerg ? ((canSupportProduction && (enemyWentGreedy || enemyIsDead)) ||
	enemyIsStrong) : false;

	BWAPI::Broodwar->drawTextScreen(240, 70, "Assessment: %d ... %d %d %d %d %d %d", _newProduceArmy, canSupportProduction, enemyWentGreedy, wayAhead, enemyIsDead, enemyMightAttack, enemyIsStrong);

	if (_newProduceArmy != _produceArmy && timeHasPassed) {
		_produceArmyTime = BWAPI::Broodwar->getFrameCount();
		_produceArmy = _newProduceArmy;
	}
	else if (_produceArmy && frameCount > 2*60 * 20) { //nonstop army production is bad [temporary fix]
		_produceArmy = false; 
		_produceArmyTime = BWAPI::Broodwar->getFrameCount();
	}

	/*
	//make a guess as to whether we possess map control
	//could base off the locations of the most recent deaths of allied units
	bool haveMapControl;

	//compute the expected time of arrival of the enemy army should they move out, or be moving out
	//double ETA = distance from base front to closest enemy unit / average speed enemy unit;
	//currently assumes and ETA of about 60 s

	
	//in droning mode, every hatchery produces drones
	//in normal mode, only expansion hatcheries with undersaturated resources will produce drones
	//droningMode = !canWin && haveMapControl && (enemyIsDefensive || (2*enemyBaseCount < nBases) ||
		)
	*/
}

// How many of our eggs will hatch into the given unit type?
// This does not adjust for zerglings or scourge, which are 2 to an egg.
int StrategyBossZerg::numInEgg(BWAPI::UnitType type) const
{
	int count = 0;

	for (auto & unit : _self->getUnits())
	{
		if (unit->getType() == Zerg_Egg && unit->getBuildType() == type)
		{
			++count;
		}
	}

	return count;
}

// Return true if the building is in the building queue with any status.
bool StrategyBossZerg::isBeingBuilt(const BWAPI::UnitType unitType) const
{
	UAB_ASSERT(unitType.isBuilding(), "SBZ isBeingBuilt: not a building: %s", unitType.c_str());
	return BuildingManager::Instance().isBeingBuilt(unitType);
}

int StrategyBossZerg::numBeingBuilt(const BWAPI::UnitType unitType) const
{
	UAB_ASSERT(unitType.isBuilding(), "SBZ numBeingBuilt: not a building");
	return BuildingManager::Instance().numBeingBuilt(unitType);
}

void StrategyBossZerg::produceCheck(const MacroAct & act)
{
	//UAB_ASSERT(unitType != None, "SBZ produceCheck: not a unit");
	produce(act); 

	if (act.isCommand()) return;
	minerals -= act.mineralPrice();
	gas -= act.gasPrice();

	if (act.isUnit()) {
		const BWAPI::UnitType unitType = act.getUnitType();
		

		if (unitType.whatBuilds().first == Zerg_Larva) {
			nLarvas--;
		}
		else if (unitType.isBuilding()) {
			nDrones--;
		}
		else if (unitType.whatBuilds().first == Zerg_Hydralisk) {
			nHydras--;
		}
		else if (unitType.whatBuilds().first == Zerg_Mutalisk) {
			nMutas--;
		}

		if (unitType == Zerg_Hydralisk) {
			nHydras++;
		}
		else if (unitType == Zerg_Mutalisk) {
			nMutas++;
		}
		else if (unitType == Zerg_Zergling) {
			nLings += 2;
		}
		else if (unitType == Zerg_Guardian) {
			nGuardians++;
		}
		else if (unitType == Zerg_Devourer) {
			nDevourers++;
		}
		else if (unitType == Zerg_Ultralisk) {
			nUltras++;
		}
		else if (unitType == Zerg_Defiler) {
			nDefilers++;
		}
		else if (unitType == Zerg_Queen) {
			nQueens++;
		}
		else if (unitType == Zerg_Lurker) {
			nLurkers++;
		}
		else if (unitType == Zerg_Scourge) {
			nScourge += 2;
		}
		else if (unitType == Zerg_Infested_Terran) {
			nInfested++;
		}
	}
}

// Severe emergency: We are out of drones and/or hatcheries.
// Cancel items to release their resources.
// TODO pay attention to priority: the least essential first
// TODO cancel research
void StrategyBossZerg::cancelStuff(int mineralsNeeded)
{
	int mineralsSoFar = _self->minerals();

	for (BWAPI::Unit u : _self->getUnits())
	{
		if (mineralsSoFar >= mineralsNeeded)
		{
			return;
		}
		if (u->getType() == Zerg_Egg && u->getBuildType() == Zerg_Overlord)
		{
			if (_self->supplyTotal() - _supplyUsed >= 6)  // enough to add 3 drones
			{
				mineralsSoFar += 100;
				u->cancelMorph();
			}
		}
		else if (u->getType() == Zerg_Egg && u->getBuildType() != Zerg_Drone ||
			u->getType() == Zerg_Lair && !u->isCompleted() ||
			u->getType() == Zerg_Creep_Colony && !u->isCompleted() ||
			u->getType() == Zerg_Evolution_Chamber && !u->isCompleted() ||
			u->getType() == Zerg_Hydralisk_Den && !u->isCompleted() ||
			u->getType() == Zerg_Queens_Nest && !u->isCompleted() ||
			u->getType() == Zerg_Hatchery && !u->isCompleted() && nHatches > 1)
		{
			mineralsSoFar += u->getType().mineralPrice();
			u->cancelMorph();
		}
	}
}

// The next item in the queue is useless and can be dropped.
// Top goal: Do not freeze the production queue by asking the impossible.
// But also try to reduce wasted production.
// NOTE Useless stuff is not always removed before it is built.
//      The order of events is: this check -> queue filling -> production.
bool StrategyBossZerg::nextInQueueIsUseless(BuildOrderQueue & queue) const
{
	if (queue.isEmpty())
	{
		return false;
	}

	const MacroAct act = queue.getHighestPriorityItem().macroAct;

	// It costs gas that we don't have and won't get.
	if (nGas == 0 && act.gasPrice() > gas && !isBeingBuilt(Zerg_Extractor))
	{
		return true;
	}

	if (act.isUpgrade())
	{
		const BWAPI::UpgradeType upInQueue = act.getUpgradeType();

		if (upping(upInQueue))
		{
			return true;
		}
		// Already have it or already getting it (due to a race condition).
		// Coordinate these two with the single/double upgrading plan.
		else if (upInQueue == Zerg_Carapace ||
			upInQueue == Zerg_Melee_Attacks ||
			upInQueue == Zerg_Missile_Attacks)
		{
			return nEvo == 0 || _self->getUpgradeLevel(upInQueue) > techLevel;
		}
		else if (upInQueue == Zerg_Flyer_Carapace ||
			upInQueue == Zerg_Flyer_Attacks) {
			return !hasSpire || _self->getUpgradeLevel(upInQueue) > techLevel;
		}
		else if (_self->getUpgradeLevel(upInQueue) > 0) //other upgrade types
		{
			return true;
		}

		// Lost the building for it in the meantime.
		if (upInQueue == Anabolic_Synthesis || upInQueue == Chitinous_Plating)
		{
			return !hasUltra;
		}

		if (upInQueue == Muscular_Augments || upInQueue == Grooved_Spines)
		{
			return !hasDen && !isBeingBuilt(Zerg_Hydralisk_Den);
		}

		if (upInQueue == Metabolic_Boost)
		{
			return !hasPool;
		}

		if (upInQueue == Adrenal_Glands)
		{
			return !hasPool || !hasHiveTech;
		}

		if (upInQueue == Gamete_Meiosis) {
			return !hasQueensNest && !isBeingBuilt(Zerg_Queens_Nest);
		}

		return false;
	}

	
	if (act.isTech())
	{
		const BWAPI::TechType techInQueue = act.getTechType();

		if (_self->hasResearched(techInQueue)) return true;

		if (techInQueue == Lurker_Aspect) {
			return (!hasLairTech && !isBeingBuilt(Zerg_Lair)) || (!hasDen && !isBeingBuilt(Zerg_Hydralisk_Den));
		}

		if (techInQueue == Spawn_Broodlings ||
			techInQueue == Ensnare) {
			return !has(Zerg_Queens_Nest);
		}

		if (techInQueue == Consume || techInQueue == Plague) {
			return !has(Zerg_Defiler_Mound);
		}

		return false;
	}
	

	// After that, we only care about units.
	if (!act.isUnit())
	{
		return false;
	}

	const BWAPI::UnitType nextInQueue = act.getUnitType();

	if (nextInQueue == Zerg_Overlord)
	{
		// We don't need overlords now. Opening book sometimes deliberately includes extras.
		// This is coordinated with makeOverlords() but skips less important steps.
		if (outOfBook)
		{
			//commented out for now
			int totalSupply = _existingSupply + _pendingSupply;
			int supplyExcess = totalSupply - _supplyUsed;
			return totalSupply >= absoluteMaxSupply ||
				(supplyExcess >= _supplyUsed / 4 + std::max(0, (minerals-1500)/100) + 16);
		}
	}
	else if (_supplyUsed + nextInQueue.supplyRequired() >= 400) { //we're maxed!
		return true;
	}

	if (nextInQueue == Zerg_Drone)
	{
		// We are planning more than the maximum reasonable number of drones.
		// nDrones can go slightly over maxDrones when queue filling adds drones.
		// It can also go over when maxDrones decreases (bases lost, minerals mined out).
		return outOfBook && (nDrones >= maxDrones);
	}
	if (nextInQueue == Zerg_Zergling)
	{
		// We lost the tech.
		return !hasPool &&
			UnitUtil::GetAllUnitCount(Zerg_Spawning_Pool) == 0 &&
			!isBeingBuilt(Zerg_Spawning_Pool);
	}
	if (nextInQueue == Zerg_Hydralisk)
	{
		// We lost the tech.
		return !hasDen &&
			UnitUtil::GetAllUnitCount(Zerg_Hydralisk_Den) == 0 &&
			!isBeingBuilt(Zerg_Hydralisk_Den);
	}
	if (nextInQueue == Zerg_Mutalisk || nextInQueue == Zerg_Scourge)
	{
		// We lost the tech. We currently do not ever make a greater spire.
		return !hasASpire &&
			UnitUtil::GetAllUnitCount(Zerg_Spire) == 0 &&
			!isBeingBuilt(Zerg_Spire) &&
			UnitUtil::GetAllUnitCount(Zerg_Greater_Spire) == 0 &&
			!isBeingBuilt(Zerg_Greater_Spire);
	}
	if (nextInQueue == Zerg_Ultralisk)
	{
		// We lost the tech.
		return UnitUtil::GetAllUnitCount(Zerg_Ultralisk_Cavern) == 0;
	}

	if (nextInQueue == Zerg_Hatchery)
	{
		// We're planning a hatchery but no longer have the drones to support it.
		// 3 drones/hatchery is the minimum: It can support ling production.
		// Also, it may still be OK if we have lots of minerals to spend.
		return nDrones < 3 * nHatches &&
			minerals < 50 + 300 * nCompletedHatches &&
			nCompletedHatches > 0;
	}
	if (nextInQueue == Zerg_Lair)
	{
		return (!hasPool && UnitUtil::GetAllUnitCount(Zerg_Spawning_Pool) == 0 && !isBeingBuilt(Zerg_Spawning_Pool)) ||
			(UnitUtil::GetAllUnitCount(Zerg_Hatchery) == 0 && !isBeingBuilt(Zerg_Hatchery)) ||
			hasLair; //just in case for this weird lair bug
	}
	if (nextInQueue == Zerg_Hive)
	{
		return !has(Zerg_Queens_Nest) ||
			UnitUtil::GetAllUnitCount(Zerg_Lair) == 0 ||
			upping(Pneumatized_Carapace) ||
			upping(Ventral_Sacs) ||
			hasHiveTech;
	}
	if (nextInQueue == Zerg_Sunken_Colony)
	{
		return (UnitUtil::GetAllUnitCount(Zerg_Spawning_Pool) == 0 && !isBeingBuilt(Zerg_Spawning_Pool)) ||
			((UnitUtil::GetAllUnitCount(Zerg_Creep_Colony) == 0) && !isBeingBuilt(Zerg_Creep_Colony));
	}
	if (nextInQueue == Zerg_Spore_Colony)
	{
		return (UnitUtil::GetAllUnitCount(Zerg_Evolution_Chamber) == 0 && !isBeingBuilt(Zerg_Evolution_Chamber)) ||
			(UnitUtil::GetAllUnitCount(Zerg_Creep_Colony) == 0 && !isBeingBuilt(Zerg_Creep_Colony));
	}
	if (nextInQueue == Zerg_Spire)
	{
		return (nLairs + nHives == 0) ||
			(nHives == 0 && (UnitUtil::GetAllUnitCount(Zerg_Spire) != 0 || 
			isBeingBuilt(Zerg_Spire)));
	}
	/* ----------------------- */
	if (nextInQueue == Zerg_Queens_Nest) //no more than one of these
	{
		return (nLairs + nHives == 0) ||
			UnitUtil::GetAllUnitCount(Zerg_Queens_Nest) > 0 || 
			isBeingBuilt(Zerg_Queens_Nest);
	}
	if (nextInQueue == Zerg_Greater_Spire)
	{
		return nHives == 0 ||
			(UnitUtil::GetAllUnitCount(Zerg_Spire) == 0 && !isBeingBuilt(Zerg_Spire)) ||
			has(Zerg_Greater_Spire);
	}
	if (nextInQueue == Zerg_Guardian || nextInQueue == Zerg_Devourer)
	{
		return nMutas == 0 ||
			(UnitUtil::GetAllUnitCount(Zerg_Greater_Spire) == 0 &&
			!isBeingBuilt(Zerg_Greater_Spire));
	}
	if (nextInQueue == Zerg_Defiler_Mound) {
		return !has(Zerg_Hive) || has(Zerg_Defiler_Mound);
	}
	if (nextInQueue == Zerg_Defiler) {
		return !hasDefiler;
	}
	/* ---------------------- */
	if (nextInQueue == Zerg_Hydralisk_Den)
	{
		return !hasPool &&
			UnitUtil::GetAllUnitCount(Zerg_Spawning_Pool) == 0 && !isBeingBuilt(Zerg_Spawning_Pool);
	}
	if (nextInQueue == Zerg_Lurker)
	{
		return nHydras == 0 ||
			(!_self->hasResearched(Lurker_Aspect) && 
			!upping(Lurker_Aspect));
	}
	if (nextInQueue == Zerg_Infested_Terran)
	{
		return !hasInfested;
	}

	return false;
}

void StrategyBossZerg::produce(const MacroAct & act)
{
	_latestBuildOrder.add(act);
	if (act.isUnit() && !act.isBuilding())
	{
		++_economyTotal;
		if (act.getUnitType() == Zerg_Drone)
		{
			++_economyDrones;
		}
	}
}

// Make a drone instead of a combat unit with this larva?
bool StrategyBossZerg::needDroneNext()
{
	int minimumWorkerCount = nMineralPatches + 3*nGas;
	return !_emergencyGroundDefense &&
		nDrones < maxDrones &&
		(_produceDrones ||
		(!_produceArmy &&
		((double(_economyDrones) / double(1 + _economyTotal) < _economyRatio) 
		)));
}

// We need overlords.
// Do this last so that nothing gets pushed in front of the overlords.
// NOTE: If you change this, coordinate the change with nextInQueueIsUseless(),
// which has a feature to recognize unneeded overlords (e.g. after big army losses).
void StrategyBossZerg::makeOverlords(BuildOrderQueue & queue)
{
	BWAPI::UnitType nextInQueue = queue.getNextUnit();

	// If an overlord is next anyway, and we're not rich, we don't need to build another overlord
	if (nextInQueue == Zerg_Overlord && minerals < 1000)
	{
		return;
	}

	int totalSupply = std::min(_existingSupply + _pendingSupply, absoluteMaxSupply);
	if (totalSupply < absoluteMaxSupply)
	{
		int supplyExcess = totalSupply - _supplyUsed;
		// Adjust the number to account for the next queue item and pending buildings.
		if (nextInQueue != UnitTypes::None)
		{
			if (nextInQueue.isBuilding())
			{
				if (!UnitUtil::IsMorphedBuildingType(nextInQueue)) {
					supplyExcess += 2;   // for the drone that will be used
				}
			}
			else {
				supplyExcess -= nextInQueue.supplyRequired();
			}
		}
		// The number of drones set to be used up making buildings.
		supplyExcess += 2 * BuildingManager::Instance().buildingsQueued().size();

		// If we're behind, catch up.
		for (; supplyExcess < 0; supplyExcess += 16) {
			queue.queueAsHighestPriority(Zerg_Overlord);
		}
		// If we're only a little ahead, stay ahead depending on the supply.
		// This is a crude calculation. It seems not too far off.
		if (totalSupply > 16 + 4 && supplyExcess <= 0)  {                      // > overlord + 2 hatcheries
			queue.queueAsHighestPriority(Zerg_Overlord);
		}
		else if (totalSupply > 32 && supplyExcess <= totalSupply / 8 - 2) {  // >= 2 overlords + 1 hatchery
			queue.queueAsHighestPriority(Zerg_Overlord);
		}
	}
}

// If necessary, take an emergency action and return true.
// Otherwise return false.
bool StrategyBossZerg::takeUrgentAction(BuildOrderQueue & queue)
{
	// Find the next thing remaining in the queue, but only if it is a unit.
	const BWAPI::UnitType nextInQueue = queue.getNextUnit();

	// There are no drones.
	// NOTE maxDrones is never zero.
	if (nDrones == 0)
	{
		WorkerManager::Instance().setCollectGas(false);
		BuildingManager::Instance().cancelQueuedBuildings();
		if (nHatches == 0)
		{
			// No hatcheries either. Queue drones for a hatchery and mining.
			ProductionManager::Instance().goOutOfBook();
			queue.queueAsLowestPriority(MacroAct(Zerg_Drone));
			queue.queueAsLowestPriority(MacroAct(Zerg_Drone));
			queue.queueAsLowestPriority(MacroAct(Zerg_Hatchery));
			cancelStuff(400);
		}
		else
		{
			if (nextInQueue != Zerg_Drone && numInEgg(Zerg_Drone) == 0)
			{
				// Queue one drone to mine minerals.
				ProductionManager::Instance().goOutOfBook();
				queue.queueAsLowestPriority(MacroAct(Zerg_Drone));
				cancelStuff(50);
			}
			BuildingManager::Instance().cancelBuildingType(Zerg_Hatchery);
		}
		return true;
	}

	// There are no hatcheries.
	if (nHatches == 0 &&
		nextInQueue != Zerg_Hatchery &&
		!isBeingBuilt(Zerg_Hatchery))
	{
		ProductionManager::Instance().goOutOfBook();
		queue.queueAsLowestPriority(MacroAct(Zerg_Hatchery));
		if (nDrones == 1)
		{
			ScoutManager::Instance().releaseWorkerScout();
			queue.queueAsHighestPriority(MacroAct(Zerg_Drone));
			cancelStuff(350);
		}
		else {
			cancelStuff(300);
		}
		return true;
	}

	// There are < 3 drones. Make up to 3.
	// Making more than 3 breaks 4 pool openings.
	if (nMineralDrones < 3 &&
		nextInQueue != Zerg_Drone &&
		nextInQueue != Zerg_Overlord)
	{
		ScoutManager::Instance().releaseWorkerScout();
		queue.queueAsHighestPriority(MacroAct(Zerg_Drone));
		if (nMineralDrones < 2)
		{
			queue.queueAsHighestPriority(MacroAct(Zerg_Drone));
		}
		// Don't cancel other stuff. A drone should be mining, it's not that big an emergency.
		return true;
	}

	// There are no drones on minerals. Turn off gas collection.
	// TODO we may want to switch between gas and minerals to make tech units
	// TODO more efficient test in WorkerMan
	if (_lastUpdateFrame >= 24 &&           // give it time!
		WorkerManager::Instance().isCollectingGas() &&
		nMineralPatches > 0 &&
		WorkerManager::Instance().getNumMineralWorkers() == 0 && 
		WorkerManager::Instance().getNumCombatWorkers() == 0 &&
		WorkerManager::Instance().getNumIdleWorkers() == 0)
	{
		// Leave the queue in place.
		ScoutManager::Instance().releaseWorkerScout();
		WorkerManager::Instance().setCollectGas(false);
		if (nHatches >= 2)
		{
			BuildingManager::Instance().cancelBuildingType(Zerg_Hatchery);
		}
		return true;
	}

	return false;
}

// React to lesser emergencies.
void StrategyBossZerg::makeUrgentReaction(BuildOrderQueue & queue)
{
	// Find the next thing remaining in the queue, but only if it is a unit.
	const BWAPI::UnitType nextInQueue = queue.getNextUnit();

	// Make scourge if needed
	if (outOfBook && nMutas < 6 && hasASpire && nGas > 0 && gas > 400 &&
		InformationManager::Instance().enemyHasAirTech() &&
		nextInQueue != Zerg_Scourge)
	{
		int totalScourge = UnitUtil::GetAllUnitCount(Zerg_Scourge) +
			2 * numInEgg(Zerg_Scourge) +
			2 * queue.numInQueue(Zerg_Scourge);

		int nScourgeNeeded = 2 + InformationManager::Instance().nScourgeNeeded();
		int nToMake = 0;
		if (nScourgeNeeded > totalScourge && nLarvas > 0)
		{
			int nPairs = std::min(1 + gas / 75, (nScourgeNeeded - totalScourge + 1) / 2);
			int limit = 3;          // how many pairs at a time, max?
			if (nLarvas > 6 && gas > 6 * 75) limit = 6;
			nToMake = std::min(nPairs, limit);
		}
		for (int i = 0; i < nToMake; ++i)
			queue.queueAsHighestPriority(Zerg_Scourge);
		// And keep going.
	}

	int queueMinerals, queueGas;
	queue.totalCosts(queueMinerals, queueGas);

	int gasDifference = 0;
	if (_gasUnit != BWAPI::UnitTypes::None) {
		int sminerals = minerals;
		if (_gasUnit.gasPrice() != 0 && _gasUnit.mineralPrice() != 0) {
			gasDifference = ((sminerals / _gasUnit.mineralPrice()) - (gas / _gasUnit.gasPrice())) * _gasUnit.gasPrice();
		}
		else {
			UAB_ASSERT(false, "Error: Gas unit with 0 mineral/gas cost specified in StrategyBossZerg.cpp");
		}
	}

	if (outOfBook) {
		bool limited = true; //only allow switches every so often in this case
		if (queue.getNextGasCost(1) > gas && (nGasDrones == 0 || nGas == 0) && WorkerManager::Instance().isCollectingGas())
		{
			// Deadlock. Can't get gas. Clear the queue.
			ProductionManager::Instance().goOutOfBook();
			return;
		} 
		else if (gas < queueGas || gasDifference > 75 || 
			(_enemyRace == BWAPI::Races::Zerg && isBeingBuilt(Zerg_Spire) && gas < std::min(minerals, 100*nLarvas)) || //save gas for mutalisk burst
			(nBases >= 4 && nDrones >= 9*nGas && nGas >= 3 && minerals - gas > 700))
		{
			if (!WorkerManager::Instance().isCollectingGas()) WorkerManager::Instance().setCollectGas(true, limited);
		}
		else {
			if (WorkerManager::Instance().isCollectingGas()) WorkerManager::Instance().setCollectGas(false, limited);
		}
	}
	else { //we're in book
		if (queue.getNextGasCost(2) > gas && (!WorkerManager::Instance().isCollectingGas()))
		{
			if ((nGas == 0 && !isBeingBuilt(Zerg_Extractor)) || nDrones < 9)
			{
				// Emergency. Give up and clear the queue.
				ProductionManager::Instance().goOutOfBook();
				return;
			}

			// Turn on gas otherwise
			WorkerManager::Instance().setCollectGas(true);
		}
	}

	// We need a macro hatchery.
	// Division of labor: Macro hatcheries are here, expansions are regular production.
	// However, some macro hatcheries may be placed at expansions (it helps assert map control).
	// Macro hatcheries are automatic only out of book. Book openings must take care of themselves.
	if (outOfBook && minerals >= 450 && nLarvas < (nCompletedHatches/3) && nDrones > 9*(1+nHatches) && !BuildingManager::Instance().isBuildingMacroHatchery())
	{
		MacroLocation loc = MacroLocation::Macro;
		//play it safe and place our first macro hatchery in the main against zerg, or if we're under attack
		/*if (_emergencyGroundDefense || _enemyRace == BWAPI::Races::Zerg && nHatches < 2) {
			loc = MacroLocation::Main;
		}
		else if (nFreeBases > 4 && nBases < 4) { //expand
			loc = MacroLocation::MinOnly;
		}*/

		queue.queueAsHighestPriority(MacroAct(Zerg_Hatchery, loc));
		// And keep going.
	}

	// If the enemy has cloaked stuff, consider overlord speed.
	if (InformationManager::Instance().enemyHasMobileCloakTech())
	{
		if (hasLair &&
			minerals >= 150 && gas >= 150 &&
			_self->getUpgradeLevel(Pneumatized_Carapace) == 0 &&
			!upping(Pneumatized_Carapace) &&
			!queue.anyInQueue(Pneumatized_Carapace))
		{
			queue.queueAsHighestPriority(MacroAct(Pneumatized_Carapace));
		}
		// And keep going.
	}
	
	// If the enemy has overlord hunters such as corsairs, prepare appropriately.
	if (InformationManager::Instance().enemyHasOverlordHunters())
	{
		if (hasLair &&
			minerals >= 150 && gas >= 150 &&
			_enemyRace != BWAPI::Races::Zerg &&
			_self->getUpgradeLevel(Pneumatized_Carapace) == 0 &&
			!upping(Pneumatized_Carapace) &&
			!queue.anyInQueue(Pneumatized_Carapace) && 
			(hasDen || hasASpire))
		{
			queue.queueAsHighestPriority(MacroAct(Pneumatized_Carapace));
		}
		else if (nEvo > 0 && nDrones >= 9 && nSpores < std::min(nBases,2) &&
			!queue.anyInQueue(Zerg_Spore_Colony) &&
			!isBeingBuilt(Zerg_Spore_Colony))
		{
			if (nSpores == 0) { //protect the main first. currently we don't replace this if it gets killed but the natural spore survives
				queue.queueAsHighestPriority(MacroAct(Zerg_Spore_Colony, MacroLocation::Main), false, DefenseLocation::Normal);
				queue.queueAsHighestPriority(Zerg_Drone);
				queue.queueAsHighestPriority(MacroAct(Zerg_Creep_Colony, MacroLocation::Main), false, DefenseLocation::Normal);
			}
			else if (nBases >= 2) { //assume we have a natural, place a colony there
				queue.queueAsHighestPriority(MacroAct(Zerg_Spore_Colony, MacroLocation::Natural), false, DefenseLocation::Chokepoint);
				queue.queueAsHighestPriority(Zerg_Drone);
				queue.queueAsHighestPriority(MacroAct(Zerg_Creep_Colony, MacroLocation::Natural), false, DefenseLocation::Chokepoint);
			}
		}
		else if (nEvo == 0 && (nDrones > 14 || _enemyRace == BWAPI::Races::Zerg) && outOfBook && hasPool &&
			!queue.anyInQueue(Zerg_Evolution_Chamber) &&
			UnitUtil::GetAllUnitCount(Zerg_Evolution_Chamber) == 0 &&
			!isBeingBuilt(Zerg_Evolution_Chamber))
		{
			queue.queueAsHighestPriority(Zerg_Evolution_Chamber);
		}
	}
}

// Check for possible ground attacks that we are may have trouble handling.
// If it seems necessary, make a limited number of sunkens.
// If a deadly attack seems impending, declare an emergency so that the
// regular production plan will concentrate on combat units.
void StrategyBossZerg::checkGroundDefenses(BuildOrderQueue & queue)
{
	// 1. Figure out where our front defense line is.
	MacroLocation front = MacroLocation::Anywhere;
	BWAPI::Unit ourHatchery = nullptr;

	if (InformationManager::Instance().getMyNaturalLocation())
	{
		ourHatchery =
			InformationManager::Instance().getBaseDepot(InformationManager::Instance().getMyNaturalLocation());
		if (UnitUtil::IsValidUnit(ourHatchery))
		{
			front = MacroLocation::Natural;
		}
	}
	if (front == MacroLocation::Anywhere)
	{
		ourHatchery =
			InformationManager::Instance().getBaseDepot(InformationManager::Instance().getMyMainBaseLocation());
		if (UnitUtil::IsValidUnit(ourHatchery))
		{
			front = MacroLocation::Main;
		}
	}
	if (!ourHatchery || front == MacroLocation::Anywhere)
	{
		// We don't have a place to put static defense
		return;
	}

	// 2. Count enemy ground power.
	int enemyPower = 0;
	int enemyPowerNearby = 0;
	bool enemyHasVultures = false;
	bool enemyHasSiege = false;
	bool enemyHasDTs = false;
	int enemyRushBuildings = 0;
	bool enemyProxy = false; 
	for (const auto & kv : InformationManager::Instance().getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (!ui.type.isBuilding() && !ui.type.isWorker() &&
			(ui.type.groundWeapon() != BWAPI::WeaponTypes::None || ui.type == BWAPI::UnitTypes::Terran_Medic) &&
			!ui.type.isFlyer())
		{
			enemyPower += ui.type.supplyRequired();
			if (ui.updateFrame >= _lastUpdateFrame - 30 * 24 &&          // seen in the last 30 seconds
				ui.lastPosition.isValid() &&
				ourHatchery->getDistance(ui.lastPosition) < 1700)		 // not far from our front base
			{
				if (ui.type.isBuilding()) {
					enemyProxy = true;
				}
				enemyPowerNearby += ui.type.supplyRequired();
			}
			if (ui.type == Terran_Vulture)
			{
				enemyHasVultures = true;
			}
			else if (ui.type == Terran_Siege_Tank_Tank_Mode ||
				ui.type == Terran_Siege_Tank_Siege_Mode ||
				ui.type == Protoss_Reaver) {
				enemyHasSiege = true;
			}
			else if (ui.type == Protoss_Dark_Templar) {
				enemyHasDTs = true;
			}
		}
		else {
			if (ui.type == Protoss_Gateway ||
				ui.type == Terran_Barracks ||
				ui.type == Terran_Academy) {
					enemyRushBuildings++;
			}
			else if (ui.type == Terran_Factory ||
				ui.type == Terran_Starport ||
				ui.type == Protoss_Robotics_Facility) {
				enemyHasSiege = true; //assume they'll have it soon
				enemyRushBuildings--;
			}
		}
	}

	// 3. Count our anti-ground power, including air units.
	int ourPower = 0;
	int ourSunkens = 0;
	int extraSunkens = 0;
	size_t nCreepColonies = 0;
	for (const BWAPI::Unit u : _self->getUnits())
	{
		if (!u->getType().isBuilding() && !u->getType().isWorker() &&
			u->getType().groundWeapon() != BWAPI::WeaponTypes::None &&
			u->getDistance(ourHatchery) < 600)
		{
			ourPower += u->getType().supplyRequired();
		}
		else if (u->getType() == Zerg_Sunken_Colony ||
			u->getType() == Zerg_Creep_Colony)          // blindly assume it will be a sunken
		{
			if (u->getType() == Zerg_Creep_Colony && u->isCompleted()) {
				++nCreepColonies;
			}
			if (ourHatchery->getDistance(u) < 450)
			{
				++ourSunkens;
			}
			else {
				++extraSunkens;
			}
		}
	}
	ourSunkens += extraSunkens / 3;

	bool enemyRushing = enemyPower >= 10 && (enemyRushBuildings >= 3 || (enemyPowerNearby >= 12 && _enemyRace == BWAPI::Races::Terran)) 
		&& BWAPI::Broodwar->elapsedTime() < 6 * 60;
	int queuedSunkens = numBeingBuilt(Zerg_Creep_Colony) + queue.numInQueue(Zerg_Creep_Colony) + queue.numInQueue(Zerg_Sunken_Colony); // +queue.numInQueue(Zerg_Creep_Colony);  // without checking location
	//Since creep colonies don't show up in the queue until building is started
	//the number of queued sunkens may be inaccurate. This is sort of acceptable
	//for the main/natural bases, but preferably not too many sunkens are built.
	ourPower += 3 * (ourSunkens + queuedSunkens);

	// 4. Build sunkens, but only if we have the eco to support them

	//don't need pool for creep colonies, but we do for zerglings and sunkens
	if (nMineralDrones >= 9)
	{
		//make at least 1 sunken if P/T has an army, at our 2nd hatch
		/*if (!_emergencyGroundDefense && (enemyPower > 12 || ourPower < enemyPower) && nCompletedHatches >= 2 && nDrones > 11) {
			//also make a sunken if we're doing some kind of greedy start build
			if (queuedSunkens + ourSunkens < 1 && nDrones >= 11 && !_emergencyGroundDefense)
			{
				queue.queueAsHighestPriority(Zerg_Drone);
				queue.queueAsHighestPriority(MacroAct(Zerg_Creep_Colony, front));
			}
		}*/


		if (enemyPower > ourPower || enemyRushing)
		{
			// Make up to 6 sunkens at the front, one at a time.
			// During an emergency sunks will die and/or cause jams, so don't bother then.
			int recSunkens = recSunkens = (enemyPower - ourPower) / 3;
			if (_enemyRace == BWAPI::Races::Terran) {
				recSunkens = (enemyPower - ourPower) / 3;
			}

			if (_enemyRace == BWAPI::Races::Zerg && recSunkens > 3) {
				recSunkens = 3; //if we need more than 3 we're probably dead anyway
			}
			else if (_enemyRace == BWAPI::Races::Terran && recSunkens > 4) { //same logic as above
				recSunkens = 4;
			}
			else if (recSunkens > 5) { // at most 5 sunkens no matter what (versus protoss)
				recSunkens = 5;
			}
			else if (recSunkens < 2 && enemyRushing) { //detected imminent enemy rush, get some sunkens
				recSunkens = 2;
				if (nHydras >= 7) recSunkens = 1;
			}

			if ((enemyHasSiege || enemyHasVultures) && recSunkens > 3) { //futile to amass sunkens against siege tanks/reavers
				recSunkens = 3;
			}

			//we have hydras or mutas! no good reason to get many sunkens!
			if ((hasDen || nHydras > 0 || hasSpire) && _enemyRace == BWAPI::Races::Protoss) {
				recSunkens /= 2;
			}

			if (enemyHasDTs && recSunkens < 2) recSunkens = 2; //at least 2 sunkens for DTs


			if (nCreepColonies == 0 && _emergencyGroundDefense && !outOfBook && !hasSpire && !hasDen && hasPool && 
				(_enemyRace == BWAPI::Races::Zerg || nHatches >= 2) && ourSunkens < 2) {
				//if we're in a critical early emergency and on book, with enough larva, make some zerglings!
				int numZerglings = std::min(enemyPower-ourPower, nLarvas) - queue.numInQueue(Zerg_Zergling);
				for (int i = 0; i < numZerglings; i++) {
					queue.queueAsHighestPriority(Zerg_Zergling);
				}
			} 
			else if (!_emergencyGroundDefense || (_enemyRace == BWAPI::Races::Terran && !enemyHasSiege) || ourSunkens >= 3)
			{
				//if we have a expo building, we wait for it to finish first
				if (nHatches == nCompletedHatches || nCompletedHatches >= 2) {
					//if we have no sunkens, allow it to build up to 3 sunkens at once
					if (queuedSunkens < std::max(2, 3-ourSunkens) && ourSunkens < recSunkens) {
						queue.queueAsHighestPriority(Zerg_Drone);
						queue.queueAsHighestPriority(MacroAct(Zerg_Creep_Colony, front));
					}
				}
			}
		}
		
		if (!isBeingBuilt(Zerg_Creep_Colony) /*&& !_emergencyGroundDefense*/ && (enemyHasVultures || (enemyPower >= 12 && nBases >= 4)))
		{
			if (queuedSunkens == 0 && nDrones >= 14) {
				//each frame check once to see if a base's mineral line is undefended
				if (InformationManager::Instance().getMyLeastDefendedLocation()) { //sadly we have to check if there is one before we queue it
					queue.queueAsHighestPriority(Zerg_Drone);
					queue.queueAsHighestPriority(MacroAct(Zerg_Creep_Colony, MacroLocation::LeastDefended), false, DefenseLocation::Minerals);
				}
			}
		}
	}

	//sunkens are morphed reactively v. p/t. note that nearly finished sunkens have higher priority than ling emergencies
	//but we should never get it in place of crucial drones
	//and don't get any if the enemy is nowhere close, unless we have many bases
	if (hasPool && nDrones > 3 && (_enemyRace == BWAPI::Races::Zerg || enemyProxy || enemyPowerNearby > 0 || nBases >= 4) && nCreepColonies > queue.numInQueue(Zerg_Sunken_Colony) && (queue.numInQueue(Zerg_Spore_Colony) == 0)) {
		//presume the unassigned, completed colony was supposed to be a sunken... 
		//this way we never block the queue if our creep colony is never completed
		queue.queueAsHighestPriority(Zerg_Sunken_Colony);
	}


	// 5. Declare an emergency.
	// The nHatches term adjusts for what we may be able to build before the enemy arrives.
	// If we have a lot of resources, we may be fine anyway
	if (enemyPowerNearby > ourPower + nHatches)
	{
		_emergencyGroundDefense = true;
		_emergencyStartFrame = _lastUpdateFrame;
	}
}

// Versus protoss, decide whether hydras or mutas are more valuable.
// Decide by looking at the protoss unit mix.
bool StrategyBossZerg::vProtossDenOverSpire()
{
	// Bias.
	int denScore   = 3;
	int spireScore = 0;
	lurkerPreference = 0;
	lingPreference = 0;
	devourerPreference = 0;
	guardianPreference = 0;
	queenPreference = 0; 

	/* If we have the tech already, should at least invest in it */
	if (hasDen && !hasSpire) {
		if (nHydras + 2*nLurkers < 12) denScore += 20;
		if (hasLurkerAspect && nLurkers < 3) denScore += 20;
	}
	else if ((hasSpire || isBeingBuilt(Zerg_Spire)) && !hasDen && nMutas < 6) {
		spireScore += 9;
	}

	for (const auto & kv : InformationManager::Instance().getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (!ui.type.isWorker() && !ui.type.isBuilding())
		{
			if (ui.type == Protoss_Archon ||
				ui.type == Protoss_Carrier ||
				ui.type == Protoss_Corsair)
			{
				if (ui.type == Protoss_Carrier) { //need our hydras to shoot them
					lurkerPreference -= 100;
					guardianPreference -= 100; //guardians really are the last thing we want against carriers
					devourerPreference++;
				}
				else if (ui.type == Protoss_Corsair) {
					denScore += 8;
					guardianPreference -= 20;
					devourerPreference++;
					lurkerPreference -= 3;
				}

				denScore += ui.type.supplyRequired();
			} // Enemy mobile combat units. [reavers, templars, shuttles, dts]
			else if (ui.type == Protoss_Dragoon) {
				lurkerPreference++;
				lingPreference++;
				if (hasHiveTech) {
					//need to support melee upgrades first, check for adrenal glands also
					//lingPreference++;
				}
			}
			else if (ui.type.airWeapon() == BWAPI::WeaponTypes::None)
			{
				if (ui.type == Protoss_Zealot) {
					lurkerPreference += 2;
					lingPreference -= 2;
				}
				else {
					lurkerPreference--;
					spireScore += ui.type.supplyRequired();
				}
			}

			if (ui.type.isFlyer()) {
				guardianPreference -= 20;
			}

			if (ui.type == Protoss_Reaver) {
				spireScore += 8;
				lingPreference += 1;
				lurkerPreference -= 4; 
			}
		}
		else if (ui.type == ui.type == Protoss_Photon_Cannon) 
		{
			denScore += 2;
			if (!hasDefiler) lingPreference -= 2;
			guardianPreference += 2;
			lurkerPreference -= 4;
		}
		else if (ui.type == Protoss_Robotics_Facility)
		{
			// Spire is good against anything from the robo fac.
			spireScore += 2;
		}
		else if (ui.type == Protoss_Robotics_Support_Bay)
		{
			// Spire is especially good against reavers.
			spireScore += 8; 
		}
	}

	if (hasDen && lurkerPreference > 24) denScore += 3*lurkerPreference; //only weight massive amounts of zealots to den score
	if (guardianPreference >= 20) spireScore += 3*guardianPreference; //they must have a hell of a lot of photon cannons 
	if (spireScore >= denScore && lingPreference > 0) lingPreference = 0; // no reason to get lings over spire tech

	return denScore > spireScore;
}

// Versus terran, decide whether hydras or mutas are more valuable.
// Decide by looking at the terran unit mix.
bool StrategyBossZerg::vTerranDenOverSpire()
{
	// Bias.
	int denScore   = 0;
	int spireScore = 3;
	lurkerPreference = 0;
	lingPreference = 0;
	queenPreference = 2; //only get 1 queen ever
	devourerPreference = 0;

	/* If we have the tech already, should at least invest in it */
	if (hasDen && !hasSpire) {
		if (nHydras + nLurkers < 5) denScore += 20;
		if (hasLurkerAspect && nLurkers < 3) denScore += 20;
	}
	else if ((hasSpire || isBeingBuilt(Zerg_Spire)) && !hasDen && nMutas < 13) {
		spireScore += 10;
	}

	for (const auto & kv : InformationManager::Instance().getUnitData(_enemy).getUnits())
	{
		const UnitInfo & ui(kv.second);

		if (ui.type == Terran_Marine ||
			ui.type == Terran_Medic ||
			ui.type == Terran_Firebat ||
			ui.type == Terran_Barracks ||
			ui.type == Terran_Academy)
		{
			lurkerPreference++;
			if (ui.type == Terran_Firebat) lingPreference -= 2;
			devourerPreference--;
			denScore += 1;
		}
		else if (ui.type == Terran_Missile_Turret)
		{
			denScore += 1;
			guardianPreference++;
			devourerPreference -= 2;
		}
		else if (ui.type == Terran_Goliath)
		{
			denScore += 2;
			guardianPreference -= 2;
			devourerPreference -= 3;
			//queenPreference++;
			lingPreference -= 5; //hydralisks do better against these
		}
		else if (ui.type == Terran_Siege_Tank_Siege_Mode ||
			ui.type == Terran_Siege_Tank_Tank_Mode)
		{
			spireScore += 17;
			//lingPreference += 4; //lings can sometimes be useful against these
			lurkerPreference -= 3;
			devourerPreference -= 4;
			//queenPreference += 2; 
			guardianPreference += 2;
		}
		else if (ui.type == Terran_Vulture) {
			spireScore += 1;
			lingPreference -= 10; //AI vultures are strong against zerglings
			lurkerPreference -= 2; //hydras are better for vultures
		}
		else if (ui.type == Terran_Valkyrie ||
			ui.type == Terran_Battlecruiser)
		{
			denScore += 4;
			lurkerPreference -= 20;
			guardianPreference-= 50;
			devourerPreference += 100;
		}
		else if (ui.type == Terran_Wraith) {
			guardianPreference -= 20;
			lurkerPreference -= 2;
			spireScore += 17;
			devourerPreference++;
		}
	}

	if (hasDen && lurkerPreference > 19) denScore += lurkerPreference; //only weight massive amounts of bio to den score
	if (guardianPreference >= 20 && (hasGreaterSpire || (hasHiveTech && hasASpire))) queenPreference = 2;
	/*if (queenPreference >= 10 && hasQueensNest && nGas >= 3) {
		denScore += 20 * queenPreference; //if we want queens, it's likely mech; we want hydras
	}
	else */
	if (spireScore > denScore) { //no queens if we want mutas
		queenPreference = 0;
	}

	//cap the number of active lings on the field until we have more hatcheries
	if (nLings >= 8 && (nCompletedHatches <= 5 || nDrones <= 30)) {
		lingPreference = -12;
	}

	return denScore > spireScore;
}

// Are hydras or mutas more valuable?
// true = hydras, false = mutas
bool StrategyBossZerg::chooseDenOverSpire()
{
	if (_enemyRace == BWAPI::Races::Protoss)
	{
		return vProtossDenOverSpire();
	}
	if (_enemyRace == BWAPI::Races::Terran)
	{
		return vTerranDenOverSpire();
	}
	// Otherwise enemy is zerg or random. Always go spire.
	return false;
}

// Set _mineralUnit and _gasUnit depending on our tech and the game situation.
// Current universe of units: Drone, Zergling, Hydralisk, Mutalisk, Ultralisk.
// This tells freshProductionPlan() what units to make.
void StrategyBossZerg::chooseUnitMix(bool denOverSpire)
{
	// No tech default
	BWAPI::UnitType minUnit = Zerg_Drone;
	BWAPI::UnitType gasUnit = UnitTypes::None;

	bool enemyHasAir = InformationManager::Instance().getAir2GroundSupply(_enemy) > 32;

	if (hasDen && (enemyHasAir || nLings > (8 + 2*nHydras))) { 
		minUnit = Zerg_Hydralisk;
	}
	else if (hasPool && (lingPreference >= -12 || minerals > gas + 2000)) { //just drone or hydra if making lings is futile
		minUnit = Zerg_Zergling;
	}
	else if (hasDen) { //no pool, better make some hydras quick!
		minUnit = Zerg_Hydralisk;
	}


	if (hasQueensNest && queenPreference > 0 && nQueens < queenPreference/2 && nDrones > 20 && nGas >= 2) {
		gasUnit = Zerg_Queen;
	}
	else if (denOverSpire) {
		if (hasDefiler && nDefilers < 2 && nDrones >= 26 && nGas >= 3) {
			gasUnit = Zerg_Defiler;
		}
		else if (hasUltra && nUltras <= 14 && !enemyHasAir && nDrones >= 20 && nGas >= 2) {
			gasUnit = Zerg_Ultralisk;
		}
		/*else if (hasPool && lingPreference > 5 && nLings < 2 * lingPreference && minUnit == Zerg_Zergling) {
			//don't set the gas unit, just produce zerglings
		}*/
		else if (hasDen && !enemyHasAir && hasLurkerAspect && lurkerPreference > -20 && (lurkerPreference >= 20 || nLurkers < 4 || (minerals < 2*gas && 6 * nLurkers < nHydras))) {
			gasUnit = Zerg_Lurker;
		}
		else if (hasDen) {
			gasUnit = Zerg_Hydralisk;
		}
	}
	else if (hasASpire) {
		if (hasGreaterSpire && 6 * nDevourers < nMutas && nMutas > 14 && devourerPreference > 0) {
			gasUnit = Zerg_Devourer;
		}
		else if (hasGreaterSpire && !enemyHasAir && nGuardians < 24 && guardianPreference > 0 && (6*nGuardians < nMutas || guardianPreference > 20)) { //cap guardians in case enemy switches to air
			gasUnit = Zerg_Guardian;
		} 
		else {
			gasUnit = Zerg_Mutalisk;
		}
	}

	setUnitMix(minUnit, gasUnit);
}

// Choose the next tech to aim for, whether sooner or later.
// This tells freshProductionPlan() what to move toward, not when to take each step.
void StrategyBossZerg::chooseTechTarget(bool denOverSpire)
{
	// True if we DON'T want it: We have it or choose to skip it.
	const bool den = _enemyRace == BWAPI::Races::Zerg || hasDen || isBeingBuilt(Zerg_Hydralisk_Den);
	const bool spire = hasSpire || isBeingBuilt(Zerg_Spire);
	const bool ultra = hasUltra || isBeingBuilt(Zerg_Ultralisk_Cavern);
	const bool lurker = hasLurkerAspect || upping(Lurker_Aspect);
	const bool greaterSpire = hasGreaterSpire || isBeingBuilt(Zerg_Greater_Spire);
	const bool defiler = has(Zerg_Defiler_Mound) > 0;

	// Default. Value at the start of the game and after all tech is available.
	_techTarget = TechTarget::None;

	// From low tech to high
	if (denOverSpire) {
		if (!den) {
			_techTarget = TechTarget::Hydralisks;
		}
		else if (!lurker && lurkerPreference >= 12) {
			_techTarget = TechTarget::Lurkers;
		}
		//else if (!ultra) {
		//	_techTarget = TechTarget::Ultralisks;
		//}
		else if (!defiler) {
			_techTarget = TechTarget::Defilers;
		}
		else if (!ultra) {
			_techTarget = TechTarget::Ultralisks;
		}
		else if (!spire && !greaterSpire) { //we may still want spire for scourges/mutas later
			_techTarget = TechTarget::Mutalisks;
		}
	}
	else {
		if (!spire) {
			_techTarget = TechTarget::Mutalisks;
		}
		else if (!den && _enemyRace == BWAPI::Races::Protoss) { //hydralisks are a core unit against protoss
			_techTarget = TechTarget::Hydralisks;
		}
		else if (_enemyRace == BWAPI::Races::Zerg) { //defilers against mutalisks
			_techTarget = TechTarget::Defilers;
		}
		else if (!greaterSpire) {
			_techTarget = TechTarget::Guardians;
		}
	} 
}

// Set the economy ratio according to the enemy race.
// If the enemy went random, the enemy race may change!
void StrategyBossZerg::chooseEconomyRatio()
{
	if (_enemyRace == BWAPI::Races::Zerg)
	{
		setEconomyRatio(0.28);
	}
	else if (_enemyRace == BWAPI::Races::Terran)
	{
		setEconomyRatio(0.46);
	}
	else if (_enemyRace == BWAPI::Races::Protoss)
	{
		setEconomyRatio(0.37);
	}
	else
	{
		// Enemy went random, race is still unknown. Choose cautiously.
		// We should find the truth soon enough.
		setEconomyRatio(0.32);
	}
}

// Choose current unit mix and next tech target to aim for.
// Called when the queue is empty and no future production is planned yet.
void StrategyBossZerg::chooseStrategy()
{
	bool denOverSpire = chooseDenOverSpire();
	chooseUnitMix(denOverSpire);
	chooseTechTarget(denOverSpire);

	// Reset the economy ratio only if the enemy's race has changed.
	// It can change from Unknown to another race if the enemy went random.
	if (_enemyRace != _enemy->getRace())
	{
		_enemyRace = _enemy->getRace();
		chooseEconomyRatio();
	}
}

std::string StrategyBossZerg::techTargetToString(TechTarget target)
{
	if (target == TechTarget::Hydralisks) return "Hydras";
	if (target == TechTarget::Mutalisks ) return "Mutas";
	if (target == TechTarget::Ultralisks) return "Ultras";

	if (target == TechTarget::Lurkers) return "Lurkers";
	if (target == TechTarget::Guardians) return "Guardians";
	if (target == TechTarget::Defilers) return "Defilers";

	return "[none]";
}

// Draw a few fundamental strategy choices at the top left,
// in place of where GameInfo would go.
void StrategyBossZerg::drawStrategySketch()
{
	if (!Config::Debug::DrawStrategySketch)
	{
		return;
	}

	const int x = 4;
	const int y = 1;

	if (outOfBook)
	{
		std::string unitMix = "";
		if (_gasUnit != UnitTypes::None)
		{
			unitMix = UnitTypeName(_gasUnit) + " ";
		}
		unitMix += UnitTypeName(_mineralUnit);
		BWAPI::Broodwar->drawTextScreen(x, y, "%cunit mix %c%s", white, green, unitMix.c_str());
		BWAPI::Broodwar->drawTextScreen(x, y + 130, "%cnext tech %c%s", white, orange,
			techTargetToString(_techTarget).c_str());
	}
	else
	{
		BWAPI::Broodwar->drawTextScreen(x, y, "%copening %c%s", white, yellow, Config::Strategy::StrategyName.c_str());
	}
}

// Draw various internal information bits, by default on the right side left of Bases.
void StrategyBossZerg::drawStrategyBossInformation()
{
	if (!Config::Debug::DrawStrategyBossInfo)
	{
		return;
	}

	const int x = 500;
	int y = 30;

	BWAPI::Broodwar->drawTextScreen(x, y, "%cStrat Boss", white);
	y += 13;
	BWAPI::Broodwar->drawTextScreen(x, y, "%cResources: %c%d %d", yellow, cyan, minerals, gas);
	y += 10;
	BWAPI::Broodwar->drawTextScreen(x, y, "%cbases %c%d/%d", yellow, cyan, nBases, nBases+nFreeBases);
	y += 10;
	BWAPI::Broodwar->drawTextScreen(x, y, "%cpatches %c%d", yellow, cyan, nMineralPatches);
	y += 10;
	BWAPI::Broodwar->drawTextScreen(x, y, "%cgas %c%d/%d", yellow, cyan, nGas, nGas + nFreeGas);
	y += 10;
	BWAPI::Broodwar->drawTextScreen(x, y, "%cdrones%c %d/%d", yellow, cyan, nDrones, maxDrones);
	y += 10;
	BWAPI::Broodwar->drawTextScreen(x, y, "%c  (min) %c%d", yellow, cyan, nMineralDrones);
	y += 10;
	BWAPI::Broodwar->drawTextScreen(x, y, "%c  (gas) %c%d", yellow, cyan, nGasDrones);
	y += 10;
	BWAPI::Broodwar->drawTextScreen(x, y, "%clarvas %c%d", yellow, cyan, nLarvas);
	y += 13;
	if (outOfBook)
	{
		BWAPI::Broodwar->drawTextScreen(x, y, "%cecon %c%.2f", yellow, cyan, _economyRatio);
		y += 10;
		BWAPI::Broodwar->drawTextScreen(x, y, "%c%s", green, UnitTypeName(_mineralUnit).c_str());
		y += 10;
		BWAPI::Broodwar->drawTextScreen(x, y, "%c%s", green, UnitTypeName(_gasUnit).c_str());
		if (_techTarget != TechTarget::None)
		{
			y += 10;
			BWAPI::Broodwar->drawTextScreen(x, y, "%cplan %c%s", white, orange,
				techTargetToString(_techTarget).c_str());
		}
	}
	else
	{
		BWAPI::Broodwar->drawTextScreen(x, y, "%c[book]", white);
	}
	if (_emergencyGroundDefense)
	{
		y += 13;
		BWAPI::Broodwar->drawTextScreen(x, y, "%cEMERGENCY", red);
	}
}

// -- -- -- -- -- -- -- -- -- -- --
// Public methods.

StrategyBossZerg & StrategyBossZerg::Instance()
{
	static StrategyBossZerg instance;
	return instance;
}

// Set the unit mix.
// The mineral unit will can set to Drone, but cannot be None.
// The mineral unit must be less gas-intensive than the gas unit.
// The idea is to make as many gas units as gas allows, and use any extra minerals
// on the mineral units (which may want gas too).
void StrategyBossZerg::setUnitMix(BWAPI::UnitType minUnit, BWAPI::UnitType gasUnit)
{
	// The mineral unit must be given.
	if (minUnit == UnitTypes::None)
	{
		// BWAPI::Broodwar->printf("mineral unit should be given");
		minUnit = Zerg_Drone;
	}

	if (gasUnit != UnitTypes::None && minUnit.gasPrice() > gasUnit.gasPrice())
	{
		// BWAPI::Broodwar->printf("mineral unit cannot want more gas");
		gasUnit = UnitTypes::None;
	}

	_mineralUnit = minUnit;
	_gasUnit = gasUnit;
}

void StrategyBossZerg::setEconomyRatio(double ratio)
{
	_economyRatio = ratio;
}

// Solve urgent production issues. Called once per frame.
// If we're in trouble, clear the production queue and/or add emergency actions.
// Or if we just need overlords, make them.
// This routine is allowed to take direct actions or cancel stuff to get or preserve resources.
void StrategyBossZerg::handleUrgentProductionIssues(BuildOrderQueue & queue)
{
	updateGameState();

	while (nextInQueueIsUseless(queue))
	{
		// BWAPI::Broodwar->printf("removing useless %s", queue.getHighestPriorityItem().macroAct.getName().c_str());

		if (queue.getHighestPriorityItem().macroAct.isUnit() &&
			queue.getHighestPriorityItem().macroAct.getUnitType() == Zerg_Hatchery)
		{
			// We only cancel a hatchery in case of dire emergency. Get the scout drone back home.
			// Also cancel hatcheries
			ScoutManager::Instance().releaseWorkerScout();
			BuildingManager::Instance().cancelBuildingType(Zerg_Hatchery);
		}
		queue.removeHighestPriorityItem();
	}

	// Check for the most urgent actions once per frame.
	if (takeUrgentAction(queue))
	{
		// These are serious emergencies, and it's no help to check further.
		makeOverlords(queue);
	}
	else
	{
		// Check for less urgent reactions less often.
		int frameOffset = BWAPI::Broodwar->getFrameCount() % 32;
		if (frameOffset == 0)
		{
			makeUrgentReaction(queue);
			makeOverlords(queue);
		}
		else if (frameOffset == 16)
		{
			checkGroundDefenses(queue);
			makeOverlords(queue);
		}
	}
}

// Called when the queue is empty, which means that we are out of book.
// Fill up the production queue with new stuff.
BuildOrder & StrategyBossZerg::freshProductionPlan()
{
	_latestBuildOrder.clearAll();
	updateGameState();
	chooseStrategy();

	bool rich = (minerals > 700) && (gas > 700) && (nBases >= 3) && (nDrones > 15);
	
	// 1. Add up to 9 drones if we're below.
	for (int i = nDrones; i < std::min(9, maxDrones); ++i)
	{
		produceCheck(Zerg_Drone);
	}

	// 2. If there is no spawning pool, we always need that.
	if (!has(Zerg_Spawning_Pool))
	{
		produce(Zerg_Spawning_Pool);
	}

	// 3. We want to expand.
	// Division of labor: Expansions are here, macro hatcheries are "urgent production issues".
	// However, some macro hatcheries may be placed at expansions.
	//don't try to expand during an emergency
	if (!_emergencyGroundDefense && nFreeBases > 0 &&
		(minerals > 2000 ||
		nDrones > nMineralPatches + 3 * (nGas + nFreeGas)) && 
		numBeingBuilt(Zerg_Hatchery) < std::min((minerals+300)/300, 3))
	{
		MacroLocation loc = MacroLocation::Expo;
		if (nBases >= 2) loc = MacroLocation::MinOnly; //we want to pick the closest base to gain map control 

		produce(MacroAct(Zerg_Hatchery, loc));
	}

	// 4. Gas
	if (nFreeGas > 0 && numBeingBuilt(Zerg_Extractor) < std::min(2, nFreeGas)) 
	{
		if (nGas + isBeingBuilt(Zerg_Extractor) == 0 && gas < 300 && nDrones >= 9 && hasPool) // first gas only
		{
			produce(Zerg_Extractor);
			if (!WorkerManager::Instance().isCollectingGas()) produce(MacroCommandType::StartGas);
		} 
		else if (shouldGet(Zerg_Extractor))
		{
			produce(Zerg_Extractor);
		} 
	}

	// Upgrading Tree. First Tier Building Tech (including Lurker Aspect, Adrenal Glands, Evo Chamber)
	/* ----------------------------------------------------- */
	if (nGas > 0 || gas > 200) { 
		if (hasPool) {
			//Make Lair
			if (shouldGet(Zerg_Lair)) produce(MacroAct(Zerg_Lair, MacroLocation::Main)); //prefer the main hatchery

			//Ling upgrades
			if (shouldGet(Metabolic_Boost)) produceCheck(Metabolic_Boost);
			else if (shouldGet(Adrenal_Glands)) produceCheck(Adrenal_Glands);
			

			//Make a hydralisk den!
			if (shouldGet(Zerg_Hydralisk_Den)) {
				if (_enemyRace == BWAPI::Races::Terran) {
					produce(MacroAct(Zerg_Hydralisk_Den, MacroLocation::Main));
				}
				else {
					produce(MacroAct(Zerg_Hydralisk_Den, MacroLocation::Natural));
				}
			} else if (shouldGet(Zerg_Evolution_Chamber)) {
				if (_enemyRace == BWAPI::Races::Terran) {
					produce(MacroAct(Zerg_Evolution_Chamber, MacroLocation::Main));
				}
				else {
					produce(MacroAct(Zerg_Evolution_Chamber, MacroLocation::Natural));
				}
			}
		}


		//Hydralisk Upgrades
		if ((_mineralUnit == Zerg_Hydralisk 
			|| _gasUnit == Zerg_Hydralisk
			|| _gasUnit == Zerg_Lurker
			|| _techTarget == TechTarget::Lurkers
			|| nHydras > 16))
		{
			if (shouldGet(Lurker_Aspect)) produceCheck(Lurker_Aspect);
			else if (shouldGet(Muscular_Augments)) produceCheck(Muscular_Augments);
			else if (shouldGet(Grooved_Spines)) produceCheck(Grooved_Spines);
		}

		// Evolution Chamber Upgrades
		if (!busy(Zerg_Evolution_Chamber) && (rich || nHydras + nLings + nUltras + nLurkers > 8) &&
			!((_techTarget != TechTarget::Mutalisks || _gasUnit == Zerg_Mutalisk) && gas < 600 && nMutas < 6))
		{
			//we include whether we're upgrading it already, to decide whether to get the other upgrade with evolution chamber
			int armorUps = _self->getUpgradeLevel(Zerg_Carapace) + upping(Zerg_Carapace);
			int missileUps = _self->getUpgradeLevel(Zerg_Missile_Attacks) + upping(Zerg_Missile_Attacks);
			int meleeUps = _self->getUpgradeLevel(Zerg_Melee_Attacks) + upping(Zerg_Melee_Attacks);

			bool eT = (_enemyRace == BWAPI::Races::Terran);
			bool eP = (_enemyRace == BWAPI::Races::Protoss);
			bool eZ = (_enemyRace == BWAPI::Races::Zerg);

			bool COverMi = (armorUps <= missileUps);
			//bool MiOverMe = (missileUps <= meleeUps);
			bool MiOverC = missileUps <= armorUps;
			bool COverMe = (armorUps <= meleeUps);

			if (armorUps <= techLevel && 
				!upping(Zerg_Carapace) && 
				((eP && !MiOverC) ||
				(eT && COverMi) ||
				(eZ && COverMe))) 
			{
				produceCheck(Zerg_Carapace);
			} else if (missileUps <= techLevel && 
				!upping(Zerg_Missile_Attacks) &&
				((eP && MiOverC) ||
				(eT && !COverMi) ||
				(eZ && (missileUps + meleeUps == 6)))) 
			{
				produceCheck(Zerg_Missile_Attacks);
			} else if (meleeUps <= techLevel && 
				!upping(Zerg_Melee_Attacks) &&
				((eP && (missileUps + armorUps == 6)) ||
				(eT && (missileUps + armorUps == 6)) ||
				(eZ && (!COverMe || hasUltra)))) 
			{
				produceCheck(Zerg_Melee_Attacks);
			}
		}
	}

	// Second Tier Building Tech (including Spire, Spire Upgrades, Queen's Nest)
	/* ----------------------------------------------------- */

	//Spire unique case for zerg matchup
	if (shouldGet(Zerg_Spire)) produce(Zerg_Spire);

	if (nGas + nFreeGas >= 2) {

		if (hasSpire && (rich || nMutas > 6) && !_emergencyGroundDefense
			&& !busy(Zerg_Spire))
		{
			int flyerArmorUps = _self->getUpgradeLevel(Zerg_Flyer_Carapace) + upping(Zerg_Flyer_Carapace);
			int flyerAttackUps = _self->getUpgradeLevel(Zerg_Flyer_Attacks) + upping(Zerg_Flyer_Attacks);

			if ((flyerAttackUps + flyerArmorUps >= 1) && 
				_techTarget == TechTarget::Guardians && !has(Zerg_Greater_Spire)) {
				produce(Zerg_Greater_Spire);
			}
			else if (flyerAttackUps < flyerArmorUps && flyerAttackUps <= techLevel) {
				produceCheck(Zerg_Flyer_Attacks);
			}
			else if (flyerArmorUps <= techLevel) {
				produceCheck(Zerg_Flyer_Carapace);
			}
		}

		if (shouldGet(Pneumatized_Carapace)) produce(Pneumatized_Carapace);
		else if (shouldGet(Ventral_Sacs)) produce(Ventral_Sacs);

		if (shouldGet(Zerg_Queens_Nest)) produce(Zerg_Queens_Nest);
		else if (shouldGet(Spawn_Broodlings)) produce(Spawn_Broodlings);
		else if (shouldGet(Ensnare)) produce(Ensnare);

		if (shouldGet(Zerg_Hive) && !busy(Zerg_Lair))
		{
			produce(MacroAct(Zerg_Hive, MacroLocation::Main)); //prefer the main
		}
	}

	// Third Tier Building Tech
	if (hasHiveTech && nGas + nFreeGas >= 3 && nDrones >= 24) {
		// Move toward ultralisks.
		if (shouldGet(Zerg_Ultralisk_Cavern)) {
			produce(Zerg_Ultralisk_Cavern);
		}
		else if (shouldGet(Anabolic_Synthesis)) {
			produceCheck(Anabolic_Synthesis);
		}
		else if (shouldGet(Chitinous_Plating)) {
			produceCheck(Chitinous_Plating);
		}
		//GreaterSpire upgrade is handled in Lair Tech section
		//defilers later:
		if (shouldGet(Zerg_Defiler_Mound)) {
			produce(Zerg_Defiler_Mound);
		}
		else if (shouldGet(Consume)) {
			produce(Consume);
		}
		else if (shouldGet(Plague)) {
			produce(Plague);
		}
	}
	
	// Unit Production
	/* -------------------------------------------------------------- */

	//produce infested terrans
	if (hasInfested && nInfested <= 1 && nDrones > 19 && minerals > 100 && gas > 50) {
		produce(Zerg_Infested_Terran);
	}

	int larvaSave = 0;
	//not sure this works...
	/*if (_enemyRace == BWAPI::Races::Zerg && isBeingBuilt(Zerg_Spire)) {
		//4 for now, until we write in actual code to determine when the spire will finish
		//and don't waste larva on hatcheries with more larva than usual
		larvaSave = std::min(std::min(4, minerals/100), std::min(3 * nHatches, 3 * nGas));
	}*/
	// Include drones according to _economyRatio.
	//  The mineral unit may also need gas.
	while (nLarvas > larvaSave)
	{
		BWAPI::UnitType type = (_gasUnit == UnitTypes::None) ? _mineralUnit : _gasUnit;

		if (needDroneNext()) {
			type = Zerg_Drone;
		}
		else if (hasPool && nDefilers >= 2 && nLings < 12 && (nHydras + nLurkers + nUltras + nMutas > 24) && nHatches >= 6) { 
			//need ammo for defilers
			type = Zerg_Zergling;
		}
		else if (type == _gasUnit && gas > type.gasPrice())
		{
			// build hydralisks if we want lurkers! 
			if (type == Zerg_Lurker) {
				if (nHydras < 6*nLurkers && !((lurkerPreference > 20 || nLurkers < 3) && nHydras != 0)) { //
					type = Zerg_Hydralisk; //need hydras!
				}
			} //build mutalisks for guardians/devourers!  
			else if (type == Zerg_Guardian || type == Zerg_Devourer) {
				if (!(nMutas > 0 && guardianPreference > 20) &&
					nMutas <= 6*nDevourers + 3*nGuardians) { 
					type = Zerg_Mutalisk; //need mutas!
				}
			}
		}
		else
		{
			type = _mineralUnit;
		}

		if (type == Zerg_Defiler && nDefilers >= 2) break; //limit number of defilers
		if (type == Zerg_Queen && nQueens >= queenPreference / 2) break; //limit number of queens
		if (gas < type.gasPrice() || minerals < type.mineralPrice()) break; //not enough money

		produceCheck(type);
	}
	
	return _latestBuildOrder;
}

//Returns whether we are currently upgrading the given type.
bool StrategyBossZerg::upping(const MacroAct & t) const {
	if (t.isUpgrade()) return _self->isUpgrading(t.getUpgradeType());
	if (t.isTech()) return _self->isResearching(t.getTechType());

	UAB_ASSERT(false, "Invalid argument to StrategyBossZerg::upping");
	return false;
}

//Returns the number of type t we have, that are either complete or in progress
int StrategyBossZerg::has(const UnitType & t, bool complete) const {
	int count = complete ? UnitUtil::GetCompletedUnitCount(t) : UnitUtil::GetAllUnitCount(t);
	if (!complete && count == 0) count = isBeingBuilt(t); //in the queue, but construction not started yet; presume at most one
	return count;
}

bool StrategyBossZerg::shouldGet(const MacroAct & t) const {
	//if (_emergencyGroundDefense) return false;

	if (t.isUpgrade()) {
		if (!canGet(t)) return false;

		auto type = t.getUpgradeType();

		switch (type.getID()) {
		//Zergling
		case (UpgradeTypes::Enum::Metabolic_Boost) : //Speed against zerg, or if we want lings
			return (lingPreference > 5 || nDefilers >= 2 || _enemyRace == BWAPI::Races::Zerg) && !(_techTarget == TechTarget::Mutalisks || _techTarget == TechTarget::Lurkers);
		case (UpgradeTypes::Enum::Adrenal_Glands) :
			return nLings > 20;

		//Hydralisk
		case (UpgradeTypes::Enum::Muscular_Augments) :
			return true;
		case (UpgradeTypes::Enum::Grooved_Spines) :
			return nHydras > 5;

		//Queen
		case (UpgradeTypes::Enum::Gamete_Meiosis) : //queen energy. Not needed.
			return false;

		//Overlord
		// Require speed for scouting. Get it sometime after we've gotten mid-tier units but not in ZvZ. 
		case (UpgradeTypes::Enum::Pneumatized_Carapace) :
			return nMutas + nHydras + nLurkers > 9 && _enemyRace != BWAPI::Races::Zerg;
		case (UpgradeTypes::Enum::Ventral_Sacs) :
			return _dropStrategy && nMutas + nHydras + nLurkers > 9 && _enemyRace != BWAPI::Races::Zerg;
		case (UpgradeTypes::Enum::Antennae) : //overlord vision. Not needed. 
			return false;

		//Ultralisk
		case (UpgradeTypes::Enum::Anabolic_Synthesis) : 
			return true;
		case (UpgradeTypes::Enum::Chitinous_Plating) :
			return true;

		//Defiler
		case (UpgradeTypes::Enum::Metasynaptic_Node) : //defiler energy. Not needed.
			return false;

		/* Currently we do not handle these cases here, but in the production plan section
		case (UpgradeTypes::Enum::Zerg_Missile_Attacks) :
			return true;
		case (UpgradeTypes::Enum::Zerg_Melee_Attacks) :
			return true;
		case (UpgradeTypes::Enum::Zerg_Carapace) :
			return true;
		case (UpgradeTypes::Enum::Zerg_Flyer_Attacks) :
			return true;
		case (UpgradeTypes::Enum::Zerg_Flyer_Carapace) :
			return true;*/

		default:
			UAB_ASSERT(false, "Unsupported type %s in function shouldGet in StrategyBossZerg.cpp", type.c_str());
			return false;
		}
	}

	if (t.isTech()) {
		if (!canGet(t)) return false;

		auto type = t.getTechType();

		switch (type.getID()) {
		//Some ground units
		case (TechTypes::Enum::Burrowing) :
			return false;

		//Lurker
		case (TechTypes::Enum::Lurker_Aspect) :
			return _techTarget == TechTarget::Lurkers;
		
		//Queen
		case (TechTypes::Enum::Ensnare) :
			return queenPreference > 0 && false;
		case (TechTypes::Enum::Spawn_Broodlings) :
			return (queenPreference > 0 && nHatches > 7) || queenPreference >= 8;
		
		//Defiler
		case (TechTypes::Enum::Consume) :
			return hasDefiler;
		case (TechTypes::Enum::Plague) :
			return hasDefiler;
		default:
			UAB_ASSERT(false, "Unsupported type %s in function shouldGet in StrategyBossZerg.cpp", type.c_str());
			return false;
		}
	}

	if (t.isBuilding()) {
		auto type = t.getUnitType();

		bool wantHiveTech = (_techTarget == TechTarget::Ultralisks) ||
			(_techTarget == TechTarget::Guardians) ||
			(_techTarget == TechTarget::Defilers) ||
			(minerals > 2500);
		bool rich = (minerals > 700) && (gas > 700) && (nBases >= 3) && (nDrones > 15);

		switch (type.getID()) {
		case (UnitTypes::Enum::Zerg_Extractor) :
			return nDrones > 5 * nBases + 3 * nGas + 4 &&
				(
				(minerals + 100) >= 2 * (gas + 100) ||
				(_gasUnit != Zerg_Hydralisk &&
				(minerals + 100) >= (gas + 100))
				);

		case (UnitTypes::Enum::Zerg_Spawning_Pool) :
			return !has(Zerg_Spawning_Pool);

		case (UnitTypes::Enum::Zerg_Evolution_Chamber) :
			return !_emergencyGroundDefense && ((hasDen || hasUltra || hasASpire) && nEvo < 2 && (nEvo == 0 || nGas > 3)) &&
				!isBeingBuilt(Zerg_Evolution_Chamber);

		case (UnitTypes::Enum::Zerg_Hydralisk_Den) :
			return !has(Zerg_Hydralisk_Den) && (_techTarget == TechTarget::Hydralisks || _techTarget == TechTarget::Lurkers);

		case (UnitTypes::Enum::Zerg_Lair) :
			return (techLevel == 0) && 
			(nDrones >= 12 || minerals + gas > 800 || (_enemyRace == BWAPI::Races::Zerg && nDrones >= 10)) &&
				!has(Zerg_Lair);

		case (UnitTypes::Enum::Zerg_Spire) :
			return _techTarget == TechTarget::Mutalisks && hasLairTech &&
				((nDrones >= 13 && nGas + nFreeGas >= 2) || (_enemyRace == BWAPI::Races::Zerg && nDrones >= 10)) &&
				!has(Zerg_Spire);

		case (UnitTypes::Enum::Zerg_Queens_Nest) :
			return wantHiveTech && hasLairTech && !_emergencyGroundDefense &&
				(nDrones >= 28 || (_enemyRace != BWAPI::Races::Zerg && nDrones >= 24)) &&
				(nHydras + nLurkers >= 12 || nMutas >= 6 || rich) &&
				!has(Zerg_Queens_Nest);

		case (UnitTypes::Enum::Zerg_Hive) :
			return wantHiveTech && !has(Zerg_Hive) && hasQueensNest && nDrones >= 24 && (nGas + nFreeGas >= 3);

		case (UnitTypes::Enum::Zerg_Ultralisk_Cavern) :
			return _techTarget == TechTarget::Ultralisks && !has(Zerg_Ultralisk_Cavern) && nGas >= 4;

		case (UnitTypes::Enum::Zerg_Defiler_Mound) :
			return _techTarget == TechTarget::Defilers && nDrones >= 24 && !has(Zerg_Defiler_Mound);

		case (UnitTypes::Enum::Zerg_Greater_Spire) :
		{
			int flyerArmorUps = _self->getUpgradeLevel(Zerg_Flyer_Carapace) + upping(Zerg_Flyer_Carapace);
			int flyerAttackUps = _self->getUpgradeLevel(Zerg_Flyer_Attacks) + upping(Zerg_Flyer_Attacks);
			return (flyerAttackUps + flyerArmorUps >= 2) && _techTarget == TechTarget::Guardians;
		}

		case (UnitTypes::Enum::Zerg_Nydus_Canal) :
			return false;

		default:
			UAB_ASSERT(false, "Unsupported type %s in function shouldGet in StrategyBossZerg.cpp", type.c_str());
			return false;
		}
	}

	UAB_ASSERT(false, "Unsupported type %s in function shouldGet in StrategyBossZerg.cpp", t.getName());
	return false;
}

//Indicates whether we can upgrade the given type. 
bool StrategyBossZerg::canGet(const MacroAct & t) const {
	if (t.isUpgrade()) {
		auto type = t.getUpgradeType();
		int level = _self->getUpgradeLevel(type);
		auto req = type.whatsRequired();
		bool hasRequired = true;
		if (req != UnitTypes::None) {
			hasRequired = has(req, true) > 0;
			if (req == Zerg_Lair) hasRequired |= has(Zerg_Hive) > 0;
		}

		/*if (level < type.maxRepeats() && !upping(type) && !busy(type.whatUpgrades())) {
			Broodwar->printf("%s needed for %s", req.c_str(), type.c_str());
		}*/

		return hasRequired && level < type.maxRepeats() && !upping(type) && !busy(type.whatUpgrades());
	}

	if (t.isTech()) {
		auto type = t.getTechType();
		auto req = type.requiredUnit();
		bool hasRequired = true;
		if (req != UnitTypes::None) {
			hasRequired = has(req, true) > 0;
			if (req == Zerg_Lair) hasRequired |= has(Zerg_Hive) > 0;
		}

		/*if (!_self->hasResearched(type) && !upping(type) && !busy(type.whatResearches())) {
			Broodwar->printf("%s needed for %s", req.c_str(), type.c_str());
		}*/

		return hasRequired && !_self->hasResearched(type) && !upping(type) && !busy(type.whatResearches());
	}

	if (t.isBuilding()) {
		auto type = t.getUnitType();
		auto reqs = type.requiredUnits();
		bool hasRequired = true;
		if (!reqs.empty()) {
			for (auto req : reqs) hasRequired &= has(req.first, true) >= req.second;
		}

		return hasRequired && !busy(type.whatBuilds().first);
	}

	UAB_ASSERT(false, "Unsupported type in function canGet in StrategyBossZerg.cpp");
	return false; 
}

//Indicates whether all completed type-instances are busy upgrading something; also true if there are no instances.
bool StrategyBossZerg::busy(const UnitType & t, bool any) const {
	int count = UnitUtil::GetCompletedUnitCount(t);
	if (any) { //notable special cases that will be worthwhile past BWAPI 4.20
		UAB_ASSERT(false, "any boolean is not yet supported by busy function in StrategyBossZerg.cpp");
		//if (t == Zerg_Spire || t == Zerg_Greater_Spire) {
		//	count = UnitUtil::GetCompletedUnitCount(Zerg_Spire) + UnitUtil::GetCompletedUnitCount(Zerg_Greater_Spire);
		//}
		//else if (t == Zerg_Hatchery) {
		//	count = UnitUtil::GetCompletedUnitCount(Zerg_Hatchery) + UnitUtil::GetCompletedUnitCount(Zerg_Lair) + UnitUtil::GetCompletedUnitCount(Zerg_Hive);
		//} else if (t == Zerg_Lair) {
		//  count = UnitUtil::GetCompletedUnitCount(Zerg_Lair) + UnitUtil::GetCompletedUnitCount(Zerg_Hive);
		//}
	} 

	switch (t.getID()) {
	case (UnitTypes::Enum::Zerg_Hatchery) :
		return count <= int(upping(Burrowing));
	case (UnitTypes::Enum::Zerg_Lair) :
		return count <= upping(Pneumatized_Carapace) + upping(Ventral_Sacs);
	case (UnitTypes::Enum::Zerg_Spawning_Pool) :
		return count <= upping(Metabolic_Boost) + upping(Adrenal_Glands);
	case (UnitTypes::Enum::Zerg_Evolution_Chamber) :
		return count <= upping(Zerg_Missile_Attacks) + upping(Zerg_Melee_Attacks) + upping(Zerg_Carapace);
	case (UnitTypes::Enum::Zerg_Hydralisk_Den) :
		return count <= upping(Lurker_Aspect) + upping(Muscular_Augments) + upping(Grooved_Spines);
	case (UnitTypes::Enum::Zerg_Spire) :
		return count <= upping(Zerg_Flyer_Attacks) + upping(Zerg_Flyer_Carapace);
	case (UnitTypes::Enum::Zerg_Queens_Nest) :
		return count <= upping(Spawn_Broodlings) + upping(Ensnare) + upping(Gamete_Meiosis);
	case (UnitTypes::Enum::Zerg_Ultralisk_Cavern) :
		return count <= upping(Anabolic_Synthesis) + upping(Chitinous_Plating);
	case (UnitTypes::Enum::Zerg_Defiler_Mound) :
		return count <= upping(Plague) + upping(Consume);
	default:
		UAB_ASSERT(false, "unknown type %s passed to busy function in StrategyBossZerg.cpp", t.c_str());
		return true;
	} //end switch/case
}