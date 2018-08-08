#pragma once

#include <Common.h>
#include <BWAPI.h>
#include <InformationManager.h>

namespace UAlbertaBot
{

namespace Micro
{      
	static std::map<BWAPI::Unit, int> stuckFrames;
	const static int minStuckTime = 24;

	void SmartUseTech(BWAPI::Unit, BWAPI::TechType tech, const BWAPI::PositionOrUnit & = NULL);
    void SmartAttackUnit(BWAPI::Unit attacker, BWAPI::Unit target);
    void SmartAttackMove(BWAPI::Unit attacker, const BWAPI::Position & targetPosition);
    void SmartMove(BWAPI::Unit attacker, const BWAPI::Position & targetPosition);
	void SmartStop(BWAPI::Unit unit);
	void SmartTravel(BWAPI::Unit, const BWAPI::Position &, BWAPI::Position = BWAPI::Position(0, 0), bool path = false);
	void SmartRightClick(BWAPI::Unit unit, BWAPI::Unit target);
    void SmartLaySpiderMine(BWAPI::Unit unit, BWAPI::Position pos);
    void SmartRepair(BWAPI::Unit unit, BWAPI::Unit target);
	bool SmartScan(const BWAPI::Position & targetPosition);
	void SmartReturnCargo(BWAPI::Unit worker);
	void SmartHoldPosition(BWAPI::Unit unit);
	void SmartPatrol(BWAPI::Unit unit, BWAPI::Position targetPosition);
	void SmartKiteTarget(BWAPI::Unit rangedUnit, BWAPI::Unit target, BWAPI::Position kiteVector = BWAPI::Position(0, 0));
    void MutaDanceTarget(BWAPI::Unit muta, BWAPI::Unit target, BWAPI::Position kiteVector = BWAPI::Position(0,0));

	BWAPI::Position GetChasePosition(BWAPI::Unit unit, BWAPI::Unit target);
	BWAPI::Position GetProjectedPosition(BWAPI::Unit unit, BWAPI::Unit target);
    BWAPI::Position GetKiteVector(BWAPI::Unit unit, BWAPI::Unit target);
	BWAPI::Position GetTangentVector(BWAPI::Unit, BWAPI::Unit, BWAPI::Position);
	bool checkMovable(BWAPI::Position, int = 4);
	bool SuperSmartMove(BWAPI::Unit unit, BWAPI::Position pos); 

	bool AvoidAllies(BWAPI::Unit unit, int radius);
	bool StackUp(BWAPI::Unit unit, int strictness, BWAPI::UnitType = BWAPI::UnitTypes::Zerg_Mutalisk);
	bool ZerglingAttack(BWAPI::Unit, BWAPI::Unit, BWAPI::Position kiteVector = BWAPI::Position(0, 0)); //unused
	bool SurroundTarget(BWAPI::Unit unit, BWAPI::Unit target);
	bool StormDodge(BWAPI::Unit);
	bool NukeDodge(BWAPI::Unit);
	bool ScarabDodge(BWAPI::Unit);
	bool CheckSpecialCases(BWAPI::Unit);
	bool Unstick(BWAPI::Unit);

	bool AvoidWorkerHazards(BWAPI::Unit);

    void Rotate(double &x, double &y, double angle);
    void Normalize(double &x, double &y);

    void drawAPM(int x, int y);
};
}