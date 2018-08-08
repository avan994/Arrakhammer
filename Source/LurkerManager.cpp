#include "LurkerManager.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

LurkerManager::LurkerManager()
{
	ambush = false;
	_lastAmbushSwitchTime = -1000;
}

/* Determines where the unit could be when the lurker is burrowed */
BWAPI::Position GetProjectedPosition(BWAPI::Unit unit, BWAPI::Unit target)
{
	double distance = unit->getDistance(target);

	//double approxTimeToBurrow = 28.0; //time needed to burrow lurker
	double approxTimeToBurrow = 6.0; //28 seems to not work out so well
	BWAPI::Position projectedMovement(int(approxTimeToBurrow * target->getVelocityX()), int(approxTimeToBurrow * target->getVelocityY()));
	BWAPI::Position chasePosition(target->getPosition() + projectedMovement);
	return chasePosition;
}

void LurkerManager::executeMicro(const BWAPI::Unitset & targets)
{

	const BWAPI::Unitset & lurkers = getUnits();

	// figure out targets
	BWAPI::Unitset LurkerTargets;
	std::copy_if(targets.begin(), targets.end(), std::inserter(LurkerTargets, LurkerTargets.end()),
		[](BWAPI::Unit u){ return u->isVisible() && !u->isFlying(); });

	//clean it up every once in a while
	if (BWAPI::Broodwar->getFrameCount() % (10 * 60 * 20) == 0) {
		ambushStatus.clear();
	}
	if (BWAPI::Broodwar->getFrameCount() % 24 == 0) { //clear it out every 24 frames
		BFFs.clear();
	}

	//construct a list of burrowed lurkers
	BWAPI::Unitset burrowedLurkers;
	for (auto & lurker : lurkers) {
		bool burrowing = lurker->getOrder() == BWAPI::Orders::Burrowing;
		bool unburrowing = lurker->getOrder() == BWAPI::Orders::Unburrowing;
		if ((lurker->isBurrowed() && !unburrowing) || burrowing) {
			burrowedLurkers.insert(lurker);
		}
		else if (BFFs.contains(lurker)) { //it's not burrowed anymore, it can't be a BFF
			BFFs.erase(lurker);
		}
	}
	BWAPI::Unit BFF = NULL;

	for (auto & lurker : lurkers)
	{
		if (!lurker) continue;
		ambushStatus[lurker] = (ambushStatus[lurker] < 0) ? ambushStatus[lurker] - 1 : 0;

		if (Micro::CheckSpecialCases(lurker)) continue;

		BFF = getClosestFriend(lurker, burrowedLurkers);
		if (BFF && BFF->getDistance(lurker) <= 32) {
			BFFs.insert(lurker);
		}
		else {
			BFF = NULL;
		}

		//bool lurkerOnDefense = false;
		BWAPI::Position movePosition = lurker->getPosition(); //no order?! well, this lurker will chill. 
		if (order.getType() == SquadOrderTypes::Attack ||
			order.getType() == SquadOrderTypes::Regroup) {
			movePosition = order.getPosition();
		}
		else if (order.getType() == SquadOrderTypes::Defend) {
			movePosition = order.getPosition(); //this should be the nearest chokepoint, really

			//(lurkerOnDefense = (order.getType() == SquadOrderTypes::Defend ||
			//	order.getType() == SquadOrderTypes::Regroup) &&
			//	(lurker->getDistance(order.getPosition()) < 3 * 32);
		}

		BWAPI::Unit target = NULL;
		if (!LurkerTargets.empty()) target = getTarget(lurker, LurkerTargets);
		if (lurker->isBurrowed() && lurker->isDetected()) switchAmbush(false); //we got found out! attack!

		if (target && target->exists()) {
			if (order.getType() != SquadOrderTypes::Regroup) {
				movePosition = target->getPosition();
			}

			//projected distance is requirement for burrowing early, or unburrowing late
			int projectedDistance = lurker->getDistance(GetProjectedPosition(lurker, target));
			int currentDistance = lurker->getDistance(target);

			bool enemyRangedUnitNearby = (target->getType().groundWeapon().maxRange() >= 4 * 32 
				&& target->getType().groundWeapon().maxRange()+16 <= lurkerRange) 
				&& currentDistance <= lurkerRange + 4 * 32;
			bool healthyLurker = lurker->getHitPoints() > lurker->getType().maxHitPoints()/2 + 10;

			if (lurker->isBurrowed() && lurker->canAttack() && currentDistance <= lurkerRange) {
				if (currentDistance <= 2 * 32 ||  //close enough. FIRE!
					target->isAttacking() || target->isAttackFrame() || target->isStartingAttack() || //an ally might be under attack. FIRE!
					(target->getType().isBuilding() || (target->getType().isWorker() && target->isGatheringMinerals()))) //we're in the enemy base! FIRE!
				{
					startAmbush(lurker);
				}

				if (ambushStatus(lurker) <= 0) {
					Micro::SmartHoldPosition(lurker);
				}
				else {
					//fancy code for drawing the splash area of the lurker attack
					/*BWAPI::Position pDiff = target->getPosition() - lurker->getPosition();
					double angle = atan2(pDiff.y, pDiff.x);
					BWAPI::Position endpoint(int(lurkerRange * cos(angle)), int(lurkerRange * sin(angle)));
					BWAPI::Position displacement(int(20 * cos(M_PI/2 + angle)), int(20 * sin(M_PI/2 + angle)));
					BWAPI::Broodwar->drawLineMap(lurker->getPosition() + displacement, lurker->getPosition() + displacement + endpoint, BWAPI::Colors::Brown);
					BWAPI::Broodwar->drawLineMap(lurker->getPosition() - displacement, lurker->getPosition() - displacement + endpoint, BWAPI::Colors::Brown);
					BWAPI::Broodwar->drawLineMap(lurker->getPosition() + displacement + endpoint, lurker->getPosition() - displacement + endpoint, BWAPI::Colors::Brown);*/

					Micro::SmartAttackUnit(lurker, target);
				}
			}
			else if (lurker->canUnburrow()) //Unburrowing section
			{
				bool unburrow = false;
				if (projectedDistance > lurkerRange) { //enemy could enter range
					if (target->getType().isBuilding() || //it's a building
						(target->getType().groundWeapon().maxRange() > lurkerRange &&
						lurker->isDetected())) { //we're outranged
						unburrow = true;
					}
					else if (enemyRangedUnitNearby) {
						//we don't unburrow
					}
					else if (BFF || !BFFs.contains(lurker)) { //we have support, or we're supporting no-one
						unburrow = true;
					}
				}

				if (unburrow) {
					lurker->unburrow();
					BFFs.erase(lurker);
					burrowedLurkers.erase(lurker);
				}
			}
			else if (lurker->canBurrow()) { //Burrowing section
				bool burrow = false;

				if (target->getType().isBuilding() || (target->getType().isWorker() && target->isGatheringMinerals())) {
					if (lurker->canBurrow()) {
						if (!target->getType().isWorker() && //burrow far from threatening buildings
							(target->getType().isDetector() ||
							target->getType().groundWeapon() != BWAPI::WeaponTypes::None ||
							target->getType() == BWAPI::UnitTypes::Terran_Bunker)
							&& currentDistance <= lurkerRange - 16) { 
							//if we try to burrow at lurkerRange it can might not be able to attack;
							//possibly it considers the building's gap size for attackability
							burrow = true;
						}
						else if (currentDistance <= 16 + 16 * (rand() % 5)) { //burrow close to nonthreatening buildings/harvesting workers
							burrow = true;
						}
					}
				}
				else if ((BFF && projectedDistance <= 3*32) || //burrow aggressively if we have support
					(!BFF && (projectedDistance < lurkerRange - 16 || currentDistance <= lurkerRange - 64)) || //burrow safely if we're alone
					(!healthyLurker && (enemyRangedUnitNearby || projectedDistance < lurkerRange || currentDistance <= lurkerRange - 16))) //burrow safely if we're weak
				{
					burrow = true;
				}

				if (burrow) {
					lurker->burrow();
					burrowedLurkers.insert(lurker);
				}
			}

			if (Config::Debug::DrawUnitTargetInfo) {
				BWAPI::Broodwar->drawLineMap(lurker->getPosition(), lurker->getTargetPosition(), BWAPI::Colors::Purple);
			}

		}
		//no target found
		else if (lurker->getDistance(order.getPosition()) < 8 * 32) { //already there? burrow for now
			if (lurker->canBurrow()) lurker->burrow();
		}
		else if (lurker->canUnburrow()) { //moving around? unburrow!
			lurker->unburrow();
		}

		if (!lurker->isBurrowed() && lurker->canMove()) {
			Micro::SmartMove(lurker, movePosition);
		}
	}
}

BWAPI::Unit LurkerManager::getTarget(BWAPI::Unit lurker, const BWAPI::Unitset & targets)
{
	int bestPriorityDistance = 1000000;

	int highPriority = 0;
	double closestDist = std::numeric_limits<double>::infinity();
	BWAPI::Unit closestTarget = nullptr;

	int lurkerRange = BWAPI::UnitTypes::Zerg_Lurker.groundWeapon().maxRange();

	BWAPI::Unitset targetsInRange;
	for (auto & target : targets)
	{
		if (!target || !(target->exists())) continue;
		if (target->getDistance(lurker) <= lurkerRange && UnitUtil::CanAttack(lurker, target))
		{
			targetsInRange.insert(target);
		}
	}

	const BWAPI::Unitset & newTargets = targetsInRange.empty() ? targets : targetsInRange;

	// check first for units that are in range of our attack that can cause damage
	// choose the highest priority one from them at the closest distance
	for (const auto & target : newTargets)
	{
		if (!target || !(target->exists())) continue;

		double distance = lurker->getDistance(target);
		int priority = 0;
		//if we're burrowed, sum the priorities of all targets that will also be hit
		if (!targetsInRange.empty() && lurker->isBurrowed()) {
			priority = getLineSplashPriority(lurker, target, targetsInRange);
		}
		else {
			priority = getAttackPriority(lurker, target);
		}

		if (!closestTarget || (priority > highPriority) || (priority == highPriority && distance < closestDist))
		{
			closestDist = distance;
			highPriority = priority;
			closestTarget = target;
		}
	}

	if (highPriority <= 0) return NULL;

	return closestTarget;
}

// get the attack priority of a type in relation to a zergling
int LurkerManager::getAttackPriority(BWAPI::Unit rangedUnit, BWAPI::Unit target)
{
	if (target->isFlying()) return 0;

	BWAPI::UnitType rangedType = rangedUnit->getType();
	BWAPI::UnitType targetType = target->getType();

	bool isThreat = UnitUtil::IsThreat(rangedUnit, target);

	//if we're burrowed and undetected, there is no need to regroup against enemy targets
	if (order.getType() == SquadOrderTypes::Regroup && !rangedUnit->isDetected()) {
		if (!isThreat) return 0;
		int distance = rangedUnit->getDistance(order.getPosition());
		if (!(rangedUnit->isBurrowed() || rangedUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Burrow) && //if the lurker can escape, consider it
			distance < target->getDistance(order.getPosition()) &&
			rangedUnit->getType().topSpeed() > target->getType().topSpeed() &&
			rangedUnit->getDistance(target) > target->getType().groundWeapon().maxRange() + 3 * 32 &&
			rangedUnit->getHitPoints() > 45) return 0;
	}

	// workers are high priority
	if (targetType.isWorker()) return 13;
	if (targetType.isBuilding() && !isThreat) return 9;

	return getCommonPriority(rangedUnit, target);
}

/* Sums the priorities of all secondary targets that will be hit as a result of hitting the main target. */
int LurkerManager::getLineSplashPriority(BWAPI::Unit rangedUnit, BWAPI::Unit mainTarget, BWAPI::Unitset & targets) {
	if (!rangedUnit || !mainTarget || !mainTarget->exists() || targets.empty()) {
		UAB_ASSERT(false, "Invalid arg to getLineSplashPriority in LurkerManager.cpp!");
		return 0;
	}
	int totalPriority = getAttackPriority(rangedUnit, mainTarget);
	if (rangedUnit->getDistance(mainTarget) >= lurkerRange + 32) {
		UAB_ASSERT(false, "Line splash target is not remotely in range in LurkerManager.cpp?!");
		return totalPriority;
	}

	//compute the x-y to u-v transform we can apply to later coordinates

	BWAPI::Position pDiff = mainTarget->getPosition() - rangedUnit->getPosition();
	double angle = -rangedUnit->getAngle(); //by default, assume the unit attacks its default direction
	//determine angle to rotate pDiff onto the x-axis
	//and use this to transform into a uv space.
	if (pDiff.y == 0 && pDiff.x == 0) {
		//UAB_ASSERT(false, "Line splash target is DIRECTLY on top of Lurker in LurkerManager.cpp?!");
		//assume default angle as set above
	}
	else {
		angle = -atan2(pDiff.y, pDiff.x); //note this is the rotation angle, as we want to do it clockwise
	}
	double cA = cos(angle);
	double sA = sin(angle);

	for (const auto & target : targets) {
		BWAPI::UnitType type = target->getType();
		if (!target || !target->exists() || target == mainTarget ||
			(type.isBuilding() && type != BWAPI::UnitTypes::Terran_Bunker && type.groundWeapon() == BWAPI::WeaponTypes::None)) continue;
		//don't accumulate buildings that are nonthreats [unless it's the main target]

		if (rangedUnit->getDistance(target) >= lurkerRange + 32) {
			UAB_ASSERT(false, "Error: Secondary line splash target is not remotely in range in LurkerManager.cpp?!");
			return totalPriority;
		}
		else { //compute whether the unit is in the AOE
			BWAPI::Position corners[4];

			BWAPI::Position velocity(int(target->getVelocityX()), int(target->getVelocityY()));
			//look one frame ahead of the unit

			//check every corner
			corners[0] = target->getPosition() + velocity + BWAPI::Position(type.dimensionLeft(), type.dimensionUp());
			corners[1] = target->getPosition() + velocity + BWAPI::Position(type.dimensionRight(), type.dimensionUp());
			corners[2] = target->getPosition() + velocity + BWAPI::Position(type.dimensionLeft(), type.dimensionDown());
			corners[3] = target->getPosition() + velocity + BWAPI::Position(type.dimensionRight(), type.dimensionDown());

			for (int i = 0; i < 3; i++) {
				//get it's position relative to the ranged unit
				pDiff = corners[i] - rangedUnit->getPosition();
				//rotate pT by the same angle needed to map the original unit to the uv space,
				//centered around the ranged unit:
				BWAPI::Position pT(int(pDiff.x * cA - pDiff.y * sA), int(pDiff.x * sA + pDiff.y * cA));

				//we know it is already in attack range due to prior checks. we need simply check if
				//the "y" is in range:
				if (pT.y >= -20 && pT.y <= 20) {
					totalPriority += getAttackPriority(rangedUnit, target);
					break;
				} // if this fails for all corners, unit was not in range
			}
		}


	}

	return totalPriority;
}

/* A single value controls whether all lurkers begin attacking (ambush=true), or wait to begin an ambush (ambush=false).
* Any lurker can start the attack, but attacking must last for at least 10*20 frames before ending */
void LurkerManager::switchAmbush(bool value) {
	if (value) {
		if (BWAPI::Broodwar->getFrameCount() - _lastAmbushSwitchTime <= 10 * 20) return;
	}

	//refresh this value each time a lurker is detected to have been fighting/acquired
	//a suitable target, or since the last time we decided to switch back to ambush mode
	_lastAmbushSwitchTime = BWAPI::Broodwar->getFrameCount();

	ambush = value;
}

void LurkerManager::startAmbush(BWAPI::Unit lurker) {
	ambushStatus[lurker] = AMBUSH_TIME;
}