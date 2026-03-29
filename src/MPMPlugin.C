#include <UT/UT_DSOVersion.h>

#include <UT/UT_Math.h>
#include <UT/UT_Interrupt.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PrimPoly.h>
#include <CH/CH_LocalVariable.h>
#include <PRM/PRM_Include.h>
#include <PRM/PRM_SpareData.h>
#include <OP/OP_Operator.h>
#include <OP/OP_OperatorTable.h>
#include <CH/CH_Manager.h>
#include <GEO/GEO_PrimPoly.h>

#include <iostream>
#include <limits.h>
#include "MPMPlugin.h"

using namespace HDK_Sample;

///
/// newSopOperator is the hook that Houdini grabs from this dll
/// and invokes to register the SOP.  In this case we add ourselves
/// to the specified operator table.
///
void
newSopOperator(OP_OperatorTable *table)
{
    table->addOperator(
	    new OP_Operator(
			"CusMPM",			// Internal name
		    "MyMPM",			// UI name
			SOP_MPM::myConstructor,	// How to build the SOP
			SOP_MPM::myTemplateList,	// My parameters
			1,				// Min # of sources
			2				// Max # of sources
		)
	);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Declare parameters
static PRM_Name substepsName("substeps", "Substeps");
static PRM_Name gravityName("gravity", "Gravity");
static PRM_Name youngName("young", "Young's Modulus");
static PRM_Name poissonName("poisson", "Poisson's Ratio");

// Setup the initial/default values for parameters
static PRM_Default substepsDefault(1);
static PRM_Default gravityDefault(9.8f);
static PRM_Default youngDefault(10.f);
static PRM_Default poissonDefault(0.2f);

// Setup the range for parameters
static PRM_Range substepsRange(PRM_RANGE_UI, 1, PRM_RANGE_UI, 10);
static PRM_Range gravityRange(PRM_RANGE_UI, 0.f, PRM_RANGE_UI, 30);
static PRM_Range youngRange(PRM_RANGE_UI, 0.f, PRM_RANGE_UI, 50.f);
static PRM_Range poissonRange(PRM_RANGE_RESTRICTED, 0.f, PRM_RANGE_RESTRICTED, 0.49f);

////////////////////////////////////////////////////////////////////////////////////////

PRM_Template
SOP_MPM::myTemplateList[] = {
	PRM_Template(PRM_INT, 1, &substepsName, &substepsDefault, 0, &substepsRange),
	PRM_Template(PRM_FLT, 1, &gravityName, &gravityDefault, 0, &gravityRange),
	PRM_Template(PRM_FLT, 1, &youngName, &youngDefault, 0, &youngRange),
	PRM_Template(PRM_FLT, 1, &poissonName, &poissonDefault, 0, &poissonRange),
    PRM_Template()
};

// Note from Joanne: we are not using it, but I'm keeping this part in case we need it in the future
// Here's how we define local variables for the SOP.
enum {
	VAR_PT,		// Point number of the star
	VAR_NPT		// Number of points in the star
};

bool
SOP_MPM::evalVariableValue(fpreal &val, int index, int thread)
{
	return false;
}

OP_Node *
SOP_MPM::myConstructor(OP_Network *net, const char *name, OP_Operator *op)
{
    return new SOP_MPM(net, name, op);
}

SOP_MPM::SOP_MPM(OP_Network *net, const char *name, OP_Operator *op)
	: SOP_Node(net, name, op)
{
}

SOP_MPM::~SOP_MPM() {}

unsigned
SOP_MPM::disableParms()
{
    return 0;
}

void SOP_MPM::writeBack() {
	GA_Offset outoff;
	int i = 0;
	GA_FOR_ALL_PTOFF(gdp, outoff)
	{
		const Particle& p = solver.getParticle(i);
		gdp->setPos3(outoff, UT_Vector3(p.pos.x, p.pos.y, p.pos.z));
		++i;
	}
}

void SOP_MPM::setParameters(float t) {
	params.gravity = evalFloat("gravity", 0, t);

	float E = evalFloat("young", 0, t);
	float nu = evalFloat("poisson", 0, t);
	if (nu < 0.0f) nu = 0.0f;
	if (nu > 0.49f) nu = 0.49f;

	params.mu = E / (2.0f * (1.0f + nu));
	params.lambda = E * nu / ((1.0f + nu) * (1.0f - 2.0f * nu));
}

OP_ERROR
SOP_MPM::cookMySop(OP_Context &context)
{
	if (lockInputs(context) >= UT_ERROR_ABORT) {
		return error();
	}

	// Copy the input geometry (input 0) into the output.
	duplicatePointSource(0, context);
	const GU_Detail* particlesGeo = inputGeo(0, context);
	const GU_Detail* boxGeo = inputGeo(1, context);

	if (!particlesGeo || !boxGeo) {
		unlockInputs();
		return error();
	}

	UT_BoundingBox bbox;
	boxGeo->getBBox(&bbox);
	if (!bbox.isValid()) return error();

	UT_Vector3 minPos = bbox.minvec();
	UT_Vector3 maxPos = bbox.maxvec();
	UT_Vector3 size = maxPos - minPos;

	params.gridOrigin = glm::vec3(minPos.x(), minPos.y(), minPos.z());

	float dxX = static_cast<float>(size.x()) / params.gridRes.x;
	float dxY = static_cast<float>(size.y()) / params.gridRes.y;
	float dxZ = static_cast<float>(size.z()) / params.gridRes.z;

	params.cellSize = glm::min(dxX, glm::min(dxY, dxZ));

	// ------------------- Playback Control ----------------------
	
	// Get dt
	fpreal t = context.getTime();

	int substeps = evalInt("substeps", 0, t);
	if (substeps < 1) substeps = 1;

	// Reset simulation state if time goes backwards or if substeps changes
	bool reset = false;

	if (prevTime < 0) {
		reset = true;
	}
	else if (t < prevTime) {
		reset = true;
	}
	else if (solver.getParticleCount() != particlesGeo->getNumPoints()) {
		reset = true;
	}

	if (reset)
	{
		setParameters(t);
		solver.init(params);

		std::vector<Particle> particles;

		GA_Offset ptoff;
		GA_FOR_ALL_PTOFF(particlesGeo, ptoff)
		{
			UT_Vector3 P = particlesGeo->getPos3(ptoff);

			Particle p;
			p.pos = glm::vec3(P.x(), P.y(), P.z());
			p.vel = glm::vec3(0.f);
			p.mass = 1.0f;
			p.F = glm::mat3(1.0f);
			p.volume = params.cellSize * params.cellSize * params.cellSize;

			particles.push_back(p);
		}

		solver.setParticles(particles);

		// Write initial positions back to output just to stay consistent
		writeBack();
		prevTime = t;
		unlockInputs();
		return error();
	}

	float frameDt = t - prevTime;
	prevTime = t;

	// ------------------- Simulation ----------------------
	setParameters(t);

	float subDt = frameDt / (float)substeps;
	for (int s = 0; s < substeps; ++s)
	{
		params.dt = subDt;
		solver.setParams(params);
		solver.step();
	}
	
	// ------------------- Write back ----------------------
	writeBack();
	unlockInputs();
	return error();
}