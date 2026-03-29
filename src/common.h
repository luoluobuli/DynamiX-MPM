#pragma once

#include "include/glm/glm.hpp"

struct Params
{
	float gravity;
	float dt;

	glm::ivec3 gridRes;
	glm::vec3 gridOrigin;
	float cellSize;

	float mu;
	float lambda;

	float thetaC = 2.5e-2;
	float thetaS = 7.5e-3;
};

struct Particle
{
	glm::vec3 pos;
	glm::vec3 vel;
	float mass;
	float volume;
	glm::mat3 F;
	float Jp;
};

struct GridCell
{
	float m;
	glm::vec3 v;
};
#pragma once
