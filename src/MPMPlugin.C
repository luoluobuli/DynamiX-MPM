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
#include <SOP/SOP_Node.h>
#include <OP/OP_Director.h>
#include <CMD/CMD_Manager.h>

#include <iostream>
#include <stdlib.h>
#include <limits.h>
#include "MPMPlugin.h"

using namespace HDK_Sample;

///
/// newSopOperator is the hook that Houdini grabs from this dll
/// and invokes to register the SOP.  In this case we add ourselves
/// to the specified operator table.
///
void
newSopOperator(OP_OperatorTable* table)
{
    table->addOperator(
        new OP_Operator(
            "CusMPM",			// Internal name
            "OurMPM",			// UI name
            SOP_MPM::myConstructor,	// How to build the SOP
            SOP_MPM::myTemplateList,	// My parameters
            1,				// Min # of sources
            3,				// Max # of sources
            SOP_MPM::myVariables // Local variables
        )
    );
}

// declare parameters here
static PRM_Name dtName("substeps", "Sub Steps");
static PRM_Name resetName("reset_sim", "Reset Simulation");
static PRM_Name timescaleName("timescale", "Time Scale");
static PRM_Name emitterName("emitter", "Enable Emitter");
static PRM_Name emitrateName("emitrate", "Emit Per Burst");
static PRM_Name gravityName("gravity", "Gravity");


//setup the initial/default values for parameters here
static PRM_Default dtDefault(4.0);
static PRM_Default emitterDefault(1);
static PRM_Default timescaleDefault(0.3);
static PRM_Default emitrateDefault(3.0);
static PRM_Default gravityDefault(9.8);
////////////////////////////////////////////////////////////////////////////////////////

PRM_Template
SOP_MPM::myTemplateList[] = {
    PRM_Template(PRM_CALLBACK, 1, &resetName, 0, 0, 0, SOP_MPM::resetSimulation),
    PRM_Template(PRM_INT, 1, &dtName, &dtDefault),
    PRM_Template(PRM_FLT, 1, &timescaleName, &timescaleDefault),
    PRM_Template(PRM_TOGGLE, 1, &emitterName, &emitterDefault),
    PRM_Template(PRM_INT, 1, &emitrateName, &emitrateDefault),
    PRM_Template(PRM_FLT, 1, &gravityName, &gravityDefault),
    PRM_Template()
};

// Here's how we define local variables for the SOP.
enum {
    VAR_PT,		// Point number of the star
    VAR_NPT		// Number of points in the star
};

CH_LocalVariable
SOP_MPM::myVariables[] = {
    { "PT",	VAR_PT, 0 },		// The table provides a mapping
    { "NPT",	VAR_NPT, 0 },		// from text string to integer token
    { 0, 0, 0 },
};

bool
SOP_MPM::evalVariableValue(fpreal& val, int index, int thread)
{
    return false;
}

OP_Node*
SOP_MPM::myConstructor(OP_Network* net, const char* name, OP_Operator* op)
{
    return new SOP_MPM(net, name, op);
}

SOP_MPM::SOP_MPM(OP_Network* net, const char* name, OP_Operator* op)
    : SOP_Node(net, name, op)
{
}

SOP_MPM::~SOP_MPM() {}

unsigned
SOP_MPM::disableParms()
{
    return 0;
}

static GA_RWHandleV3
getVelHandle(GU_Detail* gdp)
{
    GA_Attribute* v_attrib = gdp->findFloatTuple(GA_ATTRIB_POINT, "v", 3);
    if (!v_attrib)
        v_attrib = gdp->addFloatTuple(GA_ATTRIB_POINT, "v", 3);
    return GA_RWHandleV3(v_attrib);
}


int SOP_MPM::resetSimulation(void* data, int, float, const PRM_Template*)
{
    SOP_MPM* node = static_cast<SOP_MPM*>(data);
    if (!node)
        return 0;
    OPgetDirector()->getCommandManager()->execute("fcur 1", false);

    return 1;
}

static void solveCollisionWithContainer(
    UT_Vector3& pos,
    UT_Vector3& vel,
    bool has_container_bbox,
    const UT_BoundingBox& bbox)
{
    // fallback ground
    if (!has_container_bbox)
    {
        if (pos.y() < 0.001f)
        {
            pos.y() = 0.001f;
            vel.y() = (vel.y() < -0.05f) ? -vel.y() * 0.3f : 0.0f;
        }
        return;
    }

    // simple box collision
    const fpreal eps = 0.001f;
    const fpreal bounce = 0.3f;

    if (pos.x() < bbox.xmin() + eps) {
        pos.x() = bbox.xmin() + eps;
        if (vel.x() < 0.0f) vel.x() = -vel.x() * bounce;
    }
    if (pos.x() > bbox.xmax() - eps) {
        pos.x() = bbox.xmax() - eps;
        if (vel.x() > 0.0f) vel.x() = -vel.x() * bounce;
    }

    if (pos.y() < bbox.ymin() + eps) {
        pos.y() = bbox.ymin() + eps;
        if (vel.y() < 0.0f) vel.y() = -vel.y() * bounce;
    }
    /*if (pos.y() > bbox.ymax() - eps) {
        pos.y() = bbox.ymax() - eps;
        if (vel.y() > 0.0f) vel.y() = -vel.y() * bounce;
    }*/

    if (pos.z() < bbox.zmin() + eps) {
        pos.z() = bbox.zmin() + eps;
        if (vel.z() < 0.0f) vel.z() = -vel.z() * bounce;
    }
    if (pos.z() > bbox.zmax() - eps) {
        pos.z() = bbox.zmax() - eps;
        if (vel.z() > 0.0f) vel.z() = -vel.z() * bounce;
    }
}

OP_ERROR
SOP_MPM::cookMySop(OP_Context& context)
{
    flags().setTimeDep(true);

    int frame = context.getFrame();
    fpreal now = context.getTime();

    if (lockInputs(context) >= UT_ERROR_ABORT)
        return error();

    // Get dt
    fpreal fps = CHgetManager()->getSamplesPerSec();
    fpreal dt = 1.0 / fps;

    int substeps = SYSmax(evalInt("substeps", 0, now), 1);
    fpreal timescale = evalFloat("timescale", 0, now);
    dt = dt / (fpreal)substeps * timescale;

    int emit_on = evalInt("emitter", 0, now);
    int emitrate = SYSmax(evalInt("emitrate", 0, now), 1);
    fpreal gravity = evalFloat("gravity", 0, now);
    int final_frame = SYSmax(evalInt("final_frame", 0, now), 1);

    const GU_Detail* prev_state = inputGeo(0, context);   // Prev_Frame
	const GU_Detail* emit_src = inputGeo(1, context);   //emitter source
    const GU_Detail* container = inputGeo(2, context);   // bbox / collider
    std::vector<UT_Vector3> source_positions;
  
    if (emit_src)
    {
        GA_ROHandleV3 src_p(emit_src->getP());
        GA_Iterator sit(emit_src->getPointRange());
        for (; !sit.atEnd(); ++sit) {
            source_positions.push_back(src_p.get(*sit));
        }
    }

    UT_BoundingBox container_bbox;
    bool has_container_bbox = false;
    if (container)
    {
        container_bbox.initBounds();
        GA_ROHandleV3 cph(container->getP());
        GA_Iterator cit(container->getPointRange());
        for (; !cit.atEnd(); ++cit)
        {
            container_bbox.enlargeBounds(cph.get(*cit));
        }
        has_container_bbox = true;
    }
    if (!prev_state)
    {
        unlockInputs();
        addError(SOP_MESSAGE, "Missing Prev_Frame input on input 0.");
        return error();
    }

    // Always start from input geometry each cook
    duplicateSource(0, context);
    GA_RWHandleV3 p_handle(gdp->getP());
    GA_RWHandleV3 v_handle = getVelHandle(gdp);

    if (!p_handle.isValid() || !v_handle.isValid())
    {
        unlockInputs();
        addError(SOP_MESSAGE, "Failed to access P or v.");
        return error();
    }


    UT_AutoInterrupt progress("Running MPM Simulation");
   
    for (int s = 0; s < substeps; ++s)
    {
        fpreal sim_progress = ((fpreal)(s + 1)) / (fpreal)substeps;

        sim_progress = SYSclamp(sim_progress, 0.0, 1.0);
        int percent = (int)(sim_progress * 100.0);

        if (progress.wasInterrupted(percent))
        {
            unlockInputs();
            addWarning(SOP_MESSAGE, "Simulation interrupted by user.");
            return error();
        }

        GA_Iterator it(gdp->getPointRange());
        for (; !it.atEnd(); ++it)
        {
            GA_Offset ptoff = *it;

            UT_Vector3 pos = p_handle.get(ptoff);
            UT_Vector3 vel = v_handle.get(ptoff);

            vel.y() -= dt * gravity;
            pos += dt * vel;

            solveCollisionWithContainer(pos, vel, has_container_bbox, container_bbox);

            p_handle.set(ptoff, pos);
            v_handle.set(ptoff, vel);
        }
    }
        
    //emitter
    const int emit_every = 5;
    if (emit_on && !source_positions.empty() && (frame % emit_every == 0))
    {
        SYSsrand48((long)frame);
        int n_src = (int)source_positions.size();

        for (int e = 0; e < emitrate; ++e)
        {
            int idx = (int)(SYSdrand48() * n_src) % n_src;
            UT_Vector3 birth_pos = source_positions[idx];

            UT_Vector3 birth_vel(
                (fpreal)(SYSdrand48() - 0.5) * 0.5f,   // x +-0.25
                (fpreal)(SYSdrand48() * 0.5f),          // y 0~0.5
                (fpreal)(SYSdrand48() - 0.5) * 0.5f    // z+-0.25
            );

            GA_Offset newpt = gdp->appendPointOffset();
            p_handle.bind(gdp->getP());
            v_handle.bind(gdp->findFloatTuple(GA_ATTRIB_POINT, "v", 3));

            p_handle.set(newpt, birth_pos);
            v_handle.set(newpt, birth_vel);
        }
    }
        std::cerr << "frame=" << frame
            << " pts=" << gdp->getNumPoints() << "\n";

    unlockInputs();
    return error();
}