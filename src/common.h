#pragma once

#include "include/glm/glm.hpp"

struct Params
{
	float gravity;
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
