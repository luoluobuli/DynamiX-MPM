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
            1,				// Max # of sources
            SOP_MPM::myVariables // Local variables
        )
    );
}

// declare parameters here
static PRM_Name dtName("substeps", "Sub Steps");
static PRM_Name emitterName("emitter", "Enable Emitter");
static PRM_Name restartName("restart_frame", "Restart From Frame");
static PRM_Name timescaleName("timescale", "Time Scale");
static PRM_Name emitrateName("emitrate", "Emit Per Burst"); 

//setup the initial/default values for parameters here
static PRM_Default dtDefault(4.0);
static PRM_Default emitterDefault(1);
static PRM_Default restartDefault(1);
static PRM_Default timescaleDefault(0.3);
static PRM_Default emitrateDefault(3.0);

////////////////////////////////////////////////////////////////////////////////////////

PRM_Template
SOP_MPM::myTemplateList[] = {
    PRM_Template(PRM_FLT, 1, &dtName, &dtDefault),
    PRM_Template(PRM_TOGGLE, 1, &emitterName, &emitterDefault),
    PRM_Template(PRM_INT, 1, &restartName, &restartDefault),
    PRM_Template(PRM_FLT, 1, &timescaleName, &timescaleDefault), 
    PRM_Template(PRM_INT, 1, &emitrateName, &emitrateDefault),
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

// cp is position cache, cv is velocity cache for all particles at a certain frame
static void
restoreFromCache(GU_Detail* gdp,
    const std::vector<UT_Vector3>& cp,
    const std::vector<UT_Vector3>& cv)
{
    gdp->clearAndDestroy();
    gdp->appendPointBlock((GA_Size)cp.size());
    GA_RWHandleV3 ph(gdp->getP());
    GA_RWHandleV3 vh(gdp->addFloatTuple(GA_ATTRIB_POINT, "v", 3));
    GA_Iterator it(gdp->getPointRange());
    int i = 0;
    for (; !it.atEnd(); ++it, ++i) {
        ph.set(*it, cp[i]); vh.set(*it, cv[i]);
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
    int timescale = SYSmax(evalInt("timescale", 0, now), 1);
    dt = dt / (fpreal)substeps * timescale;

    int restart = evalInt("restart_frame", 0, now);
    int emit_on = evalInt("emitter", 0, now);
    int emitrate = SYSmax(evalInt("emitrate", 0, now), 1);

    std::vector<UT_Vector3> source_positions;
    {
        const GU_Detail* src = inputGeo(0, context);
        if (src)
        {
            GA_ROHandleV3 src_p(src->getP());
            GA_Iterator sit(src->getPointRange());
            for (; !sit.atEnd(); ++sit)
                source_positions.push_back(src_p.get(*sit));
        }
    }

    // cache validation
    if (frame == 1)
    {
        pos_cache.clear();
        vel_cache.clear();
        last_cached_frame = 0;
        last_restart_frame = restart;
        last_substeps = substeps;
    }

	//  if restart frame is changed and it's smaller than current cached frame, we can keep the cache up to restart frame - 1
    else if (restart != last_restart_frame)
    {
        int keep = restart - 1;
        if (keep < (int)pos_cache.size())
        {
            pos_cache.resize(keep);
            vel_cache.resize(keep);
        }
        last_cached_frame = (int)pos_cache.size();
        last_restart_frame = restart;
    }

	// if substeps changed, the cache is no longer valid, need to clear cache and restart
    else if (substeps != last_substeps)
    {
        pos_cache.clear();
        vel_cache.clear();
        last_cached_frame = 0;
        last_substeps = substeps;
    }
	// if current frame is already cached, restore from cache and return
    if (frame <= last_cached_frame && !pos_cache.empty())
    {
        int idx = frame - 1;
        if (idx < (int)pos_cache.size())
        {
            restoreFromCache(gdp, pos_cache[idx], vel_cache[idx]);
            unlockInputs();
            return error();
        }
    }
	// if current frame is ahead of last cached frame + 1, it means it attempts to read frame not cached yet, we need to restore to last cached frame and play forward from there
    if (frame > last_cached_frame + 1)
    {
        if (!pos_cache.empty())
            restoreFromCache(gdp,
                pos_cache[last_cached_frame - 1],
                vel_cache[last_cached_frame - 1]);
        else { duplicateSource(0, context); getVelHandle(gdp); }
        addWarning(SOP_MESSAGE,
            "Scrubbed past uncached frames, play forward from frame 1 first.");
        unlockInputs();
        return error();
    }

    // first time simulation
    if (last_cached_frame == 0)
    {
        duplicateSource(0, context);
        getVelHandle(gdp);
    }
    else
    {
        restoreFromCache(gdp,
            pos_cache[last_cached_frame - 1],
            vel_cache[last_cached_frame - 1]);
    }

    //Compute
    GA_RWHandleV3 p_handle(gdp->getP());
    GA_RWHandleV3 v_handle = getVelHandle(gdp);

    if (!p_handle.isValid() || !v_handle.isValid())
    {
        unlockInputs();
        addError(SOP_MESSAGE, "Failed to access P or v.");
        return error();
    }

    for (int s = 0; s < substeps; ++s)
    {
        GA_Iterator it(gdp->getPointRange());
        for (; !it.atEnd(); ++it)
        {
            GA_Offset ptoff = *it;

            UT_Vector3 pos = p_handle.get(ptoff);
            UT_Vector3 vel = v_handle.get(ptoff);

            vel.y() -= dt * 9.8f;
            pos += dt * vel;

            if (pos.y() < 0.001f)
            {
                pos.y() = 0.001f;
                vel.y() = (vel.y() < -0.05f) ? -vel.y() * 0.8f : 0.0f;
            }

            p_handle.set(ptoff, pos);
            v_handle.set(ptoff, vel);
        }
    }

  //emitter
    const int emit_every = 5;
    if (emit_on && !source_positions.empty() && (frame % emit_every == 0))
    {
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

            gdp->appendPoint();
            p_handle.bind(gdp->getP());
            v_handle.bind(gdp->findFloatTuple(GA_ATTRIB_POINT, "v", 3));

            GA_Offset newpt = gdp->pointOffset(gdp->getNumPoints() - 1);
            p_handle.set(newpt, birth_pos);
            v_handle.set(newpt, birth_vel);
        }
    }

    // save cache
    std::vector<UT_Vector3> pf, vf;
    pf.reserve(gdp->getNumPoints());
    vf.reserve(gdp->getNumPoints());
    GA_Iterator it2(gdp->getPointRange());
    for (; !it2.atEnd(); ++it2)
    {
        pf.push_back(p_handle.get(*it2));
        vf.push_back(v_handle.get(*it2));
    }
    pos_cache.push_back(std::move(pf));
    vel_cache.push_back(std::move(vf));
    last_cached_frame = frame;

    std::cerr << "frame=" << frame
        << " pts=" << gdp->getNumPoints()
        << " cached=" << (int)pos_cache.size() << "\n";

    unlockInputs();
    return error();
}