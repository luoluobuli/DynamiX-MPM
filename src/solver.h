#pragma once

#include <vector>
#include "common.h"

class Solver
{
public:
	Solver() {}
	~Solver() {}

	// Core simulation functions
	void step();
	void clearGrid();
	void particleToGrid();
	void updateGrid();
	void gridToParticle();

	void init(Params params_in) {
		particles.clear();
		gridCells.clear();
		params = params_in;

		gridCells.resize(params.gridRes.x * params.gridRes.y * params.gridRes.z);
	}

	// Setters
	void setParticles(std::vector<Particle> p) { particles = p; }

	void setParams(Params params_in) { params = params_in; }

	void setDt(float dt_in) { params.dt = dt_in; }

	void setGravity(float gravity_in) { params.gravity = gravity_in; }

	// Getters
	Particle getParticle(int index) { return particles[index]; }

	int getParticleCount() { return particles.size(); }

	// Helper functions
	int cellIndex(int i, int j, int k);
	glm::mat3 computeStress(const Particle& p) const;

private:
	std::vector<Particle> particles;
	std::vector<GridCell> gridCells;
	Params params;
};

