#pragma once

#include <vector>
#include "common.h"

class Grid
{
public:
	Grid();
	~Grid();

	void clearGrid();

private:
	glm::ivec3 resolution;
	float cellSize;
	float invCellSize;
	glm::vec3 origin;
	std::vector<GridCell> cells;
};

