#pragma once

#include <vector>
#include "include/glm/glm.hpp"

enum class MaterialType
{
    CUSTOM = 0,
    SAND = 1,
    SNOW = 2,
    JELLY = 3,
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

    float mu;
    float lambda;
    float hardening;

    float thetaC = 2.5e-2f;
    float thetaS = 7.5e-3f;
    float colliderFriction = 0.2f;

    MaterialType material = MaterialType::SAND;
	glm::vec3 domainCenter = glm::vec3(0.0f);
	glm::vec3 domainSize = glm::vec3(1.0f);
};

struct Particle
{
    glm::vec3 pos;
    glm::vec3 vel;
    float mass;
    float volume;
    glm::mat3 F;
    glm::mat3 C;
    float Jp;
    bool is_emitter_particle = false;
};

struct GridCell
{
    float m = 0.0f;
    glm::vec3 v = glm::vec3(0.0f);
    glm::vec3 f = glm::vec3(0.0f);
};