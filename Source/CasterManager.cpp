#include "CasterManager.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

CasterManager::CasterManager() 
{ 
	swarmCastTime = -100;
}

void CasterManager::executeMicro(const BWAPI::Unitset & targets)
{
	const BWAPI::Unitset & units = getUnits();

	if (units.empty())
	{
		return;
	}

	if (BWAPI::Broodwar->getFrameCount() % 5*20 == 0) broodlingTargeted.clear(); //clear every 5 seconds or so

	BWAPI::Unitset queens;
	BWAPI::Unitset infested;
	BWAPI::Unitset defilers;
	
	auto base = InformationManager::Instance().getMyMainBaseLocation();
	if (base) {
		mainBasePosition = InformationManager::Instance().getMyMainBaseLocation()->getPosition();
	}
	else {
		mainBasePosition = BWAPI::Position(0, 0);
	}

	for (BWAPI::Unit unit : units) {
		if (!unit || !(unit->exists())) {
			UAB_ASSERT(false, "Caster Manager.cpp : Error with invalid unit");
		}
		else if (unit->getType() == BWAPI::UnitTypes::Zerg_Queen) {
			queens.insert(unit);
		}
		else if (unit->getType() == BWAPI::UnitTypes::Zerg_Infested_Terran) {
			infested.insert(unit);
		}
		else if (unit->getType() == BWAPI::UnitTypes::Zerg_Defiler) {
			defilers.insert(unit);
			//BWAPI::Broodwar->printf("Unsupported unit type [Defiler] detected in caster manager");
		}
		else {
			BWAPI::Broodwar->printf("Unsupported unit type %s detected in caster manager", unit->getType().c_str());
		}
	}

	if (!(queens.empty())) assignQueenTargets(queens, targets);
	if (!(defilers.empty())) assignDefilerTargets(defilers, targets);
	if (!(infested.empty())) assignInfestedTargets(infested, targets);
}


/* Orders the unit to move to the target destination cautiously.*/ 
void CasterManager::cautiouslyTravel(BWAPI::Unit unit, BWAPI::Position destination) {
	//Micro::SmartMove(unit, destination);
	//return;

	if (!(destination.isValid())) {
		//UAB_ASSERT(false, "Destination invalid in cautiouslyTravel CasterManager");
		return;
	}
	
	/* We're very close to the SpecOps position! Rush there! */
	if (unit->getDistance(destination) < 8 * 32 && order.getType() == SquadOrderTypes::SpecOps) {
		Micro::SmartMove(unit, destination);
		return;
	}
	
	BWAPI::Unitset nearbyEnemies = unit->getUnitsInRadius(10 * 32, BWAPI::Filter::IsEnemy);
	
	for (auto & enemy : nearbyEnemies) {
		if (!enemy || !enemy->exists()) continue; //just in case

		BWAPI::UnitType type = enemy->getType();
		bool isThreat = unit->isFlying() ? type.groundWeapon() != BWAPI::UnitTypes::None : type.airWeapon() != BWAPI::UnitTypes::None;
		isThreat = isThreat || type == BWAPI::UnitTypes::Terran_Bunker;

		if (!isThreat) continue;

		if (unit->getDistance(enemy) < 8 * 32) {
			if (type.topSpeed() > unit->getType().topSpeed()) { //emergency!
				if (runToAlly(unit, enemy)) return; //run to nearest ally!
				if (runToBase(unit)) return;
			}

			//failing that, just run! 
			BWAPI::Position fleePosition(unit->getPosition() + Micro::GetKiteVector(enemy, unit));
			if (fleePosition.isValid()) {
				Micro::SmartMove(unit, fleePosition);
			}
			else {
				if (runToBase(unit)) return;
			}
		}
		else if (enemy->getDistance(destination) < unit->getDistance(destination)) {
			/*
			//circle around it!
			//find the two points on either side of the unit, determine which one is closer. 
			BWAPI::Position clockwisePosition = enemy->getPosition();
			BWAPI::Position counterclockwisePosition = enemy->getPosition();

			if (unit->getDistance(clockwisePosition) < unit->getDistance(counterclockwisePositive)) {
				//go clockwise
			}
			else {
				//go counterclockwise
			}
			*/
		}
		break; //currently, only consider the first unit we find
	}

	Micro::SmartMove(unit, destination); 
}

/* Run to any nearby ally that can fight the attacker. */
bool CasterManager::runToAlly(BWAPI::Unit unit, BWAPI::Unit attacker) {
	if (!unit || !attacker) {
		UAB_ASSERT(false, "no unit/attacker in runToAlly funct");
		return false; //huh?!
	}

	BWAPI::Unitset nearbyAllies = unit->getUnitsInRadius(8 * 32, BWAPI::Filter::IsAlly);
	if (nearbyAllies.empty()) return false;
	bool air = attacker->getType().isFlyer();

	for (auto & ally : nearbyAllies) {
		if ((!air && ally->getType().groundWeapon() != BWAPI::WeaponTypes::None) || 
			(air && ally->getType().airWeapon() != BWAPI::WeaponTypes::None)) {
			Micro::SmartMove(unit, ally->getPosition() + Micro::GetKiteVector(attacker, unit));
			return true;
		}
	}

	return false;
}

/* Run to the main base.
 * FUTURE: Run to the nearest allied base with defenses? */
bool CasterManager::runToBase(BWAPI::Unit unit) {
	if (!mainBasePosition) return false;
	Micro::SmartMove(unit, mainBasePosition);
	return true; //hopefully we always have a main base
}

/* Queen Section ---------------------------------------------- */
/* Returns the priority for a queen to target something with Broodlings */
int CasterManager::getBroodlingPriority(BWAPI::Unit unit, BWAPI::Unit target) {
	if (broodlingTargeted.contains(target)) return -2; //it's as good as dead
	if (unit->getDistance(target) > 15 * 32) return -2; //too far

	BWAPI::UnitType targetType = target->getType();

	bool isThreat = (targetType.airWeapon() != BWAPI::WeaponTypes::None) || (targetType == BWAPI::UnitTypes::Terran_Bunker);
	if (unit->getEnergy() < 150 || !canBroodling(target)) { // not enough energy/irrelevant target
		//but pay attention to nearby threats
		if (!isThreat && targetType.isBuilding()) return -3; //ignore nonthreat buildings completely
		if (!isThreat) return -1;

		return 0; 
	}

	if (targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
		targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode ||
		targetType == BWAPI::UnitTypes::Zerg_Ultralisk ||
		targetType == BWAPI::UnitTypes::Protoss_High_Templar) return 100;

	///don't cast broodlings on other target types unless we have energy
	int value = targetType.mineralPrice() + targetType.gasPrice();
	if (unit->getEnergy() >= 180 && value >= 100) return (value / 10);
	if (unit->getEnergy() >= 200) return 9;

	//pay attention to threats
	if (!isThreat && targetType.isBuilding()) return -3; //ignore nonthreat buildings completely
	if (!isThreat) return -1;

	return 0;
}

bool CasterManager::canBroodling(BWAPI::Unit target) {
	BWAPI::UnitType type = target->getType();
	if (type.isBuilding() ||
		type.isFlyer() ||
		target->isStasised() ||
		target->isInvincible() ||
		type == BWAPI::UnitTypes::Protoss_Archon ||
		type == BWAPI::UnitTypes::Protoss_Reaver ||
		type == BWAPI::UnitTypes::Protoss_Dark_Archon ||
		type == BWAPI::UnitTypes::Protoss_Probe) return false;

	return true;
}

void CasterManager::assignQueenTargets(const BWAPI::Unitset & units, const BWAPI::Unitset & targets) {
	for (auto & unit : units)
	{
		if (!unit) {
			UAB_ASSERT(false, "queen in CasterManager does not exist");
			continue;
		}
		int timeElapsed = BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame();
		bool timePast = timeElapsed > 2 * 20;
		if (unit->getOrder() == BWAPI::Orders::CastSpawnBroodlings &&
			!timePast) {
			continue;
		}
		/*if (unit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Use_Tech &&
			timePast) {
			BWAPI::Broodwar->printf("Using tech already");
			continue;
		}*/
		if ((unit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Use_Tech_Unit ||
			unit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Use_Tech ||
			unit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Use_Tech_Position) &&
			!timePast) {
			continue;
		}

		/*----------- find target*/
		BWAPI::Unit target = NULL;
		int highPriority = -2;
		if (!(targets.empty())) {
			double closestDist = std::numeric_limits<double>::infinity();
			BWAPI::Unit closestTarget = nullptr;

			// for each target possiblity
			for (auto & t: targets)
			{
				if (!t || !(t->exists())) {
					continue;
				}
				int priority = getBroodlingPriority(unit, t); //one day we will add ensnare priority, maybe
				int distance = unit->getDistance(t);

				if (t->getType() == BWAPI::UnitTypes::Terran_Command_Center && t->getHitPoints() <= 750) {
					priority = 20;
				}

				// if it's a higher priority, or it's closer, set it
				if (!closestTarget || (priority > highPriority) || (priority == highPriority && distance < closestDist))
				{
					closestDist = distance;
					highPriority = priority;
					closestTarget = t;
				}
			}
			target = closestTarget;
		}
		/*----------------*/

		if (target && target->exists()) {
			BWAPI::UnitType targetType = target->getType();

			int distance = unit->getDistance(target);

			if (highPriority == 20 && target->getType() == BWAPI::UnitTypes::Terran_Command_Center) { //infest it!
				Micro::SmartRightClick(unit, target);
				specialPriorities[target] = 0; //allies should stop attacking it; remains high priority for the queen
			}
			else if (highPriority > 0 && _self->hasResearched(BWAPI::TechTypes::Spawn_Broodlings)) { //we should cast broodlings on it!
				if (distance >= 9 * 32 - 16) {
					Micro::SmartMove(unit, target->getPosition());
				}
				else {
					broodlingTargeted.insert(target); 
					unit->useTech(BWAPI::TechTypes::Spawn_Broodlings, target);
				}
			}
			else if (distance <= 12*32) { //run away
				Micro::SmartMove(unit, unit->getPosition() + Micro::GetKiteVector(target, unit));
			}

			//same as lower half
			else {
				BWAPI::Position movePosition = mainBasePosition;
				if (unitClosestToEnemy) {
					movePosition = unitClosestToEnemy->getPosition();
				}



				if (unit->getDistance(movePosition) > 124) //get over there!
				{
					Micro::SmartMove(unit, movePosition);
				}
				else { //keep moving!
					int mag = 64;
					double theta = (rand() % 360) * 3.14 / 180;
					BWAPI::Position randomOffset(int(mag*cos(theta)), int(mag*sin(theta)));
					Micro::SmartMove(unit, unit->getPosition() + randomOffset);
				}
			}

		}
		else {
			BWAPI::Position movePosition = mainBasePosition;
			if (unitClosestToEnemy) {
				movePosition = unitClosestToEnemy->getPosition();
			}

			if (unit->getDistance(movePosition) > 124) //get over there!
			{
				Micro::SmartMove(unit, movePosition);
			}
			else { //keep moving!
				int mag = 64;
				double theta = (rand() % 360) * 3.14 / 180;
				BWAPI::Position randomOffset(int(mag*cos(theta)), int(mag*sin(theta)));
				Micro::SmartMove(unit, unit->getPosition() + randomOffset);
			}
		}

		if (Config::Debug::DrawUnitTargetInfo)
		{
			BWAPI::Broodwar->drawLineMap(unit->getPosition().x, unit->getPosition().y,
				unit->getTargetPosition().x, unit->getTargetPosition().y,
				Config::Debug::ColorLineTarget);
		}
	}
}
/* End of Queen Section -------------------------------------- */


/* Returns the priority for an infested to attack a unit */
int CasterManager::computeExplosionDamage(BWAPI::Position p) {
	//get all units in the splash radius
	BWAPI::Unitset units = BWAPI::Broodwar->getUnitsInRadius(p, 60, !BWAPI::Filter::IsFlying);
	//subtract the cost of the infested
	int dmg = -infestedValue;
	for (auto & u : units) {
		if (!u || !(u->exists()) || u->isInvincible() /*|| u->isHallucination()*/) continue;
		//it seems we can't necessarily know if something is a hallucination

		if (u->getType() == BWAPI::UnitTypes::Zerg_Infested_Terran &&
			u->getPlayer() == _self) continue; //bro, get outta the way, brooo! 

		int value = u->getType().mineralPrice() + u->getType().gasPrice();
		if (u->getType().isWorker() && u->isGatheringMinerals()) value += 25;

		int distance = u->getDistance(p);
		int damageReceived = distance <= 20 ? 500 : distance <= 40 ? 250 : 125;
		bool isSmall = u->getType().size() == BWAPI::UnitSizeTypes::Small;
		bool isMedium = u->getType().size() == BWAPI::UnitSizeTypes::Medium;
		
		//assume unit has an armor of 2 (although, nearly negligible...)
		damageReceived -= 2;

		//compute shield damage
		int shieldDamage = std::min(damageReceived, u->getShields());

		//compute hp dmg 
		damageReceived -= shieldDamage;
		damageReceived = isSmall ? damageReceived / 2 : //scale by size 
			isMedium ? damageReceived * 3 / 4 :
			damageReceived;

		//add back in the shield damage
		damageReceived += shieldDamage;

		//if the unit won't die, consider its hp as a percentage of its value
		double valueScale = 1 - (u->getHitPoints() + u->getShields() - damageReceived) / (u->getType().maxHitPoints() + u->getType().maxShields());
		if (valueScale > 1) valueScale = 1; //the unit dies

		if (u->getPlayer() == _self) {
			dmg -= int(valueScale*value);
		}
		else { //presume unit belongs to enemy
			dmg += int(valueScale*value);
			if (u->getType() == BWAPI::UnitTypes::Terran_Bunker && u->getHitPoints() > 100) {
				dmg += 200; //there may be 4 marines inside, worth exposing them
			}
		}
	}
	return dmg;
}

int CasterManager::getExplosionPriority(BWAPI::Unit unit, BWAPI::Unit target) {
	if ((target->getType().isBuilding() && target->getHitPoints() > 500) || 
		target->getType().isAddon() ||
		target->isFlying()) return -1;

	int distance = unit->getDistance(target);

	//infested should not care about regrouping
	if (distance > 10 * 32) return 1; 

	//should really compute the position in front of the target relative to the unit,
	//but this is ok. the infested computes again whether it's worth it to explode
	//when it gets into position.
	//temporary stand-in
	BWAPI::Position contactPoint = target->getPosition();

	if (distance > 10 * 32) {
		return computeExplosionDamage(contactPoint) - 300;
	}
	else {
		return computeExplosionDamage(contactPoint);
	}
}

void CasterManager::assignInfestedTargets(const BWAPI::Unitset & units, const BWAPI::Unitset & targets) {
	for (auto & unit : units)
	{
		if (!unit) {
			UAB_ASSERT(false, "infested terran in CasterManager does not exist");
			continue;
		}

		/*----------- find target*/
		BWAPI::Unit target = NULL;
		int highPriority = 0;
		if (!(targets.empty())) {
			double closestDist = std::numeric_limits<double>::infinity();
			BWAPI::Unit closestTarget = nullptr;

			// for each target possiblity
			for (auto & t : targets)
			{
				if (!t || !(t->exists())) {
					continue;
				}
				int priority = getExplosionPriority(unit, t);
				int distance = unit->getDistance(t);

				// if it's a higher priority, or it's closer, set it
				if (!closestTarget || (priority > highPriority) || (priority == highPriority && distance < closestDist))
				{
					closestDist = distance;
					highPriority = priority;
					closestTarget = t;
				}
			}
			target = closestTarget;
		}
		/*----------------*/

		if (target && target->exists()) {
			BWAPI::UnitType targetType = target->getType();

			int distance = unit->getDistance(target);

			int explosionValue = computeExplosionDamage(unit->getPosition());
			if (explosionValue >= 300 && unit->getHitPoints() < 35) { //we hit the jackpot, BLOW UP NOW!!!
				Micro::SmartAttackMove(unit, unit->getPosition());
			}
			else if (unit->getHitPoints() <= 20 && explosionValue >= -infestedValue) { //critical HP!
				Micro::SmartAttackMove(unit, unit->getPosition());
			}
			else if (distance >= 80) { //too far!
				Micro::SmartMove(unit, Micro::GetChasePosition(unit, target));
			}
			else { //only explode if cost effective to
				if (explosionValue >= 0 && distance <= 2*32) {
					Micro::SmartAttackUnit(unit, target);
				}
				else {
					Micro::SmartMove(unit, target->getPosition());
				}
			}
		}
		else if (unit->getDistance(order.getPosition()) > 96) //get over there!
		{
			Micro::SmartMove(unit, order.getPosition());
		}

		if (Config::Debug::DrawUnitTargetInfo)
		{
			BWAPI::Broodwar->drawLineMap(unit->getPosition().x, unit->getPosition().y,
				unit->getTargetPosition().x, unit->getTargetPosition().y,
				Config::Debug::ColorLineTarget);
		}
	}
}

/* ---------------------------------------------------- */
/* Defiler Section */

/* Control function for defilers. Not completed. */
void CasterManager::assignDefilerTargets(const BWAPI::Unitset & units, const BWAPI::Unitset & targets) {
	//remove already dead units from the consumed
	/*for (auto pair : consuming) {
		auto & consumed = pair.second;
		for (auto c : consumed) {
			if (!c || !c->exists() || c->getHitPoints() < 0) {
				consumed.erase(c);
			}
		}
	}*/

	//heuristic: don't cast swarm if the enemy has mainly units that can fight in swarm
	int count = 0;
	for (auto target : targets) {
		if (UnitUtil::CanAttackInSwarm(target)) {
			count--;
		}
		else {
			if (UnitUtil::CanAttackGround(target)) count += 2;
		}
	}
	bool swarmUseless = count < 6;

	//assign defiler targets
	for (auto unit : units) {
		if (!unit || !unit->exists()) continue;
		if (unit->getType() != BWAPI::UnitTypes::Zerg_Defiler) {
			UAB_ASSERT(false, "Non-defiler unit found in defiler unitset");
			continue;
		}

		auto currentAction = unit->getOrder();
		if (currentAction == BWAPI::Orders::CastDarkSwarm ||
			currentAction == BWAPI::Orders::CastPlague) continue; //it's busy; make sure it finishes its job
		//if (unit->getOrder() == BWAPI::Orders::CastConsume ||)

		//global value to prevent swarms from being cast on the same location constantly
		bool swarmCast = BWAPI::Broodwar->getFrameCount() - swarmCastTime < 8;

		if (Micro::CheckSpecialCases(unit)) continue;


		//decide where to move, what action to take
		
		if (unit->getEnergy() < 147) {
			if (consume(unit)) continue;
		}

		bool plague = false;
		if (unit->getEnergy() >= 150 && _self->hasResearched(BWAPI::TechTypes::Plague)) {
			plague = true;
		}

		bool swarm = false;
		if (!swarmUseless && !swarmCast && unit->getEnergy() >= 100) {
			swarm = true;
		}

		if (swarm || plague) {
			BWAPI::TechType tech = BWAPI::TechTypes::Dark_Swarm;
			BWAPI::Position pos(-1, -1);
			bool cast = false;

			//self-defense casts
			if (unit->isUnderAttack() && !unit->isUnderDarkSwarm() && swarm && computeSwarmValue(unit->getPosition()) >= 150) {
				cast = true;
				pos = unit->getPosition();
			}
			else {
				int highestValue = 0;

				const int plague_mode = 1;
				const int swarm_mode = 2;
				int mode = -1;

				BWAPI::Position bestPos(-1, -1);

				if (plague) {
					for (auto target : targets) {
						if (!target || !target->exists()) continue;
						if (target->getDistance(unit) > 13 * 32) continue;

						int value = computePlagueDamage(target->getPosition());

						if (value > highestValue) {
							mode = plague_mode;
							highestValue = value;
							bestPos = target->getPosition();
						}

					}
				}

				if (swarm) {
					auto allies = unit->getUnitsInRadius(16 * 32, BWAPI::Filter::IsAlly);
					bool fighting = false;
					for (auto target : allies) {
						if (!target || !target->exists()) continue;

						if (target->isUnderAttack() || target->isAttacking()) {
							fighting = true;
							break;
						} 
					}
					//at least one ally must be fighting

					if (fighting) {
						for (auto target : allies) {
							if (!target || !target->exists()) continue;
							int value = computeSwarmValue(target->getPosition());

							if (value > highestValue) {
								mode = swarm_mode;
								highestValue = value;
								bestPos = target->getPosition();
							}
						}
					}
				}

				BWAPI::Broodwar->drawTextMap(unit->getPosition(), "%d", highestValue);
				if (highestValue >= 300) {
					cast = true;
					pos = bestPos;
					tech = (mode == plague_mode) ? BWAPI::TechTypes::Plague :
						(mode == swarm_mode) ? BWAPI::TechTypes::Dark_Swarm :
						BWAPI::TechTypes::None;

					if (tech == BWAPI::TechTypes::None) {
						UAB_ASSERT(false, "Error in defiler tech type selection in CasterManager.cpp");
					}
				}
			}

			if (cast) {
				if (unit->getDistance(pos) > (9 * 32)-8) {
					Micro::SmartMove(unit, pos);
				}
				else {
					if (tech == BWAPI::TechTypes::Dark_Swarm) {
						swarmCastTime = BWAPI::Broodwar->getFrameCount();
					}
					Micro::SmartUseTech(unit, tech, pos);
				}
				continue;
			}
		}

		//we are not casting or consuming, so we're following an ally
		//get the kite vector for defilers. short-ranged, since they could block other units.
		initializeVectors();
		for (auto target : targets) {
			if (!target || !target->exists()) continue;
			updateVectors(unit, target, 1);
		}
		normalizeVectors();

		BWAPI::Position movePosition = mainBasePosition;
		if (unitClosestToEnemy && unitClosestToEnemy->exists()) {
			if (unitClosestToEnemy->getDistance(order.getPosition()) < unit->getDistance(order.getPosition())) {
				movePosition = unitClosestToEnemy->getPosition();
			}
			else {
				movePosition = unit->getPosition() + BWAPI::Position(32 * (rand() % 1), 32 * (rand() % 1));
			}
		}
		Micro::SmartTravel(unit, movePosition, kiteVector, true);

	}
}

//consumes nearby zerglings
bool CasterManager::consume(BWAPI::Unit unit) {
	if (!_self->hasResearched(BWAPI::TechTypes::Consume)) return false; //can't cast it

	/*if (consuming[unit].size() > 0) {
		auto u = *consuming[unit].begin();

		Micro::SmartUseTech(unit, BWAPI::TechTypes::Consume, u);
		return true;
	}*/

	BWAPI::Unitset units = BWAPI::Broodwar->getUnitsInRadius(unit->getPosition(), 2 * 32, BWAPI::Filter::IsAlly && 
		(BWAPI::Filter::GetType == BWAPI::UnitTypes::Zerg_Zergling) &&
		BWAPI::Filter::Exists);
	for (auto u : units) {
		if (unit->canUseTechUnit(BWAPI::TechTypes::Consume, u)) {
			u->move(unit->getPosition()); //make the unit move towards us
			Micro::SmartUseTech(unit, BWAPI::TechTypes::Consume, u);
			//consuming[unit].insert(u);
			return true;
		}
	}

	return false;
}

/* Returns the plague economic damage around the position p */
int CasterManager::computePlagueDamage(BWAPI::Position p) {
	//get all units in the splash radius
	BWAPI::Position offset(2*32, 2*32);
	BWAPI::Unitset units = BWAPI::Broodwar->getUnitsInRectangle(p - offset, p + offset, 
		!BWAPI::Filter::IsInvincible && BWAPI::Filter::Exists &&
		!BWAPI::Filter::IsPlagued && !(BWAPI::Filter::IsOrganic && BWAPI::Filter::IsIrradiated) &&
		!BWAPI::Filter::IsWorker); 

	int dmg = -75; //subtract the cost of 3 zerglings
	for (auto & u : units) {
		int value = u->getType().mineralPrice() + u->getType().gasPrice();
		int damageInflicted = std::min(300, u->getHitPoints() - 1);

		//consider its hp as a percentage of its value, factoring in shields. 
		double valueScale = double(damageInflicted) / (u->getType().maxHitPoints() + u->getType().maxShields());

		if (u->getPlayer() == _self) {
			dmg -= int(valueScale*value);
		}
		else { //presume unit belongs to enemy
			dmg += int(valueScale*value);
			if (u->getType() == BWAPI::UnitTypes::Terran_Bunker && u->getHitPoints() > 100) {
				dmg += 200; //assume 4 marines inside
			}
		}
	}
	return dmg;
}

int CasterManager::computeSwarmValue(BWAPI::Position p) {
	//get all units in the splash radius
	BWAPI::Position offset(3 * 32, 3 * 32);
	BWAPI::Unitset units = BWAPI::Broodwar->getUnitsInRectangle(p - offset, p + offset, !BWAPI::Filter::IsFlying &&
		!BWAPI::Filter::IsInvincible && BWAPI::Filter::Exists && 
		!BWAPI::Filter::IsPlagued && !(BWAPI::Filter::IsOrganic && BWAPI::Filter::IsIrradiated) &&
		!BWAPI::Filter::IsUnderDarkSwarm);

	//note that buildings are still attackable by ranged units in dark swarms

	//note: not actually "dmg" but swarm value
	int dmg = -50; //subtract the cost of 2 zerglings
	int swarmStrength = 0;
	int presumptiveDamage = 0;

	int averageRange = 0;
	int nRanged = 0;

	for (auto & u : units) {
		int value = u->getType().mineralPrice() + u->getType().gasPrice();
		if (u->getPlayer() == _self) {
			if (!u->getType().isBuilding()) {
				dmg += value;
				if (u->getType().isWorker() && u->isGatheringMinerals()) dmg += 25;

				if (UnitUtil::CanAttackInSwarm(u)) {
					swarmStrength += value;
				}
				
				//get its attack range relative to itself. sort of hacky.
				//considers lurkers
				int range = UnitUtil::AttackRange(u, u);
				if (range >= 4*32) {
					averageRange += UnitUtil::AttackRange(u, u);
					nRanged++;
				}
			}
		}
		else { //presume unit belongs to enemy
			if (!u->isUnderDarkSwarm()) {
				if (u->getType() == BWAPI::UnitTypes::Terran_Bunker) {
					value += 200;
				}
				if (!u->getType().isBuilding()) dmg -= value;
				presumptiveDamage += 2 * value;
				if (UnitUtil::CanAttackInSwarm(u)) swarmStrength -= value;
			}
		}
	}

	//if we have sufficiently more strength than them,
	//assume every enemy unit in the cloud dies
	if (swarmStrength < 0) {
		//we're weaker than them! it would be a bad idea to cast in this situation!
		if (dmg > 0) dmg = -dmg;
	}
	else {
		if (swarmStrength > 200) {
			dmg += presumptiveDamage;
		}
		
		if (nRanged >= 6) {
			averageRange /= nRanged;
			if (averageRange >= 4 * 32) {
				int searchRadius = std::min(8 * 32, 2 * 32 + averageRange);
				BWAPI::Unitset enemies = BWAPI::Broodwar->getUnitsInRadius(p, searchRadius, BWAPI::Filter::IsEnemy
					&& BWAPI::Filter::GroundWeapon != BWAPI::WeaponTypes::None &&
					(BWAPI::Filter::IsBuilding || !BWAPI::Filter::IsUnderDarkSwarm));

				for (auto u : enemies) {
					if (!u->isFlying() && u->getDistance(p) < 4 * 32) continue;

					if (UnitUtil::CanAttackInSwarm(u)) continue;
					if (!UnitUtil::CanAttackGround(u)) continue;

					dmg += u->getType().mineralPrice() + u->getType().gasPrice();
				}
			}
		}
	}

	return dmg;
}