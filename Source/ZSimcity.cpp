#include "ZSimcity.h"

using namespace UAlbertaBot;

ZSimcity::ZSimcity()
	: computed(false)
	//, 
{
}

ZSimcity & ZSimcity::Instance()
{
	static ZSimcity instance;
	return instance;
}

bool ZSimcity::mostlyUnwalkable(BWAPI::TilePosition p, int scoreMin) {
	BWAPI::WalkPosition wp = BWAPI::WalkPosition(p);
	bool validated = true;
	int score = 0;
	for (auto x = 0; x < 4; x++) {
		for (auto y = 0; y < 4; y++) {
			auto walkP = BWAPI::WalkPosition(wp.x + x, wp.y + y);
			auto drawP = BWAPI::Position(walkP) + BWAPI::Position(4, 4);
			if (BWAPI::Broodwar->isWalkable(wp.x + x, wp.y + y)) {
				//BWAPI::Broodwar->drawCircleMap(drawP, 4, BWAPI::Colors::Orange);
			}
			else {
				score++;
				BWAPI::Broodwar->drawCircleMap(drawP, 4, BWAPI::Colors::Blue);
			}

			if (score >= scoreMin) return true;
		}
	}

	return false;
}

bool ZSimcity::checkBuildable(BWAPI::TilePosition sp, int x, int y) {
	//bool invalidated = false;
	//const BWAPI::Position halfTileOffset(16, 16);

	for (int i = 0; i < x; i++) {
		for (int j = 0; j < y; j++) {
			auto p = sp + BWAPI::TilePosition(i, j);
			if (!BWAPI::Broodwar->isBuildable(p, true)) {
				//BWAPI::Broodwar->drawCircleMap(BWAPI::Position(p) + halfTileOffset, 16, BWAPI::Colors::Red);
				//invalidated = true;
				return false;
			}
			else {
				//BWAPI::Broodwar->drawCircleMap(BWAPI::Position(p) + halfTileOffset, 16, BWAPI::Colors::Green);
			}
		}
	}

	//return !invalidated;

	return true;
}

//assume it's a hatchery for now
bool ZSimcity::checkGaps(BWAPI::TilePosition sp, int side, BWAPI::UnitType b) {
	const BWAPI::Position halfTileOffset(16, 16);

	//left or right
	if (side == LEFT || side == RIGHT) {
		int x = (side == LEFT) ? -1 : 4;

		bool found = false;
		for (int y = -1; y <= b.tileHeight(); y++) {
			auto p = BWAPI::TilePosition(sp.x + x, sp.y + y);

			if (mostlyUnwalkable(p)) {
				//break;

				found = true;
				BWAPI::Broodwar->drawCircleMap(BWAPI::Position(p) + halfTileOffset, 14, BWAPI::Colors::Blue);
			}
			else {
				//BWAPI::Broodwar->drawCircleMap(BWAPI::Position(p) + halfTileOffset, 14, BWAPI::Colors::Orange);
			}
		}

		return found;

	}
	//top or bottom
	else if (side == TOP || side == BOTTOM) {
		int y = (side == TOP) ? -1 : 3;

		bool found = false;
		for (int x = -1; x <= b.tileWidth(); x++) {
			auto p = BWAPI::TilePosition(sp.x + x, sp.y + y);

			if (mostlyUnwalkable(p)) {
				//break;
				BWAPI::Broodwar->drawCircleMap(BWAPI::Position(p) + halfTileOffset, 14, BWAPI::Colors::Blue);

				found = true;
			}
		}

		return found;
	}

	return false;
}

int ZSimcity::oppositeSide(int side) {
	switch (side) {
	case LEFT: return RIGHT;
	case RIGHT: return LEFT;
	case TOP: return BOTTOM;
	case BOTTOM: return TOP;
	default: return -1;
	}
}

//try placing a simcity hatchery at sp, checking the gaps at side
bool ZSimcity::tryPlaceHatchery(BWAPI::TilePosition sp, int side) {
	if (!BWAPI::Broodwar->canBuildHere(sp, BWAPI::UnitTypes::Zerg_Hatchery)) {
		BWAPI::Broodwar->drawCircleMap(BWAPI::Position(sp) + BWAPI::Position(16, 16), 8, BWAPI::Colors::Cyan);
		return false;
	}

	bool noGaps[4];
	for (int i = 0; i < 4; i++) {
		noGaps[i] = checkGaps(sp, i);
	}

	if (!noGaps[side]) {
		if (side == LEFT || side == RIGHT) {
			if (!noGaps[TOP] && !noGaps[BOTTOM]) {
				return false;
			}
		}
		else if (side == TOP || side == BOTTOM) {
			if (!noGaps[LEFT] && !noGaps[RIGHT]) {
				return false;
			}
		}
	}

	bool blocking = (noGaps[LEFT] && noGaps[RIGHT]) || (noGaps[TOP] && noGaps[BOTTOM]);
	if (blocking) return false;

	/*if (!checkGaps(sp, side)) {
	if (side == LEFT || side == RIGHT) {
	if (!checkGaps(sp, TOP) && !checkGaps(sp, BOTTOM)) {
	BWAPI::Broodwar->drawCircleMap(BWAPI::Position(sp) + BWAPI::Position(16, 16), 8, BWAPI::Colors::Brown);
	return false;
	}
	}
	else if (side == TOP || side == BOTTOM) {
	if (!checkGaps(sp, LEFT) && !checkGaps(sp, RIGHT)) {
	BWAPI::Broodwar->drawCircleMap(BWAPI::Position(sp) + BWAPI::Position(16, 16), 8, BWAPI::Colors::Orange);
	return false;
	}
	}
	}*/

	return true;
}

//compute the side
//returns an arbitrary 0 for perfectly horizontal/vertical lines
std::pair<int, int> ZSimcity::computeWhichSide(BWAPI::Position a, BWAPI::Position b, BWAPI::Position c) {
	int dx = b.x - a.x;
	int dy = b.y - a.y;
	if (dx == 0) return std::make_pair(c.x > b.x, 0);
	if (dy == 0) return std::make_pair(0, c.y > b.y);

	int slope = dy / dx;
	int yIntercept = a.y - slope * a.x;

	int yR = slope * c.x + yIntercept;
	int xR = (c.y - yIntercept) / slope;

	return std::make_pair(c.x > xR, c.y > yR);
}

void ZSimcity::computeWall() {
	//BWAPI::Broodwar->sendText("Result : %d %d", wall_test_result);
	for (auto wall_test_result : wall_test_results) {
		BWAPI::Position p(wall_test_result.first);
		BWAPI::Position p2(wall_test_result.second);


		BWAPI::Position p3 = BWAPI::Broodwar->getScreenPosition();
		BWAPI::Position estimatedSSCAITresolution(1280, 960);
		BWAPI::Position p4 = p3 + estimatedSSCAITresolution;

		//only display stuff in the viewport
		if ((p.x > p3.x || p2.x > p3.x) && (p.y > p3.y || p2.y > p3.y) && (p.x < p4.x || p2.x < p4.x) && (p.y < p4.y || p2.y < p4.y)) {
			BWAPI::Broodwar->drawBoxMap(p2, p2 + BWAPI::Position(32 * 3, 32 * 2), BWAPI::Colors::Blue);
			BWAPI::Broodwar->drawBoxMap(p, p + BWAPI::Position(32 * 4, 32 * 3), BWAPI::Colors::Red);
			BWAPI::Broodwar->drawLineMap(p, p2, BWAPI::Colors::Green);
		}
	}

	if (computed) return;

	auto startLocs = BWAPI::Broodwar->getStartLocations();
	auto myStart = BWAPI::Broodwar->self()->getStartLocation();
	if (startLocs[0] == myStart)
		startLocs.pop_front();

	for (auto base : BWTA::getBaseLocations()) {
		//if (base != InformationManager::Instance().getMyNaturalLocation()) continue;
		//if (base->getPosition() == BWAPI::Position(startLocs[0])) continue;
		if (!BWTA::isConnected(base->getTilePosition(), BWAPI::TilePosition(startLocs[0]))) continue;
		BWTA::Chokepoint * cpp = BWTA::getShortestPath2(base->getTilePosition(), startLocs[0]).front();
		if (!cpp) continue;
		auto sides = cpp->getSides();

		BWAPI::TilePosition wall_test_result(-1, -1);

		BWAPI::Position dp = base->getPosition();
		BWAPI::Position cp = cpp->getCenter();

		BWAPI::Broodwar->drawLineMap(dp, cp, BWAPI::Colors::Red);

		BWAPI::Position pDiff;
		auto hatcheryLocation = base->getTilePosition();
		int yOffset = -3;
		int end = 4; //5
		BWAPI::TilePosition start(-1, -1);

		/*int xSide;
		if (pDiff.x > 0) {
		start = hatcheryLocation + BWAPI::TilePosition(3 + 4, yOffset);
		xSide = RIGHT;
		}
		else if (pDiff.x <= 0) {
		start = hatcheryLocation + BWAPI::TilePosition(-3 - 4, yOffset);
		xSide = LEFT;
		}*/
		//else {
		//check both?
		//}



		//we check both sides here and filter the results later
		for (int xSide = 0; xSide <= 1; xSide++) { //try both sides
			start = hatcheryLocation + ((xSide == LEFT) ? BWAPI::TilePosition(-3 - 4, yOffset) : BWAPI::TilePosition(3 + 4, yOffset));

			for (int i = -1; i < 8; i++) {
				//try placing the hatchery first
				auto p = start + BWAPI::TilePosition(0, i);
				bool success = tryPlaceHatchery(p, xSide);
				if (!success) {
					BWAPI::Broodwar->drawCircleMap(BWAPI::Position(p) + BWAPI::Position(16, 16), 8, BWAPI::Colors::Red);
					continue;
				}
				else {
					BWAPI::Broodwar->drawCircleMap(BWAPI::Position(p) + BWAPI::Position(16, 16), 8, BWAPI::Colors::Green);
				}

				//try placing a den next to it
				auto p2 = p + ((xSide == 0) ? BWAPI::TilePosition(4, 0) : BWAPI::TilePosition(-3, 0));

				//try the best positions first, in order
				int ySide = 2 + computeWhichSide(BWAPI::Position(p), BWAPI::Position(hatcheryLocation), cp).second;

				int ymax = std::min(hatcheryLocation.y, p.y) + 3;
				int ymin = std::max(hatcheryLocation.y, p.y) - 1;

				success = false;
				int n = 0;
				BWAPI::Broodwar->drawLineMap(32 * p2.x + 1, 32 * ymin, 32 * p2.x + 1, 32 * ymax, BWAPI::Colors::Grey);
				while (!success) {
					if (ySide == BOTTOM) {
						p2.y = ymin + n;

						if (p2.y > ymax) break;
					}
					else if (ySide == TOP) {
						p2.y = ymax - n;

						if (p2.y < ymin) break;
					}

					success = checkBuildable(p2, 3, 2);

					if (success && !computed) wall_test_results.push_back(std::make_pair(p, p2));

					n++;
				}


			}



			// This section tries a different kind of placement; still uses the loop on xSide
			//----------------------
			int ySide;
			pDiff = cp - dp;
			if (pDiff.y < 0) {
				ySide = TOP;

				int sign = (xSide == LEFT) ? 1 : -1;
				if (xSide == LEFT) {
					start = start + BWAPI::TilePosition(1, 0);
				}
				else if (xSide == RIGHT) {
					start = start - BWAPI::TilePosition(1, 0);
				}
				else {
					break;
				}

				bool found = false;
				for (int j = -1; j < 2; j++) {
					//place the den first
					BWAPI::TilePosition p2 = hatcheryLocation + BWAPI::TilePosition((xSide == LEFT ? -3 : 4), j);
					if (!checkBuildable(p2, 3, 2)) continue;

					for (int i = 0; i < 5; i++) {
						if (i >= 3 && j == 1) break;

						auto p = start + BWAPI::TilePosition(sign*i, j);
						bool success = tryPlaceHatchery(p, ySide);
						if (!success) {
							BWAPI::Broodwar->drawCircleMap(BWAPI::Position(p) + BWAPI::Position(16, 16), 8, BWAPI::Colors::Red);
							continue;
						}
						else {
							BWAPI::Broodwar->drawCircleMap(BWAPI::Position(p) + BWAPI::Position(16, 16), 8, BWAPI::Colors::Green);
						}

						//record the success. we iterated in the order of the best one first, so move on
						if (!computed) wall_test_results.push_back(std::make_pair(p, p2));

						found = true;

						break;
					}
					if (found) break;
				}

			}
			else if (pDiff.y > 0) {
				ySide = BOTTOM;

				int sign = (xSide == LEFT) ? 1 : -1;
				start = start + BWAPI::TilePosition(0, 3 + 3); //below the hatchery
				if (xSide == LEFT) {
					start = start + BWAPI::TilePosition(1, 0);
				}
				else if (xSide == RIGHT) {
					start = start - BWAPI::TilePosition(1, 0);
				}
				else {
					break;
				}

				bool found = false;
				for (int j = 0; j < 4; j++) {
					//place the den first
					BWAPI::TilePosition p2 = hatcheryLocation + BWAPI::TilePosition((xSide == LEFT ? -2 : 3) + sign*j, 3);
					if (!checkBuildable(p2, 3, 2)) continue;

					for (int i = 0; i < 2; i++) {
						auto p = start + BWAPI::TilePosition(sign*j, i);
						bool success = tryPlaceHatchery(p, ySide);
						if (!success) {
							BWAPI::Broodwar->drawCircleMap(BWAPI::Position(p) + BWAPI::Position(16, 16), 12, BWAPI::Colors::Red);
							continue;
						}
						else {
							BWAPI::Broodwar->drawCircleMap(BWAPI::Position(p) + BWAPI::Position(16, 16), 12, BWAPI::Colors::Green);
						}

						//record the success. we iterated in the order of the best one first, so move on
						if (!computed) wall_test_results.push_back(std::make_pair(p, p2));

						found = true;
						break;
					}
					if (found) break; //the further in we go, presumably the worse the result is
				}

			}
		} // end xSide loop

		//filter the test results for this base


	}

	computed = true;
}