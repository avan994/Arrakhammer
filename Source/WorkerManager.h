#pragma once

#include <Common.h>
#include "BuildingManager.h"
#include "WorkerData.h"

namespace UAlbertaBot
{
class Building;

class WorkerManager
{
	void handleWorkers();

	const double minWorkersPerPatch = 0.9;
	const double midWorkersPerPatch = minWorkersPerPatch + (Config::Macro::WorkersPerPatch - minWorkersPerPatch)/2.0;
	const double maxWorkersPerPatch = Config::Macro::WorkersPerPatch; //should be greater than 1.0
	//const double absoluteMaxWorkersPerPatch = 2.0;

    WorkerData  workerData;
    BWAPI::Unit previousClosestWorker;
	bool		_collectGas;
	int			_lastFrameWorkerGasTransfer;
	const int	minTimeBetweenGasTransfer = 24 * 10;
	int			_lastFrameGasChanged;
	const int	minTimeBetweenGasChange = 24 * 4;

	void		putWorkerToWork(BWAPI::Unit);
	bool		refineryHasDepot(BWAPI::Unit refinery);
	bool        isGasStealRefinery(BWAPI::Unit unit);

	void        handleGasWorkers();

	BWAPI::Unit getClosestEnemyUnit(BWAPI::Unit worker);
	bool executeSelfDefense(BWAPI::Unit);

	WorkerManager();

public:

    void        update();
    void        onUnitDestroy(BWAPI::Unit unit);
    void        onUnitMorph(BWAPI::Unit unit);
    void        onUnitShow(BWAPI::Unit unit);
    void        onUnitRenegade(BWAPI::Unit unit);
	void		onUnitComplete(BWAPI::Unit unit);

    void        finishedWithWorker(BWAPI::Unit unit);
    void        finishedWithCombatWorkers();

    void        drawResourceDebugInfo();
    void        updateWorkerStatus();
    void        drawWorkerInformation(int x,int y);

    int         getNumMineralWorkers() const;
    int         getNumGasWorkers() const;
	int         getNumReturnCargoWorkers() const;
	int			getNumCombatWorkers() const;
	int         getNumIdleWorkers() const;
	int			getMaxWorkers() const;


	// Note: _collectGas == false allows that a little more gas may still be collected.
	bool		isCollectingGas()              { return _collectGas; };
	void		setCollectGas(bool collectGas, bool limited = false) { //only allow if enough time since the last time since we changed gas has passed
		if (limited && BWAPI::Broodwar->getFrameCount() - _lastFrameGasChanged < minTimeBetweenGasChange) return;
		_lastFrameGasChanged = BWAPI::Broodwar->getFrameCount();
		_collectGas = collectGas; 
	};

    bool        isWorkerScout(BWAPI::Unit worker);
    bool        isFree(BWAPI::Unit worker);
    bool        isBuilder(BWAPI::Unit worker);
	bool		isCombatWorker(BWAPI::Unit);

    BWAPI::Unit getBuilder(const Building & b,bool setJobAsBuilder = true);
    BWAPI::Unit getMoveWorker(BWAPI::Position p);
    BWAPI::Unit getClosestDepot(BWAPI::Unit worker, bool anyDepotIsFine = false);
	BWAPI::Unit WorkerManager::getClosestRefinery(BWAPI::Unit);
    BWAPI::Unit getGasWorker(BWAPI::Unit refinery);
    BWAPI::Unit getClosestMineralWorkerTo(BWAPI::Unit enemyUnit);
    BWAPI::Unit getWorkerScout();

	BWAPI::Unit WorkerManager::getBestDepot();

	void        setScoutWorker(BWAPI::Unit worker);
    void        setBuildingWorker(BWAPI::Unit worker,Building & b);
    void        setRepairWorker(BWAPI::Unit worker,BWAPI::Unit unitToRepair);
    void        setMoveWorker(int m,int g,BWAPI::Position p);
    void        setCombatWorker(BWAPI::Unit worker);

	bool		shouldPullWorker(BWAPI::Unit);

    bool        willHaveResources(int mineralsRequired,int gasRequired,double distance);

    static WorkerManager &  Instance();
};
}