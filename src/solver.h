#pragma once

#include <vector>
#include "common.h"
#include "grid.h"

class Solver
{
public:
	Solver();
	~Solver();

	void step();
	void clearGrid();
	void particleToGrid();
	void updateGrid();
	void gridToParticle();

private:
	std::vector<Particle> particles;
	Grid grid;
	Params params;
};

