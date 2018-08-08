#pragma once

#include "Common.h"
#include "MapGrid.h"
#include "SquadOrder.h"
#include "InformationManager.h"
#include "Micro.h"
#include "UnitUtil.h"

namespace UAlbertaBot
{
struct AirThreat
{
	BWAPI::Unit	unit;
	double			weight;
};

struct GroundThreat
{
	BWAPI::Unit	unit;
	double			weight;
};

class MicroManager
{
	BWAPI::Unitset  _units;

protected:
	
	SquadOrder			order;
	static std::map<BWAPI::Unit, int> specialPriorities;
	static int lastFrameComputedSpecialPriorities;

	//std::map<BWAPI::Unit, int> & specialPriorities;
	//int & lastFrameComputedSpecialPriorities;

	//std::map<BWAPI::Unit, int> & specialPriorities = InformationManager::Instance().specialPriorities;
	//int & lastFrameComputedSpecialPriorities = InformationManager::Instance().lastFrameComputedSpecialPriorities;


	BWAPI::Position kiteVector; //cumulative kite vector of all threats in the area
	BWAPI::Position tangentVector; //cumulative rotational vector of all relevant threats towards destination
	int accumulatedTangents;
	//must be computed for every unit separately
	void				initializeVectors();
	void				updateVectors(BWAPI::Unit, BWAPI::Unit, int = 4, BWAPI::Position = BWAPI::Position(-1,-1));
	void				normalizeVectors();
	BWAPI::Position		computeAttractionVector(BWAPI::Unit);

	virtual void        executeMicro(const BWAPI::Unitset & targets) = 0;
	bool                checkPositionWalkable(BWAPI::Position pos);
	void                drawOrderText();
	bool                unitNearEnemy(BWAPI::Unit unit);
	bool                unitNearChokepoint(BWAPI::Unit unit) const;
	void                trainSubUnits(BWAPI::Unit unit) const;
    
	int					getCommonPriority(BWAPI::Unit, BWAPI::Unit);
	void				computeSpecialPriorities(const BWAPI::Unitset &);
	int					getSpecialPriority(BWAPI::Unit, BWAPI::Unit);

	const BWAPI::Unit getClosestFriend(const BWAPI::Unit, const BWAPI::Unitset) const;

public:
						MicroManager();
    virtual				~MicroManager(){}

	const BWAPI::Unitset & getUnits() const;
	BWAPI::Position     calcCenter() const;

	void				setUnits(const BWAPI::Unitset & u);
	void				execute(const SquadOrder & order);
	void				regroup(const BWAPI::Position & regroupPosition) const;

	bool _fighting;
	bool _formed;
	bool _doNotForm;
	bool                formSquad(const BWAPI::Unitset & targets, int dist, int radius, double angle, int interval);

};

}