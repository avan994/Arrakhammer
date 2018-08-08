#pragma once

#include "Common.h"
#include "BuildingData.h"
//#include "MacroAct.h"
#include "InformationManager.h"

namespace UAlbertaBot
{


class BuildingPlacer
{
    BuildingPlacer();

    std::vector< std::vector<bool> > _reserveMap;

    int     _boxTop;
    int	    _boxBottom;
    int	    _boxLeft;
    int	    _boxRight;

    void    computeBuildableTileDistance(BWAPI::TilePosition tp);

public:

    static BuildingPlacer & Instance();

    // queries for various BuildingPlacer data
    bool					buildable(const Building & b,int x,int y) const;
    bool					isReserved(int x,int y) const;
    bool					isInResourceBox(int x,int y) const;
    bool					tileOverlapsBaseLocation(BWAPI::TilePosition tile,BWAPI::UnitType type) const;
    bool                    tileBlocksAddon(BWAPI::TilePosition position) const;

    // determines whether we can build at a given location
    bool					canBuildHere(BWAPI::TilePosition position,const Building & b) const;
    bool					canBuildHereWithSpace(BWAPI::TilePosition position,const Building & b,int buildDist,bool horizontalOnly = false) const;

    // returns a build location near a building's desired location
    BWAPI::TilePosition		getBuildLocationNear(const Building & b,int buildDist,bool horizontalOnly = false) const;

	void					reserveTiles(BWAPI::TilePosition position, int width, int height);
    void					freeTiles(BWAPI::TilePosition position,int width,int height);

    void					drawReservedTiles();
    void					computeResourceBox(BWAPI::Position);

    BWAPI::TilePosition		getRefineryPosition();

};
}