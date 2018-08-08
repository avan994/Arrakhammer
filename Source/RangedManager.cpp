#include "RangedManager.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

RangedManager::RangedManager() 
{ 
	scoutedBaseRecently = false;
	lastTimeScouted = -1;
}

void RangedManager::executeMicro(const BWAPI::Unitset & targets) 
{
	if (formSquad(targets, 32 * 7, 32 * 8, 90, 32)){
		_formed = true;
	}
	else {
		_formed = false;
	}


	assignTargets(targets);
}

void RangedManager::assignTargets(const BWAPI::Unitset & targets)
{
    const BWAPI::Unitset & rangedUnits = getUnits();

	//every minute or so
	if (BWAPI::Broodwar->getFrameCount() - lastTimeScouted > 60 * 20 ||
		order.getType() != SquadOrderTypes::Attack) {
		scoutedBaseRecently = false;
		lastTimeScouted = BWAPI::Broodwar->getFrameCount();
	}

	for (auto enemy : specialPriorities) {
		if (!enemy.first || !enemy.first->exists()) continue;
		BWAPI::Broodwar->drawTextMap(enemy.first->getPosition(), "%d", enemy.second);

		/*if (enemy.first->getType() == BWAPI::UnitTypes::Protoss_Pylon) {
			BWAPI::Position pOffset(6 * 32, 3 * 32);
			BWAPI::Broodwar->drawBoxMap(enemy.first->getPosition() - pOffset, enemy.first->getPosition() + pOffset, BWAPI::Colors::Blue);
			if (enemy.second >= 12) BWAPI::Broodwar->drawCircleMap(enemy.first->getPosition(), 64, BWAPI::Colors::Blue);
		}*/

		if (enemy.first->getType() == BWAPI::UnitTypes::Terran_Command_Center) {
			BWAPI::Broodwar->printf("Detected Terran Command Center designated as special priority.");
		}
	}

	targeted.clear();

	// figure out targets
	BWAPI::Unitset rangedUnitTargets;
    std::copy_if(targets.begin(), targets.end(), std::inserter(rangedUnitTargets, rangedUnitTargets.end()),
		[](BWAPI::Unit u) {
		  return u && u->exists() && 
			u->isVisible() &&
			u->getType() != BWAPI::UnitTypes::Zerg_Larva &&
			u->getType() != BWAPI::UnitTypes::Zerg_Egg &&
			!(u->isStasised()) &&
			!(u->isUnderDarkSwarm());
	});

    for (auto & rangedUnit : rangedUnits)
	{
		if (!rangedUnit || !rangedUnit->isCompleted()) continue;
		//note that this does not currently support valkyries, corsairs, carriers, or reavers; 
		//which needed to be added in Squad.cpp as well

		initializeVectors();
		if (Micro::CheckSpecialCases(rangedUnit)) continue;

		// train scarabs or interceptors
		trainSubUnits(rangedUnit);

		BWAPI::Unit target = NULL;
		if (!(rangedUnitTargets.empty())) {
			target = getTarget(rangedUnit, rangedUnitTargets);
		}

		if (target && target->exists())
		{
			if (Config::Debug::DrawUnitTargetInfo)
			{
				BWAPI::Broodwar->drawLineMap(rangedUnit->getPosition(), rangedUnit->getTargetPosition(), BWAPI::Colors::Purple);
			}

			bool isMutalisk = rangedUnit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk;
			if (isMutalisk) {
				BWAPI::UnitType targetType = target->getType();
				//found something worth attacking, dangerous, or searched and found nothing
				bool isDangerous = targetType.airWeapon() != BWAPI::UnitTypes::None ||
					targetType.groundWeapon() != BWAPI::UnitTypes::None ||
					targetType.isSpellcaster() ||
					targetType == BWAPI::UnitTypes::Terran_Bunker ||
					targetType == BWAPI::UnitTypes::Protoss_Reaver;
				if (isDangerous ||
					targetType.isResourceDepot() ||
					targetType.isWorker() ||
					rangedUnit->getDistance(order.getPosition()) < 4*32) {
					scoutedBaseRecently = true;
					lastTimeScouted = BWAPI::Broodwar->getFrameCount();
				}
			}
			bool shouldScoutFirst = !scoutedBaseRecently &&
				rangedUnit->getDistance(order.getPosition()) < 20 * 32 &&
				order.getType() == SquadOrderTypes::Attack;

			//if (!(isMutalisk && shouldScoutFirst))

			// attack it
			if (!Config::Micro::KiteWithRangedUnits)
			{
				Micro::SmartAttackUnit(rangedUnit, target);
				continue;
			}

			//otherwise we use the kiting code

			BWAPI::Position attractionVector(0, 0);
			//split up preemptively upon seeing dangerous units
			if (rangedUnit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk 
				//|| rangedUnit->getType() == BWAPI::UnitTypes::Zerg_Scourge //is not helping
				) {
				BWAPI::UnitType targetType = target->getType();
				if (
					(
					(targetType == BWAPI::UnitTypes::Protoss_Corsair 
					|| targetType == BWAPI::UnitTypes::Terran_Valkyrie) 
					&& UnitUtil::IsFacing(target, rangedUnit)
					)
					|| (
					(targetType == BWAPI::UnitTypes::Protoss_High_Templar 
					//|| targetType == BWAPI::UnitTypes::Terran_Science_Vessel //not much of a threat really
					|| targetType == BWAPI::UnitTypes::Zerg_Defiler) 
					&& target->getEnergy() > 70
					)
					|| targetType == BWAPI::UnitTypes::Protoss_Archon)
				{
					if (Micro::AvoidAllies(rangedUnit, 1)) continue;
				}
				else if (rangedUnit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk &&
					UnitUtil::IsThreat(rangedUnit, target, false)) {
					//if (Micro::StackUp(rangedUnit, 16)) continue; //wider range stack

					if (kiteVector != BWAPI::Position(0, 0)) {
						attractionVector = computeAttractionVector(rangedUnit);
						kiteVector += attractionVector;
					}
					else if (BWAPI::Broodwar->getFrameCount() % 24 <= 12) { 
						//otherwise only occasionally stack them
						//need to do this for a few frames at least
						attractionVector = computeAttractionVector(rangedUnit);
						kiteVector += attractionVector;
					}
				}
			}

			if (rangedUnit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk 
				/*|| rangedUnit->getType() == BWAPI::UnitTypes::Terran_Vulture
					SmartKite micro probably better*/)
			{
				if (rangedUnit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk &&
					(target->getType() == BWAPI::UnitTypes::Zerg_Spore_Colony ||
					target->getType() == BWAPI::UnitTypes::Protoss_Photon_Cannon ||
					target->getType() == BWAPI::UnitTypes::Terran_Missile_Turret ||
					target->getType() == BWAPI::UnitTypes::Terran_Bunker) 
					&& target->isCompleted() && getUnits().size() < 7) {
					if (kiteVector != BWAPI::Position(0, 0)) {
						Micro::SmartMove(rangedUnit, target->getPosition() + kiteVector);
					}
					else if (tangentVector != BWAPI::Position(0, 0)) {
						//No visible effect right now. Seems like the mutalisk quickly changes targets or never targets these.
						//BWAPI::Broodwar->sendText("Mutalisk attempting turning");
						Micro::SmartMove(rangedUnit, target->getPosition() + tangentVector);
					}
					else {
						Micro::MutaDanceTarget(rangedUnit, target, kiteVector);
					}
				}
				else {
					Micro::MutaDanceTarget(rangedUnit, target, kiteVector);
				}
			}
			else if (rangedUnit->getType() == BWAPI::UnitTypes::Zerg_Scourge) {
				if (rangedUnit->getDistance(target) <= 3 + 24) {
					Micro::SmartAttackUnit(rangedUnit, target);
				}
				else {
					BWAPI::Position p = Micro::GetChasePosition(rangedUnit, target);
					if (rangedUnit->getDistance(p) <= 128) { //if it's close then add a little so that it stays at full speed
						p += Micro::GetKiteVector(rangedUnit, target);
					}
					Micro::SmartMove(rangedUnit, Micro::GetChasePosition(rangedUnit, target));
				}
			}
			else
			{
				Micro::SmartKiteTarget(rangedUnit, target);
			}

		} // No target was found.
		else
		{
			// if we're not near the order position, go there
			if (rangedUnit->getDistance(order.getPosition()) > 100)
			{
				//Micro::SmartMove(rangedUnit, order.getPosition());
				bool path = false;
				BWAPI::Position inputVector = kiteVector;
				if (rangedUnit->isFlying()) {
					path = true; //kite around unwanted enemies when moving
					//if (inputVector == BWAPI::Position(0, 0)) inputVector = tangentVector; //unclear if this does anything still;
					//there may be a better place to use it
					if (rangedUnit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk) {
						if (BWAPI::Broodwar->getFrameCount() % 24 <= 2) { //occasionally, strictly stack them
							if (Micro::StackUp(rangedUnit, 0)) continue;
						}
					}
				}
				Micro::SmartMove(rangedUnit, order.getPosition()); 

				//Micro::SmartTravel(rangedUnit, order.getPosition(), inputVector, path);
				//still having trouble escaping bases after diving into them
			}
			else if (rangedUnit->getType() == BWAPI::UnitTypes::Zerg_Guardian ||
				rangedUnit->getType() == BWAPI::UnitTypes::Zerg_Devourer) {
				if (rangedUnit->getType() == BWAPI::UnitTypes::Zerg_Devourer) {
					if (Micro::StackUp(rangedUnit, 1)) continue; //stack up with nearby mutalisks
				}
				//when there are no targets, kite away from nearby threats
				Micro::SmartTravel(rangedUnit, order.getPosition(), kiteVector, true);
			}
			else if (rangedUnit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk) {
				Micro::StackUp(rangedUnit, 0); //strictly stack them
			}
		}
	}
}

BWAPI::Unit RangedManager::getTarget(BWAPI::Unit rangedUnit, const BWAPI::Unitset & targets)
{
    int highPriority = 0;
	double closestDist = std::numeric_limits<double>::infinity();
	BWAPI::Unit closestTarget = nullptr;

    for (const auto & target : targets) {
        double distance         = rangedUnit->getDistance(target);
        int priority            = getAttackPriority(rangedUnit, target);

		//focus fire marines or interceptors our allies are targeting
		//originally it would focus fire zealots and ignore the zealot in front of it...
		//could tweak it more in the future
		if ((target->getType() == BWAPI::UnitTypes::Terran_Marine ||
			target->getType() == BWAPI::UnitTypes::Protoss_Interceptor) &&
			//target->getType() != BWAPI::UnitTypes::Terran_Vulture_Spider_Mine &&
			order.getType() != SquadOrderTypes::Regroup &&
			UnitUtil::IsThreat(rangedUnit, target, false) &&
			rangedUnit->getType() == BWAPI::UnitTypes::Zerg_Hydralisk && targeted.contains(target) &&
			rangedUnit->getDistance(target) <= UnitUtil::AttackRange(rangedUnit, target)) priority++;

		if (!closestTarget || priority > highPriority || (priority == highPriority && distance < closestDist))
		{
			closestDist = distance;
			highPriority = priority;
			closestTarget = target;
		} 

		if (order.getType() == SquadOrderTypes::Regroup || rangedUnit->isFlying()) {
			updateVectors(rangedUnit, target, 1, order.getPosition());
		}
		else {
			updateVectors(rangedUnit, target);
		}
    }

	normalizeVectors();

	if (highPriority == 0) return NULL;

	targeted.insert(closestTarget);

    return closestTarget;
}

// get the attack priority of a target unit
int RangedManager::getAttackPriority(BWAPI::Unit rangedUnit, BWAPI::Unit target) 
{
	if (!UnitUtil::CanAttack(rangedUnit, target)) return 0;

	BWAPI::UnitType rangedType = rangedUnit->getType();
	BWAPI::UnitType targetType = target->getType();

	if (rangedType == BWAPI::UnitTypes::Zerg_Scourge)
    {
		//ignore lifted buildings
		if (!targetType.isFlyer()) return 0;

		// not worth scourge
		if (targetType == BWAPI::UnitTypes::Zerg_Overlord || targetType == BWAPI::UnitTypes::Zerg_Scourge || targetType == BWAPI::UnitTypes::Protoss_Interceptor)
			return 0;
		
		return 100;
	}

	bool isThreat = UnitUtil::IsThreat(rangedUnit, target);

	if (rangedUnit->getType() == BWAPI::UnitTypes::Zerg_Hydralisk &&
		rangedUnit->isUnderDarkSwarm() && !(target->isUnderDarkSwarm())) {
		//if we're under dark swarm, hit the closest thing
		if (getSpecialPriority(rangedUnit, target) == 0) return 0;
		return 13;
	}

	if ((order.getType() == SquadOrderTypes::Regroup || order.getType() == SquadOrderTypes::Defend) && 
		UnitUtil::CanEscape(rangedUnit, target)) {
		int distance = rangedUnit->getDistance(order.getPosition());

		//in general mutalisks should obey the regroup command.
		if (rangedType == BWAPI::UnitTypes::Zerg_Mutalisk) {
			if (order.getType() == SquadOrderTypes::Defend) {
				// mutalisks should not try to base race when ordered to defend
				//however it's possible they may have to fight enemies on the way back
				if (distance > 25 * 32) {
					if (target->getDistance(order.getPosition()) > distance) {
						return 0;
					}
				}
			} else if (distance > std::max(20*32, order.getRadius())) {
				return 0;
			}
		}
		else {
			//hydralisks fight when flanked
			//the better long-term solution to this would be dynamic clustering and grouping of units
			int targDistance = target->getDistance(order.getPosition());
			int clipRadius = std::max(20 * 32, order.getRadius());
			if ((!isThreat && targDistance > clipRadius) ||
				(distance < targDistance &&
				distance > clipRadius)) {
				return 0;
			}
		}
	}

	if (rangedType == BWAPI::UnitTypes::Zerg_Devourer)
	{
		if (targetType.isBuilding())
		{
			// A lifted building is less important.
			return 1;
		}
		//too far away
		if (target->getDistance(rangedUnit) > rangedType.airWeapon().maxRange() + 5 * 32) return 2;

		//not important to target these at all
		if (target->getType() == BWAPI::UnitTypes::Protoss_Interceptor) return 2; 
		
		//otherwise hit target with least acid spore count
		if (isThreat) {
			return 100 - target->getAcidSporeCount();
		}
		else {
			return 50 - target->getAcidSporeCount();
		}
	}
	
	if (rangedType == BWAPI::UnitTypes::Zerg_Guardian)
	{ 
		BWAPI::UnitType type = target->getType();
		if (rangedUnit->getDistance(target) >= (8 + 3) * 32) return 1;
		if (isThreat) return 100;
		if (type.groundWeapon() != BWAPI::UnitTypes::None) return 99;
		return 90;
	}

    // if the target is building something near our base something is fishy
    BWAPI::Position ourBasePosition = BWAPI::Position(InformationManager::Instance().getMyMainBaseLocation()->getPosition());
	if (target->getDistance(ourBasePosition) < 1200) {
		if (target->getType().isWorker() && (target->isConstructing() || target->isRepairing()))
		{
			return 101;
		}
		// nearby threats still more important than whatever building it is, unless it's a threat
		if (target->getType().isBuilding())
		{
			if (targetType.groundWeapon() != BWAPI::WeaponTypes::None) {
				return 90;
			}
			else if (rangedType == BWAPI::UnitTypes::Zerg_Mutalisk && 
				targetType.isBuilding() && target->isLifted() && target->getDistance(order.getPosition()) > 15 * 32) {
				//Ignore floating buildings too far away from the order position
				return 0;
			}
			else {
				return 11;
			}
		}
	}


	/* Compute values to determine if a unit is within reasonable range of a target */
	double distance = rangedUnit->getDistance(target); 
	double speed = rangedType.topSpeed();
	double enemySpeed = targetType.topSpeed();
	double enemyRange = UnitUtil::AttackRange(target, rangedUnit);
	double range = UnitUtil::AttackRange(rangedUnit, target);

	if (rangedType.isFlyer()) { // Exceptions if we're a flyer (other than scourge).
		if (targetType == BWAPI::UnitTypes::Zerg_Scourge)
		{
			if (distance < 5 * 32) {
				return 20;
			}
			else if (distance < 6 * 32) {
				//if it's moving towards us, increase the priority
				auto pproj = Micro::GetChasePosition(rangedUnit, target);
				bool runningAway = target->isMoving() && rangedUnit->getDistance(pproj) > rangedUnit->getDistance(target);
				if (!runningAway) return 20;
			}
		}
	}
	else
	{

		// Exceptions if we're a ground unit.
		int distance = rangedUnit->getDistance(target);
		if (distance <= range+32) {
			if (targetType == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine ||
				targetType == BWAPI::UnitTypes::Zerg_Infested_Terran)
			{
				if (target->isBurrowed() && distance >= 4*32) { //moving ones are more dangerous than burrowed ones
					return 18;
				}
				else {
					return 20;
				}
			}
			if (targetType == BWAPI::UnitTypes::Protoss_Reaver ||
				targetType == BWAPI::UnitTypes::Zerg_Lurker ||
				targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
				targetType == BWAPI::UnitTypes::Protoss_High_Templar)
			{
				return 19;
			}
		}
		else if (distance <= range + 3 * 32) {
			if (targetType == BWAPI::UnitTypes::Protoss_Reaver ||
				targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
				target->getOrder() == BWAPI::Orders::Sieging)
			{
				return 18;
			}
		}
	} 

	// Exception: Workers are not threats after all.
	if (!(target->getType().isWorker())) {
		// we don't clip out the target as a possibility if we're a mutalisk and it's been
		// targeted by a fellow mutalisk.
		if (rangedType != BWAPI::UnitTypes::Zerg_Mutalisk || !(rangedUnit->getDistance(target) < 10*32 && targeted.contains(target))) {
			bool canAttackUs = enemyRange + 96 >= distance;
			if (targetType.isBuilding()) canAttackUs = enemyRange >= distance;
			bool isAttackable = range + 64 >= distance;

			bool isNonthreateningBuilding = targetType.isBuilding() && !isThreat;

			// De-prioritize non-worker, non-building threats outside mutual range
			if ((!targetType.isBuilding() || isThreat) && !isAttackable && !canAttackUs) {
				return 1;
			}

			// De-prioritize very far away targets, or fast targets
			if (order.getType() == SquadOrderTypes::Defend) {
				//don't clip out targets when we're on defense
			}
			else if (speed < enemySpeed && target->isMoving()) {
				if (distance > 2.5 * range) {
					return 0;
				}
				else if (distance >= 1.5*range) {
					return 1;
				}
			}
			else if (distance >= 3 * (speed - enemySpeed + range)) {
				return 1;
			}
		}
	}

	// Deprioritize terran static defense unless we can burst them down
	if (rangedType.isFlyer()) { //mutalisks
		if (targetType == BWAPI::UnitTypes::Terran_Missile_Turret) {
			if (getUnits().size() < 11) return 9;
		}

		if (targetType == BWAPI::UnitTypes::Terran_Bunker) {
			if (getUnits().size() < 14) return 9;
		}
		else
		//threatening buildings in general are not important unless they are attacking 
		if (targetType.isBuilding() && UnitUtil::IsThreat(rangedUnit, target, false)) {
			if (!target->isAttacking()) return 9;
		}
	}
	
	if (rangedType == BWAPI::UnitTypes::Zerg_Hydralisk) {
		if (target->isRepairing() && target->getDistance(rangedUnit) < range + 32) {
			return 13;
		}

		if (targetType == BWAPI::UnitTypes::Protoss_Carrier) {
			if (distance <= range) return 13;
			if (order.getType() == SquadOrderTypes::Defend) return 13;
			if (distance <= range+96 &&
				BWAPI::Broodwar->getGroundHeight(target->getTilePosition()) == BWAPI::Broodwar->getGroundHeight(rangedUnit->getTilePosition())) {
				return 13;
			}
		}

		if (targetType.isResourceDepot() && getUnits().size() >= 10 && getSpecialPriority(rangedUnit, target) != 0) {
			return 12;
		}
	}
	else if (rangedType == BWAPI::UnitTypes::Zerg_Mutalisk) { 
		//shoot all nearby workers
		if (targetType.isWorker() && distance < range + 32 && !rangedUnit->isUnderAttack()) {
			return 12;
		}
		else if (targetType == BWAPI::UnitTypes::Zerg_Zergling) {
			if (order.getType() == SquadOrderTypes::Defend) {
				return 13;
			}
			else {
				return 11;
			}
		}
	}

	return getCommonPriority(rangedUnit, target);
}