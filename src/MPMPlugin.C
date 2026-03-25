#include <UT/UT_DSOVersion.h>
//#include <RE/RE_EGLServer.h>


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

#include <iostream>
#include <limits.h>
#include "MPMPlugin.h"

using namespace HDK_Sample;

//
// Help is stored in a "wiki" style text file. 
//
// See the sample_install.sh file for an example.
//
// NOTE : Follow this tutorial if you have any problems setting up your visual studio 2008 for Houdini 
//  http://www.apileofgrains.nl/setting-up-the-hdk-for-houdini-12-with-visual-studio-2008/


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
			1				// Max # of sources
		)
	);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//PUT YOUR CODE HERE
//You need to declare your parameters here
//Example to declare a variable for angle you can do like this :
//static PRM_Name		angleName("angle", "Angle");

static PRM_Name gravityName("gravity", "Gravity");

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//				     ^^^^^^^^    ^^^^^^^^^^^^^^^
//				     internal    descriptive version


// PUT YOUR CODE HERE
// You need to setup the initial/default values for your parameters here
// For example : If you are declaring the inital value for the angle parameter
// static PRM_Default angleDefault(30.0);	

static PRM_Default gravityDefault(9.8);

////////////////////////////////////////////////////////////////////////////////////////

PRM_Template
SOP_MPM::myTemplateList[] = {
// PUT YOUR CODE HERE
// You now need to fill this template with your parameter name and their default value
// EXAMPLE : For the angle parameter this is how you should add into the template
// PRM_Template(PRM_FLT,	PRM_Template::PRM_EXPORT_MIN, 1, &angleName, &angleDefault, 0),
// Similarly add all the other parameters in the template format here


/////////////////////////////////////////////////////////////////////////////////////////////

	PRM_Template(PRM_FLT, 1, &gravityName, &gravityDefault),
    PRM_Template()
};


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
	params.init();
	solver.init(params);
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

OP_ERROR
SOP_MPM::cookMySop(OP_Context &context)
{
	fpreal now = context.getTime();

	if (lockInputs(context) >= UT_ERROR_ABORT) {
		return error();
	}

	// Copy the input geometry (input 0) into the output.
	duplicatePointSource(0, context);
	const GU_Detail* input = inputGeo(0, context);
	if (!input)
	{
		unlockInputs();
		return error();
	}

	// ------------------- Playback Control ----------------------
	
	// Get dt
	fpreal t = context.getTime();

	// Reset simulation state if time goes backwards or if substeps changes
	bool reset = false;

	if (prevTime < 0) {
		reset = true;
	}
	else if (t < prevTime) {
		reset = true;
	}
	else if (solver.getParticleCount() != input->getNumPoints()) {
		reset = true;
	}

	if (reset)
	{
		params.init();
		params.gravity = evalFloat("gravity", 0, now);
		solver.init(params);

		GA_Offset ptoff;
		GA_FOR_ALL_PTOFF(input, ptoff)
		{
			UT_Vector3 P = input->getPos3(ptoff);

			Particle p;
			p.pos = glm::vec3(P.x(), P.y(), P.z());
			p.vel = glm::vec3(0.f);
			p.mass = 1.0f;

			solver.addParticle(p);
		}

		// Write initial positions back to output just to stay consistent
		writeBack();
		prevTime = t;
		unlockInputs();
		return error();
	}

	fpreal dt = t - prevTime;
	prevTime = t;

	// ------------------- Simulation ----------------------
	params.init();
	params.gravity = evalFloat("gravity", 0, now);
	params.dt = dt;

	solver.setParams(params);

	solver.step();
	
	// ------------------- Write back ----------------------
	writeBack();
	unlockInputs();
	return error();

	//GA_RWHandleV3 p_handle(gdp->getP());
	//GA_Attribute* v_attrib = gdp->findFloatTuple(GA_ATTRIB_POINT, "v", 3);
	//if (!v_attrib)
	//	v_attrib = gdp->addFloatTuple(GA_ATTRIB_POINT, "v", 3);

	//GA_RWHandleV3 v_handle(v_attrib);

	//if (!p_handle.isValid() || !v_handle.isValid())
	//{
	//	unlockInputs();
	//	addError(SOP_MESSAGE, "Failed to access P or v.");
	//	return error();
	//}

	//GA_Iterator it(gdp->getPointRange());
	//for (; !it.atEnd(); ++it)
	//{
	//	GA_Offset ptoff = *it;

	//	UT_Vector3 pos = p_handle.get(ptoff);
	//	UT_Vector3 vel = v_handle.get(ptoff);

	//	vel.y() -= dt * evalFloat("gravity", 0, now);

	//	if (pos.y() < 0.001f) {
	//		vel.y() = -vel.y() * 0.8f;
	//	}

	//	pos += dt * vel;

	//	v_handle.set(ptoff, vel);
	//	p_handle.set(ptoff, pos);
	//}

	//// ------------------- End of Simulation ----------------------

	//unlockInputs();

 //   return error();
}

