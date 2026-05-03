#include "Solver.h"
#include <limits>

static inline void quadraticWeights(float f, float w[3])
{
	w[0] = 0.5f * (1.5f - f) * (1.5f - f);
	w[1] = 0.75f - (f - 1.0f) * (f - 1.0f);
	w[2] = 0.5f * (f - 0.5f) * (f - 0.5f);
}

int Solver::cellIndex(int i, int j, int k) {
	return i + j * params.gridRes.x + k * params.gridRes.x * params.gridRes.y;
}

void Solver::step() {
	colliderVelocity = (colliderTranslation - prevColliderTranslation) / params.dt;
	prevColliderTranslation = colliderTranslation;

	clearGrid();
	particleToGrid();
	updateGrid();
	gridToParticle();
}

void Solver::clearGrid() {
	//for (auto& cell : gridCells) {
	//	cell.m = 0.0f;
	//	cell.v = glm::vec3(0.0f);
	//}

	for (auto coord : activeNodes)
	{
		int idx = cellIndex(coord.x, coord.y, coord.z);
		gridCells[idx].m = 0.0f;
		gridCells[idx].v = glm::vec3(0.0f);
	}
	activeNodes.clear();
}

void Solver::particleToGrid() {
	float invCellSize = 1.f / params.cellSize;

	for (const auto& p : particles) {
		// Find particle's grid coordinates
		glm::vec3 gridCoord = (p.pos - params.gridOrigin) * invCellSize;

		// Find the base cell& neighboring cells
		glm::ivec3 base = glm::ivec3(
			floor(gridCoord.x - 0.5f),
			floor(gridCoord.y - 0.5f),
			floor(gridCoord.z - 0.5f)
		);

		glm::vec3 fx = gridCoord - glm::vec3(base);

		float wx[3], wy[3], wz[3];
		quadraticWeights(fx.x, wx);
		quadraticWeights(fx.y, wy);
		quadraticWeights(fx.z, wz);

		// Compute stress
		float J = glm::determinant(p.F);
		if (!std::isfinite(J) || J <= 1e-6f) continue;

		float mu = params.mu;
		float lambda = params.lambda;

		if (params.material == MaterialType::SNOW)
		{
			float h = expf(params.hardening * (1.0f - p.Jp));

			// Optional safety clamp
			h = glm::clamp(h, 0.2f, 10.0f);

			mu *= h;
			lambda *= h;
		}

		glm::mat3 FinvT = glm::transpose(glm::inverse(p.F));
		glm::mat3 stress = mu * (p.F - FinvT) + lambda * log(J) * FinvT;

		// MLS-MPM
		glm::mat3 stressTerm = (-4.f * invCellSize * invCellSize * params.dt * p.volume) * stress * glm::transpose(p.F);
		glm::mat3 affine = stressTerm + p.mass * p.C;

		for (int i = 0; i < 3; ++i) {
			for (int j = 0; j < 3; ++j) {
				for (int k = 0; k < 3; ++k) {
					int cellX = base.x + i;
					int cellY = base.y + j;
					int cellZ = base.z + k;
					// Check bounds
					if (cellX < 0 || cellX >= params.gridRes.x ||
						cellY < 0 || cellY >= params.gridRes.y ||
						cellZ < 0 || cellZ >= params.gridRes.z) {
						continue;
					}

					// Compute weight
					float w = wx[i] * wy[j] * wz[k];
					glm::vec3 dpos = (glm::vec3(i, j, k) - fx) * params.cellSize;

					GridCell& cell = gridCells[cellIndex(cellX, cellY, cellZ)];
					if (cell.m == 0.0f)
					{
						activeNodes.push_back(glm::ivec3(cellX, cellY, cellZ));
					}

					cell.m += w * p.mass;
					cell.v += w * (p.mass * p.vel + affine * dpos);
				}
			}
		}
	}
}

void Solver::updateGrid() {
	//for (int k = 0; k < params.gridRes.z; ++k) {
	//	for (int j = 0; j < params.gridRes.y; ++j) {
	//		for (int i = 0; i < params.gridRes.x; ++i) {
	//			GridCell& cell = gridCells[cellIndex(i, j, k)];
	//			if (cell.m > 0.01f) {
	//				cell.v /= cell.m;
	//				cell.v.y -= params.gravity * params.dt;

	//				applyGridCollision(i, j, k, cell);
	//			}
	//			else {
	//				cell.v = glm::vec3(0.0f);
	//			}
	//		}
	//	}
	//}

	for (auto coord : activeNodes)
	{
		int idx = cellIndex(coord.x, coord.y, coord.z);
		GridCell& cell = gridCells[idx];

		if (cell.m <= 0.0f)
			continue;

		cell.v /= cell.m;
		cell.v.y -= params.gravity * params.dt;

		applyGridCollision(coord.x, coord.y, coord.z, cell);
	}
}

void Solver::gridToParticle() {
	float invCellSize = 1.f / params.cellSize;

	for (auto& p : particles) {
		glm::vec3 gridCoord = (p.pos - params.gridOrigin) * invCellSize;

		glm::ivec3 base = glm::ivec3(
			floor(gridCoord.x - 0.5f),
			floor(gridCoord.y - 0.5f),
			floor(gridCoord.z - 0.5f)
		);

		glm::vec3 fx = gridCoord - glm::vec3(base);

		float wx[3], wy[3], wz[3];
		quadraticWeights(fx.x, wx);
		quadraticWeights(fx.y, wy);
		quadraticWeights(fx.z, wz);

		glm::vec3 newVel(0.0f);
		glm::mat3 newC(0.0f);

		for (int i = 0; i < 3; ++i) {
			for (int j = 0; j < 3; ++j) {
				for (int k = 0; k < 3; ++k) {
					int cellX = base.x + i;
					int cellY = base.y + j;
					int cellZ = base.z + k;
					if (cellX < 0 || cellX >= params.gridRes.x ||
						cellY < 0 || cellY >= params.gridRes.y ||
						cellZ < 0 || cellZ >= params.gridRes.z) {
						continue;
					}

					// Weight
					float w = wx[i] * wy[j] * wz[k];
					glm::vec3 dpos = glm::vec3(i, j, k) - fx;

					GridCell& cell = gridCells[cellIndex(cellX, cellY, cellZ)];
					
					newVel += w * cell.v;
					newC += 4.0f * invCellSize * w * glm::outerProduct(cell.v, dpos);
				}
			}
		}
		p.vel = newVel;
		if (p.is_emitter_particle) {
			p.vel.y -= 9.8f * params.dt;
		}
		
		p.C = newC;
		p.pos += p.vel * params.dt;
		 
		// deformation update
		p.F = (glm::mat3(1.0f) + params.dt * p.C) * p.F;

		// material specific projection
		projectDeformation(p);

		// sdf particle collision
		applyParticleCollision(p);
	}
}


void Solver::projectDeformation(Particle& p)
{
	switch (params.material)
	{
	case MaterialType::SAND:
		projectSand(p);
		break;

	case MaterialType::SNOW:
		projectSnow(p);
		break;

	case MaterialType::JELLY:
		projectJelly(p);
		break;
	case MaterialType::CUSTOM:
		projectDefault(p);
		break;
	default:
		projectDefault(p);
		break;
	}
}

void Solver::projectDefault(Particle& p) {
	// Elastic deformation
	float J = glm::determinant(p.F);

	// Plastic deformation
	if (std::isfinite(J) && J > 1e-6f)
	{
		float Jmin = 1.0f - params.thetaC;
		float Jmax = 1.0f + params.thetaS;

		float clampedJ = glm::clamp(J, Jmin, Jmax);
		float scale = cbrtf(clampedJ / J);
		p.F *= scale;

		p.Jp *= J / clampedJ;
	}
	else
	{
		p.F = glm::mat3(1.0f);
		p.Jp = 1.0f;
	}
}

void Solver::projectSand(Particle& p)
{
	float J = glm::determinant(p.F);

	if (std::isfinite(J) && J > 1e-6f)
	{
		const float Jmin = 1.0f - 0.08f;
		const float Jmax = 1.0f + 0.002f;

		float clampedJ = glm::clamp(J, Jmin, Jmax);
		float scale = cbrtf(clampedJ / J);
		p.F *= scale;

		p.Jp *= J / clampedJ;
	}
	else
	{
		p.F = glm::mat3(1.0f);
		p.Jp = 1.0f;
	}
}

void Solver::projectSnow(Particle& p)
{
	float J = glm::determinant(p.F);

	if (std::isfinite(J) && J > 1e-6f)
	{
		const float Jmin = 1.0f - 0.04f;
		const float Jmax = 1.0f + 0.015f;

		float clampedJ = glm::clamp(J, Jmin, Jmax);
		float scale = cbrtf(clampedJ / J);
		p.F *= scale;
		p.Jp *= J / clampedJ;
		p.F = glm::mix(glm::mat3(1.0f), p.F, 0.98f);
	}
	else
	{
		p.F = glm::mat3(1.0f);
		p.Jp = 1.0f;
	}
}
void Solver::projectJelly(Particle& p)
{
	float J = glm::determinant(p.F);

	if (!std::isfinite(J) || J <= 1e-6f)
	{
		p.F = glm::mat3(1.0f);
		p.Jp = 1.0f;
		return;
	}

	// jelly: mostly elastic
	// no real plastic projection, only stability guard
	const float Jmin = 0.85f;
	const float Jmax = 1.15f;

	if (J < Jmin || J > Jmax)
	{
		float clampedJ = glm::clamp(J, Jmin, Jmax);
		float scale = cbrtf(clampedJ / J);
		p.F *= scale;
	}

	// optional tiny damping toward identity to avoid blow-up
	p.F = glm::mix(glm::mat3(1.0f), p.F, 0.995f);

	float newJ = glm::determinant(p.F);
	if (!std::isfinite(newJ) || newJ <= 1e-6f)
	{
		p.F = glm::mat3(1.0f);
		p.Jp = 1.0f;
	}
}
static inline float lerpFloat(float a, float b, float t)
{
	return a + (b - a) * t;
}

//helper: handle casese if at edge of grid
float Solver::fetch(int i, int j, int k) const
{
	if (!collisionSDF.valid())
		return std::numeric_limits<float>::max();
	i = glm::clamp(i, 0, collisionSDF.res.x - 1);
	j = glm::clamp(j, 0, collisionSDF.res.y - 1);
	k = glm::clamp(k, 0, collisionSDF.res.z - 1);
	return collisionSDF.phi[collisionSDF.index(i, j, k)];
}

// particle position in the grid in 1D
float Solver::sampleSDF(const glm::vec3& p) const
{
	if (!collisionSDF.valid())
		return std::numeric_limits<float>::max();

	//point out of the grid
	glm::vec3 g = (p - collisionSDF.origin) / collisionSDF.dx;
	if (g.x < 0 || g.x >= collisionSDF.res.x - 1 ||
		g.y < 0 || g.y >= collisionSDF.res.y - 1 ||
		g.z < 0 || g.z >= collisionSDF.res.z - 1)
	{
		return 1e10f;
	}
	int x = (int)floor(g.x);
	int y = (int)floor(g.y);
	int z = (int)floor(g.z);
	// distance away from LEFTCORNER in the grid
	float fx = g.x - x;
	float fy = g.y - y;
	float fz = g.z - z;

	float c000 = fetch(x, y, z);
	float c100 = fetch(x + 1, y, z);
	float c010 = fetch(x, y + 1, z);
	float c110 = fetch(x + 1, y + 1, z);
	float c001 = fetch(x, y, z + 1);
	float c101 = fetch(x + 1, y, z + 1);
	float c011 = fetch(x, y + 1, z + 1);
	float c111 = fetch(x + 1, y + 1, z + 1);

	float c00 = lerpFloat(c000, c100, fx);
	float c10 = lerpFloat(c010, c110, fx);
	float c01 = lerpFloat(c001, c101, fx);
	float c11 = lerpFloat(c011, c111, fx);

	float c0 = lerpFloat(c00, c10, fy);
	float c1 = lerpFloat(c01, c11, fy);

	return lerpFloat(c0, c1, fz);
}

glm::vec3 Solver::sampleSDFNormal(const glm::vec3& p) const
{
	if (!collisionSDF.valid())
		return glm::vec3(0.0f, 1.0f, 0.0f);

	float h = collisionSDF.dx;

	float dx = sampleSDF(p + glm::vec3(h, 0, 0)) - sampleSDF(p - glm::vec3(h, 0, 0));
	float dy = sampleSDF(p + glm::vec3(0, h, 0)) - sampleSDF(p - glm::vec3(0, h, 0));
	float dz = sampleSDF(p + glm::vec3(0, 0, h)) - sampleSDF(p - glm::vec3(0, 0, h));

	glm::vec3 n(dx, dy, dz);
	float len = glm::length(n);

	if (len < 1e-8f)
		return glm::vec3(0.0f, 1.0f, 0.0f);

	return n / len;
}

void Solver::applyGridCollision(int i, int j, int k, GridCell& cell)
{
	if (cell.m <= 0.0f || !collisionSDF.valid())
		return;

	glm::vec3 x = params.gridOrigin + params.cellSize * glm::vec3((float)i, (float)j, (float)k);

	float phi = sampleSDF(x);

	if (phi > 0.001f) return;

	glm::vec3 n = sampleSDFNormal(x);

	glm::vec3 v = cell.v - colliderVelocity;
	float vn = glm::dot(v, n);

	if (vn < 0.0f)
	{
		glm::vec3 v_normal = vn * n;
		glm::vec3 v_tangent = v - v_normal;

		float bounce = 0.0f;
		switch (params.material) {
		case MaterialType::JELLY: bounce = 0.5f; break;
		case MaterialType::SNOW:  bounce = 0.02f; break;
		case MaterialType::SAND:  bounce = 0.01f; break;
		}

		v_normal *= -bounce;

		// friction
		float vtLen = glm::length(v_tangent);
		if (vtLen > 1e-8f)
		{
			float maxReduce = params.colliderFriction * (-vn);
			v_tangent *= glm::max(0.0f, 1.0f - maxReduce / vtLen);
		}

		cell.v = v_normal + v_tangent;
		cell.v *= n * 3.0f; // explosion factor
	}
}

void Solver::applyParticleCollision(Particle& p)
{
	if (!collisionSDF.valid())
		return;

	float phi = sampleSDF(p.pos);
	// if particle is inside collider
	if (phi < 0.0f)
	{
		glm::vec3 n = sampleSDFNormal(p.pos);

		// push particle out of collider
		p.pos -= phi * n;
		p.pos += 1e-4f * n;

		//// projected velocity length on normal direction
		//float vn = glm::dot(p.vel, n);
		//// collide into collider
		//if (vn < 0.0f)
		//{
		//	//tangent velocity
		//	glm::vec3 vt = p.vel - vn * n;
		//	float vtLen = glm::length(vt);

		//	float frictionScale = 1.0f;
		//	if (vtLen > 1e-8f)
		//	{
		//		float maxReduce = params.colliderFriction * (-vn);
		//		frictionScale = glm::max(0.0f, 1.0f - maxReduce / vtLen);
		//	}

		//	p.vel = vt * frictionScale;
		//}
	}
}

