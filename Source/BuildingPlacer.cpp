#include "Common.h"
#include "BuildingPlacer.h"
#include "MapGrid.h"
#include "MapTools.h"

using namespace UAlbertaBot;

BuildingPlacer::BuildingPlacer()
    : _boxTop       (std::numeric_limits<int>::max())
    , _boxBottom    (std::numeric_limits<int>::lowest())
    , _boxLeft      (std::numeric_limits<int>::max())
    , _boxRight     (std::numeric_limits<int>::lowest())
{
    _reserveMap = std::vector< std::vector<bool> >(BWAPI::Broodwar->mapWidth(),std::vector<bool>(BWAPI::Broodwar->mapHeight(),false));

	for (auto base : BWTA::getBaseLocations()) {
		auto center = base->getPosition();
		computeResourceBox(center);
	}
}

BuildingPlacer & BuildingPlacer::Instance() 
{
    static BuildingPlacer instance;
    return instance;
}

bool BuildingPlacer::isInResourceBox(int x, int y) const
{
    int posX(x * 32);
    int posY(y * 32);

    return (posX >= _boxLeft) && (posX < _boxRight) && (posY >= _boxTop) && (posY < _boxBottom);
}

void BuildingPlacer::computeResourceBox(BWAPI::Position center)
{
	_boxTop = 0;
	_boxBottom = 0;
	_boxLeft = 0;
	_boxRight = 0;

	BWAPI::TilePosition px(1, 0);
	BWAPI::TilePosition py(0, 1);

	for (auto & unit : BWAPI::Broodwar->getUnitsInRadius(center, 300, BWAPI::Filter::IsResourceContainer))
    {
		auto tp = unit->getTilePosition();
		int x = unit->getPosition().x;
		int y = unit->getPosition().y;

		if (unit->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser) {
			//typical x/y differences are 7/5
			if (abs(center.x - x) > abs(center.y - y)) {
				if (center.x - x < 0) {
					reserveTiles(tp - px - py, 1, 4);
					reserveTiles(tp - px - px, 1, 1);
				}
				else {
					reserveTiles(tp + px + px + px + px - py, 1, 4);
					reserveTiles(tp + px + px + px + px + px, 1, 1);
				}
			}
			else {
				if (center.y - y < 0) {
					reserveTiles(tp + px - py - py, 1, 1);
					reserveTiles(tp - py, 4, 1);
				}
				else {
					reserveTiles(tp + px + py + py + py, 1, 1);
					reserveTiles(tp + py + py, 4, 1);
				}
			}
		}
		else {
			//typical x/y differences are 6/4
			reserveTiles(tp, 2, 1); //the mineral itself -- this purely makes it look prettier.

			bool xFar = abs(center.x - x) > 6 * 32;
			bool yFar = abs(center.y - y) > 4 * 32;
			if (center.x - x < 0) {
				reserveTiles(tp - px, 1, 1);
				if (xFar) reserveTiles(tp - px - px, 1, 1);
			}
			else {
				reserveTiles(tp + px + px, 1, 1);
				if (xFar) reserveTiles(tp + px + px + px, 1, 1);
			}

			if (center.y - y < 0) {
				reserveTiles(tp - py, 2, 1);
				if (yFar) reserveTiles(tp - py - py, 1, 1);
			}
			else {
				reserveTiles(tp + py, 2, 1);
				if (yFar) reserveTiles(tp + py + py, 1, 1);
			}
		}
    }
}

// makes final checks to see if a building can be built at a certain location
bool BuildingPlacer::canBuildHere(BWAPI::TilePosition position,const Building & b) const
{
    if (!BWAPI::Broodwar->canBuildHere(position,b.type,b.builderUnit))
    {
        return false;
    }

    // check the reserve map
    for (int x = position.x; x < position.x + b.type.tileWidth(); x++)
    {
        for (int y = position.y; y < position.y + b.type.tileHeight(); y++)
        {
            if (_reserveMap[x][y])
            {
                return false;
            }
        }
    }

    // if it overlaps a base location return false
    if (tileOverlapsBaseLocation(position,b.type))
    {
        return false;
    }

    return true;
}

bool BuildingPlacer::tileBlocksAddon(BWAPI::TilePosition position) const
{

    for (int i=0; i<=2; ++i)
    {
        for (auto unit : BWAPI::Broodwar->getUnitsOnTile(position.x - i,position.y))
        {
            if (unit->getType() == BWAPI::UnitTypes::Terran_Command_Center ||
                unit->getType() == BWAPI::UnitTypes::Terran_Factory ||
                unit->getType() == BWAPI::UnitTypes::Terran_Starport ||
                unit->getType() == BWAPI::UnitTypes::Terran_Science_Facility)
            {
                return true;
            }
        }
    }

    return false;
}

// Can we build this building here with the specified amount of space?
// Space value is buildDist. horizontalOnly means only horizontal spacing.
bool BuildingPlacer::canBuildHereWithSpace(BWAPI::TilePosition position,const Building & b,int buildDist,bool horizontalOnly) const
{
    //if we can't build here, we of course can't build here with space
    if (!canBuildHere(position,b))
    {
        return false;
    }

    // building height & width
    int width(b.type.tileWidth());
    int height(b.type.tileHeight());

    //leave space for add-ons
    if (b.type == BWAPI::UnitTypes::Terran_Command_Center ||
        b.type == BWAPI::UnitTypes::Terran_Factory ||
        b.type == BWAPI::UnitTypes::Terran_Starport ||
        b.type == BWAPI::UnitTypes::Terran_Science_Facility)
    {
        width += 2;
    }

    // define the rectangle of the building spot
    int startx = position.x - buildDist;
    int starty = position.y - buildDist;
    int endx   = position.x + width + buildDist;
	int endy = position.y + height + buildDist;

	if (b.type.isAddon())
	{
		const BWAPI::UnitType builderType = b.type.whatBuilds().first;

		BWAPI::TilePosition builderTile(position.x - builderType.tileWidth(), position.y + 2 - builderType.tileHeight());

		startx = builderTile.x - buildDist;
		starty = builderTile.y - buildDist;
		endx = position.x + width + buildDist;
		endy = position.y + height + buildDist;
	}

	if (horizontalOnly)
	{
		starty += buildDist;
		endy -= buildDist;
	}

	// if this rectangle doesn't fit on the map we can't build here
	if (startx < 0 || starty < 0 || endx > BWAPI::Broodwar->mapWidth() || endx < position.x + width || endy > BWAPI::Broodwar->mapHeight())
	{
		return false;
	}

	// if space is reserved, or it's in the resource box, we can't build here
	for (int x = startx; x < endx; x++)
	{
		for (int y = starty; y < endy; y++)
		{
			if (!b.type.isRefinery())
			{
				if (!buildable(b, x, y) || _reserveMap[x][y] || ((b.type != BWAPI::UnitTypes::Protoss_Photon_Cannon) && isInResourceBox(x, y)))
				{
					return false;
				}
			}
		}
	}

	return true;
}

BWAPI::TilePosition BuildingPlacer::getBuildLocationNear(const Building & b, int buildDist, bool horizontalOnly) const
{
	BWAPI::Position dp = BWAPI::Position(b.desiredPosition);
	int bd = buildDist;
	if (b.type == BWAPI::UnitTypes::Zerg_Creep_Colony ||
		b.type == BWAPI::UnitTypes::Zerg_Hydralisk_Den ||
		b.type == BWAPI::UnitTypes::Zerg_Evolution_Chamber ||
		b.type == BWAPI::UnitTypes::Zerg_Spawning_Pool) {


		if (b.defenseLocation == DefenseLocation::Chokepoint) {
			BWAPI::Position cp;

			if (true) { //b.macroLocation == MacroLocation::Main) {
				//cp = BWTA::getNearestChokepoint(b.desiredPosition)->getCenter();
				auto myStart = BWAPI::Broodwar->self()->getStartLocation();
				auto startLocs = BWAPI::Broodwar->getStartLocations();
				if (startLocs[0] == myStart)
					startLocs.pop_front();
				cp = BWTA::getShortestPath2(BWAPI::TilePosition(dp), startLocs[0]).front()->getCenter();
			}

			BWAPI::Position pDiff = cp - dp;
			double angle = atan2(pDiff.y, pDiff.x);
			int scale = 7*32;
			if (b.type == BWAPI::UnitTypes::Zerg_Creep_Colony) scale = 5*32;
			if (b.type == BWAPI::UnitTypes::Zerg_Spawning_Pool && BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran) {
				dp = dp - BWAPI::Position(int(scale * cos(angle)), int(scale * sin(angle)));
				//hide the pool from bunker and worker rushes
			}
			else {
				dp = dp + BWAPI::Position(int(scale * cos(angle)), int(scale * sin(angle)));
			}

			double distance = cp.getDistance(dp);
			if (distance > 25 * 32.0) { //something's not right, go back to default position
				dp = BWAPI::Position(b.desiredPosition);
			}

			BWAPI::Unitset buildingsNear = BWAPI::Broodwar->getUnitsInRadius(dp, 5 * 32, BWAPI::Filter::IsBuilding && !BWAPI::Filter::IsResourceContainer);
			int size = 0;
			//don't count extractors or hatcheries, in particular
			for (auto nearbyBuilding : buildingsNear) {
				auto type = nearbyBuilding->getType();
				if (type == BWAPI::UnitTypes::Zerg_Sunken_Colony ||
					type == BWAPI::UnitTypes::Zerg_Creep_Colony ||
					type == BWAPI::UnitTypes::Zerg_Spore_Colony ||
					type == BWAPI::UnitTypes::Zerg_Evolution_Chamber ||
					type == BWAPI::UnitTypes::Zerg_Hydralisk_Den) {
					size++;
				}
			}

			if (size >= 4 && bd == 0) { 
				//if there's a lot of buildings there already, demand more space.
				//an imperfect anti-walling measure
				bd = 1;
			}

			//compute some offset based on buildings near
			if (b.type == BWAPI::UnitTypes::Zerg_Creep_Colony) {
				BWAPI::Position offset = BWAPI::Position(0,0);
				int ysign = pDiff.y > 0 ? 1 : -1;
				int xsign = pDiff.x > 0 ? 1 : -1;
				if (abs(angle) < M_PI / 4 || abs(angle) > 3*M_PI/4) {
					//then build vertical
					offset = BWAPI::Position(xsign*(size % 2 * 32), ysign*(size/2 - 2) * 64);
				}
				else if (abs(abs(angle) - M_PI/2) < M_PI / 4) {
					//then build horizontal
					offset = BWAPI::Position(xsign*(size/2 - 2) * 64, ysign*(size % 2 * 32));
				}

				dp += offset;
			}

		}
		else if (b.defenseLocation == DefenseLocation::Minerals) {
			//get average mineral position
			auto minerals = BWAPI::Broodwar->getUnitsInRadius(dp, 6 * 32, BWAPI::Filter::IsMineralField);
			int n = 0;
			if (!minerals.empty()) {
				BWAPI::Position AvgMineralPosition(0, 0);
				for (auto & mineral : minerals) {
					n++;
					AvgMineralPosition += mineral->getPosition();
				}
				AvgMineralPosition = BWAPI::Position(AvgMineralPosition.x / n, AvgMineralPosition.y / n);

				dp = BWAPI::Position((dp.x + 2 * AvgMineralPosition.x) / 3, (dp.y + 2 * AvgMineralPosition.y) / 3);
			}

			//if we can't find any minerals, it doesn't matter. 
		}
		else if (b.defenseLocation == DefenseLocation::Random) {
			BWAPI::Position randomOffset(32 * rand() % 7, 32 * rand() % 7);
			dp += randomOffset;
		}
		/* else { //if b.defenseLocation == DefenseLocation::Normal
		//no need to do anything
		}*/
	} //end special defense buildings

    SparCraft::Timer t;
    t.start();

	// get the precomputed vector of tile positions which are sorted closest to this location
    const std::vector<BWAPI::TilePosition> & closestToBuilding = MapTools::Instance().getClosestTilesTo(dp);

    double ms1 = t.getElapsedTimeInMilliSec();

    // iterate through the list until we've found a suitable location
    for (size_t i(0); i < closestToBuilding.size(); ++i)
    {

		//give up trying to build the defense if we're going way out of range
		if (b.type == BWAPI::UnitTypes::Zerg_Creep_Colony && closestToBuilding[i].getDistance(BWAPI::TilePosition(dp)) > 8 * 32) return BWAPI::TilePositions::None;
        
		if (canBuildHereWithSpace(closestToBuilding[i],b,bd,horizontalOnly))
        {
            double ms = t.getElapsedTimeInMilliSec();
            // BWAPI::Broodwar->printf("Building Placer took %d iterations, lasting %lf ms @ %lf iterations/ms, %lf setup ms", i, ms, (i / ms), ms1);
			// BWAPI::Broodwar->printf("Building Placer took %d iterations, lasting %lf ms, finding %d, %d", i, ms, closestToBuilding[i].x, closestToBuilding[i].y);

            return closestToBuilding[i];
        }
    }

    double ms = t.getElapsedTimeInMilliSec();
    // BWAPI::Broodwar->printf("Building Placer took %lf ms, found nothing", ms);

    return  BWAPI::TilePositions::None;
}

bool BuildingPlacer::tileOverlapsBaseLocation(BWAPI::TilePosition tile, BWAPI::UnitType type) const
{
    // if it's a resource depot we don't care if it overlaps
    if (type.isResourceDepot())
    {
        return false;
    }

    // dimensions of the proposed location
    int tx1 = tile.x;
    int ty1 = tile.y;
    int tx2 = tx1 + type.tileWidth();
    int ty2 = ty1 + type.tileHeight();

    // for each base location
    for (BWTA::BaseLocation * base : BWTA::getBaseLocations())
    {
        // dimensions of the base location
        int bx1 = base->getTilePosition().x;
        int by1 = base->getTilePosition().y;
        int bx2 = bx1 + BWAPI::Broodwar->self()->getRace().getCenter().tileWidth();
        int by2 = by1 + BWAPI::Broodwar->self()->getRace().getCenter().tileHeight();

        // conditions for non-overlap are easy
        bool noOverlap = (tx2 < bx1) || (tx1 > bx2) || (ty2 < by1) || (ty1 > by2);

        // if the reverse is true, return true
        if (!noOverlap)
        {
            return true;
        }
    }

    // otherwise there is no overlap
    return false;
}

bool BuildingPlacer::buildable(const Building & b,int x,int y) const
{
	BWAPI::TilePosition tp(x, y);

	if (!tp.isValid())
	{
		return false;
	}

	if (!BWAPI::Broodwar->isBuildable(x, y, true))
    {
		// Unbuildable according to the map, or because the location is blocked
		// by a visible building. Unseen buildings (even if known) are "buildable" on.
        return false;
    }

	if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Terran && tileBlocksAddon(tp))
    {
        return false;
    }

	// getUnitsOnTile() only returns visible units, even if they are buildings.
    for (auto & unit : BWAPI::Broodwar->getUnitsOnTile(x,y))
    {
        if ((b.builderUnit != nullptr) && (unit != b.builderUnit))
        {
            return false;
        }
    }

    return true;
}

void BuildingPlacer::reserveTiles(BWAPI::TilePosition position,int width,int height)
{
    int rwidth = _reserveMap.size();
    int rheight = _reserveMap[0].size();
    for (int x = position.x; x < position.x + width && x < rwidth; x++)
    {
        for (int y = position.y; y < position.y + height && y < rheight; y++)
        {
            _reserveMap[x][y] = true;
        }
    }
}

void BuildingPlacer::drawReservedTiles()
{
    if (!Config::Debug::DrawReservedBuildingTiles)
    {
        return;
    }

    int rwidth = _reserveMap.size();
    int rheight = _reserveMap[0].size();

    for (int x = 0; x < rwidth; ++x)
    {
        for (int y = 0; y < rheight; ++y)
        {
            if (_reserveMap[x][y] || isInResourceBox(x,y))
            {
                int x1 = x*32 + 8;
                int y1 = y*32 + 8;
                int x2 = (x+1)*32 - 8;
                int y2 = (y+1)*32 - 8;

                BWAPI::Broodwar->drawBoxMap(x1,y1,x2,y2,BWAPI::Colors::Yellow,false);
            }
        }
    }
}

void BuildingPlacer::freeTiles(BWAPI::TilePosition position, int width, int height)
{
    int rwidth = _reserveMap.size();
    int rheight = _reserveMap[0].size();

    for (int x = position.x; x < position.x + width && x < rwidth; x++)
    {
        for (int y = position.y; y < position.y + height && y < rheight; y++)
        {
            _reserveMap[x][y] = false;
        }
    }
}

BWAPI::TilePosition BuildingPlacer::getRefineryPosition()
{
    BWAPI::TilePosition closestGeyser = BWAPI::TilePositions::None;
    int minGeyserDistanceFromHome = 100000;
	BWAPI::Position homePosition = InformationManager::Instance().getMyMainBaseLocation()->getPosition();

	// iterates through all valid geysers
	// whether or not they were previously built on and destroyed
	for (auto & geyser : BWAPI::Broodwar->getGeysers())
	{
		if (!geyser || !geyser->exists()) {
			continue;
		}

        // check to see if it's near one of our depots
        bool nearDepot = false;
        for (auto & unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType().isResourceDepot() && unit->getDistance(geyser) < 300)
            {
                nearDepot = true;
				break;
            }
        }

        if (nearDepot)
        {
            int homeDistance = geyser->getDistance(homePosition);

            if (homeDistance < minGeyserDistanceFromHome)
            {
                minGeyserDistanceFromHome = homeDistance;
                closestGeyser = geyser->getTilePosition();
            }
		}
    }

    return closestGeyser;
}

bool BuildingPlacer::isReserved(int x, int y) const
{
    int rwidth = _reserveMap.size();
    int rheight = _reserveMap[0].size();
    if (x < 0 || y < 0 || x >= rwidth || y >= rheight)
    {
        return false;
    }

    return _reserveMap[x][y];
}

