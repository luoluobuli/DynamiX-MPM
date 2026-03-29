#pragma once

#include "include/glm/glm.hpp"

struct Params
{
	float gravity;
	float dt;

	glm::ivec3 gridRes = glm::ivec3(16, 16, 16);
	glm::vec3 gridOrigin = glm::vec3(-0.5f);
	float cellSize = 1.0f;

	float mu;
	float lambda;
};

struct Particle
{
public:
	glm::vec3 pos;
	glm::vec3 vel;
	float mass;
	float volume;
	glm::mat3 F;
};

struct GridCell
{
	float m;
	glm::vec3 v;
};
#pragma once
