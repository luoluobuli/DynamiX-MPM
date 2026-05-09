#include "HDKcommon.h"
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
            3				// Max # of sources
        )
    );
}

// declare parameters here
static PRM_Name substepsName("substeps", "Sub Steps");
static PRM_Name resetName("reset_sim", "Reset Simulation");
static PRM_Name timescaleName("timescale", "Time Scale");
static PRM_Name emitterName("emitter", "Enable Emitter");
static PRM_Name emitrateName("emitrate", "Emit Per Burst");
static PRM_Name gravityName("gravity", "Gravity");
static PRM_Name gridResName("gridRes", "Grid Resolution");
static PRM_Name massName("mass", "Mass");
static PRM_Name initVelocityName("initVelocity", "Initial Velocity");
static PRM_Name materialName("material", "Material");
static PRM_Name materialMenuItems[] = {
    PRM_Name("custom", "Custom"),
    PRM_Name("jelly", "Jelly"),
    PRM_Name("sand",  "Sand"),
    PRM_Name("snow",  "Snow"),
    PRM_Name(0)
};

static PRM_ChoiceList materialMenu(PRM_CHOICELIST_SINGLE, materialMenuItems);

static PRM_Name youngName("young", "Young's Modulus");
static PRM_Name poissonName("poisson", "Poisson's Ratio");
// Setup the initial/default values for parameters
static PRM_Default substepsDefault(4);
static PRM_Default gridResDefault(16);
static PRM_Default gravityDefault(9.8f);
static PRM_Default massDefault(1.f);
static PRM_Default initVelocityDefault[] = {
    PRM_Default(0.0f),
    PRM_Default(0.0f),
    PRM_Default(0.0f)
};

static PRM_Default emitterDefault(1);
static PRM_Default timescaleDefault(0.3);
static PRM_Default emitrateDefault(3.0);

static PRM_Default materialDefault(0);
static PRM_Default youngDefault(10.f);
static PRM_Default poissonDefault(0.2f);

// Setup the range for parameters
static PRM_Range substepsRange(PRM_RANGE_UI, 1, PRM_RANGE_UI, 10);
static PRM_Range gridResRange(PRM_RANGE_UI, 8, PRM_RANGE_UI, 128);
static PRM_Range gravityRange(PRM_RANGE_UI, 0.f, PRM_RANGE_UI, 30);
static PRM_Range massRange(PRM_RANGE_UI, 0.1f, PRM_RANGE_UI, 5.f);

static PRM_Range youngRange(PRM_RANGE_UI, 0.f, PRM_RANGE_UI, 5000.f);
static PRM_Range poissonRange(PRM_RANGE_RESTRICTED, 0.f, PRM_RANGE_RESTRICTED, 0.49f);

////////////////////////////////////////////////////////////////////////////////////////

PRM_Template
SOP_MPM::myTemplateList[] = {
    PRM_Template(PRM_CALLBACK, 1, &resetName, 0, 0, 0, SOP_MPM::resetSimulation),
    PRM_Template(PRM_FLT, 1, &timescaleName, &timescaleDefault),
    PRM_Template(PRM_TOGGLE, 1, &emitterName, &emitterDefault),
    PRM_Template(PRM_INT, 1, &emitrateName, &emitrateDefault),

    PRM_Template(PRM_INT, 1, &substepsName, &substepsDefault, 0, &substepsRange),
    PRM_Template(PRM_INT, 1, &gridResName, &gridResDefault, 0, &gridResRange),
    PRM_Template(PRM_FLT, 1, &gravityName, &gravityDefault, 0, &gravityRange),
    PRM_Template(PRM_FLT, 1, &massName, &massDefault, 0, &massRange),
    PRM_Template(PRM_XYZ, 3, &initVelocityName, initVelocityDefault),

    PRM_Template(PRM_ORD, 1, &materialName, &materialDefault, &materialMenu),
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

static const GEO_PrimVDB*
findFirstVDB(const GU_Detail* geo)
{
    if (!geo)
        return nullptr;

    for (GA_Iterator it(geo->getPrimitiveRange()); !it.atEnd(); ++it)
    {
        const GEO_Primitive* prim = geo->getGEOPrimitive(*it);
        if (const GEO_PrimVDB* vdb = dynamic_cast<const GEO_PrimVDB*>(prim))
            return vdb;
    }

    return nullptr;
}

static bool
getContainerBBox(const GU_Detail* container, UT_BoundingBox& bbox)
{
    bbox.initBounds();

    if (!container)
        return false;

    if (const GEO_PrimVDB* vdb = findFirstVDB(container))
        return vdb->getBBox(&bbox);

    if (container->getNumPoints() > 0)
    {
        GA_ROHandleV3 h(container->getP());
        for (GA_Iterator it(container->getPointRange()); !it.atEnd(); ++it)
            bbox.enlargeBounds(h.get(*it));
        return bbox.isValid();
    }

    return false;
}

int SOP_MPM::resetSimulation(void* data, int, float, const PRM_Template*)
{
    SOP_MPM* node = static_cast<SOP_MPM*>(data);
    if (!node)
        return 0;
    node->prevTime = -1.0f;
    node->solver.setParticles(std::vector<Particle>{});
    OPgetDirector()->getCommandManager()->execute("fcur 1", false);

    return 1;
}

void SOP_MPM::writeBack()
{
    gdp->clearAndDestroy();

    GA_Offset ptoff;
    for (int i = 0; i < solver.getParticleCount(); ++i)
    {
        ptoff = gdp->appendPointOffset();
    }

    GA_RWHandleV3 vHandle = getVelHandle(gdp);

    int i = 0;
    GA_FOR_ALL_PTOFF(gdp, ptoff)
    {
        const Particle& p = solver.getParticle(i);
        gdp->setPos3(ptoff, UT_Vector3(p.pos.x, p.pos.y, p.pos.z));
        vHandle.set(ptoff, UT_Vector3(p.vel.x, p.vel.y, p.vel.z));
        ++i;
    }
}

CollisionSDF
SOP_MPM::buildCollisionSDF(const GU_Detail* container) const
{
    CollisionSDF sdf;

    const GEO_PrimVDB* vdb = findFirstVDB(container);
    if (!vdb)
        return sdf;

    sdf.res = params.gridRes + glm::ivec3(1);
    sdf.origin = params.gridOrigin;
    sdf.dx = params.cellSize;
    sdf.phi.resize(sdf.res.x * sdf.res.y * sdf.res.z);

    for (int k = 0; k < sdf.res.z; ++k)
    {
        for (int j = 0; j < sdf.res.y; ++j)
        {
            for (int i = 0; i < sdf.res.x; ++i)
            {
                UT_Vector3 pos(
                    sdf.origin.x + sdf.dx * (float)i,
                    sdf.origin.y + sdf.dx * (float)j,
                    sdf.origin.z + sdf.dx * (float)k
                );

                sdf.phi[sdf.index(i, j, k)] = vdb->getValueF(pos);
            }
        }
    }

    return sdf;
}

void SOP_MPM::setParameters(float t)
{
    const int gridRes = SYSmax(evalInt("gridRes", 0, t), 1);

    params.gridRes = glm::vec3(gridRes);
    params.gravity = evalFloat("gravity", 0, t);

    float E = evalFloat("young", 0, t);
    float nu = evalFloat("poisson", 0, t);

    if (nu < 0.0f)  nu = 0.0f;
    if (nu > 0.49f) nu = 0.49f;

    params.mu = E / (2.0f * (1.0f + nu));
    params.lambda = E * nu / ((1.0f + nu) * (1.0f - 2.0f * nu));
    params.hardening = 10.f;

    const int mat = evalInt("material", 0, t);
    switch (mat)
    {
    case 0:
        params.material = MaterialType::CUSTOM;
        break;
    case 1:
        params.material = MaterialType::SAND;
        params.thetaC = 0.08f;
        params.thetaS = 0.002f;
        break;
    case 2:
        params.material = MaterialType::SNOW;
        params.thetaC = 0.04f;
        params.thetaS = 0.015f;
        break;
    case 3:
        params.material = MaterialType::JELLY;
        params.thetaC = 0.15f;
        params.thetaS = 0.15f;
        break;
    default:
        params.material = MaterialType::CUSTOM;
        break;
    }
}

OP_ERROR
SOP_MPM::cookMySop(OP_Context& context)
{
    flags().setTimeDep(true);

    const int frame = context.getFrame();
    const fpreal now = context.getTime();

    if (lockInputs(context) >= UT_ERROR_ABORT)
        return error();

    const GU_Detail* seed_geo = inputGeo(0, context);   // initial particles / prev frame source
    const GU_Detail* collider_geo = inputGeo(1, context);   // collider
    const GU_Detail* container_geo= inputGeo(2, context);   // container

    if (!seed_geo)
    {
        unlockInputs();
        addError(SOP_MESSAGE, "Missing input 0.");
        return error();
    }
    if (!collider_geo)
    {
        unlockInputs();
        addError(SOP_MESSAGE, "Missing input 1.");
        return error();
    }
    if (!container_geo)
    {
        unlockInputs();
        addError(SOP_MESSAGE, "Missing input 2.");
        return error();
    }


    //setParameters(now);

    const int gridRes = SYSmax(evalInt("gridRes", 0, now), 1);
    UT_BoundingBox containerBox;
    if (getContainerBBox(container_geo, containerBox))
    {
        UT_Vector3 minv = containerBox.minvec();
        UT_Vector3 maxv = containerBox.maxvec();

        UT_Vector3 center = 0.5f * (minv + maxv);
        UT_Vector3 size = maxv - minv;

        if (size.x() != size.y() || size.y() != size.z() || size.x() != size.z())
        {
            std::cerr << "Warning: Non-cubic container bounding box detected. This will lead to incorrect cell sizes." << std::endl;
		}

        glm::vec3 halfSize = 0.5f * glm::vec3(size.x(), size.y(), size.z());
        params.gridOrigin = glm::vec3(center.x(), center.y(), center.z()) - halfSize;
        params.cellSize = size.x() / (fpreal)gridRes;
    }

    // ------------------- Playback Control ----------------------
    // Get dt
    fpreal t = context.getTime();

    const int substeps = SYSmax(evalInt("substeps", 0, now), 1);
    const fpreal timescale = evalFloat("timescale", 0, now);
    const int emit_on = evalInt("emitter", 0, now);
    const int emitrate = SYSmax(evalInt("emitrate", 0, now), 1);
    const fpreal mass = evalFloat("mass", 0, now);
    const fpreal fps = CHgetManager()->getSamplesPerSec();

    // decide reset
    bool reset = false;

    if (frame == 1 || prevTime < 0.0f || t < prevTime) {
        reset = true;
    }


    if (reset)
    {
        setParameters(t);
        solver.init(params);

        std::vector<Particle> particles;
        GA_Offset ptoff;
        GA_FOR_ALL_PTOFF(seed_geo, ptoff)
        {
            UT_Vector3 P = seed_geo->getPos3(ptoff);

            Particle p;
            p.pos = glm::vec3(P.x(), P.y(), P.z());
            p.vel = glm::vec3(
                evalFloat("initVelocity", 0, t),
                evalFloat("initVelocity", 1, t),
                evalFloat("initVelocity", 2, t)
			);
            p.mass = evalFloat("mass", 0, t);
            p.F = glm::mat3(1.0f);
			p.C = glm::mat3(0.0f);
            p.volume = params.cellSize * params.cellSize * params.cellSize;
            p.Jp = 1.0f;

            particles.push_back(p);
        }
        solver.setParticles(particles);

        // Write initial positions back to output just to stay consistent
        writeBack();
        prevTime = t;
        unlockInputs();
        return error();
    }

    fpreal rawFrameDt = reset ? (1.0 / fps) : (t - prevTime);
    if (rawFrameDt <= 0.0f)
        rawFrameDt = 1.0 / fps;

    fpreal frameDt = rawFrameDt * timescale;

    prevTime = t;

    CollisionSDF sdf = buildCollisionSDF(collider_geo);
    if (sdf.valid()) {
        solver.setCollisionSDF(sdf);
    }
    else {
        solver.clearCollisionSDF();
    }
 


    setParameters(t);
    float subDt = frameDt / (float)substeps;
    

    UT_AutoInterrupt progress("Running MPM Simulation");
    for (int s = 0; s < substeps; ++s)
    {
        int percent = (int)(100.0 * (s + 1) / substeps);
        if (progress.wasInterrupted(percent))
        {
            unlockInputs();
            addWarning(SOP_MESSAGE, "Simulation interrupted by user.");
            return error();
        }
        params.dt = subDt;
        solver.setParams(params);
        // emitter: add to solver, not gdp
        if (emit_on && frame % 5 == 0)
        {
            UT_BoundingBox emitBox;
            if (getContainerBBox(container_geo, emitBox))
            {
                std::vector<Particle> particles;
                particles.reserve(solver.getParticleCount() + emitrate);

                for (int i = 0; i < solver.getParticleCount(); ++i)
                    particles.push_back(solver.getParticle(i));

                UT_Vector3 minv = emitBox.minvec();
                UT_Vector3 maxv = emitBox.maxvec();

                SYSsrand48((long)frame);

                for (int e = 0; e < emitrate; ++e)
                {
                    float rx = (float)SYSdrand48();
                    float ry = (float)SYSdrand48();
                    float rz = (float)SYSdrand48();

                    UT_Vector3 birth_pos(
                        minv.x() + rx * (maxv.x() - minv.x()),
                        minv.y() + ry * (maxv.y() - minv.y()),
                        minv.z() + rz * (maxv.z() - minv.z())
                    );


                    Particle p;
                    p.pos = glm::vec3(birth_pos.x(), birth_pos.y(), birth_pos.z());
                    p.vel = glm::vec3(
                        (fpreal)(SYSdrand48() - 0.5) * 0.5f,
                        (fpreal)(SYSdrand48() * 0.5f),
                        (fpreal)(SYSdrand48() - 0.5) * 0.5f
                    );
                    p.mass = evalFloat("mass", 0, now);
                    p.is_emitter_particle = true;
                    p.F = glm::mat3(1.0f);
                    p.volume = params.cellSize * params.cellSize * params.cellSize;
                    p.Jp = 1.0f;

                    particles.push_back(p);
                }

                solver.setParticles(particles);
            }
        }
        solver.step();
    }

    writeBack();
    prevTime = now;

    unlockInputs();
    return error();
}