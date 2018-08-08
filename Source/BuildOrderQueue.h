#pragma once

#include "Common.h"

#include "MacroAct.h"
#include "BuildingData.h"

namespace UAlbertaBot
{
struct BuildOrderItem
{
    MacroAct macroAct;	   // the thing we want to produce
    bool     isGasSteal;
	size_t   defenseLocation;

    BuildOrderItem(MacroAct m, bool gasSteal = false, size_t defenseLocation = DefenseLocation::Chokepoint)
        : macroAct(m)
        , isGasSteal(gasSteal) 
		, defenseLocation(defenseLocation)
    {
    }
};

class BuildOrderQueue
{
    std::deque< BuildOrderItem > queue;

	void queueItem(BuildOrderItem b);							// queues something with a given priority

public:

    BuildOrderQueue();

    void clearAll();											// clears the entire build order queue
    void queueAsHighestPriority(MacroAct m,bool gasSteal = false, size_t defenseLocation = DefenseLocation::Chokepoint);		// queues something at the highest priority
    void queueAsLowestPriority(MacroAct m);						// queues something at the lowest priority
    void removeHighestPriorityItem();							// removes the highest priority item

    size_t size() const;										// returns the size of the queue
	bool isEmpty() const;
	
    void removeAll(MacroAct m);									// removes all matching meta types from queue

    BuildOrderItem & getHighestPriorityItem();					// returns the highest priority item
	BWAPI::UnitType getNextUnit();								// skip commands and return item if it's a unit
	int getNextGasCost(int n);									// look n ahead, return next nonzero gas cost
	
	bool anyInQueue(BWAPI::UpgradeType type);
	bool anyInQueue(BWAPI::UnitType type);
	size_t numInQueue(BWAPI::UnitType type);
	void totalCosts(int & minerals, int & gas);

	void drawQueueInformation(int x, int y, bool outOfBook);

    // overload the bracket operator for ease of use
    BuildOrderItem operator [] (int i);
};
}