#include "Common.h"
#include "BuildingManager.h"
#include "Micro.h"
#include "ScoutManager.h"
#include "UnitUtil.h"

using namespace UAlbertaBot;

BuildingManager::BuildingManager()
    : _debugMode(false)
    , _reservedMinerals(0)
    , _reservedGas(0)
{
}

// gets called every frame from GameCommander
void BuildingManager::update()
{
    validateWorkersAndBuildings();          // check to see if assigned workers have died en route or while constructing
    assignWorkersToUnassignedBuildings();   // assign workers to the unassigned buildings and label them 'planned'    
    constructAssignedBuildings();           // for each planned building, if the worker isn't constructing, send the command    
    checkForStartedConstruction();          // check to see if any buildings have started construction and update data structures    
    checkForDeadTerranBuilders();           // if we are terran and a building is under construction without a worker, assign a new one    
    checkForCompletedBuildings();           // check to see if any buildings have completed and update data structures
}

// In the building queue with any status.
bool BuildingManager::isBeingBuilt(BWAPI::UnitType type)
{
    for (auto & b : _buildings)
    {
        if (b.type == type)
        {
            return true;
        }
    }

    return false;
}

// Counts the number in the building queue, up to 3. 
// Only used for counting hatcheries and sunkens right now.
// presumably we wouldn't build more than 3 at a time
int BuildingManager::numBeingBuilt(BWAPI::UnitType building)
{
	int n = 0;
	for (auto & b : _buildings)
	{
		if (b.type == building) n++;
		if (n >= 3) break;
	}

	return n;
}

bool BuildingManager::isBuildingMacroHatchery() {
	for (auto & b : _buildings)
	{
		if (b.type == BWAPI::UnitTypes::Zerg_Hatchery && b.macroLocation == MacroLocation::Macro)
		{
			return true;
		}
	}

	return false;
}

// STEP 1: DO BOOK KEEPING ON BUILDINGS WHICH MAY HAVE DIED
void BuildingManager::validateWorkersAndBuildings()
{
    std::vector<Building> toRemove;
    
    // find any buildings which have become obsolete
    for (auto & b : _buildings)
    {
        if (b.status != BuildingStatus::UnderConstruction)
        {
            continue;
        }

		if (!b.buildingUnit ||
			!b.buildingUnit->exists() ||
			b.buildingUnit->getHitPoints() <= 0 ||
			!b.buildingUnit->getType().isBuilding())
        {
            toRemove.push_back(b);
        }
    }

    undoBuildings(toRemove);
}

// STEP 2: ASSIGN WORKERS TO BUILDINGS WITHOUT THEM
// Also places the building.
void BuildingManager::assignWorkersToUnassignedBuildings()
{
    // for each building that doesn't have a builder, assign one
    for (Building & b : _buildings)
    {
		if (b.delayUntil > 0 && b.delayUntil > BWAPI::Broodwar->getFrameCount()-(24*20)) {
			BWAPI::Broodwar->drawTextMap(BWAPI::Position(b.finalPosition), "Building Delayed: %d", (b.delayUntil - BWAPI::Broodwar->getFrameCount()) / 24);
		}

		//if the previous worker died, we delay retrying for a few frames
		//by default b.delayUntil is -100 -- presumably the framecount is always positive
        if (b.status != BuildingStatus::Unassigned || BWAPI::Broodwar->getFrameCount() < b.delayUntil)
        {
            continue;
        }

		// BWAPI::Broodwar->printf("Assigning Worker To: %s", b.type.getName().c_str());

        // TODO: special case of terran building whose worker died mid construction
        //       send the right click command to the buildingUnit to resume construction
        //		 skip the buildingsAssigned step and push it back into buildingsUnderConstruction

        BWAPI::TilePosition testLocation = getBuildingLocation(b);
        if (!testLocation.isValid())
        {
			auto mb = InformationManager::Instance().getMainBaseLocation(BWAPI::Broodwar->self())->getTilePosition();
			auto tL = testLocation;
			continue;
        }

		b.finalPosition = testLocation;

		// grab the worker unit from WorkerManager which is closest to this final position
		b.builderUnit = WorkerManager::Instance().getBuilder(b);
		if (!b.builderUnit || !b.builderUnit->exists())
		{
			continue;
		}

		b.status = BuildingStatus::Assigned;

        // reserve this area if it's a depot
        BuildingPlacer::Instance().reserveTiles(b.finalPosition,b.type.tileWidth(),b.type.tileHeight());
		if (b.type.isResourceDepot() && b.macroLocation != MacroLocation::Macro) {
			//MapTools is much more inefficient, runs the next-expo algorithm again, can just match bases.
			//MapTools::Instance().reserveNextExpansion(b.macroLocation == MacroLocation::Hidden, b.macroLocation == MacroLocation::MinOnly);
			for (auto base : BWTA::getBaseLocations()) {
				if (!base) continue;
				if (base->getTilePosition() == b.finalPosition) {
					InformationManager::Instance().reserveBase(base);
					break;
				}
			}
		}
	}
}

// STEP 3: ISSUE CONSTRUCTION ORDERS TO ASSIGNED BUILDINGS AS NEEDED
void BuildingManager::constructAssignedBuildings()
{
    for (auto & b : _buildings)
    {
        if (b.status != BuildingStatus::Assigned)
        {
            continue;
        }

		if (true) {
			BWAPI::Position bfp(b.finalPosition);
			BWAPI::Position tlc(bfp.x, bfp.y);
			BWAPI::Position brc(bfp.x + 32 * b.type.tileWidth(), bfp.y + b.type.tileHeight() * 32);
			BWAPI::Broodwar->drawBoxMap(tlc, brc, BWAPI::Colors::Red);
		}

		//consider b.underConstruction for drones morphing into extractors - drone is lost, but
		//building is in progress
		if (!b.builderUnit || !b.builderUnit->exists())
		{
			// The builder unit no longer exists() and must be replaced.

			if (b.type == BWAPI::UnitTypes::Zerg_Extractor) {
				auto extractors = BWAPI::Broodwar->getUnitsOnTile(b.finalPosition, BWAPI::Filter::IsBuilding && BWAPI::Filter::IsAlly && !BWAPI::Filter::IsFlying);
				bool found = false;
				for (auto extractor : extractors) {
					if (extractor->getType() == BWAPI::UnitTypes::Zerg_Extractor && extractor->getTilePosition() == b.finalPosition &&
						extractor->getPlayer() == BWAPI::Broodwar->self()) {
						found = true;
					}
				}
				if (found) {
					continue; //eventually it'll be found and marked as under construction
				} //else the worker died, and we have to replace it
			}

			//BWAPI::Broodwar->sendText("Bad builder unit");
			
			//BWAPI::Broodwar->printf("b.builderUnit gone, b.type = %s", b.type.getName().c_str());

			if (b.builderUnit) WorkerManager::Instance().finishedWithWorker(b.builderUnit); 
			b.builderUnit = nullptr;
			b.buildCommandGiven = false;
			b.status = BuildingStatus::Unassigned;
			BuildingPlacer::Instance().freeTiles(b.finalPosition, b.type.tileWidth(), b.type.tileHeight());
			//also unreserve the base if it was an intended expansion
			if (b.type.isResourceDepot() && b.macroLocation != MacroLocation::Macro) {
				InformationManager::Instance().unreserveBase(b.finalPosition);
				b.delayUntil = BWAPI::Broodwar->getFrameCount() + 40 * 24; //delay building by 40 seconds if the previous drone died
			}
			else {
				b.delayUntil = BWAPI::Broodwar->getFrameCount() + 10 * 24; //delay building by 10 seconds if the previous drone died
			}
		}
        else 
		if (!b.builderUnit->isConstructing())
        {
			//the worker may have been reassigned due to idleness - grab a new builder. likely the same builder. 
			if (!WorkerManager::Instance().isBuilder(b.builderUnit)) {
				b.builderUnit = WorkerManager::Instance().getBuilder(b);

				//apparently the worker can be invalid
				if (!b.builderUnit || !b.builderUnit->exists() || b.builderUnit->getHitPoints() <= 0) continue;
			}
			//then immediately order it to start building

			//returning cargo and blocking minerals are handled by worker manager
			if (b.builderUnit->isCarryingMinerals() || b.builderUnit->isCarryingGas()) continue;
			if (b.builderUnit->isGatheringMinerals() && b.builderUnit->getTarget() && b.builderUnit->getTarget()->getResources() == 0) continue;

			//if it's our natural, assume it's not blocked by any minerals and force the worker to keep moving

            // if we haven't explored the build position, go there
			if (!isBuildingPositionExplored(b) || b.builderUnit->getDistance(BWAPI::Position(b.finalPosition)) > 10 * 32)
            {
				//paths along it, should help on Benzene -- but still no good for the kite vector. 
				//should centralize it into worker manager later.

				if (isBuildingPositionExplored(b)) {
					// if we can see it, check if we can mineral walk to a mineral there
					// otherwise just move normally
					auto minerals = BWAPI::Broodwar->getUnitsInRadius(BWAPI::Position(b.finalPosition), 8 * 32, BWAPI::Filter::IsMineralField);
					if (!minerals.empty()) {
						for (auto mineral : minerals) {
							Micro::SmartRightClick(b.builderUnit, mineral);
							break;
						}
						continue;
					}
				}

				Micro::SmartTravel(b.builderUnit, BWAPI::Position(b.finalPosition), BWAPI::Position(0, 0), true);
            }
            // if this is not the first time we've sent this guy to build this
            // it must be the case that something was in the way
			else if (b.buildCommandGiven)
            {
                // tell worker manager the unit we had is not needed now, since we might not be able
                // to get a valid location soon enough

                WorkerManager::Instance().finishedWithWorker(b.builderUnit);

                b.builderUnit = nullptr;
                b.buildCommandGiven = false;
                b.status = BuildingStatus::Unassigned;

				// Unreserve the building location. The building will mark its own location.
				//also unreserve the base if it was an intended expansion
				BuildingPlacer::Instance().freeTiles(b.finalPosition, b.type.tileWidth(), b.type.tileHeight());
				if (b.type.isResourceDepot() && b.macroLocation != MacroLocation::Macro) {
					InformationManager::Instance().unreserveBase(b.finalPosition);
				}
			}
            else
            {
				// Issue the build order!

				//check if there's something blocking the building site if we're trying to build a resource depot
				if (b.builderUnit && b.builderUnit->exists() && b.type.isResourceDepot()) {
					BWAPI::Position bfp(b.finalPosition); //top left of building
					BWAPI::Position tlc(bfp.x, bfp.y);
					BWAPI::Position brc(bfp.x + 32 * b.type.tileWidth(), bfp.y + 32*b.type.tileHeight());

					auto enemies = BWAPI::Broodwar->getUnitsInRadius(bfp, 10 * 32, BWAPI::Filter::IsBuilding &&
						BWAPI::Filter::IsEnemy);
					if (!(enemies.empty())) { 
						//we arrived and found enemy buildings there pretend we tried to build (and failed)
						b.buildCommandGiven = true;
						return;
					}

					//get everything in the build area of the depot
					auto blockers = BWAPI::Broodwar->getUnitsInRectangle(tlc, brc, !BWAPI::Filter::IsFlying);
					bool wait = false;
					for (auto blocker : blockers) {
						if (!blocker || !blocker->exists() || blocker == b.builderUnit) continue;
						if (blocker->isStasised()) {
							break; //try building and give up --- the area is blocked
						}
						else if (blocker->getType() == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine) {
							b.builderUnit->attack(blocker);
							return;
						}
						else if (blocker->getPlayer() != BWAPI::Broodwar->self()) { //not our unit
							if (blocker->getHitPoints() <= 20 || blocker->getType().isCritter()) {
								b.builderUnit->attack(blocker);
								return;
							} //else just try to build, fail, and then unassign the worker
							wait = false; //we don't wait to fail, because it might be an enemy
							break;
						}
						else { //it's our unit, tell it to move its ass outta the way
							if (blocker->canLift()) {
								if (blocker->isTraining()) {
									blocker->cancelTrain();
								}
								blocker->lift();
							}
							else {
								//they can't move
								if (blocker->isMaelstrommed() ||
									blocker->isLockedDown()) {
									wait = false;
									break;
								}

								if (blocker->canUnburrow()) blocker->unburrow();
								if (blocker->canUnsiege()) blocker->unsiege();

								if (blocker->canMove()) {
									int randomX = 6 * 32 * (rand() % 3 - 1);
									int randomY = 6 * 32 * (rand() % 2 ? 1 : -1);

									//this isn't quite good enough -- need to prevent the blocker from
									//being issued a command that brings it back inside the location afterward.
									Micro::SmartMove(blocker, blocker->getPosition() + BWAPI::Position(randomX, randomY));
								}
								wait = true;
							}
							//wait a bit for them to move away
						}
					}
					if (wait) return;
				}

				bool success = b.builderUnit->build(b.type, b.finalPosition);

				if (success)
				{
					b.buildCommandGiven = true;
				}
           }
        }
    }
}

// STEP 4: UPDATE DATA STRUCTURES FOR BUILDINGS STARTING CONSTRUCTION
void BuildingManager::checkForStartedConstruction()
{
    // for each building unit which is being constructed
    for (auto & buildingStarted : BWAPI::Broodwar->self()->getUnits())
    {
        // filter out units which aren't buildings under construction
        if (!buildingStarted->getType().isBuilding() || !buildingStarted->isBeingConstructed())
        {
            continue;
        }

        // check all our building status objects to see if we have a match and if we do, update it
        for (auto & b : _buildings)
        {
            if (b.status != BuildingStatus::Assigned)
            {
                continue;
            }
        
            // check if the positions match
            if (b.finalPosition == buildingStarted->getTilePosition())
            {
                // the resources should now be spent, so unreserve them
                _reservedMinerals -= buildingStarted->getType().mineralPrice();
                _reservedGas      -= buildingStarted->getType().gasPrice();

                // flag it as started and set the buildingUnit
                b.underConstruction = true;
                b.buildingUnit = buildingStarted;

                if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Zerg)
                {
					// if we are zerg, the builderUnit now becomes nullptr since it's destroyed
					b.builderUnit = nullptr;
                }
                else if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Protoss)
                {
					// if we are protoss, give the worker back to worker manager
					// if this was the gas steal unit then it's the scout worker so give it back to the scout manager
                    if (b.isGasSteal)
                    {
                        ScoutManager::Instance().setWorkerScout(b.builderUnit);
                    }
                    // otherwise tell the worker manager we're finished with this unit
                    else
                    {
                        WorkerManager::Instance().finishedWithWorker(b.builderUnit);
                    }

                    b.builderUnit = nullptr;
                }

                b.status = BuildingStatus::UnderConstruction;

                BuildingPlacer::Instance().freeTiles(b.finalPosition,b.type.tileWidth(),b.type.tileHeight());

                // only one building will match
                break;
            }
        }
    }
}

// STEP 5: IF THE SCV DIED DURING CONSTRUCTION, ASSIGN A NEW ONE
void BuildingManager::checkForDeadTerranBuilders()
{
	if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Terran)
	{
		return;
	}

	for (auto & b : _buildings)
	{
		if (b.status != BuildingStatus::UnderConstruction)
		{
			continue;
		}

		UAB_ASSERT(b.buildingUnit, "null buildingUnit");

		if (!UnitUtil::IsValidUnit(b.builderUnit))
		{
			b.builderUnit = WorkerManager::Instance().getBuilder(b);
			if (b.builderUnit && b.builderUnit->exists())
			{
				b.builderUnit->rightClick(b.buildingUnit);
			}
		}
	}
}

// STEP 6: CHECK FOR COMPLETED BUILDINGS
void BuildingManager::checkForCompletedBuildings()
{
    std::vector<Building> toRemove;

    // for each of our buildings under construction
    for (auto & b : _buildings)
    {
        if (b.status != BuildingStatus::UnderConstruction)
        {
            continue;       
        }

		UAB_ASSERT(b.buildingUnit, "null buildingUnit");

        // if the unit has completed
        if (b.buildingUnit->isCompleted())
        {
            // if we are terran, give the worker back to worker manager
            if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Terran)
            {
                if (b.isGasSteal)
                {
                    ScoutManager::Instance().setWorkerScout(b.builderUnit);
                }
                // otherwise tell the worker manager we're finished with this unit
                else
                {
                    WorkerManager::Instance().finishedWithWorker(b.builderUnit);
                }
            }

            // remove this unit from the under construction vector
            toRemove.push_back(b);
		}
		else {
			//building is undeer attack and about to die! cancel it!
			int hitpoints = b.buildingUnit->getHitPoints();
			if (hitpoints < 20 || (hitpoints < 50 && hitpoints < b.buildingUnit->getType().maxHitPoints() / 20)) {
				cancelBuilding(b);
			}
		}
    }

    removeBuildings(toRemove);
}

// Add a new building to be constructed and return it.
Building & BuildingManager::addTrackedBuildingTask(const MacroAct & act, BWAPI::TilePosition desiredLocation, BuildOrderItem & item)
{
	UAB_ASSERT(act.isBuilding(), "trying to build a non-building");

	BWAPI::UnitType type = act.getUnitType();

	_reservedMinerals += type.mineralPrice();
	_reservedGas += type.gasPrice();

	Building b(type, desiredLocation);
	b.macroLocation = act.getMacroLocation();
	b.isGasSteal = item.isGasSteal;
	b.defenseLocation = item.defenseLocation;
	b.status = BuildingStatus::Unassigned;

	_buildings.push_back(b);      // makes a "permanent" copy of the Building object
	return _buildings.back();     // return a reference to the permanent copy
}

// Add a new building to be constructed.
void BuildingManager::addBuildingTask(const MacroAct & act, BWAPI::TilePosition desiredLocation, BuildOrderItem & item)
{
	(void) addTrackedBuildingTask(act, desiredLocation, item);
}

bool BuildingManager::isBuildingPositionExplored(const Building & b) const
{
    BWAPI::TilePosition tile = b.finalPosition;

    // for each tile where the building will be built
    for (int x=0; x<b.type.tileWidth(); ++x)
    {
        for (int y=0; y<b.type.tileHeight(); ++y)
        {
            if (!BWAPI::Broodwar->isExplored(tile.x + x,tile.y + y))
            {
                return false;
            }
        }
    }

    return true;
}

char BuildingManager::getBuildingWorkerCode(const Building & b) const
{
    return b.builderUnit == nullptr ? 'X' : 'W';
}

int BuildingManager::getReservedMinerals() const
{
    return _reservedMinerals;
}

int BuildingManager::getReservedGas() const
{
    return _reservedGas;
}

void BuildingManager::drawBuildingInformation(int x,int y)
{
    if (!Config::Debug::DrawBuildingInfo)
    {
        return;
    }

    for (auto & unit : BWAPI::Broodwar->self()->getUnits())
    {
        BWAPI::Broodwar->drawTextMap(unit->getPosition().x,unit->getPosition().y+5,"\x07%d",unit->getID());
    }

	for (auto geyser : BWAPI::Broodwar->getAllUnits())
	{
		if (geyser->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser)
		{
			BWAPI::Broodwar->drawTextMap(geyser->getPosition().x, geyser->getPosition().y + 5, "\x07%d", geyser->getType());
		}
	}
	
	BWAPI::Broodwar->drawTextScreen(x, y, "\x04 Building Information:");
    BWAPI::Broodwar->drawTextScreen(x,y+20,"\x04 Name");
    BWAPI::Broodwar->drawTextScreen(x+150,y+20,"\x04 State");

    int yspace = 0;

	for (const auto & b : _buildings)
    {
        if (b.status == BuildingStatus::Unassigned)
        {
			int x1 = b.desiredPosition.x * 32;
			int y1 = b.desiredPosition.y * 32;
			int x2 = (b.desiredPosition.x + b.type.tileWidth()) * 32;
			int y2 = (b.desiredPosition.y + b.type.tileHeight()) * 32;

			BWAPI::Broodwar->drawBoxMap(x1, y1, x2, y2, BWAPI::Colors::Green, false);
			BWAPI::Broodwar->drawTextScreen(x, y + 40 + ((yspace)* 10), "\x03 %s", b.type.getName().c_str());
			BWAPI::Broodwar->drawTextScreen(x + 150, y + 40 + ((yspace++) * 10), "\x03 Need %c", getBuildingWorkerCode(b));
        }
        else if (b.status == BuildingStatus::Assigned)
        {
            BWAPI::Broodwar->drawTextScreen(x,y+40+((yspace)*10),"\x03 %s %d",b.type.getName().c_str(),b.builderUnit->getID());
            BWAPI::Broodwar->drawTextScreen(x+150,y+40+((yspace++)*10),"\x03 A %c (%d,%d)",getBuildingWorkerCode(b),b.finalPosition.x,b.finalPosition.y);

            int x1 = b.finalPosition.x*32;
            int y1 = b.finalPosition.y*32;
            int x2 = (b.finalPosition.x + b.type.tileWidth())*32;
            int y2 = (b.finalPosition.y + b.type.tileHeight())*32;

            BWAPI::Broodwar->drawLineMap(b.builderUnit->getPosition().x,b.builderUnit->getPosition().y,(x1+x2)/2,(y1+y2)/2,BWAPI::Colors::Orange);
            BWAPI::Broodwar->drawBoxMap(x1,y1,x2,y2,BWAPI::Colors::Red,false);
        }
        else if (b.status == BuildingStatus::UnderConstruction)
        {
            BWAPI::Broodwar->drawTextScreen(x,y+40+((yspace)*10),"\x03 %s %d",b.type.getName().c_str(),b.buildingUnit->getID());
            BWAPI::Broodwar->drawTextScreen(x+150,y+40+((yspace++)*10),"\x03 Const %c",getBuildingWorkerCode(b));
        }
    }
}

BuildingManager & BuildingManager::Instance()
{
    static BuildingManager instance;
    return instance;
}

// The buildings queued and not yet started.
std::vector<BWAPI::UnitType> BuildingManager::buildingsQueued()
{
    std::vector<BWAPI::UnitType> buildingsQueued;

    for (const auto & b : _buildings)
    {
        if (b.status == BuildingStatus::Unassigned || b.status == BuildingStatus::Assigned)
        {
            buildingsQueued.push_back(b.type);
        }
    }

    return buildingsQueued;
}

// Cancel a given building when possible.
// Used as part of the extractor trick or in an emergency.
void BuildingManager::cancelBuilding(Building & b)
{
	if (b.status == BuildingStatus::Unassigned)
	{
		_reservedMinerals -= b.type.mineralPrice();
		_reservedGas -= b.type.gasPrice();
		undoBuildings({ b });
	}
	else if (b.status == BuildingStatus::Assigned)
	{
		_reservedMinerals -= b.type.mineralPrice();
		_reservedGas -= b.type.gasPrice();
		WorkerManager::Instance().finishedWithWorker(b.builderUnit);
		BuildingPlacer::Instance().freeTiles(b.finalPosition, b.type.tileWidth(), b.type.tileHeight());
		undoBuildings({ b });
	}
	else if (b.status == BuildingStatus::UnderConstruction)
	{
		if (b.buildingUnit && b.buildingUnit->exists() && !b.buildingUnit->isCompleted())
		{
			b.buildingUnit->cancelConstruction();
			BuildingPlacer::Instance().freeTiles(b.finalPosition, b.type.tileWidth(), b.type.tileHeight());

			/* Need to indicate the base is lost as well. */
			if (b.type.isResourceDepot() && b.macroLocation != MacroLocation::Macro && b.finalPosition.isValid())
			{
				InformationManager::Instance().baseLost(b.finalPosition);
			}
		}

		undoBuildings({ b });
	}
	else
	{
		UAB_ASSERT(false, "unexpected building status");
	}
}

// It's an emergency. Cancel all buildings which are not yet started.
void BuildingManager::cancelQueuedBuildings()
{
	for (Building & b : _buildings)
	{
		if (b.status == BuildingStatus::Unassigned || b.status == BuildingStatus::Assigned)
		{
			cancelBuilding(b);
		}
	}
}

// It's an emergency. Cancel all buildings of a given type.
void BuildingManager::cancelBuildingType(BWAPI::UnitType t)
{
	for (Building & b : _buildings)
	{
		if (b.type == t)
		{
			cancelBuilding(b);
		}
	}
}

// TODO fails in placing a hatchery after all others are destroyed - why?
BWAPI::TilePosition BuildingManager::getBuildingLocation(const Building & b)
{
	if (b.isGasSteal)
    {
        BWTA::BaseLocation * enemyBaseLocation = InformationManager::Instance().getEnemyMainBaseLocation();
        UAB_ASSERT(enemyBaseLocation,"Should find enemy base before gas steal");
        UAB_ASSERT(enemyBaseLocation->getGeysers().size() > 0,"Should have spotted an enemy geyser");

        for (auto & unit : enemyBaseLocation->getGeysers())
        {
            return BWAPI::TilePosition(unit->getInitialTilePosition());
        }
    }

	int numPylons = BWAPI::Broodwar->self()->completedUnitCount(BWAPI::UnitTypes::Protoss_Pylon);
	if (b.type.requiresPsi() && numPylons == 0)
	{
		return BWAPI::TilePositions::None;
	}

	if (b.type.isRefinery())
	{
		return BuildingPlacer::Instance().getRefineryPosition();
	}

	if (b.type.isResourceDepot())
	{
		BWTA::BaseLocation * natural = InformationManager::Instance().getMyNaturalLocation();
		if (b.macroLocation == MacroLocation::Natural && natural)
		{
			return natural->getTilePosition();
		}
		if (b.macroLocation != MacroLocation::Macro)
		{
			return MapTools::Instance().getNextExpansion(b.macroLocation == MacroLocation::Hidden, b.macroLocation == MacroLocation::MinOnly);
		}
		// Else if it's a macro hatchery, treat it like any other building.
	}

    int distance = Config::Macro::BuildingSpacing;
	if (b.type == BWAPI::UnitTypes::Terran_Bunker ||
		b.type == BWAPI::UnitTypes::Protoss_Photon_Cannon ||
		b.type == BWAPI::UnitTypes::Zerg_Creep_Colony)
	{
		// Pack defenses tightly together.
		distance = 0;
	}
	else if (b.type == BWAPI::UnitTypes::Protoss_Pylon)
    {
		if (numPylons < 3)
		{
			// Early pylons may be spaced differently than other buildings.
			distance = Config::Macro::PylonSpacing;
		}
		else
		{
			// Building spacing == 1 is usual. Be more generous with pylons.
			distance = 2;
		}
	}
	else if (b.type == BWAPI::UnitTypes::Zerg_Spawning_Pool || //these buildings might be used as walls
		//b.type == BWAPI::UnitTypes::Zerg_Evolution_Chamber || //risky, could end up walling
		b.type == BWAPI::UnitTypes::Zerg_Hydralisk_Den) {
		distance = 0;
	}

	// Try to pack protoss buildings more closely together. Space can run out.
	bool noVerticalSpacing = false;
	if (b.type == BWAPI::UnitTypes::Protoss_Gateway ||
		b.type == BWAPI::UnitTypes::Protoss_Forge || 
		b.type == BWAPI::UnitTypes::Protoss_Stargate || 
		b.type == BWAPI::UnitTypes::Protoss_Citadel_of_Adun || 
		b.type == BWAPI::UnitTypes::Protoss_Templar_Archives || 
		b.type == BWAPI::UnitTypes::Protoss_Gateway)
	{
		noVerticalSpacing = true;
	}

	// Get a position within our region.
	return BuildingPlacer::Instance().getBuildLocationNear(b, distance, noVerticalSpacing);
}

// The building failed or is canceled.
// Undo any connections with other data structures, then delete.
void BuildingManager::undoBuildings(const std::vector<Building> & toRemove)
{
	for (auto & b : toRemove)
	{
		// If the building was to establish a base, unreserve the base location.
		if (b.type.isResourceDepot() && b.macroLocation != MacroLocation::Macro && b.finalPosition.isValid())
		{
			InformationManager::Instance().unreserveBase(b.finalPosition);
		}
	}

	removeBuildings(toRemove);
}

// Remove buildings from the list of buildings--nothing more, nothing less.
void BuildingManager::removeBuildings(const std::vector<Building> & toRemove)
{
    for (auto & b : toRemove)
    {
		auto & it = std::find(_buildings.begin(), _buildings.end(), b);

        if (it != _buildings.end())
        {
            _buildings.erase(it);
        }
    }
}
