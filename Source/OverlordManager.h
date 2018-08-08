#pragma once;

#include <Common.h>
#include "MicroManager.h"
#include "MapTools.h"
#include "InformationManager.h"

namespace UAlbertaBot
{
	class MicroManager;

	class OverlordManager : public MicroManager
	{

		BWAPI::Player _self = BWAPI::Broodwar->self();

		//detector manager
		std::map<BWAPI::Unit, bool>	cloakedUnitMap;
		bool isAssigned(BWAPI::Unit unit);
		BWAPI::Unit unitClosestToEnemy;

		//new overlord functions

		BWAPI::Position mainBasePosition;

		std::map<BWAPI::Unit, BWTA::BaseLocation *> overlordToBase;
		std::map<BWTA::BaseLocation *, BWAPI::Unit> baseToOverlord;

		double overlordSpeed;
		int searchRadius; 

		BWAPI::Position OverlordManager::getSurveyLocation(BWAPI::Unit);

		void detect(const BWAPI::Unitset & overlords);
		void idle(const BWAPI::Unitset & overlords);

		bool overlordThreatDetected = false;

		bool runToAlly(BWAPI::Unit overlord, BWAPI::Unit attacker);
		bool runToBase(BWAPI::Unit overlord);
		void unloadAt(BWAPI::Unit overlord, BWAPI::Position destination);
		void cautiouslyTravel(BWAPI::Unit overlord, BWAPI::Position destination);

		//transport manager
		/*std::vector<BWAPI::Position>    _mapEdgeVertices;

		int                             _currentRegionVertexIndex;
		BWAPI::Position					_minCorner;
		BWAPI::Position					_maxCorner;
		std::vector<BWAPI::Position>	_waypoints;

		void							calculateMapEdgeVertices();
		void							drawTransportInformation(int x, int y);
		void							moveTransport();
		void							moveTroops();

		BWAPI::Position                 getFleePosition(int clockwise = 1);
		void                            followPerimeter(int clockwise = 1);
		void							followPerimeter(BWAPI::Position to, BWAPI::Position from);
		int                             getClosestVertexIndex(BWAPI::UnitInterface * unit);
		int								getClosestVertexIndex(BWAPI::Position p);
		std::pair<int, int>				findSafePath(BWAPI::Position from, BWAPI::Position to);*/
		/* end transport manager code */

	public:

		void executeMicro(const BWAPI::Unitset & targets);

		OverlordManager();
		~OverlordManager() {}

		//from detector manager
		void setUnitClosestToEnemy(BWAPI::Unit unit) { unitClosestToEnemy = unit; }

		BWAPI::Unit closestCloakedUnit(const BWAPI::Unitset & cloakedUnits, BWAPI::Unit detectorUnit);


		//from transport manager
		/*void							update();
		void							setTransportShip(BWAPI::UnitInterface * unit);
		void							setFrom(BWAPI::Position from);
		void							setTo(BWAPI::Position to);*/
	};
}