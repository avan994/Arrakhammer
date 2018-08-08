#include "Common.h"
#include "UnitData.h"

using namespace UAlbertaBot;

UnitData::UnitData() 
	: mineralsLost(0)
	, gasLost(0)
{
	int maxTypeID(0);
	for (const BWAPI::UnitType & t : BWAPI::UnitTypes::allUnitTypes())
	{
		maxTypeID = maxTypeID > t.getID() ? maxTypeID : t.getID();
	}

	numDeadUnits	    = std::vector<int>(maxTypeID + 1, 0);
	numUnits		    = std::vector<int>(maxTypeID + 1, 0);
}

void UnitData::updateUnit(BWAPI::Unit unit)
{
	if (!unit) { return; }

    bool firstSeen = false;
    auto & it = unitMap.find(unit);
    if (it == unitMap.end())
    {
        firstSeen = true;
        unitMap[unit] = UnitInfo();
    }
    
	UnitInfo & ui   = unitMap[unit];
    ui.unit         = unit;
	ui.updateFrame	= BWAPI::Broodwar->getFrameCount();
    ui.player       = unit->getPlayer();
	ui.lastPosition = unit->getPosition();
	ui.lastHealth   = unit->getHitPoints();
    ui.lastShields  = unit->getShields();
	ui.unitID       = unit->getID();
	ui.type         = unit->getType();
    ui.completed    = unit->isCompleted();

    if (firstSeen)
    {
        numUnits[unit->getType().getID()]++;
    }
}

void UnitData::removeUnit(BWAPI::Unit unit)
{
	if (!unit) { return; }

	BWAPI::UnitType type = unit->getType();

	/* The unit was infested. */
	if (unit->getType() == BWAPI::UnitTypes::Zerg_Infested_Command_Center &&
		unitMap[unit].type == BWAPI::UnitTypes::Terran_Command_Center) {
		type = unitMap[unit].type; 
	}

	mineralsLost += type.mineralPrice();
	gasLost += type.gasPrice();
	numUnits[type.getID()]--;
	numDeadUnits[type.getID()]++;
		
	unitMap.erase(unit);
}

void UnitData::removeBadUnits()
{
	for (auto iter(unitMap.begin()); iter != unitMap.end();)
	{
		if (badUnitInfo(iter->second))
		{
			numUnits[iter->second.type.getID()]--;
			iter = unitMap.erase(iter);
		}
		else
		{
			iter++;
		}
	}
}

BWAPI::TilePosition UnitData::findBadDepot()
{
	for (auto iter(unitMap.begin()); iter != unitMap.end();)
	{
		const UnitInfo & ui = iter->second;
		if ((ui.type.isResourceDepot() || ui.type == BWAPI::UnitTypes::Zerg_Lair ||
			ui.type == BWAPI::UnitTypes::Zerg_Hive) && 
			BWAPI::Broodwar->isVisible(ui.lastPosition.x / 32, ui.lastPosition.y / 32) && !ui.unit->isVisible())
		{
			BWAPI::TilePosition positionSave = BWAPI::TilePosition(ui.lastPosition);
			numUnits[iter->second.type.getID()]--;
			iter = unitMap.erase(iter); //erase the entry for now
			return positionSave;
		}
		else
		{
			iter++;
		}
	}

	return BWAPI::TilePosition(-1, -1);
}

const bool UnitData::badUnitInfo(const UnitInfo & ui) const
{
    if (!ui.unit)
    {
        return false;
    }

	// Cull away any refineries/assimilators/extractors that were destroyed and reverted to vespene geysers
	if (ui.unit->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser)
	{ 
		return true;
	}

	// If the unit is a building and we can currently see its position and it is not there.
	// NOTE A terran building could have lifted off and moved away.
	if (ui.type.isBuilding() && BWAPI::Broodwar->isVisible(ui.lastPosition.x/32, ui.lastPosition.y/32) && !ui.unit->isVisible())
	{
		return true;
	}

	return false;
}

int UnitData::getGasLost() const 
{ 
    return gasLost; 
}

int UnitData::getMineralsLost() const 
{ 
    return mineralsLost; 
}

int UnitData::getNumUnits(BWAPI::UnitType t) const 
{ 
    return numUnits[t.getID()]; 
}

int UnitData::getNumDeadUnits(BWAPI::UnitType t) const 
{ 
    return numDeadUnits[t.getID()]; 
}

const std::map<BWAPI::Unit,UnitInfo> & UnitData::getUnits() const 
{ 
    return unitMap; 
}