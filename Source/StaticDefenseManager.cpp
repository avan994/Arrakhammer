#include "StaticDefenseManager.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

StaticDefenseManager::StaticDefenseManager() 
{ 
}

void StaticDefenseManager::executeMicro(const BWAPI::Unitset & targets) 
{
	assignTargets(targets);
}

void StaticDefenseManager::assignTargets(const BWAPI::Unitset & targets)
{
	// we don't use the targets input.

    const BWAPI::Unitset & units = getUnits();

	targeted.clear();

    for (auto & unit : units)
	{
		if (!unit || !unit->isCompleted()) continue;

		BWAPI::Unit target = NULL;
		BWAPI::Unitset unitTargets;
		auto unitType = unit->getType();
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Sunken_Colony) {
			//sunken colony attacked recently (~15 frames after attacking, ~32 frame cooldown)
			if (unit->getGroundWeaponCooldown() > 16 || unit->isStartingAttack()) {
				//BWAPI::Broodwar->drawBoxMap(unit->getPosition() - BWAPI::Position(16, 16), unit->getPosition() + BWAPI::Position(16, 16), BWAPI::Colors::Red);

				BWAPI::Unit target = unit->getOrderTarget();
				if (target && target->exists()) {
					targeted[target] += UnitUtil::GetDamage(unit, target);
					//BWAPI::Broodwar->drawCircleMap(target->getPosition(), 32, BWAPI::Colors::Red);
				}
				continue;
			} else { //select next target
				//BWAPI::Broodwar->drawBoxMap(unit->getPosition() - BWAPI::Position(16, 16), unit->getPosition() + BWAPI::Position(16, 16), BWAPI::Colors::Brown);

				unitTargets = unit->getUnitsInRadius(48 + 7 * 32, BWAPI::Filter::IsEnemy && !BWAPI::Filter::IsFlying);
			}
		}
		else if (unit->getType() == BWAPI::UnitTypes::Zerg_Spore_Colony || unit->getType() == BWAPI::UnitTypes::Terran_Missile_Turret) {
			unitTargets = unit->getUnitsInRadius(48 + 7 * 32, BWAPI::Filter::IsEnemy && BWAPI::Filter::IsFlying);
			//unitTargets = unit->getUnitsInWeaponRange(unit->getType().airWeapon(), BWAPI::Filter::IsFlying);
		}
		else if (unit->getType() == BWAPI::UnitTypes::Protoss_Photon_Cannon) {
			unitTargets = unit->getUnitsInWeaponRange(unit->getType().groundWeapon());
			UAB_ASSERT(false, "Somewhat unsupported unit type in Static Defense Manager: Photon Cannon");
		}
		else {
			UAB_ASSERT(false, "Unsupported unit type in Static Defense Manager: Unknown");
		}

		if (!(unitTargets.empty())) {
			target = getTarget(unit, unitTargets);
		}

		if (target && target->exists()) //we found an appropriate target
		{
			//BWAPI::Broodwar->drawCircleMap(target->getPosition(), 32, BWAPI::Colors::Brown);
			if (Config::Debug::DrawUnitTargetInfo)
			{
				BWAPI::Broodwar->drawLineMap(unit->getPosition(), unit->getTargetPosition(), BWAPI::Colors::Purple);
			}
			
			targeted[target] += UnitUtil::GetDamage(unit, target);

			Micro::SmartAttackUnit(unit, target);
		}
		//if we can't find a target, the sunken AI will likely find one anyway
	}
}

BWAPI::Unit StaticDefenseManager::getTarget(BWAPI::Unit unit, const BWAPI::Unitset & targets)
{
    int highPriority = 0;
	BWAPI::Unit bestTarget = nullptr;

    for (const auto & target : targets)
    {
		if (target->getHitPoints() <= 0) { //do not divide by 0
			//UAB_ASSERT(false, "Unit with <= 0 HP sighted in StaticDefenseManager LTD calculation.");
			continue;
		}

        int priority            = getAttackPriority(unit, target);
		if (priority == 0) continue;

		int bonus = target->getType() == BWAPI::UnitTypes::Terran_Marine ? 15 : 1; //try to account for medic healing/normal healing
		int finalHP = bonus + target->getHitPoints() + target->getShields() - targeted[target];
		if (finalHP <= 0) { // it's already dead
			continue;
		}
		else if (finalHP - UnitUtil::GetDamage(unit, target) <= 0 ||//the unit will die if we hit it
			targeted[target] > 0) { //it's being targeted by another unit, too -- potential tiebreaker
			priority++;
		} 

		if (priority > highPriority)
		{
			highPriority = priority;
			bestTarget = target;
		} 
    }

	if (highPriority == 0) return NULL;
    return bestTarget;
}

// get the attack priority of a target unit
int StaticDefenseManager::getAttackPriority(BWAPI::Unit unit, BWAPI::Unit target) 
{

	BWAPI::UnitType StaticDefenseType = unit->getType();
	BWAPI::UnitType targetType = target->getType();

	if (StaticDefenseType == BWAPI::UnitTypes::Terran_Missile_Turret &&
		targetType == BWAPI::UnitTypes::Protoss_Observer &&
		target->getDistance(unit) <= 8) return 0; //it's too close

	bool isThreat = targetType.groundWeapon() != BWAPI::WeaponTypes::None; 

    // if the target is building something near our base something is fishy
    BWAPI::Position ourBasePosition = BWAPI::Position(InformationManager::Instance().getMyMainBaseLocation()->getPosition());
	if (target->getDistance(ourBasePosition) < 1200) {
		if (target->getType().isWorker() && (target->isConstructing() || target->isRepairing()))
		{
			return 1001;
		}
		// nearby threats still more important than whatever building it is, unless it's a threat
		if (target->getType().isBuilding())
		{
			if (targetType.groundWeapon() != BWAPI::WeaponTypes::None) {
				return 1000;
			}
			else if (target->isCompleted() && targetType == BWAPI::UnitTypes::Terran_Bunker) {
				return 1000;
			}
		}
	}

	// Exception: Workers are not threats after all.
	if (target->getType().isWorker())
	{
		isThreat = false;
	}
	else if (targetType == BWAPI::UnitTypes::Protoss_Reaver || //extremely dangerous units! if they're in range, attack them!
		targetType == BWAPI::UnitTypes::Zerg_Infested_Terran ||
		targetType == BWAPI::UnitTypes::Zerg_Defiler ||
		targetType == BWAPI::UnitTypes::Protoss_High_Templar ||
		targetType == BWAPI::UnitTypes::Protoss_Shuttle ||
		targetType == BWAPI::UnitTypes::Zerg_Scourge ||
		targetType == BWAPI::UnitTypes::Zerg_Guardian ||
		targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
		targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode ||
		targetType == BWAPI::UnitTypes::Terran_Ghost ||
		targetType == BWAPI::UnitTypes::Protoss_Arbiter) {
		return 1000;
	}

	if (targetType == BWAPI::UnitTypes::Protoss_Carrier) {
		if (target->getShields() + target->getHitPoints() <= 60) { //it's almost dead!
			return 1000;
		}
		else {
			return 6;
		}
	}

	if (isThreat) {
		//in some cases we might prefer ignoring concussive dmg units as buildings,
		//but this could be fatal for workers/zerglings if we did
		double LTD = UnitUtil::CalculateLTD(target, unit);
		if (target->isStimmed()) LTD *= 2; //doubled for stimmed units

		//not technically correct as the unit might have shields, but dealing with shields precisely
		//is troublesome

		//we presume LTD ranges from ~0 to 2, hp from 0 to 400. We want numbers between 11 and 999
		//hence divide hp by about 400 and multiply LTD by at least 10 so we get the decimals
		int bonus = target->getType() == BWAPI::UnitTypes::Terran_Marine ? 15 : 1; //approximate medic healing
		double LTDperHP = LTD / (bonus + target->getHitPoints() + target->getShields() - targeted[target]);
		if (LTDperHP < 0) return 0; //looks like it's already set to die
		int value = std::min(int(11 + (20.0*400.0)*LTDperHP), 999);
		return value;
	}

	// Carriers and spell casters are as important as key buildings.
	// Also remember to target non-threat combat units.
	if (targetType.isSpellcaster() ||
		targetType.groundWeapon() != BWAPI::WeaponTypes::None ||
		targetType.airWeapon() != BWAPI::WeaponTypes::None
		)
	{
		return 6;
	}

	// Mildly useful for sunken/cannon rushes
	if (targetType == BWAPI::UnitTypes::Protoss_Pylon) {
		return 5;
	}
	// Downgrade unfinished/unpowered/on-fire buildings
	bool onFire = targetType.getRace() == BWAPI::Races::Terran && target->getHitPoints() < targetType.maxHitPoints() / 3;
	if (targetType.isBuilding() &&
		(!target->isCompleted() || !target->isPowered() || onFire))
	{
		return 2;
	}
	//anything that costs money
	if (targetType.gasPrice() + targetType.mineralPrice() > 0)
	{
		return 3;
	}
	// Finally everything else.
	return 1;
}


