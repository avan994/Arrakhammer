#include "WorkerData.h"
#include "Micro.h"

using namespace UAlbertaBot;

WorkerData::WorkerData() 
{
    for (auto & unit : BWAPI::Broodwar->getAllUnits())
	{
		if ((unit->getType() == BWAPI::UnitTypes::Resource_Mineral_Field))
		{
            workersOnMineralPatch[unit] = 0;
		}
	}
}

void WorkerData::workerDestroyed(BWAPI::Unit unit)
{
	if (!unit) { return; }

	clearPreviousJob(unit);
	workers.erase(unit);
}

void WorkerData::addWorker(BWAPI::Unit unit)
{
	if (!unit || !unit->exists()) { return; }

	workers.insert(unit);
	workerJobMap[unit] = Default;
}

void WorkerData::addWorker(BWAPI::Unit unit, WorkerJob job, BWAPI::Unit jobUnit)
{
	if (!unit || !unit->exists() || !jobUnit || !jobUnit->exists()) { return; }

	assert(workers.find(unit) == workers.end());

	workers.insert(unit);
	setWorkerJob(unit, job, jobUnit);
}

void WorkerData::addWorker(BWAPI::Unit unit, WorkerJob job, BWAPI::UnitType jobUnitType)
{
	if (!unit || !unit->exists()) { return; }

	assert(workers.find(unit) == workers.end());
	workers.insert(unit);
	setWorkerJob(unit, job, jobUnitType);
}

void WorkerData::addDepot(BWAPI::Unit unit)
{
	if (!unit || !unit->exists()) return;

	assert(depots.find(unit) == depots.end());
	depots.insert(unit);
	depotWorkerCount[unit] = 0;
	
	depotAverageMineralPosition[unit] = unit->getUnitsInRadius(300, BWAPI::Filter::IsMineralField).getPosition();
}

void WorkerData::removeDepot(BWAPI::Unit unit)
{	
	//usually used when the depot is dead
	depots.erase(unit);
	depotWorkerCount.erase(unit);
	depotAverageMineralPosition.erase(unit);

	// re-balance workers in here
	for (auto & worker : workers)
	{
		// if a worker was working at this depot
		if (workerDepotMap[worker] == unit)
		{
			setWorkerJob(worker, Idle, nullptr);
		}
	}
}

void WorkerData::addRefinery(BWAPI::Unit unit)
{
	if (!unit || !unit->exists()) return;

	UAB_ASSERT(refineries.find(unit) == refineries.end(), "Tried to add refinery twice");
	refineries.insert(unit);
	refineryWorkerCount[unit] = 0;
}

void WorkerData::removeRefinery(BWAPI::Unit unit)
{
	refineries.erase(unit);
	refineryWorkerCount.erase(unit);
}

void WorkerData::eraseWorkerMineralAssignment(BWAPI::Unit) {
	if (workerMineralAssignment.find(unit) == workersOnMineralPatch.end()) 
	{
		return; //nothing to erase
	}

	addToMineralPatch(workerMineralAssignment[unit], -1);
	workerMineralAssignment.erase(unit);
}

void WorkerData::reassignWorkerToMineral(BWAPI::Unit) {
	UAB_ASSERT(workerJobMap[unit] == Minerals, "Unit with non-mineral job, reassigned to mineral");

	assignMineralToMine(unit);
}

int WorkerData::computeTimeToMine(BWAPI::Unit worker, BWAPI::Unit mineral) {
	double dist = worker->getDistance(mineral);
	double speed = worker->getType().topSpeed();

	int time = BWAPI::Broodwar->getFrameCount() + int(dist / speed);

	if (mineralHarvestedTime.find(mineral) != workersOnMineralPatch.end()) {
		mineralHarvestedTime[mineral] = 0;
	}

	if (time < mineralHarvestedTime[mineral]) { //worker will arrive there early, before it's been harvested
		time = mineralHarvestedTime[mineral] + 80;
	}
	else { //worker will arrive there late, after it's been harvested
		time = time + 80;
	}

	return time;
}

void WorkerData::addToMineralPatch(BWAPI::Unit unit, int num)
{
    if (workersOnMineralPatch.find(unit) == workersOnMineralPatch.end())
    {
        workersOnMineralPatch[unit] = num;
    }
    else
    {
        workersOnMineralPatch[unit] += num;
    }
}

void WorkerData::setWorkerJob(BWAPI::Unit unit, WorkerJob job, BWAPI::Unit jobUnit)
{
	if (!unit || !unit->exists()) { return; }

	clearPreviousJob(unit);
	workerJobMap[unit] = job;

	if (job == Minerals)
	{
		// increase the number of workers assigned to this nexus
		depotWorkerCount[jobUnit] += 1;

		// set the mineral the worker is working on
		workerDepotMap[unit] = jobUnit;

        assignMineralToMine(unit);
	}
	else if (job == Gas)
	{
		auto depot = getClosestDepot(jobUnit);
		UAB_ASSERT(depot, "WorkerData: Depot in gas assignment is NULL!");
		workerDepotMap[unit] = getClosestDepot(jobUnit); //we presume the depot provided is a valid depot

		refineryWorkerCount[jobUnit] += 1;

		workerRefineryMap[unit] = jobUnit; // set the refinery the worker is working on
	}
    else if (job == Repair)
    {
        // only SCVs can repair
        assert(unit->getType() == BWAPI::UnitTypes::Terran_SCV);

        // set the building the worker is to repair
        workerRepairMap[unit] = jobUnit;
    }
//	else if (job == Scout)
//	{
//
//	}
    else if (job == Build)
    {
        // BWAPI::Broodwar->printf("Setting worker job to build");
    }
}

void WorkerData::setWorkerJob(BWAPI::Unit unit, WorkerJob job, BWAPI::UnitType jobUnitType)
{
	if (!unit) { return; }
	clearPreviousJob(unit);
	workerJobMap[unit] = job;

	if (job == Build)
	{
		workerBuildingTypeMap[unit] = jobUnitType;
	}
	else
	{
		BWAPI::Broodwar->printf("Called build-only setWorkerJob with wrong job");
	}
}

void WorkerData::setWorkerJob(BWAPI::Unit unit, WorkerJob job, WorkerMoveData wmd)
{
	if (!unit) { return; }

	clearPreviousJob(unit);
	workerJobMap[unit] = job;

	if (job == Move)
	{
		workerMoveMap[unit] = wmd;
	}

	if (workerJobMap[unit] != Move)
	{
		BWAPI::Broodwar->printf("Something went horribly wrong");
	}
}

void WorkerData::clearPreviousJob(BWAPI::Unit unit)
{
	if (!unit) { return; }

	WorkerJob previousJob = getWorkerJob(unit);

	if (previousJob == Minerals)
	{
		depotWorkerCount[workerDepotMap[unit]] -= 1;

		workerDepotMap.erase(unit);

        // remove a worker from this unit's assigned mineral patch
        addToMineralPatch(workerMineralAssignment[unit], -1);
        workerMineralAssignment.erase(unit);
	}
	else if (previousJob == Gas)
	{
		refineryWorkerCount[workerRefineryMap[unit]] -= 1;
		if (refineryWorkerCount[workerRefineryMap[unit]] < 0) {
			UAB_ASSERT(false, "Error: refinery worker count went below 0");
		}
		workerRefineryMap.erase(unit);


		workerDepotMap.erase(unit);
	}
	else if (previousJob == Build)
	{
		workerBuildingTypeMap.erase(unit);
	}
	else if (previousJob == Repair)
	{
		workerRepairMap.erase(unit);
	}
	else if (previousJob == Move)
	{
		workerMoveMap.erase(unit);
	}

	workerJobMap.erase(unit);
}

int WorkerData::getNumWorkers() const
{
	return workers.size();
}

int WorkerData::getNumMineralWorkers() const
{
	size_t num = 0;
	for (auto & unit : workers)
	{
		if (workerJobMap.at(unit) == WorkerData::Minerals)
		{
			num++;
		}
	}
	return num;
}

int WorkerData::getNumGasWorkers() const
{
	size_t num = 0;
	for (auto & unit : workers)
	{
		if (workerJobMap.at(unit) == WorkerData::Gas)
		{
			num++;
		}
	}
	return num;
}

int WorkerData::getNumCombatWorkers() const
{
	size_t num = 0;
	for (auto & unit : workers)
	{
		if (workerJobMap.at(unit) == WorkerData::Combat)
		{
			num++;
		}
	}
	return num;
}

int WorkerData::getNumIdleWorkers() const
{
	size_t num = 0;
	for (auto & unit : workers)
	{
		if (workerJobMap.at(unit) == WorkerData::Idle)
		{
			num++;
		}
	}
	return num;
}

enum WorkerData::WorkerJob WorkerData::getWorkerJob(BWAPI::Unit unit)
{
	if (!unit) { return Default; }

	auto it = workerJobMap.find(unit);

	if (it != workerJobMap.end())
	{
		return it->second;
	}

	return Default;
}

bool WorkerData::depotIsFull(BWAPI::Unit depot, double ratio)
{
	if (!depot) { return false; }

	int assignedWorkers = getNumAssignedWorkers(depot);
	int mineralsNearDepot = getMineralsNearDepot(depot);

	return assignedWorkers >= int (ratio * mineralsNearDepot + 0.5);
}

BWAPI::Unitset WorkerData::getMineralPatchesNearDepot(BWAPI::Unit depot)
{
    // if there are minerals near the depot, add them to the set
	int radius = 300;
	BWAPI::Unitset mineralsNearDepot = depot->getUnitsInRadius(radius, BWAPI::Filter::IsMineralField);

    // if we didn't find any, use the whole map
    if (mineralsNearDepot.empty()) mineralsNearDepot = BWAPI::Broodwar->getMinerals();

    return mineralsNearDepot;
}

int WorkerData::getMineralsNearDepot(BWAPI::Unit depot)
{
	if (!depot) { return 0; }

	int radius = 300;
	BWAPI::Unitset nearbyDepots = depot->getUnitsInRadius(radius, BWAPI::Filter::IsResourceDepot && (BWAPI::Filter::GetPlayer == BWAPI::Broodwar->self()));
	BWAPI::Unitset nearbyMinerals = depot->getUnitsInRadius(radius, BWAPI::Filter::IsMineralField);

	if (nearbyDepots.empty()) return nearbyMinerals.size();

	//otherwise, verify this is the closest depot to the mineral between all nearby depots
	int mineralsNearDepot = 0;
	for (auto & unit : nearbyMinerals) {
		bool closestDepot = true;
		for (auto & odepot : nearbyDepots) {
			if (unit->getDistance(depot) > unit->getDistance(odepot)) {
				closestDepot = false;
				break;
			}
		}
		if (closestDepot) mineralsNearDepot++;
	}

	return mineralsNearDepot;
}

BWAPI::Unit WorkerData::getWorkerResource(BWAPI::Unit unit)
{
	if (!unit) { return nullptr; }

	// if the worker is mining, set the iterator to the mineral map
	if (getWorkerJob(unit) == Minerals)
	{
		auto it = workerMineralAssignment.find(unit);
		if (it != workerMineralAssignment.end())
		{
			return it->second;
		}
		else {
			return workerData.reassignWorkerToMineral(unit);
		}
	}
	else if (getWorkerJob(unit) == Gas)
	{
		auto it = workerRefineryMap.find(unit);
		if (it != workerRefineryMap.end())
		{
			return it->second;
		}	
	}

	return nullptr;
}

BWAPI::Unit WorkerData::assignMineralToMine(BWAPI::Unit worker)
{
	if (!worker) { return nullptr; }

	// get the depot associated with this unit
	BWAPI::Unit depot = getWorkerDepot(worker);
	BWAPI::Unit bestMineral = nullptr;
	double bestTime = 10000000000;

	if (depot)
	{
        BWAPI::Unitset mineralPatches = getMineralPatchesNearDepot(depot);
		
		for (auto & mineral : mineralPatches)
		{
            double numAssigned = workersOnMineralPatch[mineral];
			if (workersOnMineralPatch[mineral] >= 2) continue; //no more than 2 per patch!

			int time = computeTimeToMine(worker, mineral);
            if (time < bestTime)
            {
                bestMineral = mineral;
				bestTime = time;
            }
		}
	}

	if (bestMineral != nullptr) {
		workerMineralAssignment[worker] = bestMineral;
		mineralHarvestedTime[bestMineral] = bestTime;
		addToMineralPatch(bestMineral, 1);
	}

	return bestMineral;
}

BWAPI::Unit WorkerData::getWorkerRepairUnit(BWAPI::Unit unit)
{
	if (!unit) { return nullptr; }

	auto it = workerRepairMap.find(unit);

	if (it != workerRepairMap.end())
	{
		return it->second;
	}	

	return nullptr;
}

BWAPI::Unit WorkerData::getWorkerDepot(BWAPI::Unit unit)
{
	if (!unit) { return nullptr; }

	auto it = workerDepotMap.find(unit);

	if (it != workerDepotMap.end())
	{
		return it->second;
	}	

	return nullptr;
}

BWAPI::UnitType	WorkerData::getWorkerBuildingType(BWAPI::Unit unit)
{
	if (!unit) { return BWAPI::UnitTypes::None; }

	auto it = workerBuildingTypeMap.find(unit);

	if (it != workerBuildingTypeMap.end())
	{
		return it->second;
	}	

	return BWAPI::UnitTypes::None;
}

WorkerMoveData WorkerData::getWorkerMoveData(BWAPI::Unit unit)
{
	auto it = workerMoveMap.find(unit);

	assert(it != workerMoveMap.end());
	
	return (it->second);
}

int WorkerData::getNumAssignedWorkers(BWAPI::Unit unit)
{
	if (!unit) { return 0; }

	// if the worker is mining, set the iterator to the mineral map
	if (unit->getType().isResourceDepot())
	{
		auto it = depotWorkerCount.find(unit);

		// if there is an entry, return it
		if (it != depotWorkerCount.end())
		{
			return it->second;
		}
	}
	else if (unit->getType().isRefinery())
	{
		auto it = refineryWorkerCount.find(unit);

		// if there is an entry, return it
		if (it != refineryWorkerCount.end())
		{
			return it->second;
		}
	}
	else if (unit->getType().isMineralField())
	{
		return workersOnMineralPatch[unit];
	}

	// when all else fails, return 0
	return 0;
}

char WorkerData::getJobCode(BWAPI::Unit unit)
{
	if (!unit) { return 'X'; }

	WorkerData::WorkerJob j = getWorkerJob(unit);

	if (j == WorkerData::Minerals) return 'M';
	if (j == WorkerData::Gas) return 'G';
	if (j == WorkerData::Build) return 'B';
	if (j == WorkerData::Combat) return 'C';
	if (j == WorkerData::Idle) return 'I';
	if (j == WorkerData::Repair) return 'R';
	if (j == WorkerData::Move) return '>';
	if (j == WorkerData::Scout) return 'S';
	if (j == WorkerData::Default) return '?';       // e.g. incomplete SCV
	return 'X';
}

// Add all gas workers to the given set.
void WorkerData::getGasWorkers(std::set<BWAPI::Unit> & mw)
{
	for (auto kv : workerRefineryMap)
	{
		mw.insert(kv.first);
	}
}

void WorkerData::drawDepotDebugInfo()
{
	if (!Config::Debug::DrawWorkerInfo) return;
	for (auto & depot : depots)
	{
		UAB_ASSERT(depot, "Unit was null");

		int x = depot->getPosition().x - 64;
		int y = depot->getPosition().y - 32;

		BWAPI::Broodwar->drawBoxMap(x-2, y-1, x+75, y+14, BWAPI::Colors::Black, true);
		BWAPI::Broodwar->drawTextMap(x, y, "\x04 Workers: %d / %d", getNumAssignedWorkers(depot), getMineralsNearDepot(depot));

        BWAPI::Unitset minerals = getMineralPatchesNearDepot(depot);

        for (auto & mineral : minerals)
        {
            int x = mineral->getPosition().x;
		    int y = mineral->getPosition().y;

            if (workersOnMineralPatch.find(mineral) != workersOnMineralPatch.end())
            {
                 //if (Config::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawBoxMap(x-2, y-1, x+75, y+14, BWAPI::Colors::Black, true);
                 //if (Config::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawTextMap(x, y, "\x04 Workers: %d", workersOnMineralPatch[mineral]);
            }
        }
	}
}

BWAPI::Unit WorkerData::getClosestDepot(BWAPI::Unit unit) {
	UAB_ASSERT(unit, "Unit in WorkerData::getClosestDepot was null");

	BWAPI::Unit closestDepot = nullptr;
	double closestDistance = 0;

	for (auto & depot : depots)
	{
		UAB_ASSERT(depot, "Unit was null");

		double distance = depot->getDistance(unit);
		if (!closestDepot || distance < closestDistance)
		{
			closestDepot = depot;
			closestDistance = distance;
		}
	}

	return closestDepot;
}