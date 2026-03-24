#include "Grid.h"

Grid::Grid() {}
Grid::~Grid() {}

void Grid::clear() {
	for (auto& cell : cells) {
		cell.m = 0.0f;
		cell.v = glm::vec3(0.0f);
	}
}