#include "CombatSimulation.h"
#include "FAP.h"
#include "UnitUtil.h"

#include "WorkerManager.h"

using namespace UAlbertaBot;

//Sparcraft was causing timeouts with large numbers of zerglings [and cannot handle them]
//and otherwise lagging near late game...
//but zerglings will be necessary in the future, so I've switched over to using FAP for now. 

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


/* Sparcraft code. 

#include "CombatSimulation.h"

using namespace UAlbertaBot;

CombatSimulation::CombatSimulation()
{
}

// sets the starting states based on the combat units within a radius of a given position
// this center will most likely be the position of the forwardmost combat unit we control
void CombatSimulation::setCombatUnits(const BWAPI::Position & center, const int radius)
{
	SparCraft::GameState s;

	if (Config::Debug::DrawCombatSimulationInfo)
	{
		BWAPI::Broodwar->drawCircleMap(center.x, center.y, 6, BWAPI::Colors::Red, true);
		BWAPI::Broodwar->drawCircleMap(center.x, center.y, radius, BWAPI::Colors::Red);
	}

	BWAPI::Unitset ourCombatUnits;
	std::vector<UnitInfo> enemyCombatUnits;

	MapGrid::Instance().GetUnits(ourCombatUnits, center, radius, true, false);
	InformationManager::Instance().getNearbyForce(enemyCombatUnits, center, BWAPI::Broodwar->enemy(), radius);

	bool canShootAir = false;
	bool flyingSquad = false;

	double groundRange = 0;
	int nUnits = 0;

	for (auto & unit : ourCombatUnits)
	{
        if (unit->getType().isWorker())
        {
            continue;
        }
		if (!unit->isCompleted()) continue;

		if (unit && unit->exists()) {
			groundRange += double(BWAPI::Broodwar->self()->weaponMaxRange(unit->getType().groundWeapon()));
			nUnits++;
		}

		if (unit->getType() == BWAPI::UnitTypes::Zerg_Zergling) 
		{
			//if (unit->getHitPoints() < Config::Micro::RetreatMeleeUnitHP && !unit->isAttacking()) continue;
		}
		else if (unit->getType().isBuilding()) {
			if (!(unit->getClosestUnit(BWAPI::Filter::IsEnemy, 7 * 32))) continue;
		}
		else if (unit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk) {
			if (unit->getHitPoints() <= 40) continue;
		}

		if (!canShootAir && unit && unit->exists() && unit->getType().airWeapon() != BWAPI::WeaponTypes::None) {
			canShootAir = true;
		}
		if (!flyingSquad && unit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk) { //don't count overlords and queens
			flyingSquad = true;
		}

		// Pretend that a guardian is a siege tank in tank mode! :D
		if (unit->getType() == BWAPI::UnitTypes::Zerg_Guardian)
		{
			//double hpRatio = static_cast<double>(unit->lastHealth) / unit->type.maxHitPoints();

			SparCraft::Unit guardian(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode,
				SparCraft::Position(unit->getPosition()),
				unit->getID(),
				getSparCraftPlayerID(unit->getPlayer()),
				static_cast<int>(unit->getHitPoints()),
				0,
				BWAPI::Broodwar->getFrameCount(),
				BWAPI::Broodwar->getFrameCount());
			try {
				s.addUnit(guardian);
			}
			catch (int e) {
				e = 0;
				BWAPI::Broodwar->printf("Problem Adding Guardian as unsieged siege tank!", unit->getID());
			}

			continue;
		}

        if (InformationManager::Instance().isCombatUnit(unit->getType()) && SparCraft::System::isSupportedUnitType(unit->getType()))
		{
            try
            {
			    s.addUnit(getSparCraftUnit(unit));
            }
            catch (int e)
            {
				// Ignore the exception and the unit.
				e = 0;    // use the variable to avoid a pointless warning
				//BWAPI::Broodwar->printf("Problem Adding Self Unit with ID: %d", unit->getID());
            }
		}
	}

	if (nUnits > 0) groundRange /= nUnits;

	int nCannons = 0;
	BWAPI::Player enemyCannonPlayer;
	BWAPI::Position averageCannonPosition(0, 0);
	int lastCannonId = -1;

	for (UnitInfo & ui : enemyCombatUnits)
	{
        if (ui.type.isWorker())
        {
            continue;
        }
		if (!ui.completed) continue;

		if (ui.unit && ui.unit->exists()) {
			auto unit = ui.unit;
			double speed = sqrt(pow(unit->getVelocityX(), 2) + pow(unit->getVelocityY(), 2));

			if (groundRange >= 3*32 &&
				(speed < unit->getType().topSpeed() / 4 && !unit->isAttacking()) &&
				double(BWAPI::Broodwar->enemy()->weaponMaxRange(unit->getType().groundWeapon())) < 32) {
				//they must be stuck somewhere, maybe due to a terrain feature
				continue;
			}
		}

		if (ui.type == BWAPI::UnitTypes::Protoss_Photon_Cannon) {
			enemyCannonPlayer = ui.player;
			averageCannonPosition += ui.lastPosition;
			nCannons++;
			lastCannonId = ui.unitID;
		}


        if (ui.type == BWAPI::UnitTypes::Terran_Bunker)
        {
            double hpRatio = static_cast<double>(ui.lastHealth) / ui.type.maxHitPoints();

			//pretend it's 2 photon cannons with half the hp of the bunker
			SparCraft::Unit u(BWAPI::UnitTypes::Protoss_Photon_Cannon,
				SparCraft::Position(ui.lastPosition),
				ui.unitID,
				getSparCraftPlayerID(ui.player),
				static_cast<int>(75 + ui.lastHealth/2),
				0,
				BWAPI::Broodwar->getFrameCount(),
				BWAPI::Broodwar->getFrameCount());

			s.addUnit(u);
			s.addUnit(u);
            
            continue;
		}

		// SparCraft claims to support mutas, wraiths, BCs, scouts.
		//we only worry about air if we can shoot air
        if ( (canShootAir || !ui.type.isFlyer()) && SparCraft::System::isSupportedUnitType(ui.type) && ui.completed)
		{
            try
            {
				//only worry about these if we have flying units in our team
				if (flyingSquad && (ui.type == BWAPI::UnitTypes::Terran_Missile_Turret || ui.type == BWAPI::UnitTypes::Zerg_Spore_Colony)) {
					//add these as photon cannons
					SparCraft::Unit u(BWAPI::UnitTypes::Protoss_Photon_Cannon,
						SparCraft::Position(ui.lastPosition),
						ui.unitID,
						getSparCraftPlayerID(ui.player),
						static_cast<int>(ui.lastHealth),
						0,
						BWAPI::Broodwar->getFrameCount(),
						BWAPI::Broodwar->getFrameCount());

					s.addUnit(getSparCraftUnit(ui)); 
				}
				else if (ui.type == BWAPI::UnitTypes::Terran_Marine && (ui.unit && ui.unit->exists() && ui.unit->isStimmed())) {
					SparCraft::Unit u(BWAPI::UnitTypes::Terran_Marine,
						SparCraft::Position(ui.lastPosition),
						ui.unitID,
						getSparCraftPlayerID(ui.player),
						static_cast<int>(5 + ui.lastHealth/2),
						0,
						BWAPI::Broodwar->getFrameCount(),
						BWAPI::Broodwar->getFrameCount());

					s.addUnit(u); //add it again if it's a stimmed marine 
					s.addUnit(u);
				}
				else if (ui.type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) {
					//let's make the siege tank LESS scary
					SparCraft::Unit u(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode,
						SparCraft::Position(ui.lastPosition),
						ui.unitID,
						getSparCraftPlayerID(ui.player),
						static_cast<int>(ui.lastHealth),
						0,
						BWAPI::Broodwar->getFrameCount(),
						BWAPI::Broodwar->getFrameCount());

					s.addUnit(u); 
				}
				else {
					s.addUnit(getSparCraftUnit(ui));
				}
            }
            catch (int e)
            {
				// Ignore the exception and the unit.
				e = 0;    // use the variable to avoid a pointless warning
                //BWAPI::Broodwar->printf("Problem Adding Enemy Unit with ID: %d %d", ui.unitID, e);
            }
		}
		else {
			if (canShootAir && ui.type == BWAPI::UnitTypes::Protoss_Carrier) {
				SparCraft::Unit u(BWAPI::UnitTypes::Terran_Marine,
					SparCraft::Position(ui.lastPosition),
					ui.unitID,
					getSparCraftPlayerID(ui.player),
					static_cast<int>(1 + ui.lastHealth / 2),
					0,
					BWAPI::Broodwar->getFrameCount(),
					BWAPI::Broodwar->getFrameCount());

				try {
					for (size_t i(0); i < 4; ++i)
					{
						s.addUnit(u);
					}
				} 
				catch (int e) {
					e = 0;
				}
			}
		}
	}

	if (nCannons >= 6) {
		//enemy has a lot of cannons here
		//pretend they have 2 more photon cannons
		averageCannonPosition /= nCannons;
		SparCraft::Unit u(BWAPI::UnitTypes::Protoss_Photon_Cannon,
			SparCraft::Position(averageCannonPosition),
			lastCannonId,
			getSparCraftPlayerID(enemyCannonPlayer),
			static_cast<int>(150),
			0,
			BWAPI::Broodwar->getFrameCount(),
			BWAPI::Broodwar->getFrameCount());

		s.addUnit(u);
		s.addUnit(u);
	}

	s.finishedMoving();

	state = s;
}

// Gets a SparCraft unit from a BWAPI::Unit, used for our own units since we have all their info
const SparCraft::Unit CombatSimulation::getSparCraftUnit(BWAPI::Unit unit) const
{
    return SparCraft::Unit( unit->getType(),
                            SparCraft::Position(unit->getPosition()), 
                            unit->getID(), 
                            getSparCraftPlayerID(unit->getPlayer()), 
                            unit->getHitPoints() + unit->getShields(), 
                            0,
		                    BWAPI::Broodwar->getFrameCount(), 
                            BWAPI::Broodwar->getFrameCount());	
}

// Gets a SparCraft unit from a UnitInfo struct, needed to get units of enemy behind FoW
const SparCraft::Unit CombatSimulation::getSparCraftUnit(const UnitInfo & ui) const
{
	BWAPI::UnitType type = ui.type;

    // treat medics as goliaths
	if (type == BWAPI::UnitTypes::Terran_Medic)
	{
		type = BWAPI::UnitTypes::Terran_Marine;
	}
	// treat firebats as vultures
	else if (type == BWAPI::UnitTypes::Terran_Firebat) {
		type = BWAPI::UnitTypes::Terran_Vulture;
	} 

    return SparCraft::Unit( type, 
                            SparCraft::Position(ui.lastPosition), 
                            ui.unitID, 
                            getSparCraftPlayerID(ui.player), 
                            ui.lastHealth, 
                            0,
		                    BWAPI::Broodwar->getFrameCount(), 
                            BWAPI::Broodwar->getFrameCount());	
}

SparCraft::ScoreType CombatSimulation::simulateCombat()
{
    try
    {
	    SparCraft::GameState s1(state);

        SparCraft::PlayerPtr selfNOK(new SparCraft::Player_NOKDPS(getSparCraftPlayerID(BWAPI::Broodwar->self())));

	    SparCraft::PlayerPtr enemyNOK(new SparCraft::Player_NOKDPS(getSparCraftPlayerID(BWAPI::Broodwar->enemy())));

	    SparCraft::Game g (s1, selfNOK, enemyNOK, 2000);

	    g.play();
	
	    SparCraft::ScoreType eval =  g.getState().eval(SparCraft::Players::Player_One, SparCraft::EvaluationMethods::LTD2).val();

        if (Config::Debug::DrawCombatSimulationInfo)
        {
            std::stringstream ss1;
            ss1 << "Initial State:\n";
            ss1 << s1.toStringCompact() << "\n\n";

            std::stringstream ss2;

            ss2 << "Predicted Outcome: " << eval << "\n";
            ss2 << g.getState().toStringCompact() << "\n";

            BWAPI::Broodwar->drawTextScreen(150,200,"%s", ss1.str().c_str());
            BWAPI::Broodwar->drawTextScreen(300,200,"%s", ss2.str().c_str());

			std::string prefix = (eval < 0) ? "\x06" : "\x07";
	        BWAPI::Broodwar->drawTextScreen(240, 280, "Combat Sim : %s%d", prefix.c_str(), eval);
        }
        
	    return eval;
    }
    catch (int e)
    {
        //BWAPI::Broodwar->printf("SparCraft FatalError, simulateCombat() threw");

        return e;
    }
}

const SparCraft::GameState & CombatSimulation::getSparCraftState() const
{
	return state;
}

const SparCraft::IDType CombatSimulation::getSparCraftPlayerID(BWAPI::Player player) const
{
	if (player == BWAPI::Broodwar->self())
	{
		return SparCraft::Players::Player_One;
	}
	if (player == BWAPI::Broodwar->enemy())
	{
		return SparCraft::Players::Player_Two;
	}

	return SparCraft::Players::Player_None;
}
*/