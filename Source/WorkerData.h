#pragma once

#include "Common.h"

namespace UAlbertaBot
{
class WorkerMoveData
{
public:

	int mineralsNeeded;
	int gasNeeded;
	BWAPI::Position position;

	WorkerMoveData(int m, int g, BWAPI::Position p)
        : mineralsNeeded(m)
        , gasNeeded(g)
        , position(p)
	{
	}

	WorkerMoveData() {}
};

class WorkerData 
{

public:

	enum WorkerJob {Minerals, Gas, Build, Combat, Idle, Repair, Move, Scout, Default};

private:

	BWAPI::Unitset workers;
	BWAPI::Unitset depots;
	BWAPI::Unitset refineries;

	BWAPI::Unit				getClosestDepot(BWAPI::Unit);

	std::unordered_map<BWAPI::Unit, BWAPI::Position>    depotAverageMineralPosition;

	std::unordered_map<BWAPI::Unit, enum WorkerJob>		workerJobMap;           // worker -> job
	std::unordered_map<BWAPI::Unit, BWAPI::Unit>		workerDepotMap;         // worker -> resource depot (hatchery)
	std::unordered_map<BWAPI::Unit, BWAPI::Unit>		workerRefineryMap;      // worker -> refinery
	std::unordered_map<BWAPI::Unit, BWAPI::Unit>		workerRepairMap;        // worker -> unit to repair
	std::unordered_map<BWAPI::Unit, WorkerMoveData>		workerMoveMap;          // worker -> location
	std::unordered_map<BWAPI::Unit, BWAPI::UnitType>	workerBuildingTypeMap;  // worker -> building type

	std::unordered_map<BWAPI::Unit, int>				depotWorkerCount;       // mineral workers per depot
	std::unordered_map<BWAPI::Unit, int>				refineryWorkerCount;    // gas workers per refinery

	std::unordered_map<BWAPI::Unit, int>				workersOnMineralPatch;  // workers per mineral patch
	std::unordered_map<BWAPI::Unit, BWAPI::Unit>		workerMineralAssignment;// worker -> mineral patch

	void clearPreviousJob(BWAPI::Unit unit);

public:

	WorkerData();

	void					workerDestroyed(BWAPI::Unit unit);
	void					addDepot(BWAPI::Unit unit);
	void					removeDepot(BWAPI::Unit unit);
	void					addRefinery(BWAPI::Unit);
	void					removeRefinery(BWAPI::Unit);

	void					addWorker(BWAPI::Unit unit);
	void					addWorker(BWAPI::Unit unit, WorkerJob job, BWAPI::Unit jobUnit);
	void					addWorker(BWAPI::Unit unit, WorkerJob job, BWAPI::UnitType jobUnitType);
	void					setWorkerJob(BWAPI::Unit unit, WorkerJob job, BWAPI::Unit jobUnit);
	void					setWorkerJob(BWAPI::Unit unit, WorkerJob job, WorkerMoveData wmd);
	void					setWorkerJob(BWAPI::Unit unit, WorkerJob job, BWAPI::UnitType jobUnitType);

	int						getNumWorkers() const;
	int						getNumMineralWorkers() const;
	int						getNumGasWorkers() const;
	int						getNumCombatWorkers() const;
	int						getNumIdleWorkers() const;
	char					getJobCode(BWAPI::Unit unit);

	void					getMineralWorkers(std::set<BWAPI::Unit> & mw);
	void					getGasWorkers(std::set<BWAPI::Unit> & mw);
	void					getBuildingWorkers(std::set<BWAPI::Unit> & mw);
	void					getRepairWorkers(std::set<BWAPI::Unit> & mw);
	
	bool					depotIsFull(BWAPI::Unit depot, double = Config::Macro::WorkersPerPatch);
	int						getMineralsNearDepot(BWAPI::Unit depot);

	int						getNumAssignedWorkers(BWAPI::Unit unit);
	BWAPI::Unit				getMineralToMine(BWAPI::Unit worker);

	enum WorkerJob			getWorkerJob(BWAPI::Unit unit);
	BWAPI::Unit				getWorkerResource(BWAPI::Unit unit);
	BWAPI::Unit				getWorkerDepot(BWAPI::Unit unit);
	BWAPI::Unit				getWorkerRepairUnit(BWAPI::Unit unit);
	BWAPI::UnitType			getWorkerBuildingType(BWAPI::Unit unit);
	WorkerMoveData			getWorkerMoveData(BWAPI::Unit unit);

    BWAPI::Unitset          getMineralPatchesNearDepot(BWAPI::Unit depot);
    void                    addToMineralPatch(BWAPI::Unit unit, int num);
	void					drawDepotDebugInfo();

	const BWAPI::Unitset & getWorkers() const { return workers; }
	const BWAPI::Unitset & getDepots() const { return depots; }
	const BWAPI::Unitset & getRefineries() const { return refineries; }

	const BWAPI::Position getAverageMineralPosition(BWAPI::Unit unit) { return depotAverageMineralPosition[unit]; }
};
}