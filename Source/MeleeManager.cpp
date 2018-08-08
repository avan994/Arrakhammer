#include "MeleeManager.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

// Note: Melee units are ground units only. Scourge is a "ranged" unit.

MeleeManager::MeleeManager() 
{ 
	runby_done = false;
}

void MeleeManager::executeMicro(const BWAPI::Unitset & targets) 
{
	assignTargets(targets);
}

void MeleeManager::assignTargets(const BWAPI::Unitset & targets)
{
    const BWAPI::Unitset & meleeUnits = getUnits();

	// figure out targets

	BWAPI::Unitset meleeUnitTargets;
	unsigned int nDefenses = 0;
	for (auto & target : targets)
	{
		// conditions for targeting
		if (target && target->exists() &&
			target->isVisible() &&
			!target->isFlying() &&
			(target->getType() != BWAPI::UnitTypes::Zerg_Larva) &&
			(target->getType() != BWAPI::UnitTypes::Zerg_Egg) &&
			!target->isStasised() &&
			!target->isUnderDisruptionWeb())             // melee unit can't attack under dweb
		{
			meleeUnitTargets.insert(target);
		}

		if ((target->getType() == BWAPI::UnitTypes::Terran_Bunker
			|| target->getType() == BWAPI::UnitTypes::Protoss_Photon_Cannon
			) &&
			target->getDistance(order.getPosition()) > 10 * 32
			) {
			if (target->getType() == BWAPI::UnitTypes::Protoss_Photon_Cannon &&
				target->isCompleted()) nDefenses++;
			if (target->getType() == BWAPI::UnitTypes::Terran_Bunker) nDefenses += 2;
		}
	}
	//cancel the runby if there are too many enemy defenses or we can kill their defenses
	auto runbySize = runby_triggered.size();
	//need a more sophisticated evaluation mechanism for canceling runbies.
	//for now, only ever doing it once each game is probably fine
	if (false /*nDefenses >= 3 || (nDefenses <= 1 && runbySize >= 6) || (runbySize >= 6*nDefenses + 6)*/) {
		if (runbySize > 0) {
			runby_triggered.clear();
			runby_location_reached.clear();
			BWAPI::Broodwar->sendText("Canceling runby!");
		}
	}
	else if (!runby_done &&
		BWAPI::Broodwar->getFrameCount() <= 8000 &&
		order.getType() == SquadOrderTypes::Attack &&
		nDefenses >= 1 &&
		//unit->getDistance(order.getPosition()) < 25 * 32 &&
		getUnits().size() >= 6) {
		runby_done = true;

		for (auto unit : meleeUnits) {
			if (unit->getType() != BWAPI::UnitTypes::Zerg_Zergling) continue;
			runby_triggered[unit] = order.getPosition();
		}
		BWAPI::Broodwar->sendText("Ordering runby!");
	}

	bool recomputeBias = (BWAPI::Broodwar->getFrameCount() % 256 == 0);

	int i = 0;
	for (auto & meleeUnit : meleeUnits)
	{
		if (runby_triggered.count(meleeUnit) > 0) BWAPI::Broodwar->drawCircleMap(meleeUnit->getPosition(), 8, BWAPI::Colors::Red);

		initializeVectors();
		if (Micro::CheckSpecialCases(meleeUnit)) continue;

		i++;

		BWAPI::Unit target = NULL;
		if (!meleeUnitTargets.empty()) {
			target = getTarget(meleeUnit, meleeUnitTargets);
		}
			
		if (target && target->exists()) {
			BWAPI::UnitType targetType = target->getType();
			if (meleeUnit->getType() == BWAPI::UnitTypes::Zerg_Zergling
				&& UnitUtil::IsFacing(target, meleeUnit) &&
				(targetType == BWAPI::UnitTypes::Terran_Firebat
				|| targetType == BWAPI::UnitTypes::Zerg_Infested_Terran
				|| targetType == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine
				|| (meleeUnit->getDistance(target) > 64 && (targetType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode
				|| targetType == BWAPI::UnitTypes::Zerg_Lurker
				|| targetType == BWAPI::UnitTypes::Protoss_Reaver)))) {
				if (Micro::AvoidAllies(meleeUnit, 32 / 4)) continue;
			}

			//not enough zerglings to break through a bunch of photons off a 9hatchling...
			//testing: runby a bunker
			//crude runby code for zerglings. could be applied to hydralisks later, and/or consolidated
			if (runby_triggered.count(meleeUnit) > 0) {
				auto targetPosition = runby_triggered[meleeUnit];

				if (meleeUnit->getDistance(targetPosition) < 6 * 32) {
					runby_location_reached[meleeUnit] = true;
				}
				
				//send the unit back to the runby location if it gets baited back to the static defense
				if (target->getDistance(order.getPosition()) > 10 * 32 &&
					(target->getType() == BWAPI::UnitTypes::Terran_Bunker ||
					target->getType() == BWAPI::UnitTypes::Protoss_Photon_Cannon) &&
					runby_location_reached[meleeUnit] &&
					BWAPI::Broodwar->getFrameCount() <= 8000) {
					//BWAPI::Broodwar->sendText("Zergling ordered to get back to runby.");
					//runby_location_reached[meleeUnit] = false;
				}

				if (target->getType() != BWAPI::UnitTypes::Terran_Marine &&
					target->getDistance(targetPosition) > 10*32 &&
					meleeUnit->getDistance(targetPosition) > 6*32 &&
					!runby_location_reached[meleeUnit]) {
					BWAPI::Broodwar->drawCircleMap(target->getPosition(), 16, BWAPI::Colors::Red);
					Micro::SmartMove(meleeUnit, targetPosition);
					continue;
				}
			}

			int distance = meleeUnit->getDistance(target);

			//crude harass code for zerglings. should be consolidated later
			if (meleeUnit->getType() == BWAPI::UnitTypes::Zerg_Zergling && target->getType().isWorker() && distance < 10*32 &&
				!target->isUnderAttack()) {
				if (meleeUnit->getHitPoints() <= 16) {
					if (kiteVector != BWAPI::Position(0, 0)) {
						Micro::SuperSmartMove(meleeUnit, meleeUnit->getPosition() + kiteVector);
						continue;
					}
					else if (distance < 2 * 32 + 16) { 
						if (distance < 16) {
							Micro::SmartAttackUnit(meleeUnit, target);
							continue;
						}
						else {
							auto inputVector = kiteVector + Micro::GetKiteVector(target, meleeUnit);
							Micro::SuperSmartMove(meleeUnit, meleeUnit->getPosition() + inputVector);
							continue;
						}
					}
					else
					{
						auto buildings = meleeUnit->getUnitsInRadius(124, BWAPI::Filter::IsBuilding && BWAPI::Filter::IsEnemy);
						bool found = false;
						for (auto b : buildings) {
							if (b->getDistance(target) >= 64) {
								Micro::SmartAttackUnit(meleeUnit, b);
								found = true;
								break;
							}
						}
						if (!found) Micro::SmartAttackMove(meleeUnit, meleeUnit->getPosition());
						continue;
					}
				}
				else if (distance < 36) {
					auto units = meleeUnit->getUnitsInRadius(36, BWAPI::Filter::IsWorker &&
						(BWAPI::Filter::IsAttacking || BWAPI::Filter::IsStartingAttack ||
						!(BWAPI::Filter::IsGatheringGas || BWAPI::Filter::IsGatheringMinerals)));
					if (units.size() >= 3 && meleeUnit->getHitPoints() <= 25 || 
						(target->getType() == BWAPI::UnitTypes::Terran_SCV && meleeUnit->getDistance(target) <= 11 &&
						UnitUtil::IsFacing(target, meleeUnit, 1))) {
						auto inputVector = kiteVector;
						if (kiteVector == BWAPI::Position(0, 0)) inputVector = Micro::GetKiteVector(target, meleeUnit);
						Micro::SuperSmartMove(meleeUnit, meleeUnit->getPosition() + inputVector);
						continue;
					}
				}
			}
			//end harass code
			
			if (distance > 32 + 3 * meleeUnit->getType().topSpeed() + meleeUnit->getType().groundWeapon().maxRange()) {
				BWAPI::Position chasePosition = Micro::GetChasePosition(meleeUnit, target);

				if (chasePosition.isValid()) {
					if (distance < 5 * 32) { //very close, make sure we're not being blocked by allies!
						Micro::SuperSmartMove(meleeUnit, chasePosition);
					}
					else {
						Micro::SmartMove(meleeUnit, chasePosition);
					}
				}
				else {
					Micro::SmartAttackUnit(meleeUnit, target);
				}
			}
			else {
				if (meleeUnit->getType() == BWAPI::UnitTypes::Zerg_Zergling) {
					if (Micro::SurroundTarget(meleeUnit, target)) continue;
				}
				else if (meleeUnit->getType() == BWAPI::UnitTypes::Zerg_Drone) {
					if (meleeUnit->getGroundWeaponCooldown() >= 6 && UnitUtil::IsMelee(target, meleeUnit)) {
						//kite back. simple code for now...
						Micro::SmartMove(meleeUnit, meleeUnit->getPosition() + Micro::GetKiteVector(target, meleeUnit));
						continue;
					}
				}

				Micro::SmartAttackUnit(meleeUnit, target);
			}
			/* ------------------------------- */
		}
		else if (runby_triggered.count(meleeUnit) > 0 && !runby_location_reached[meleeUnit]) {
			auto targetPosition = runby_triggered[meleeUnit];

			if (meleeUnit->getDistance(targetPosition) < 6 * 32) {
				runby_location_reached[meleeUnit] = true;
			}

			Micro::SmartMove(meleeUnit, targetPosition);
		}
		else if (meleeUnit->getDistance(order.getPosition()) > 96)
		{
			double magI = 8*sqrt(i);
			double thetaI = (90)*sqrt(i);
			BWAPI::Position spiralOut(int(magI*cos(thetaI)), int(magI*sin(thetaI)));
			BWAPI::Position movePosition = order.getPosition() + spiralOut;

			if (!(movePosition.isValid())) movePosition = order.getPosition();
			//regrouping should move, not regrouping should attack move just in case
			if (order.getType() == SquadOrderTypes::Regroup) {
				/* not having the desired results. 
				if (meleeUnit->getType() == BWAPI::UnitTypes::Zerg_Zergling) {
					Micro::SmartTravel(meleeUnit, movePosition, kiteVector, true);
				}
				else {*/
				Micro::SmartTravel(meleeUnit, movePosition);
				//}
			}
			else {
				Micro::SmartAttackMove(meleeUnit, movePosition);
			}
		}

		if (Config::Debug::DrawUnitTargetInfo)
		{
			BWAPI::Broodwar->drawLineMap(meleeUnit->getPosition().x, meleeUnit->getPosition().y, 
				meleeUnit->getTargetPosition().x, meleeUnit->getTargetPosition().y,
				Config::Debug::ColorLineTarget);
		}
	}
}

// get a target for the meleeUnit to attack
BWAPI::Unit MeleeManager::getTarget(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets)
{
	int highPriority = 0;
	double closestDist = std::numeric_limits<double>::infinity();
	BWAPI::Unit closestTarget = nullptr;

	// for each target possiblity
	for (auto & unit : targets)
	{
		int priority = getAttackPriority(meleeUnit, unit);
		int distance = meleeUnit->getDistance(unit);

		// if it's a higher priority, or it's closer, set it
		if (!closestTarget || (priority > highPriority) || (priority == highPriority && distance < closestDist))
		{
			closestDist = distance;
			highPriority = priority;
			closestTarget = unit;
		}

		updateVectors(meleeUnit, unit, 1);
	}

	normalizeVectors();

	if (highPriority == 0) return NULL;

	return closestTarget;
}

// get the attack priority of a type
int MeleeManager::getAttackPriority(BWAPI::Unit attacker, BWAPI::Unit unit) 
{
	if (unit->isFlying()) return 0;

	if (attacker->isUnderDarkSwarm()) return 13; //always attack closest under swarm

	BWAPI::UnitType type = unit->getType();
	bool isThreat = UnitUtil::IsThreat(attacker, unit);

	int distance = attacker->getDistance(unit);
	if (runby_triggered.count(attacker) > 0) {
		//runby units will primarily ignore units too far from themselves,
		//but they will ignore the regroup command. 
		if (distance > 10*32 && unit->getDistance(runby_triggered[attacker]) > 10*32) return 0;
	}

	else if (order.getType() == SquadOrderTypes::Regroup && UnitUtil::CanEscape(attacker, unit)) {
		
		//finish them!
		if (distance < 24 && (unit->getHitPoints() + unit->getShields() <= 2 * (attacker->getType().groundWeapon().damageAmount()))) return 1;

		//ultralisks will still fight if flanked
		int odistance = attacker->getDistance(order.getPosition());
		if (!isThreat ||
			(odistance < unit->getDistance(order.getPosition()) &&
			odistance > order.getRadius() &&
			attacker->getDistance(unit) < 32)) {
			return 0;
		}

		return 0; 
		//Zerglings ignore everything on their way out!
	}

	if (distance > 25 * 32) return 1; // way too far!

	if (type == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine || type == BWAPI::UnitTypes::Zerg_Infested_Terran 
		&& distance < 32) return 100;

	//ultralisks always attack the closest
	if (attacker->getType() == BWAPI::UnitTypes::Zerg_Ultralisk) {
		if (type.isBuilding()) {
			if (unitNearChokepoint(unit)) return 12; //no buildings allowed at chokes! 
			return 9;
		}
		return 12;
	}

	if (attacker->getType() == BWAPI::UnitTypes::Protoss_Dark_Templar ||
		attacker->getType() == BWAPI::UnitTypes::Zerg_Zergling) {
		if (type == BWAPI::UnitTypes::Terran_Marine && distance < 64) {
			return 14;
		}
		else if (type.isWorker()) { //kill non-moving workers!
			int distance = attacker->getDistance(unit);
			if (distance < 24) return 14;
			if (!unit->isMoving() || unit->isBraking() || unit->isGatheringMinerals() ||
				unit->isGatheringGas()) {
				if (distance < 48) return 14;
				if (distance < 4 * 32) return 12;
			}
		}
		else if (type == BWAPI::UnitTypes::Zerg_Zergling) {
			//don't goose chase irrelevant enemy zerglings
			auto pproj = Micro::GetChasePosition(attacker, unit);
			bool runningAway = attacker->getDistance(pproj) > attacker->getDistance(unit);
			if (order.getType() == SquadOrderTypes::Attack &&
				distance > 4 * 32 && (unit->isMoving() && runningAway) &&
				attacker->getDistance(order.getPosition()) < unit->getDistance(order.getPosition())) {
				return 0;
			}
		}

		if (type.isResourceDepot()) { //kill hatcheries!
			return 9;
		}
		else if (type == BWAPI::UnitTypes::Terran_Factory || //vultures are dangerous!
			type == BWAPI::UnitTypes::Zerg_Spawning_Pool ||
			type == BWAPI::UnitTypes::Zerg_Spire) { //lings are dangerous!
			return 10;
		}
	}


	if (type.groundWeapon() != BWAPI::WeaponTypes::None) {
		int range = UnitUtil::AttackRange(unit, attacker);
		if (distance > range + 32) return 8;
	}

	if (type.isSpellcaster() && attacker->getDistance(unit) > 48) {
		return 8;
	}

	// Attempt to detect a wall made of depots/bunkers if it's close and near a chokepoint. 
	if ((type == BWAPI::UnitTypes::Terran_Supply_Depot || type == BWAPI::UnitTypes::Terran_Bunker) 
		&& unitNearChokepoint(unit) && attacker->getDistance(unit) < 128 + type.groundWeapon().maxRange()) {
		if (unit->getHitPoints() < unit->getType().maxHitPoints()) return 14;
		return 13;
	}

	// Medics could block pathing -- note far spellcasters have priority 8
	if (type == BWAPI::UnitTypes::Terran_Medic) return 13;
	return getCommonPriority(attacker, unit);
}

// Retreat hurt units to allow them to regenerate health (zerg) or shields (protoss).
bool MeleeManager::meleeUnitShouldRetreat(BWAPI::Unit meleeUnit, const BWAPI::Unitset & targets)
{
	if (meleeUnit->getType() != BWAPI::UnitTypes::Zerg_Zergling) return false; // the only unit we want to retreat is zerglings
	// A broodling should not retreat since it is on a timer and regeneration does it no good.
	if (meleeUnit->getType() == BWAPI::UnitTypes::Zerg_Broodling)
	{
		return false;
	} 
	else if (meleeUnit->getType().getRace() == BWAPI::Races::Terran) // terran don't regen so it doesn't make sense to retreat
    {
        return false;
    }

	if (getUnits().size() > 18) return false; //too many units to bother running

	//return false; //temporary rigging of retreat function 

    // we don't want to retreat the melee unit if its shields or hit points are above the threshold set in the config file
    // set those values to zero if you never want the unit to retreat from combat individually
    /*if (meleeUnit->getShields() > Config::Micro::RetreatMeleeUnitShields || meleeUnit->getHitPoints() > Config::Micro::RetreatMeleeUnitHP)
    {
        return false;
    }*/

    // if there is a ranged enemy unit within attack range of this melee unit then we shouldn't bother retreating since it could fire and kill it anyway
	int highestDamage = 0; //check highest damage enemy in area
	int sumDmg = 0; //sum together dmg of enemy melee units practically in range to attack
	int numEnemyWorkers = 0; 

	for (auto & unit : targets)
    {
        int groundWeaponRange = unit->getType().groundWeapon().maxRange();
        if (groundWeaponRange >= 64 && unit->getDistance(meleeUnit) < groundWeaponRange)
        {
            return false;
        }

		//enemy zerglings should be handled by SH's usual code for now...
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Zergling) {
			continue;
		}

		/* stay just close enough to get back into the fight... */
		if (unit->getDistance(meleeUnit) < 13*32 + groundWeaponRange) {
			BWAPI::WeaponType weapon = unit->getType().groundWeapon();

			int dmg = (weapon.damageAmount() - unit->getType().armor());
			int scaleFactor = 1;
			if (unit->getType().size() == BWAPI::UnitSizeTypes::Small) {
				if (weapon.damageType() == BWAPI::DamageTypes::Explosive) {
					dmg = dmg / 2;
				}
			}
			else if (weapon.damageType() == BWAPI::DamageTypes::Concussive) {
				dmg = dmg / 2; //concussive v. large is negligible. 
			}
			dmg = dmg*weapon.damageFactor();

			// no point running from a dark templar that always oneshots you 
			if (dmg > meleeUnit->getType().maxHitPoints()) {
				//do this check early to end the loop earlier
				return false;
			}
			if (dmg > highestDamage) highestDamage = dmg;

			if (unit->getDistance(meleeUnit) <= 32) {
				if (meleeUnit->getType() == BWAPI::UnitTypes::Zerg_Zergling && unit->getType().isWorker()) {
					numEnemyWorkers++;
					// never get into a situation where workers surround you as a zergling. 
					if (numEnemyWorkers >= 3) {
						return true;
					}
				}
				sumDmg += dmg;
			}
		}
    }
	int HP = meleeUnit->getHitPoints();
	// no threats in area, no reason to run!
	if (highestDamage == 0) {
		return false; 
	}

	//should avoid disadvantageous conflicts with probes/zealots
	if (sumDmg > highestDamage) {
		highestDamage = sumDmg;
	}

	// will we get oneshot?
	if (HP > highestDamage) {
		return false;
	}
	else if (meleeUnit->getHitPoints() < Config::Micro::RetreatMeleeUnitHP) {
		return true;
	}

	return true;
}