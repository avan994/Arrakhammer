#include "UnitUtil.h"

using namespace UAlbertaBot;

// Building morphed from another, not constructed.
bool UnitUtil::IsMorphedBuildingType(BWAPI::UnitType type)
{
	return
		type == BWAPI::UnitTypes::Zerg_Sunken_Colony ||
		type == BWAPI::UnitTypes::Zerg_Spore_Colony ||
		type == BWAPI::UnitTypes::Zerg_Lair ||
		type == BWAPI::UnitTypes::Zerg_Hive ||
		type == BWAPI::UnitTypes::Zerg_Greater_Spire;
}

// This is a combat unit for purposes of combat simulation.
// The combat simulation does not support spellcasters other than medics.
// It does not support bunkers, reavers, or carriers--but we fake bunkers.
bool UnitUtil::IsCombatSimUnit(BWAPI::UnitType type)
{
	if (type.isWorker())
	{
		return false;
	}

	return
		type.canAttack() ||            // includes static defense except bunkers, excludes spellcasters
		type == BWAPI::UnitTypes::Terran_Medic ||
		type.isDetector();
}

bool UnitUtil::IsCombatUnit(BWAPI::Unit unit)
{
    UAB_ASSERT(unit != nullptr, "Unit was null");
    if (!unit)
    {
        return false;
    }

	auto type = unit->getType();

	// No workers, buildings, or carrier interceptors (which are not controllable).
	// Buildings include static defense buildings; they are not put into squads.
	if (type.isWorker() ||
		(type.isBuilding() && !type.canAttack()) ||
		type == BWAPI::UnitTypes::Protoss_Interceptor)  // interceptors canAttack()
	{
		return false;
	}

	if (type.canAttack() ||                             // includes carriers and reavers
		type.isDetector() ||
		type.isSpellcaster() ||
		(type.isFlyer() && type.spaceProvided() > 0))     // transports
	{
		return true;
	}

	return false;
}

bool UnitUtil::IsValidUnit(BWAPI::Unit unit)
{
	return unit
		&& unit->exists()
		&& (unit->isCompleted() || IsMorphedBuildingType(unit->getType()))
		&& unit->getHitPoints() > 0
		&& unit->getType() != BWAPI::UnitTypes::Unknown
		&& unit->getPosition().isValid();
}

Rect UnitUtil::GetRect(BWAPI::Unit unit)
{
	Rect r;

	r.x = unit->getLeft();
	r.y = unit->getTop();
	r.height = unit->getBottom() - unit->getTop();
	r.width = unit->getLeft() - unit->getRight();

	return r;
}

double UnitUtil::GetDistanceBetweenTwoRectangles(Rect & rect1, Rect & rect2)
{
	Rect & mostLeft = rect1.x < rect2.x ? rect1 : rect2;
	Rect & mostRight = rect2.x < rect1.x ? rect1 : rect2;
	Rect & upper = rect1.y < rect2.y ? rect1 : rect2;
	Rect & lower = rect2.y < rect1.y ? rect1 : rect2;

	int diffX = std::max(0, mostLeft.x == mostRight.x ? 0 : mostRight.x - (mostLeft.x + mostLeft.width));
	int diffY = std::max(0, upper.y == lower.y ? 0 : lower.y - (upper.y + upper.height));

	return std::sqrtf(static_cast<float>(diffX*diffX + diffY*diffY));
}

bool UnitUtil::CanAttack(BWAPI::Unit attacker, BWAPI::Unit target)
{
	return GetWeapon(attacker, target) != BWAPI::WeaponTypes::None ||
		attacker->getType() == BWAPI::UnitTypes::Terran_Bunker;
}

bool UnitUtil::CanAttackAir(BWAPI::Unit unit)
{
	auto type = unit->getType();
	return TypeCanAttackAir(type);
}

bool UnitUtil::TypeCanAttackAir(BWAPI::UnitType type) {
	return type.airWeapon() != BWAPI::WeaponTypes::None ||
		type == BWAPI::UnitTypes::Terran_Bunker ||
		type == BWAPI::UnitTypes::Protoss_Carrier;
}

bool UnitUtil::CanAttackGround(BWAPI::Unit unit)
{
	auto type = unit->getType();
	return TypeCanAttackGround(type);
}

bool UnitUtil::TypeCanAttackGround(BWAPI::UnitType type)
{
	return type.groundWeapon() != BWAPI::WeaponTypes::None ||
		type == BWAPI::UnitTypes::Terran_Bunker ||
		type == BWAPI::UnitTypes::Protoss_Carrier ||
		type == BWAPI::UnitTypes::Protoss_Reaver;
}

bool UnitUtil::CanEscape(BWAPI::Unit unit, BWAPI::Unit attacker) {
	if (!unit || !unit->exists() || !attacker || !attacker->exists()) {
		UAB_ASSERT(false, "Invalid arg to UnitUtil::CanEscape");
		return true;
	}

	auto uType = unit->getType();
	if ((unit->isIrradiated() && uType.isOrganic()) || 
		(unit->isPlagued() && unit->getShields() <= 20) ||
		uType == BWAPI::UnitTypes::Zerg_Broodling) { //fight to the very end!
		return false;
	}

	int dist = attacker->getDistance(unit);
	bool dist6 = dist <= 6 * 32;
	bool dist5 = dist <= 5 * 32;
	bool dist4 = dist <= 4 * 32;
	bool dist3 = dist <= 3 * 32;
	bool dist2 = dist <= 2 * 32;
	bool dist016 = dist <= 16;

	auto aType = attacker->getType();

	//could afford to replace this with something more sensible
	if (unit->getType() == BWAPI::UnitTypes::Zerg_Zergling ||
		unit->getType() == BWAPI::UnitTypes::Zerg_Ultralisk) {
		//should not run from these once we have started a fight. finish them off, or die trying!
		if (dist2) {
			if (aType == BWAPI::UnitTypes::Terran_Bunker ||
				aType == BWAPI::UnitTypes::Zerg_Sunken_Colony ||
				aType == BWAPI::UnitTypes::Protoss_Photon_Cannon) return false;

			if (UnitUtil::AttackRange(attacker, unit) >= 4 * 32) return false;
		}
	}
	else if (unit->getType() == BWAPI::UnitTypes::Zerg_Hydralisk &&
		aType == BWAPI::UnitTypes::Terran_Vulture &&
		dist6) {
		return false;
	}

	if (unit->isFlying()) { //includes flying buildings, too...
		if (aType == BWAPI::UnitTypes::Zerg_Scourge &&
			dist4) return false;
	}
	else { //ground units
		if ((!attacker->isBurrowed() && 
			(aType == BWAPI::UnitTypes::Zerg_Infested_Terran ||
			aType == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine) ||
			aType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode)
			&& dist6) return false;
	}

	return true;
}

double UnitUtil::CalculateLTD(BWAPI::Unit attacker, BWAPI::Unit target)
{
	BWAPI::WeaponType weapon = GetWeapon(attacker, target);

	if (weapon == BWAPI::WeaponTypes::None || weapon.damageCooldown() <= 0)
	{
		return 0;
	}

	return double(weapon.damageAmount()) / weapon.damageCooldown();
}

BWAPI::WeaponType UnitUtil::GetWeapon(BWAPI::Unit attacker, BWAPI::Unit target)
{
	auto atype = attacker->getType();
	if (atype == BWAPI::UnitTypes::Protoss_Carrier)
	{
		//note: not representative of the carrier's actual DPS
		atype = BWAPI::UnitTypes::Protoss_Interceptor;
	}
	else if (atype == BWAPI::UnitTypes::Protoss_Reaver) {
		atype = BWAPI::UnitTypes::Protoss_Scarab;
	}

	return target->isFlying() ? atype.airWeapon() : atype.groundWeapon();
}

BWAPI::WeaponType UnitUtil::GetWeapon(BWAPI::UnitType attacker, BWAPI::UnitType target)
{
	return target.isFlyer() ? attacker.airWeapon() : attacker.groundWeapon();
}

// Returns the range, accounting for player upgrades. 
int UnitUtil::AttackRange(BWAPI::Unit attacker, BWAPI::Unit target)
{
	if (!attacker || !target || !attacker->exists() || !target->exists()) {
		UAB_ASSERT(false, "Invalid arg to UnitUtil::AttackRange");
		return 0;
	}
	auto type = attacker->getType();

	if (type == BWAPI::UnitTypes::Protoss_Reaver) {
		if (!target->isFlying()) return 8*32;
		return 0;
	}
	if (type == BWAPI::UnitTypes::Protoss_Carrier) return 11*32; //they can release interceptors at 8, but once they're out, it's 11

	BWAPI::WeaponType weapon = GetWeapon(attacker, target);
	if (weapon == BWAPI::WeaponTypes::None) {

		if (type == BWAPI::UnitTypes::Terran_Bunker) { //assume marines are in it
			weapon = BWAPI::WeaponTypes::Gauss_Rifle;
		}
		else {
			return 0;
		}
	}

	return attacker->getPlayer()->weaponMaxRange(weapon);
}

bool UnitUtil::IsMelee(BWAPI::Unit attacker, BWAPI::Unit target) {
	if (UnitUtil::GetWeapon(attacker, target) == BWAPI::WeaponTypes::None) return false;
	if (UnitUtil::AttackRange(attacker, target) > 32) return false;
	return true;
}

int UnitUtil::GetAttackRangeAssumingUpgrades(BWAPI::UnitType attacker, BWAPI::UnitType target)
{
    BWAPI::WeaponType weapon = GetWeapon(attacker, target);

    if (weapon == BWAPI::WeaponTypes::None)
    {
        return 0;
    }

    int range = weapon.maxRange();

	// Assume that any upgrades are researched.
	if (attacker == BWAPI::UnitTypes::Protoss_Dragoon)
	{
		range = 6 * 32;
	}
	else if (attacker == BWAPI::UnitTypes::Terran_Marine)
	{
		range = 5 * 32;
	}
	else if (attacker == BWAPI::UnitTypes::Terran_Bunker)
	{
		range = 6 * 32;
	}
	else if (attacker == BWAPI::UnitTypes::Terran_Goliath && target.isFlyer())
	{
		range = 8 * 32;
	}
	else if (attacker == BWAPI::UnitTypes::Zerg_Hydralisk)
	{
		range = 5 * 32;
	}

	return range;
}

// All our units, whether completed or not.
size_t UnitUtil::GetAllUnitCount(BWAPI::UnitType type)
{
    size_t count = 0;
    for (const auto & unit : BWAPI::Broodwar->self()->getUnits())
    {
        // trivial case: unit which exists matches the type
        if (unit->getType() == type)
        {
            count++;
        }

        // case where a zerg egg contains the unit type
        if (unit->getType() == BWAPI::UnitTypes::Zerg_Egg && unit->getBuildType() == type)
        {
            count += type.isTwoUnitsInOneEgg() ? 2 : 1;
        }

        // case where a building has started constructing a unit but it doesn't yet have a unit associated with it
        if (unit->getRemainingTrainTime() > 0)
        {
            BWAPI::UnitType trainType = unit->getLastCommand().getUnitType();

            if (trainType == type && unit->getRemainingTrainTime() == trainType.buildTime())
            {
                count++;
            }
        }
    }

    return count;
}

// Only our completed units.
size_t UnitUtil::GetCompletedUnitCount(BWAPI::UnitType type)
{
	size_t count = 0;
	for (const auto & unit : BWAPI::Broodwar->self()->getUnits())
	{
		if (unit->getType() == type && unit->isCompleted())
		{
			count++;
		}
	}

	return count;
}

BWAPI::Unit UnitUtil::GetClosestUnitTypeToTarget(BWAPI::UnitType type, BWAPI::Position target)
{
	BWAPI::Unit closestUnit = nullptr;
	double closestDist = 100000;

	for (auto & unit : BWAPI::Broodwar->self()->getUnits())
	{
		if (unit->getType() == type)
		{
			double dist = unit->getDistance(target);
			if (!closestUnit || dist < closestDist)
			{
				closestUnit = unit;
				closestDist = dist;
			}
		}
	}

	return closestUnit;
}

/* Computes the damage dealt by unit to target, assuming no upgrades. */
int UnitUtil::GetDamage(BWAPI::Unit unit, BWAPI::Unit target) 
{
	if (!unit || !target || !target->exists()) {
		UAB_ASSERT(false, "Invalid arguments to UnitUtil::GetDamage");
		return 0;
	}

	BWAPI::WeaponType weapon = GetWeapon(unit, target);

	if (weapon == BWAPI::WeaponTypes::None || weapon.damageCooldown() <= 0)
	{
		return 0;
	}

	if (target->getShields() >= weapon.damageAmount()) {
		return int(weapon.damageAmount());
		//attacks do full damage to shields
	}
	else {
		int shields = target->getShields();

		double scale = 1;
		if (weapon.damageType() == BWAPI::DamageTypes::Explosive) {
			auto size = target->getType().size();
			if (size == BWAPI::UnitSizeTypes::Small) {
				scale = 0.5;
			}
			else if (size == BWAPI::UnitSizeTypes::Medium) {
				scale = 0.75;
			}
		}
		else if (weapon.damageType() == BWAPI::DamageTypes::Concussive) {
			auto size = target->getType().size();
			if (size == BWAPI::UnitSizeTypes::Medium) {
				scale = 0.5;
			}
			else if (size == BWAPI::UnitSizeTypes::Large) {
				scale = 0.25;
			}
		} //else assume normal damage type

		if (shields > 0) {
			return shields + int((weapon.damageAmount() - shields - target->getType().armor()) * scale);
		}
		else {
			return int((weapon.damageAmount() - target->getType().armor()) * scale);
		}
	}
}

//indicates whether target is a threat to unit
//Including dangerous units includes units that may be a threat to allies, as well
//Note, this function does not include carriers
bool UnitUtil::IsThreat(BWAPI::Unit unit, BWAPI::Unit target, bool includeDangerous) {
	if (!unit || !target || !unit->exists() || !target->exists()) {
		UAB_ASSERT(false, "Invalid arg to UnitUtil::isThreat");
		return false;
	}
	if (target->getPlayer() == unit->getPlayer()) {
		UAB_ASSERT(false, "Target in UnitUtil::isThreat has same owner as unit!");
		return false;
	}

	BWAPI::UnitType type = target->getType();

	if (type.isWorker()) {
		if (target->isAttacking() && !unit->isFlying()) return true;
		return false;
	}

	if (type == BWAPI::UnitTypes::Terran_Bunker ||
		(!unit->isFlying() && type == BWAPI::UnitTypes::Protoss_Reaver) ||
		type == BWAPI::UnitTypes::Protoss_Carrier) {
		return true;
	}

	if (includeDangerous) { //consider spellcasters and dangerous units even if they can't hit us
		//additional high threat targets
		if (type == BWAPI::UnitTypes::Protoss_Observer ||
			type == BWAPI::UnitTypes::Protoss_Shuttle ||
			type == BWAPI::UnitTypes::Protoss_High_Templar ||
			type == BWAPI::UnitTypes::Zerg_Defiler ||
			type == BWAPI::UnitTypes::Protoss_Reaver || 
			(target->getEnergy() >= 70 && //harder-to-kill spellcasters, but only with energy
			(type == BWAPI::UnitTypes::Protoss_Dark_Archon ||
			type == BWAPI::UnitTypes::Zerg_Queen ||
			type == BWAPI::UnitTypes::Terran_Science_Vessel)) ||
			//type == BWAPI::UnitTypes::Zerg_Zergling || //mutalisks should see these as threats
			type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
			type == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode ||
			type == BWAPI::UnitTypes::Zerg_Cocoon) {
			return true;
		}

		if ((unit->isCloaked() || unit->isBurrowed() || unit->getType() == BWAPI::UnitTypes::Zerg_Lurker) && type.isDetector()) return true;

		if (target->isAttacking()) return true; //anything hitting allies is a threat to us - missile turrets, corsairs
	}

	return unit->getType().isFlyer() ? type.airWeapon() != BWAPI::WeaponTypes::None
		: type.groundWeapon() != BWAPI::WeaponTypes::None;
}

//Checks if unit would need less than "slack" frames to face target
//If so, returns true for facing --- otherwise, unit is not facing.
bool UnitUtil::IsFacing(BWAPI::Unit unit, BWAPI::Unit target, int slack) {
	if (!unit || !target || !unit->exists() || !target->exists() || slack <= 0) {
		UAB_ASSERT(false, "Invalid input to UnitUtil::isFacing");
	}
	auto type = unit->getType();

	if (type.isBuilding()) return false;

	double turningRate = type.turnRadius() * (360 / 256); //convert to degrees
	if (turningRate <= 0) return false;

	auto pDiff = target->getPosition() - unit->getPosition();
	double angle = atan2(pDiff.y, pDiff.x);
	double facingAngle = unit->getAngle();

	double aDiff1 = (int(180 / M_PI * abs(angle - facingAngle)) % 360);
	double aDiff2 = (int(180 / M_PI * abs(2*M_PI + angle - facingAngle)) % 360);
	double aDiff = std::min(aDiff1, aDiff2);

	return (round(aDiff / turningRate) <= slack);
}

bool UnitUtil::CanAttackInSwarm(BWAPI::Unit unit) {
	auto type = unit->getType();
	if (type.isWorker()) return false; //not the best unit to be attacking with
	if (type == BWAPI::UnitTypes::Zerg_Lurker ||
		type == BWAPI::UnitTypes::Protoss_Archon ||
		type == BWAPI::UnitTypes::Protoss_Reaver) return true;

	auto weapon = type.groundWeapon();
	if (weapon == BWAPI::WeaponTypes::None) return false;
	if (weapon.maxRange() > 32) return false; //most melee unit ranges
	
	//disqualifies spells
	if (weapon.damageType() != BWAPI::DamageTypes::Concussive &&
		weapon.damageType() != BWAPI::DamageTypes::Explosive &&
		weapon.damageType() != BWAPI::DamageTypes::Normal) return false;

	return true;
}