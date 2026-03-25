#pragma once

#include "include/glm/glm.hpp"

struct Params
{
	float gravity;
	float dt;

	glm::ivec3 gridRes;
	glm::vec3 gridOrigin;
	float cellSize;

	void init()	{
		gravity = 9.8f;
		dt = 0.f;
		gridRes = glm::ivec3(16, 16, 16);
		gridOrigin = glm::vec3(0.0f);
		cellSize = 1.0f;
	}
};

struct Particle
{
public:
	glm::vec3 pos;
	glm::vec3 vel;
	float mass;
};

struct GridCell
{
	float m;
	glm::vec3 v;
};
#pragma once
