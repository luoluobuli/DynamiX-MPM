#pragma once

#include <vector>
#include "common.h"

class Grid
{
public:
	Grid();
	Grid(int resX, int resY, int resZ) : resolution(glm::ivec3(resX, resY, resZ)) {}
	~Grid();

	void clear();

private:
	glm::ivec3 resolution = glm::ivec3(10, 10, 10);
	float cellSize = 0.5;
	float invCellSize = 1 / 0.5;
	glm::vec3 origin = glm::vec3(0.f);
	std::vector<GridCell> cells;
};