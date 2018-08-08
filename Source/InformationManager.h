#pragma once

#include "Common.h"
#include "BWTA.h"

#include "Base.h"
#include "UnitData.h"

#include "..\..\SparCraft\source\SparCraft.h"

namespace UAlbertaBot
{

class InformationManager 
{
	BWAPI::Player	_self;
    BWAPI::Player	_enemy;

	bool			_enemyProxy;

	bool			_weHaveCombatUnits;
	bool			_enemyHasAntiAir;
	bool			_enemyHasAirTech;
	bool			_enemyHasCloakTech;
	bool			_enemyHasMobileCloakTech;
	bool			_enemyHasOverlordHunters;
	bool			_enemyHasMobileDetection;

	std::map<BWAPI::Player, UnitData>                   _unitData;
	std::map<BWAPI::Player, BWTA::BaseLocation *>       _mainBaseLocations;
	BWTA::BaseLocation *								_myNaturalBaseLocation;  // whether taken yet or not
	std::map<BWAPI::Player, std::set<BWTA::Region *> >  _occupiedRegions;        // contains any building
	std::map<BWTA::BaseLocation *, Base *>			_theBases;

	InformationManager();

	void					initializeTheBases();
	void                    initializeRegionInformation();
	void					initializeNaturalBase();

	int                     getIndex(BWAPI::Player player) const;

	void					baseInferred(BWTA::BaseLocation * base);
	void					baseFound(BWAPI::Unit depot);
	void					maybeAddBase(BWAPI::Unit unit);
	bool					closeEnough(BWAPI::TilePosition a, BWAPI::TilePosition b);
	void					chooseNewMainBase();

	void                    updateUnit(BWAPI::Unit unit);
    void                    updateUnitInfo();
    void                    updateBaseLocationInfo();
    void                    updateOccupiedRegions(BWTA::Region * region,BWAPI::Player player);
    bool                    isValidUnit(BWAPI::Unit unit);

	void					sortInformation();

	void					checkBaseLost(BWAPI::Unit unit);

public:
	BWAPI::Unitset scarabs;
	BWAPI::Unitset nukes;
	BWAPI::Unitset hazards;
	BWAPI::Bulletset storms;
	BWAPI::Bulletset emps; //unused
	BWAPI::Bulletset ensnares; //unused
	std::map<BWAPI::Unit, int> specialPriorities;
	int lastFrameComputedSpecialPriorities;


	void					baseLost(BWAPI::TilePosition basePosition);

    void                    update();

    // event driven stuff
	void					onUnitShow(BWAPI::Unit unit)        { updateUnit(unit); maybeAddBase(unit); }
    void					onUnitHide(BWAPI::Unit unit)        { updateUnit(unit); }
	void					onUnitCreate(BWAPI::Unit unit)		{ updateUnit(unit); maybeAddBase(unit); }
	void					onUnitComplete(BWAPI::Unit unit)    { checkBaseLost(unit); updateUnit(unit); } //Note complete is called before morph upon hatchery cancel
	void					onUnitMorph(BWAPI::Unit unit)       { updateUnit(unit); maybeAddBase(unit); }
	void					onUnitRenegade(BWAPI::Unit unit)    { updateUnit(unit); }
    void					onUnitDestroy(BWAPI::Unit unit);

	bool					isEnemyBuildingInRegion(BWTA::Region * region);
    int						getNumUnits(BWAPI::UnitType type,BWAPI::Player player);
    bool					nearbyForceHasCloaked(BWAPI::Position p,BWAPI::Player player,int radius);
    bool					isCombatUnit(BWAPI::UnitType type) const;

    void                    getNearbyForce(std::vector<UnitInfo> & unitInfo,BWAPI::Position p,BWAPI::Player player,int radius);

    const UIMap &           getUnitInfo(BWAPI::Player player) const;

    std::set<BWTA::Region *> &  getOccupiedRegions(BWAPI::Player player);
    BWTA::BaseLocation *    getMainBaseLocation(BWAPI::Player player);
	BWTA::BaseLocation *	getMyMainBaseLocation();
	BWTA::BaseLocation *	getEnemyMainBaseLocation();
	BWAPI::Player			getBaseOwner(BWTA::BaseLocation * base);
	BWAPI::Unit 			getBaseDepot(BWTA::BaseLocation * base);
	BWTA::BaseLocation *	getMyNaturalLocation();
	BWTA::BaseLocation *	getMyLeastDefendedLocation();

	int						getTotalNumBases() const;
	int						getNumBases(BWAPI::Player player);
	int						getNumFreeLandBases();
	int						getMyNumMineralPatches();
	int						getMyNumGeysers();
	void					getMyGasCounts(int & nRefineries, int & nFreeGeysers);

	int						getAir2GroundSupply(BWAPI::Player player) const;

	bool					isBaseReserved(BWTA::BaseLocation * base);
	void					reserveBase(BWTA::BaseLocation * base);
	void					unreserveBase(BWTA::BaseLocation * base);
	void					unreserveBase(BWAPI::TilePosition baseTilePosition);

	bool					weHaveCombatUnits();

	bool					enemyHasAntiAir();
	bool					enemyHasAirTech();
	bool                    enemyHasCloakTech();
	bool                    enemyHasMobileCloakTech();
	bool					enemyHasOverlordHunters();
	bool					enemyHasMobileDetection();

	bool					needAirDefense();

	int						nScourgeNeeded();           // zerg specific

    void                    drawExtendedInterface();
    void                    drawUnitInformation(int x,int y);
    void                    drawMapInformation();
	void					drawBaseInformation(int x, int y);

    const UnitData &        getUnitData(BWAPI::Player player) const;

	// yay for singletons!
	static InformationManager & Instance();
};
}
