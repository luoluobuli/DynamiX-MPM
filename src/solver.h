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

	// Setters
	void clear(Params params_in) {
		particles.clear();
		gridCells.clear();
		params = params_in;
	}

	void init(Params params_in) {
		particles.clear();
		gridCells.clear();
		params = params_in;

		gridCells.resize(params.gridRes.x * params.gridRes.y * params.gridRes.z);
	}
<<<<<<< Updated upstream

	void addParticle(Particle p) { particles.push_back(p); }
=======
>>>>>>> Stashed changes

	void setParams(Params params_in) { params = params_in; }

	void setDt(float dt_in) { params.dt = dt_in; }

	void setGravity(float gravity_in) { params.gravity = gravity_in; }

	void setCollisionSDF(const CollisionSDF& sdf_in) { collisionSDF = sdf_in; }
	void clearCollisionSDF() { collisionSDF = CollisionSDF(); }

	// Getters
	Particle getParticle(int index) { return particles[index]; }

	int getParticleCount() { return particles.size(); }

	// Helper functions
	int cellIndex(int i, int j, int k);

private:
	std::vector<Particle> particles;
	std::vector<GridCell> gridCells;
	Params params;
	CollisionSDF collisionSDF;

	void projectDeformation(Particle& p);
	void projectSand(Particle& p);
	void projectSnow(Particle& p);
	void projectJelly(Particle& p);

	float sampleSDF(const glm::vec3& p) const;
	glm::vec3 sampleSDFNormal(const glm::vec3& p) const;
	float fetch(int i, int j, int k) const;

	void applyParticleCollision(Particle& p);
	void applyGridCollision(int i, int j, int k, GridCell& cell);
};

