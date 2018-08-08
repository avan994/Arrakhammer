#pragma once

#include "Common.h"

namespace UAlbertaBot
{
	class ZSimcity
	{

	private:
		static const int LEFT = 0;
		static const int RIGHT = 1;
		static const int TOP = 3;
		static const int BOTTOM = 2;


		std::vector< std::pair<BWAPI::TilePosition, BWAPI::TilePosition> > wall_test_results;
		bool computed = false;


		bool mostlyUnwalkable(BWAPI::TilePosition p, int scoreMin = 8);
		bool checkBuildable(BWAPI::TilePosition sp, int x, int y);
		bool checkGaps(BWAPI::TilePosition sp, int side, BWAPI::UnitType b = BWAPI::UnitTypes::Zerg_Hatchery);
		int oppositeSide(int side);

		bool tryPlaceHatchery(BWAPI::TilePosition sp, int side);
		std::pair<int, int> computeWhichSide(BWAPI::Position a, BWAPI::Position b, BWAPI::Position c);

		ZSimcity();

	protected:

	public:
		//Currently just a test function that draws out placements
		void computeWall();


		static ZSimcity & ZSimcity::Instance();
	};

}