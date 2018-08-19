#include "Common.h"
#include "WorkerManager.h"
#include "Micro.h"
#include "ProductionManager.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

WorkerManager::WorkerManager()
	: previousClosestWorker(nullptr)
	, _collectGas(true)
	, _lastFrameWorkerGasTransfer(-1000)
	, _lastFrameGasChanged(-1000)
{
}

WorkerManager & WorkerManager::Instance() 
{
	static WorkerManager instance;
	return instance;
}

void WorkerManager::handleWorkers() {
	handleGasWorkers();
	updateWorkerStatus();

	for (auto & worker : workerData.getWorkers())
	{
		UAB_ASSERT(worker, "Worker was null");

		//scarab/storm/irradiate splitting for drones
		if (workerData.getWorkerJob(worker) == WorkerData::Combat) continue; 

		if (Micro::CheckSpecialCases(worker)) continue;
		//if (Micro::AvoidWorkerHazards(worker)) continue; 
		//had questionable value. needs more debugging. 

		bool isCarrying = worker->isCarryingMinerals() || worker->isCarryingGas();

		if (worker->isCarryingMinerals()) { 
			//reset its mineral assignment to allow other workers to claim it,
			//and so that it has to decide again once it's finished returning
			workerData.eraseWorkerMineralAssignment(BWAPI::Unit);
		}

		if (isCarrying)
		{
			//it's already returning cargo
			auto order = worker->getOrder();

			if (order == BWAPI::Orders::ReturnGas || order == BWAPI::Orders::ReturnMinerals) 
				continue;

			bool depotsExist = !workerData.getDepots().empty();
			if (depotsExist) {
				Micro::SmartReturnCargo(worker);
				continue;
			}
		}

		auto job = workerData.getWorkerJob(worker);

		//harvest empty minerals that are close enough
		//assume the mineral is not blocking it if we're on one base -- this should work for current maps
		if (InformationManager::Instance().getNumBases(BWAPI::Broodwar->self()) > 1 && 
				(/*job == WorkerData::Minerals ||*/ job == WorkerData::Move || job == WorkerData::Build)) {
			if (worker->isGatheringMinerals() && worker->getTarget()) {
				if (job == WorkerData::Minerals || worker->getTarget()->getResources() < 10) continue;
			}
			auto minerals = worker->getUnitsInRadius(5 * 32, BWAPI::Filter::IsMineralField && (BWAPI::Filter::Resources < 10) &&
				!BWAPI::Filter::IsBeingGathered);
			bool found = false;

			for (auto min : minerals) {
				if (workerData.getNumAssignedWorkers(min) > 0) continue;
				Micro::SmartRightClick(worker, min);
				found = true;
				break;
			}
			if (found) continue;
		}

		switch (job) {
			//Minerals, Gas, Build, Combat, Idle, Repair, Move, Scout, Default
		case (WorkerData::Minerals) :
		case (WorkerData::Gas) :
		//lock the worker to the mineral. also, attack any nearby threats.
		{
			if (executeSelfDefense(worker)) break;

			auto target = workerData.getWorkerResource(worker);
			if (worker->getTarget() && worker->getTarget() == target) break;
			Micro::SmartRightClick(worker, target);
			break;
		}
		case (WorkerData::Build) :
			BWAPI::Broodwar->drawTextMap(worker->getPosition(), "%c", workerData.getJobCode(worker));
			//still handled by build manager; should centralize later
			break;
		case (WorkerData::Combat) : //handled by melee manager
			break;
		case (WorkerData::Idle) :
			BWAPI::Broodwar->drawTextMap(worker->getPosition(), "Idle");
			putWorkerToWork(worker);
			break;
		case (WorkerData::Repair) :
			//this section of code is not tested, since the bot is primarily zerg 
			//but should work for terran - Arrak
		{
			if (executeSelfDefense(worker)) break;
			auto target = workerData.getWorkerRepairUnit(worker);

			if (!target || !target->exists() || (worker->getTarget() && worker->getTarget() == target)) break;
			Micro::SmartRepair(worker, target);
			break;
		}

		case (WorkerData::Move) :
			BWAPI::Broodwar->drawTextMap(worker->getPosition(), "%c", workerData.getJobCode(worker));
			Micro::SmartTravel(worker, workerData.getWorkerMoveData(worker).position);
			break;
		//case (WorkerData::Scout) :
			//still handled by scout manager
		//	break;
		case (WorkerData::Default) : //incomplete workers
			break;
		default:
			break;

		}
	}
}

void WorkerManager::update() 
{
	handleWorkers();

	drawResourceDebugInfo();
	drawWorkerInformation(450,20);

	workerData.drawDepotDebugInfo();
}

// Adjust worker jobs. This is done first, before handling each job.
// NOTE A mineral worker may go briefly idle after collecting minerals.
// That's OK; we don't change its status then.
void WorkerManager::updateWorkerStatus() 
{
	// If any buildings are due for construction, assume that builders are not idle.
	/*const bool catchIdleBuilders =
		!BuildingManager::Instance().anythingBeingBuilt() &&
		!ProductionManager::Instance().nextIsBuilding();*/ //still insufficient...

	// for each of our Workers
	for (auto & worker : workerData.getWorkers())
	{
		//if (!worker->isCompleted()) continue;     // the worker list includes drone eggs

		// Idleness.
		// Order can be PlayerGuard for a drone that tries to build and fails.
		// There are other causes.
		auto job = workerData.getWorkerJob(worker);
		//bool idleBuilder = catchIdleBuilders && (job == WorkerData::Build || job == WorkerData::Move);
		if ((worker->isIdle() || worker->getOrder() == BWAPI::Orders::PlayerGuard) &&
			/*(idleBuilder ||
			(job != WorkerData::Build &&
			job != WorkerData::Move &&*/
			job != WorkerData::Minerals &&
			job != WorkerData::Combat &&
			job != WorkerData::Scout)//))
		{
			workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
		}
		
		else if (workerData.getWorkerJob(worker) == WorkerData::Gas)
		{
			BWAPI::Unit refinery = workerData.getWorkerResource(worker);

			// If the refinery is gone. A missing resource depot is dealt with in handleGasWorkers().
			if (!refinery || !refinery->exists() || refinery->getHitPoints() <= 0)
			{
				workerData.removeRefinery(refinery);
				workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
			}
			else
			{
				if (refinery->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser) {
					UAB_ASSERT(false, "Untaken geyser found in refinery list.");
				}

				auto resource = workerData.getWorkerResource(worker);
				bool invalidResource = !resource || !resource->exists();

				if (invalidResource)
				{
					workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
				}
			}
		}
		
		else if (workerData.getWorkerJob(worker) == WorkerData::Minerals)
		{
			auto resource = workerData.getWorkerResource(worker);
			bool invalidResource = !resource || !resource->exists();

			if (invalidResource)
			{
				workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
			}
			else { //invalid depot check
				auto depot = workerData.getWorkerDepot(worker);
				if (!depot || !depot->exists()) {
					workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
				}
			}
		}
	}
}

void WorkerManager::handleGasWorkers() 
{
	std::vector<BWAPI::Unit> badRefineries;
	for (auto unit : workerData.getRefineries()) {
		if (!unit || !unit->exists() || unit->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser || unit->getHitPoints() <= 0) {
			badRefineries.push_back(unit);
		}
	}

	if (!badRefineries.empty()) {
		UAB_ASSERT(false, "Removing bad refineries...");

		for (auto unit : badRefineries) {
			if (!unit || !unit->exists()) {
				UAB_ASSERT(false, "Bad NULL-refinery.");
			}
			else if (unit->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser) {
				UAB_ASSERT(false, "Bad geyser-refinery.");
			}
			else if (unit->getHitPoints() < 0) {
				UAB_ASSERT(false, "Bad Dead-refinery.");
			}
			workerData.removeRefinery(unit);
		}
	}


	if (!_collectGas) { //remove all working gas workers off gas
		std::set<BWAPI::Unit> gasWorkers;
		workerData.getGasWorkers(gasWorkers);
		for (auto worker : gasWorkers)
		{
			if (worker->getOrder() == BWAPI::Orders::HarvestGas) continue; //inside the refinery

			auto depot = workerData.getWorkerDepot(worker);
			if (!depot || !depot->exists()) { //depot is invalid!
				if (workerData.getWorkerJob(worker) != WorkerData::Gas) {
					BWAPI::Broodwar->sendText("DEBUG: No-depot gas worker not actually on gas: %d", workerData.getJobCode(worker));
				}
				else {
					BWAPI::Broodwar->sendText("DEBUG: No-depot gas worker");
				}
			}
			else if (workerData.depotIsFull(depot)) { //the nearby depot is full - allow the worker to keep working
				BWAPI::Broodwar->drawCircleMap(worker->getPosition(), 32, BWAPI::Colors::Red);
				continue;
			}

			workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
		}

		return;
	}

	//otherwise we want to collect gas
	for (auto unit : workerData.getRefineries())
	{
		if (!unit || !unit->exists()) {
			UAB_ASSERT(false, "Invalid refinery detected.");
			continue;
		}

		BWAPI::Broodwar->drawTextMap(unit->getPosition(), "%d: %d", workerData.getRefineries().count(unit), workerData.getNumAssignedWorkers(unit));


		// Don't collect gas if gas collection is off, or if the resource depot is missing.
		int maxAssignedWorkers = Config::Macro::WorkersPerRefinery;
		if (!refineryHasDepot(unit)) { // If any workers are assigned, take them off.
			maxAssignedWorkers = 0;
		}

		// Assign or de-assign workers from harvesting
		int numAssigned = workerData.getNumAssignedWorkers(unit);
		if (numAssigned > maxAssignedWorkers) {
			if (maxAssignedWorkers > Config::Macro::WorkersPerRefinery) {
				UAB_ASSERT(false, "Error: Too many workers assigned to refinery!");
			}

			std::set<BWAPI::Unit> gasWorkers;
			workerData.getGasWorkers(gasWorkers);
			int n = numAssigned - 3;
			for (auto gasWorker : gasWorkers)
			{
				if (workerData.getWorkerResource(gasWorker) != unit) continue;

				if (gasWorker->getOrder() != BWAPI::Orders::HarvestGas) // not inside the refinery
				{
					workerData.setWorkerJob(gasWorker, WorkerData::Idle, nullptr);
					n--;
				}

				if (n <= maxAssignedWorkers) break;
			}
		}
		else {
			if (numAssigned < maxAssignedWorkers) //assign at most 1 worker per frame, per refinery
			{
				if (numAssigned > 0 && !unit->isBeingGathered()) continue; 
				//avoid colliding with other workers

				BWAPI::Unit gasWorker = getGasWorker(unit);
				if (gasWorker)
				{
					workerData.setWorkerJob(gasWorker, WorkerData::Gas, unit);
				}
				//we only assign if there's a near enough worker for the gas;
				//long distance gas worker transfers are allowed every once in a while
			}
		}
	}
}

// Is our refinery near a resource depot that it can deliver gas to?
bool WorkerManager::refineryHasDepot(BWAPI::Unit refinery)
{
	auto depots = refinery->getUnitsInRadius(600,
		(BWAPI::Filter::IsResourceDepot && BWAPI::Filter::IsCompleted) ||
		BWAPI::Filter::GetType == BWAPI::UnitTypes::Zerg_Lair ||
		BWAPI::Filter::GetType == BWAPI::UnitTypes::Zerg_Hive);

	if (!depots.empty()) return true;

	return false;
}

// Used for worker self-defense.
// Only count enemy units that can be targeted by workers.
BWAPI::Unit WorkerManager::getClosestEnemyUnit(BWAPI::Unit worker)
{
    UAB_ASSERT(worker, "Worker was null");

	BWAPI::Unit closestUnit = nullptr;
	int closestDist = 1000;         // ignore anything farther away

	for (auto & unit : BWAPI::Broodwar->enemy()->getUnits())
	{
		if (unit->isVisible() && unit->isDetected() && !unit->isFlying())
		{
			int dist = unit->getDistance(worker);

			if (dist < closestDist)
			{
				closestUnit = unit;
				closestDist = dist;
			}
		}
	}

	return closestUnit;
}

void WorkerManager::finishedWithCombatWorkers()
{
	for (auto & worker : workerData.getWorkers())
	{
        UAB_ASSERT(worker, "Worker was null");

		if (workerData.getWorkerJob(worker) == WorkerData::Combat)
		{
			workerData.setWorkerJob(worker, WorkerData::Idle, nullptr);
		}
	}
}

BWAPI::Unit WorkerManager::getClosestMineralWorkerTo(BWAPI::Unit enemyUnit)
{
    UAB_ASSERT(enemyUnit, "enemyUnit was null");

    BWAPI::Unit closestMineralWorker = nullptr;
    double closestDist = 100000;

	// Former closest worker may have died or (if zerg) morphed into a building.
	if (UnitUtil::IsValidUnit(previousClosestWorker) && previousClosestWorker->getType().isWorker())
	{
		return previousClosestWorker;
    }

	for (auto & worker : workerData.getWorkers())
	{
        UAB_ASSERT(worker, "Worker was null");

        if (workerData.getWorkerJob(worker) == WorkerData::Minerals) 
		{
			double dist = worker->getDistance(enemyUnit);
            if (dist < closestDist)
            {
                closestMineralWorker = worker;
                dist = closestDist;
            }
		}
	}

    previousClosestWorker = closestMineralWorker;
    return closestMineralWorker;
}

BWAPI::Unit WorkerManager::getWorkerScout()
{
	for (auto & worker : workerData.getWorkers())
	{
        UAB_ASSERT(worker, "Worker was null");
        if (workerData.getWorkerJob(worker) == WorkerData::Scout) 
		{
			return worker;
		}
	}

    return nullptr;
}

// Send the worker to mine minerals at the closest resource depot, if any.
void WorkerManager::putWorkerToWork(BWAPI::Unit unit)
{
    UAB_ASSERT(unit, "Unit was null");

	BWAPI::Unit depot = getClosestDepot(unit);

	if (depot && (unit->getDistance(depot) < 12*32 || _collectGas)) //if we're collecting gas then handleGasWorkers handles all assignments
	{
		workerData.setWorkerJob(unit, WorkerData::Minerals, depot);
	}
	else
	{
		//Our options are pretty far; consider going to a refinery instead, if the refinery is substantially closer
		BWAPI::Unit refinery = getClosestRefinery(unit);

		if (!refinery) {
			if (depot) workerData.setWorkerJob(unit, WorkerData::Minerals, depot);
		}
		else if (!depot) {
			if (refinery) {
				workerData.setWorkerJob(unit, WorkerData::Gas, refinery);
			}
		}
		else if (refinery->getDistance(unit) + 8*32 < depot->getDistance(unit)) {
			workerData.setWorkerJob(unit, WorkerData::Gas, refinery);
		}
		else {
			workerData.setWorkerJob(unit, WorkerData::Minerals, depot);
		}
	}
}

BWAPI::Unit WorkerManager::getClosestDepot(BWAPI::Unit worker, bool anyDepotIsFine)
{
	UAB_ASSERT(worker, "Worker was null");

	BWAPI::Unit closestDepot = nullptr;
	double closestDistance = 0;

	for (auto & unit : workerData.getDepots())
	{
        UAB_ASSERT(unit, "Unit was null");

		if (/*unit->getType().isResourceDepot() &&
			(unit->isCompleted() || unit->getType() == BWAPI::UnitTypes::Zerg_Lair || unit->getType() == BWAPI::UnitTypes::Zerg_Hive) &&*/
			(!workerData.depotIsFull(unit) || anyDepotIsFine))
		{
			double distance = unit->getDistance(worker);
			if (!closestDepot || distance < closestDistance)
			{
				closestDepot = unit;
				closestDistance = distance;
			}
		}
	}

	return closestDepot;
}

BWAPI::Unit WorkerManager::getClosestRefinery(BWAPI::Unit worker)
{
	UAB_ASSERT(worker, "Worker was null in WorkerManager::getClosestRefinery");

	BWAPI::Unit closestRefinery = nullptr;
	double closestDistance = 0;

	for (auto & unit : workerData.getRefineries())
	{
		UAB_ASSERT(unit, "Refinery was null in WorkerManager::getClosestRefinery");

		if (workerData.getNumAssignedWorkers(unit) >= Config::Macro::WorkersPerRefinery) continue;

		if (!refineryHasDepot(unit)) continue;

		double distance = unit->getDistance(worker);
		if (!closestRefinery || distance < closestDistance)
		{
			closestRefinery = unit;
			closestDistance = distance;
		}
	}

	return closestRefinery;
}


/* Get best depot for new worker by saturation and larva count. */
BWAPI::Unit WorkerManager::getBestDepot() {
	BWAPI::Unit depot = NULL;

	int bestLarvaCount = 0;
	bool minDepot = false;
	
	for (auto & unit : workerData.getDepots())
	{
		UAB_ASSERT(unit, "Unit was null");
		auto type = unit->getType();

		//we only want completed depots
		if (!(type.isResourceDepot() &&
			(unit->isCompleted() || type == BWAPI::UnitTypes::Zerg_Lair || type == BWAPI::UnitTypes::Zerg_Hive))) continue;

		//if we're zerg, also factor in larva, prioritize high-larva hatcheries
		if (type.getRace() == BWAPI::Races::Zerg) { //hatcheries
			int larvaCount = unit->getLarva().size();
			if (larvaCount == 0) continue; //no larva here!

			if (!workerData.depotIsFull(unit, minWorkersPerPatch)) {
				if (larvaCount > 2) return unit;

				if (!minDepot || larvaCount > bestLarvaCount) {
					depot = unit;
					bestLarvaCount = larvaCount;
					minDepot = true;
				}
			}
			else if (!minDepot && !workerData.depotIsFull(unit, midWorkersPerPatch)) {
				if (larvaCount > bestLarvaCount) {
					depot = unit;
					bestLarvaCount = larvaCount;
				}
			} 
		} //for protoss and terran
		else if (!unit->isTraining()) { //nexuses and command centers
			if (!workerData.depotIsFull(unit, minWorkersPerPatch)) {
				return unit;
			}
			else if (!depot && !workerData.depotIsFull(unit, midWorkersPerPatch)) {
				depot = unit;
			}
		} 
	}

	return depot;
}

// other managers that need workers call this when they're done with a unit
void WorkerManager::finishedWithWorker(BWAPI::Unit unit) 
{
	UAB_ASSERT(unit, "Unit was null");
	workerData.setWorkerJob(unit, WorkerData::Idle, nullptr);
}

// Find a worker to be reassigned to gas duty.
BWAPI::Unit WorkerManager::getGasWorker(BWAPI::Unit refinery)
{
	UAB_ASSERT(refinery != nullptr, "Refinery was null");

	BWAPI::Unit closestWorker = nullptr;
	double closestDistance = 0;

	auto currentFrame = BWAPI::Broodwar->getFrameCount();

	for (auto & unit : workerData.getWorkers())
	{
		if (!isFree(unit)) continue;

		//rate-limits worker transfers to far gas
		double distance = unit->getDistance(refinery);
		if (distance > 600 && currentFrame - _lastFrameWorkerGasTransfer < minTimeBetweenGasTransfer) {
			continue;
		}

		if (!closestWorker || distance < closestDistance)
		{
			closestWorker = unit;
			closestDistance = distance;
		}
	}

	if (closestWorker && closestWorker->exists() && closestWorker->getDistance(refinery) > 600) {
		_lastFrameWorkerGasTransfer = currentFrame;
	}

	return closestWorker;
}

void WorkerManager::setBuildingWorker(BWAPI::Unit worker, Building & b)
{
     UAB_ASSERT(worker, "Worker was null");

     workerData.setWorkerJob(worker, WorkerData::Build, b.type);
}

// Get a builder for BuildingManager.
// if setJobAsBuilder is true (default), it will be flagged as a builder unit
// set 'setJobAsBuilder' to false if we just want to see which worker will build a building
BWAPI::Unit WorkerManager::getBuilder(const Building & b, bool setJobAsBuilder)
{
	// variables to hold the closest worker of each type to the building
	BWAPI::Unit closestMovingWorker = nullptr;
	BWAPI::Unit closestMiningWorker = nullptr;
	double closestMovingWorkerDistance = 0;
	double closestMiningWorkerDistance = 0;

	// look through each worker that had moved there first
	for (auto & unit : workerData.getWorkers())
	{
        UAB_ASSERT(unit, "Unit was null");

        // gas steal building uses scout worker
        if (b.isGasSteal && (workerData.getWorkerJob(unit) == WorkerData::Scout))
        {
            if (setJobAsBuilder)
            {
                workerData.setWorkerJob(unit, WorkerData::Build, b.type);
            }
            return unit;
        }

		if (!unit->isCompleted()) continue;

		// mining/idle worker check
		if (isFree(unit))
		{
			// if it is a new closest distance, set the pointer
			double distance = unit->getDistance(BWAPI::Position(b.finalPosition));
			if (!closestMiningWorker || distance < closestMiningWorkerDistance)
			{
				closestMiningWorker = unit;
				closestMiningWorkerDistance = distance;
			}
		}

		// moving worker check
		if (workerData.getWorkerJob(unit) == WorkerData::Move)
		{
			// if it is a new closest distance, set the pointer
			double distance = unit->getDistance(BWAPI::Position(b.finalPosition));
			if (!closestMovingWorker || distance < closestMovingWorkerDistance)
			{
				closestMovingWorker = unit;
				closestMovingWorkerDistance = distance;
			}
		}
	}

	// prefer closeby moving worker over mining workers, unless moving worker is very far -- moving worker may be stuck
	BWAPI::Unit chosenWorker = (closestMovingWorker && closestMovingWorkerDistance < closestMiningWorkerDistance + 4*32) ? closestMovingWorker : closestMiningWorker;

	// if the worker exists (one may not have been found in rare cases)
	if (chosenWorker && setJobAsBuilder)
	{
		workerData.setWorkerJob(chosenWorker, WorkerData::Build, b.type);
	}

	return chosenWorker;
}

// sets a worker as a scout
void WorkerManager::setScoutWorker(BWAPI::Unit worker)
{
	UAB_ASSERT(worker, "Worker was null");
	workerData.setWorkerJob(worker, WorkerData::Scout, nullptr);
}

// gets a worker which will move to a current location
BWAPI::Unit WorkerManager::getMoveWorker(BWAPI::Position p)
{
	// set up the pointer
	BWAPI::Unit closestWorker = nullptr;
	double closestDistance = 0;

	// for each worker we currently have
	for (auto & unit : workerData.getWorkers())
	{
		// only consider it if it's a mineral worker
		if (isFree(unit))
		{
			// if it is a new closest distance, set the pointer
			double distance = unit->getDistance(p);
			if (!closestWorker || distance < closestDistance)
			{
				closestWorker = unit;
				closestDistance = distance;
			}
		}
	}

	// return the worker
	return closestWorker;
}

// sets a worker to move to a given location
void WorkerManager::setMoveWorker(int mineralsNeeded, int gasNeeded, BWAPI::Position p)
{
	BWAPI::Unit closestWorker = nullptr;
	double closestDistance = 0;

	for (auto & unit : workerData.getWorkers())
	{
		if (isFree(unit))
		{
			double distance = unit->getDistance(p);
			if (!closestWorker || distance < closestDistance)
			{
				closestWorker = unit;
				closestDistance = distance;
			}
		}
	}

	if (!closestWorker) return;
	workerData.setWorkerJob(closestWorker, WorkerData::Move, WorkerMoveData(mineralsNeeded, gasNeeded, p));
}

// will we have the required resources by the time a worker can travel a certain distance
bool WorkerManager::willHaveResources(int mineralsRequired, int gasRequired, double distance)
{
	// if we don't require anything, we will have it
	if (mineralsRequired <= 0 && gasRequired <= 0)
	{
		return true;
	}

	double speed = BWAPI::Broodwar->self()->getRace().getWorker().topSpeed();

	// how many frames it will take us to move to the building location
	// add a little to account for worker getting stuck. better early than late
	double framesToMove = (distance / speed) + 24 + std::min(0.0, 0.1*(distance - 100)/speed);

	// magic numbers to predict income rates
	double mineralRate = getNumMineralWorkers() * 0.045;
	double gasRate = getNumGasWorkers() * 0.07;

	// calculate if we will have enough by the time the worker gets there
	return
		mineralRate * framesToMove >= mineralsRequired &&
		gasRate * framesToMove >= gasRequired;
}

void WorkerManager::setCombatWorker(BWAPI::Unit worker)
{
	UAB_ASSERT(worker, "Worker was null");
	workerData.setWorkerJob(worker, WorkerData::Combat, nullptr);
}

bool WorkerManager::shouldPullWorker(BWAPI::Unit worker) {
	UAB_ASSERT(worker, "Worker was null");
	auto job = workerData.getWorkerJob(worker);
	if ((job == WorkerData::Minerals || job == WorkerData::Idle) &&
		worker->getHitPoints() > 20 && (!worker->isCarryingGas() && !worker->isCarryingMinerals())) return true;

	return false;
}

void WorkerManager::onUnitMorph(BWAPI::Unit unit)
{
	UAB_ASSERT(unit, "Unit was null");

	if ((!unit->exists() || unit->getType().isBuilding()) && unit->getPlayer() == BWAPI::Broodwar->self() && unit->getPlayer()->getRace() == BWAPI::Races::Zerg)
	{
		workerData.workerDestroyed(unit);
	}

	if (unit->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser) {
		//UAB_ASSERT(false, "Morphed geyser detected");
		//looks like this can happen when an extractor is killed?
		workerData.removeRefinery(unit);
	}

}

void WorkerManager::onUnitShow(BWAPI::Unit unit)
{
	UAB_ASSERT(unit && unit->exists(), "bad unit");

	/*if (unit->getPlayer() == BWAPI::Broodwar->self()) //this does not seem to work
		if (unit->getType().isResourceDepot()) { 
			workerData.addDepot(unit);
		}
		// if something morphs into a worker, add it
		else if (unit->getType().isWorker() && unit->getHitPoints() >= 0) {
			workerData.addWorker(unit);
		}
	}*/
}

void WorkerManager::onUnitComplete(BWAPI::Unit unit)
{
	UAB_ASSERT(unit && unit->exists(), "bad unit");

	if (unit->getPlayer() == BWAPI::Broodwar->self()) {
		if (unit->getType().isRefinery()) {
			workerData.addRefinery(unit);
		}
		else if (unit->getType().isResourceDepot() && 
			!(unit->getType() == BWAPI::UnitTypes::Zerg_Lair ||
			unit->getType() == BWAPI::UnitTypes::Zerg_Hive)) {
			workerData.addDepot(unit);
		}
		else if (unit->getType().isWorker() && unit->getPlayer() == BWAPI::Broodwar->self() && unit->getHitPoints() >= 0)
		{
			workerData.addWorker(unit);
		}
		else if (unit->getType() == BWAPI::UnitTypes::Zerg_Larva) {
			//performs the larva trick
			auto hatchery = unit->getHatchery();
			if (hatchery) {
				//on the first frame, larva can apparently be called before pMins is computed. 
				//add the hatchery so it computes it first
				if (BWAPI::Broodwar->getFrameCount() == 0 && workerData.getDepots().empty()) {
					workerData.addDepot(hatchery);
				}

				BWAPI::Position pMins = workerData.getAverageMineralPosition(hatchery);
				//approximate position larva will end up at
				BWAPI::Position pLeft = BWAPI::Position(hatchery->getTilePosition()) + BWAPI::Position(-32, 32);

				//if the left position is closer to the average mineral position than larva's birth position
				if (pLeft.getDistance(pMins) < unit->getPosition().getDistance(pMins)) {
					Micro::SmartStop(unit);
				}
			}
		}
	}

	if (unit->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser) {
		if (BWAPI::Broodwar->getFrameCount() <= 10) return;

		if (workerData.getRefineries().contains(unit)) {
			UAB_ASSERT(false, "Geyser completed that is in refineries");
		}
		else {
			//UAB_ASSERT(false, "Geyser completed; Not in refineries");
		}
	}
}

void WorkerManager::onUnitDestroy(BWAPI::Unit unit) 
{
	UAB_ASSERT(unit, "Unit was null");

	if (unit->getPlayer() == BWAPI::Broodwar->self()) {
		if (unit->getType().isResourceDepot()) {
			workerData.removeDepot(unit);
		}
		else if (unit->getType().isWorker()) {
			workerData.workerDestroyed(unit);
		}
		else if (unit->getType().isRefinery()) { //this may have not been enough
			//BWAPI::Broodwar->sendText("Dead extractor detected");
			workerData.removeRefinery(unit);
		}
	}
}

void WorkerManager::drawResourceDebugInfo() 
{
    if (!Config::Debug::DrawResourceInfo) return;

	for (auto & worker : workerData.getWorkers()) 
    {
        UAB_ASSERT(worker, "Worker was null");

		char job = workerData.getJobCode(worker);

		BWAPI::Position pos = worker->getTargetPosition();

		BWAPI::Broodwar->drawTextMap(worker->getPosition().x, worker->getPosition().y - 5, "\x07%c", job);
		BWAPI::Broodwar->drawTextMap(worker->getPosition().x, worker->getPosition().y + 5, "\x03%s", worker->getOrder().getName().c_str());

		BWAPI::Broodwar->drawLineMap(worker->getPosition().x, worker->getPosition().y, pos.x, pos.y, BWAPI::Colors::Cyan);

		BWAPI::Unit depot = workerData.getWorkerDepot(worker);
		if (depot)
		{
			int assignedWorkers = workerData.getNumAssignedWorkers(depot);
			int mineralsNearDepot = workerData.getMineralsNearDepot(depot);

			BWAPI::Broodwar->drawLineMap(worker->getPosition().x, worker->getPosition().y, depot->getPosition().x, depot->getPosition().y, BWAPI::Colors::Orange);
		}
	}
	
	for (auto & depot : workerData.getDepots()) {
		BWAPI::Broodwar->drawCircleMap(workerData.getAverageMineralPosition(depot), 32, BWAPI::Colors::Blue);
		BWAPI::Broodwar->drawLineMap(workerData.getAverageMineralPosition(depot), depot->getPosition(), BWAPI::Colors::Blue);
	}
}

void WorkerManager::drawWorkerInformation(int x, int y) 
{
    if (!Config::Debug::DrawWorkerInfo) return;

	BWAPI::Broodwar->drawTextScreen(x, y, "\x04 Workers %d", workerData.getNumMineralWorkers());
	BWAPI::Broodwar->drawTextScreen(x, y+20, "\x04 UnitID");
	BWAPI::Broodwar->drawTextScreen(x+50, y+20, "\x04 State");

	int yspace = 0;

	for (auto & unit : workerData.getWorkers())
	{
        UAB_ASSERT(unit, "Worker was null");

		BWAPI::Broodwar->drawTextScreen(x, y+40+((yspace)*10), "\x03 %d", unit->getID());
		BWAPI::Broodwar->drawTextScreen(x+50, y+40+((yspace++)*10), "\x03 %c", workerData.getJobCode(unit));
	}
}

bool WorkerManager::isFree(BWAPI::Unit worker)
{
    UAB_ASSERT(worker, "Worker was null");
	return (workerData.getWorkerJob(worker) == WorkerData::Minerals || workerData.getWorkerJob(worker) == WorkerData::Idle) && unit->isCompleted();
}

bool WorkerManager::isWorkerScout(BWAPI::Unit worker)
{
	UAB_ASSERT(worker, "Worker was null");
	return (workerData.getWorkerJob(worker) == WorkerData::Scout);
}

bool WorkerManager::isBuilder(BWAPI::Unit worker)
{
	UAB_ASSERT(worker, "Worker was null");
	return (workerData.getWorkerJob(worker) == WorkerData::Build);
}

bool WorkerManager::isCombatWorker(BWAPI::Unit worker) {
	UAB_ASSERT(worker, "Worker was null");
	return (workerData.getWorkerJob(worker) == WorkerData::Combat);
}

int WorkerManager::getNumMineralWorkers() const
{
	return workerData.getNumMineralWorkers();
}

int WorkerManager::getNumGasWorkers() const
{
	return workerData.getNumGasWorkers();
}

int WorkerManager::getNumCombatWorkers() const
{
	return workerData.getNumCombatWorkers();
}

int WorkerManager::getNumIdleWorkers() const
{
	return workerData.getNumIdleWorkers();
}

// The largest number of workers that it is efficient to have right now.
// NOTE Does not take into account possible preparations for future expansions.
int WorkerManager::getMaxWorkers() const
{
	int patches = InformationManager::Instance().getMyNumMineralPatches();
	int refineries, geysers;
	InformationManager::Instance().getMyGasCounts(refineries, geysers);

	// Never let the max number of workers fall to 0!
	// Set aside 1 for future opportunities.
	return 1 + int(Config::Macro::WorkersPerPatch * patches + Config::Macro::WorkersPerRefinery * refineries + 0.5);
}

bool WorkerManager::executeSelfDefense(BWAPI::Unit worker) {
	//self defense

	UAB_ASSERT(worker, "Worker was null");

	BWAPI::Unit target = nullptr;
	int closestDist = 1000;         // ignore anything farther away

	BWAPI::Unit min1 = nullptr;
	BWAPI::Unit min2 = nullptr;

	int nEnemies = 0;

	auto units = worker->getUnitsInRadius(4 * 32, !BWAPI::Filter::IsFlying && BWAPI::Filter::IsDetected 
		&& !BWAPI::Filter::IsBuilding); //if we need drones to attack buildings, they should be combat workers instead


	for (auto & unit : units)
	{
		auto uType = unit->getType();
		if (unit->getPlayer() == BWAPI::Broodwar->enemy()) {
			int dist = unit->getDistance(worker);

			if (dist < closestDist)
			{
				target = unit;
				closestDist = dist;
			}

			if (unit->getType() == BWAPI::UnitTypes::Protoss_Zealot) {
				nEnemies += 2; //a zealot counts as 3
			}
			nEnemies++;
		}

		if (uType.isMineralField()) {
			if (!min1 || !min2) {
				min1 = unit;
				min2 = unit;
			}
			else {
				int d12 = min2->getDistance(min1);
				int du2 = min2->getDistance(unit);
				int du1 = min1->getDistance(unit);

				if (du1 > d12 || du2 > d12) {
					if (du1 > du2) {
						min2 = unit;
					}
					else { //if (du2 > du1) 
						min1 = unit;
					}
				}
			}
		}
	}

	if (target)
	{
		BWAPI::Broodwar->drawLineMap(worker->getPosition(), target->getPosition(), BWAPI::Colors::Red);

		auto allies = target->getUnitsInRadius(64, BWAPI::Filter::IsAlly);
		int nAllies = allies.size();

		if ((worker->getDistance(target) <= 96 && worker->getHitPoints() <= 15) ||
			(nEnemies > nAllies && worker->getGroundWeaponCooldown() > 2 && worker->getDistance(target) <= 64)) {
			//technically, we should assign the worker to a far enough away mineral field
			//and allow it to, say, "recover" until it exceeds 20 hitpoints
			//this simply makes it run to a field as long as the target is rather close
			//but won't necessarily ensure it will keep harvesting
			//note: not working. we never see the drone trying to retreat in certain situations. why?

			BWAPI::Unit farthestMineral = NULL;
			BWAPI::Unit assignedResource = workerData.getWorkerResource(worker);
			if (min1 && min2) {
				farthestMineral = min1->getDistance(target) > min2->getDistance(target) ? min1 : min2;
				if (assignedResource) {
					farthestMineral = (assignedResource->getDistance(target) > farthestMineral->getDistance(target)) ?
					assignedResource : farthestMineral;
				}
			}
			else {
				farthestMineral = assignedResource;
			}

			if (farthestMineral && farthestMineral->getDistance(target) > 32) {
				Micro::SmartRightClick(worker, farthestMineral);
			}
			else if (assignedResource && assignedResource->getDistance(target) > 32) {
				Micro::SmartRightClick(worker, assignedResource);
			}
			else {
				Micro::SmartMove(worker, worker->getPosition() + Micro::GetKiteVector(target, worker));
			}

			return true;
		}

		if (worker->getDistance(target) <= 64) {
			if (!target->isMoving() || target->isStuck() || worker->getDistance(target) <= 32) {
				Micro::SmartAttackUnit(worker, target);
				return true;
			}
		} 

		if (allies.empty() || nAllies > nEnemies || allies.contains(worker)) return false; //the enemy can be held off by current allies

		//else get closer to the target
		if (worker->getDistance(target) > 64 && worker->getDistance(target) < 96) {
			auto closestMineral = target->getClosestUnit(BWAPI::Filter::IsMineralField, 32);
			if (closestMineral && int(closestMineral->getUnitsInRadius(32, BWAPI::Filter::IsAlly).size()) < nEnemies) {
				Micro::SmartRightClick(worker, closestMineral);
				return true;
			}
		}
	}

	return false;
}