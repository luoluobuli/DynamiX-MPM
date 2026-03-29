#include "Solver.h"

int Solver::cellIndex(int i, int j, int k) {
	return i + j * params.gridRes.x + k * params.gridRes.x * params.gridRes.y;
}

void Solver::step() {
	clearGrid();
	particleToGrid();
	updateGrid();
	gridToParticle();
}

void Solver::clearGrid() {
	for (auto& cell : gridCells) {
		cell.m = 0.0f;
		cell.v = glm::vec3(0.0f);
	}
}

void Solver::particleToGrid() {
	float invCellSize = 1.f / params.cellSize;

	for (const auto& p : particles) {
		// Find particle's grid coordinates
		glm::vec3 gridCoord = (p.pos - params.gridOrigin) * invCellSize;

		// Find the base cell& neighboring cells
		// Linear interpolation now; move to quadradic later
		int x = floor(gridCoord.x);
		int y = floor(gridCoord.y);
		int z = floor(gridCoord.z);

		float fx = gridCoord.x - x;
		float fy = gridCoord.y - y;
		float fz = gridCoord.z - z;

		float wx[2] = { 1.0f - fx, fx };
		float wy[2] = { 1.0f - fy, fy };
		float wz[2] = { 1.0f - fz, fz };

		float dwx[2] = { -1.0f, 1.0f };
		float dwy[2] = { -1.0f, 1.0f };
		float dwz[2] = { -1.0f, 1.0f };

		// Compute stress
		float J = glm::determinant(p.F);
		J = glm::max(J, 1e-6f);
		glm::mat3 FinvT = glm::transpose(glm::inverse(p.F));
		glm::mat3 stress = params.mu * (p.F - FinvT) + params.lambda * log(J) * FinvT;

		for (int i = 0; i <= 1; ++i) {
			for (int j = 0; j <= 1; ++j) {
				for (int k = 0; k <= 1; ++k) {
					int cellX = x + i;
					int cellY = y + j;
					int cellZ = z + k;
					// Check bounds
					if (cellX < 0 || cellX >= params.gridRes.x ||
						cellY < 0 || cellY >= params.gridRes.y ||
						cellZ < 0 || cellZ >= params.gridRes.z) {
						continue;
					}

					// Compute weight
					float w = wx[i] * wy[j] * wz[k];

					// Compute weight gradient
					glm::vec3 gradW(
						dwx[i] * wy[j] * wz[k],
						wx[i] * dwy[j] * wz[k],
						wx[i] * wy[j] * dwz[k]
					);
					gradW *= invCellSize;

					GridCell& cell = gridCells[cellIndex(cellX, cellY, cellZ)];
					cell.m += w * p.mass;
					cell.v += w * p.mass * p.vel;

					// elastic force
					glm::vec3 force = -p.volume * (stress * gradW);
					cell.v += params.dt * force;
				}
			}
		}
	}
}

void Solver::updateGrid() {
	for (auto& cell : gridCells) {
		if (cell.m > 0.0f) {
			cell.v /= cell.m;
			cell.v.y -= params.gravity * params.dt;
		}
	}
}

void Solver::gridToParticle() {
	float invCellSize = 1.f / params.cellSize;

	for (auto& p : particles) {
		glm::vec3 gridCoord = (p.pos - params.gridOrigin) * invCellSize;

		int x = floor(gridCoord.x);
		int y = floor(gridCoord.y);
		int z = floor(gridCoord.z);

		float fx = gridCoord.x - x;
		float fy = gridCoord.y - y;
		float fz = gridCoord.z - z;

		float wx[2] = { 1.0f - fx, fx };
		float wy[2] = { 1.0f - fy, fy };
		float wz[2] = { 1.0f - fz, fz };

		float dwx[2] = { -1.0f, 1.0f };
		float dwy[2] = { -1.0f, 1.0f };
		float dwz[2] = { -1.0f, 1.0f };

		glm::vec3 newVel(0.0f);
		glm::mat3 gradVel(0.0f);

		for (int i = 0; i <= 1; ++i) {
			for (int j = 0; j <= 1; ++j) {
				for (int k = 0; k <= 1; ++k) {
					int cellX = x + i;
					int cellY = y + j;
					int cellZ = z + k;
					if (cellX < 0 || cellX >= params.gridRes.x ||
						cellY < 0 || cellY >= params.gridRes.y ||
						cellZ < 0 || cellZ >= params.gridRes.z) {
						continue;
					}

					// Weight
					float w = wx[i] * wy[j] * wz[k];

					// Weight gradient
					glm::vec3 gradW(
						dwx[i] * wy[j] * wz[k],
						wx[i] * dwy[j] * wz[k],
						wx[i] * wy[j] * dwz[k]
					);
					gradW *= invCellSize;

					GridCell& cell = gridCells[cellIndex(cellX, cellY, cellZ)];

					newVel += w * cell.v;
					gradVel += glm::outerProduct(cell.v, gradW);
				}
			}
		}
		p.vel = newVel;
		p.pos += p.vel * params.dt;
		p.F = (glm::mat3(1.0f) + params.dt * gradVel) * p.F;
		float J = glm::determinant(p.F);
		if (!std::isfinite(J) || J <= 1e-6f) {
			p.F = glm::mat3(1.0f);
		}
	}
}