#include "CombatSimulation.h"
#include "FAP.h"
#include "UnitUtil.h"

#include "WorkerManager.h"

using namespace UAlbertaBot;

//Sparcraft was causing timeouts with large numbers of zerglings, and lag end-game
//FAP appears to have since taken over for combat simulation

CombatSimulation::CombatSimulation()
{
}

// sets the starting states based on the combat units within a radius of a given position
// this center will most likely be the position of the forwardmost combat unit we control
void CombatSimulation::setCombatUnits(const BWAPI::Position & center, const int radius, bool flying)
{
	fap.clearState();

	if (Config::Debug::DrawCombatSimulationInfo)
	{
		BWAPI::Broodwar->drawCircleMap(center.x, center.y, 6, BWAPI::Colors::Red, true);
		BWAPI::Broodwar->drawCircleMap(center.x, center.y, radius, BWAPI::Colors::Red);
	}

	BWAPI::Unitset ourCombatUnits;
	std::vector<UnitInfo> enemyCombatUnits;

	// Both of these return only completed units, so we don't need to check that later.
	MapGrid::Instance().GetUnits(ourCombatUnits, center, radius, true, false);
	InformationManager::Instance().getNearbyForce(enemyCombatUnits, center, BWAPI::Broodwar->enemy(), radius);

	// Add our units.
	double groundRange = 0;
	int nUnits = 0;
	bool canShootAir = false;
	bool onlyFlying = flying;

	for (const auto unit : ourCombatUnits)
	{
		if (!unit || !unit->exists() || unit->getHitPoints() < 0) continue;

		if (!UnitUtil::IsCombatSimUnit(unit->getType()) && !WorkerManager::Instance().isCombatWorker(unit)) continue;

		if (flying && !unit->isFlying()) {
			onlyFlying = false; //we have grounded allies nearby
		}

		// Skip uncompleted or unpowered static defense.
		if (unit->getType().isBuilding() && (!unit->isCompleted() || !unit->isPowered())) {
			continue;
		}

		if (unit && unit->exists()) {
			groundRange += double(BWAPI::Broodwar->self()->weaponMaxRange(unit->getType().groundWeapon()));
			nUnits++;
		}

		if (unit->getType().isBuilding()) {
			if (!(unit->getClosestUnit(BWAPI::Filter::IsEnemy, 7 * 32))) continue;
		}
		else if (unit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk) {
			if (unit->getHitPoints() <= 30) continue;
		}

		if (!canShootAir && unit && unit->exists() && unit->getType().airWeapon() != BWAPI::WeaponTypes::None) {
			canShootAir = true;
		}

		fap.addIfCombatUnitPlayer1(unit);
	}

	if (nUnits > 0) groundRange /= nUnits;

	int nCannons = 0;
	// Add enemy units
	for (const UnitInfo & ui : enemyCombatUnits)
	{
		if (ui.lastHealth == 0 || ui.type == BWAPI::UnitTypes::Unknown) {
			continue;
		}

		if (ui.type == BWAPI::UnitTypes::Protoss_Interceptor) {
			continue;
		}

		if (ui.type.isWorker()) {
			if (!flying) {
				continue; //flying might want to kill the workers
			}
		}
		else if (onlyFlying && !UnitUtil::TypeCanAttackAir(ui.type)) {
			if (ui.type == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode ||
				ui.type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
				ui.type == BWAPI::UnitTypes::Protoss_High_Templar) {
				//except for these targets
			}
			else {
				continue; //only include units that can attack air if we're purely a flying squad
			}
		}

		if (ui.type.isFlyer() && !canShootAir) continue; //we only worry about air if we can shoot air

		// Skip uncompleted or unpowered static defense.
		if (ui.type.isBuilding() && (!ui.completed || (ui.unit->exists() && !ui.unit->isPowered()))) {
			continue;
		}
		if (ui.unit && ui.unit->exists()) {
			auto unit = ui.unit;

			
			if (unit->getOrder() && (unit->getOrder() == BWAPI::Orders::Unsieging || unit->getOrder() == BWAPI::Orders::Sieging)) continue;

			double speed = pow(unit->getVelocityX(), 2) + pow(unit->getVelocityY(), 2);

			if (groundRange >= 3 * 32 &&
				(speed < pow(unit->getType().topSpeed(), 2) / 16 && !unit->isAttacking()) &&
				BWAPI::Broodwar->enemy()->weaponMaxRange(unit->getType().groundWeapon()) <= 32) {
				//they must be stuck somewhere, maybe due to a terrain feature
				continue;
			}
		}

		if (ui.type == BWAPI::UnitTypes::Protoss_Photon_Cannon) {
			nCannons++;
			if (nCannons >= 4) { //add the cannon twice, along with the usual addition at the bottom
				fap.addIfCombatUnitPlayer2(ui);
				nCannons -= 2; //double every 2nd cannon after
			}
		}



		//add the unit
		fap.addIfCombatUnitPlayer2(ui);
	}
}

double CombatSimulation::simulateCombat()
{
	fap.simulate(24*24);
	std::pair<int, int> scores = fap.playerScores();

	int score = scores.first - scores.second;

	if (Config::Debug::DrawCombatSimulationInfo)
	{
		BWAPI::Broodwar->drawTextScreen(150, 200, "%cCombat sim: us %c%d %c- them %c%d %c= %c%d",
			white, orange, scores.first, white, orange, scores.second, white,
			score >= 0 ? green : red, score);
	}

	//display the unit at the end
	if (Config::Debug::DrawCombatSimulationInfo)
	{
		auto state = fap.getState();
		int yStart = 220, y = yStart;

		std::map <BWAPI::UnitType, int> unitCountsFriendly, unitCountsEnemy;

		for (auto &u : *state.first)
			++unitCountsFriendly[u.unitType];
		for (auto &u : *state.second)
			++unitCountsEnemy[u.unitType];

		for (auto &e : unitCountsFriendly) {
			BWAPI::Broodwar->drawTextScreen(150, y, "%s, %d", e.first.c_str(), e.second);
			y += 20;
		}

		y = yStart;

		for (auto &e : unitCountsEnemy) {
			BWAPI::Broodwar->drawTextScreen(250, y, "%s, %d", e.first.c_str(), e.second);
			y += 20;
		}

		BWAPI::Broodwar->drawTextScreen(150, 200, "%cCombat sim: us %c%d %c- them %c%d %c= %c%d",
			white, orange, scores.first, white, orange, scores.second, white,
			score >= 0 ? green : red, score);
	}

	return double(score);
}