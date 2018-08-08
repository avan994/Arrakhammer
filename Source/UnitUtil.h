#pragma once

#include <Common.h>
#include <BWAPI.h>

namespace UAlbertaBot
{
namespace UnitUtil
{      
	bool IsMorphedBuildingType(BWAPI::UnitType unitType);
	
	bool IsThreat(BWAPI::Unit unit, BWAPI::Unit target, bool includeDangerous = true);
	bool IsFacing(BWAPI::Unit unit, BWAPI::Unit target, int slack = 2);
	bool IsCombatSimUnit(BWAPI::UnitType type);
	bool IsCombatUnit(BWAPI::Unit unit);
    bool IsValidUnit(BWAPI::Unit unit);
	bool CanAttackInSwarm(BWAPI::Unit unit);
    
	bool CanAttackAir(BWAPI::Unit unit);
	bool TypeCanAttackAir(BWAPI::UnitType);
    bool CanAttackGround(BWAPI::Unit unit);
	bool TypeCanAttackGround(BWAPI::UnitType);
    bool IsGroundTarget(BWAPI::Unit unit);
    bool IsAirTarget(BWAPI::Unit unit);

	bool IsMelee(BWAPI::Unit, BWAPI::Unit);

    bool CanAttack(BWAPI::Unit attacker, BWAPI::Unit target);
    bool CanAttack(BWAPI::UnitType attacker, BWAPI::UnitType target);
	bool CanEscape(BWAPI::Unit, BWAPI::Unit);
    double CalculateLTD(BWAPI::Unit attacker, BWAPI::Unit target);
    int AttackRange(BWAPI::Unit attacker, BWAPI::Unit target);
	int GetAttackRangeAssumingUpgrades(BWAPI::UnitType attacker, BWAPI::UnitType target);
    int GetTransportSize(BWAPI::UnitType type);

	int GetDamage(BWAPI::Unit unit, BWAPI::Unit target);

    size_t GetAllUnitCount(BWAPI::UnitType type);
	size_t GetCompletedUnitCount(BWAPI::UnitType type);
	
    BWAPI::Unit GetClosestUnitTypeToTarget(BWAPI::UnitType type, BWAPI::Position target);
    BWAPI::WeaponType GetWeapon(BWAPI::Unit attacker, BWAPI::Unit target);
    BWAPI::WeaponType GetWeapon(BWAPI::UnitType attacker, BWAPI::UnitType target);

    double GetDistanceBetweenTwoRectangles(Rect & rect1, Rect & rect2);
    Rect GetRect(BWAPI::Unit unit);
};
}