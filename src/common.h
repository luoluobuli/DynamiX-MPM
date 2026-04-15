#pragma once
#include <vector>
#include "include/glm/glm.hpp"


enum class MaterialType
{
	SAND = 0,
	SNOW = 1,
	JELLY = 2
};

struct CollisionSDF
{
	glm::ivec3 res = glm::ivec3(0);
	glm::vec3 origin = glm::vec3(0.0f);
	float dx = 1.0f;
	std::vector<float> phi;

	bool valid() const
	{
		return !phi.empty();
	}

	int index(int i, int j, int k) const
	{
		return i + j * res.x + k * res.x * res.y;
	}
};

struct Params
{
	float gravity;
	float dt;

	glm::ivec3 gridRes;
	glm::vec3 gridOrigin;
	float cellSize;

<<<<<<< Updated upstream
	void init()	{
		gravity = 9.8f;
		dt = 0.f;
		gridRes = glm::ivec3(16, 16, 16);
		gridOrigin = glm::vec3(0.0f);
		cellSize = 1.0f;
	}
=======
	float mu;
	float lambda;

	float thetaC = 2.5e-2;
	float thetaS = 7.5e-3;
	float colliderFriction = 0.2f;

	MaterialType material = MaterialType::SAND;

	glm::vec3 domainCenter = glm::vec3(0.0f);
	glm::vec3 domainSize = glm::vec3(1.0f);
>>>>>>> Stashed changes
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
	glm::vec3 f = glm::vec3(0.0f);
	float m = 0.0f;
	glm::vec3 v = glm::vec3(0.0f);
};
#pragma once
