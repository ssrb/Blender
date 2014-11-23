#include "MOD_modifiertypes.h"
#include "BKE_cdderivedmesh.h"
#include "DNA_object_types.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _OPENMP
#define OMP_MIN_RES 18
#endif

static bool GotoSection(FILE *stream, const char *tag) {
	char *line = NULL;
	size_t blen;
	ssize_t len;
	bool found = false;
	while((len = getline(&line, &blen, stream)) > 0) {
		line[len - 1] = '\0';
	  	if (!strcmp(line, tag)) {
	  		found = true;
	  		break;
	  	}
	}
	free(line);
	return found;
}

static bool ReadTriangles(	FILE *meshStream, 										
							DerivedMesh *mesh) {
	int totalNbTriangles, ti, *origindex;
	int domainId;
	MPoly *mpolys = NULL;
	MLoop *mloops = NULL;

	rewind(meshStream);

	if (!GotoSection(meshStream, "Triangles")) {
		return false;
	}
	
	fscanf(meshStream, "%d\n", &totalNbTriangles);
	
	mpolys = CDDM_get_polys(mesh);
	mloops = CDDM_get_loops(mesh);
	origindex = CustomData_get_layer(&mesh->polyData, CD_ORIGINDEX);
	
	for (ti = 0; ti < totalNbTriangles; ++ti) {

		MPoly *mp = &mpolys[ti];
		MLoop *ml = &mloops[ti * 3];

		if (4 != fscanf(meshStream, "%d %d %d %d\n", &ml[0].v, &ml[1].v, &ml[2].v, &domainId)) {
			return false;
		}

		// Vertices in .mesh are 1-indexed
		--ml[0].v;
		--ml[1].v;
		--ml[2].v;

		mp->loopstart = ti * 3;
		mp->totloop = 3;

		//mp->flag |= ME_SMOOTH;

		origindex[ti] = ORIGINDEX_NONE;	
	}

	return true;
}

static bool ReadVertices(FILE *meshStream, DerivedMesh *mesh) {

	int totalNbVertices, boundaryId, vi;
	MVert *mverts;

	rewind(meshStream);
	
	if (!GotoSection(meshStream, "Vertices")) {
		return false;
	}

	mverts = CDDM_get_verts(mesh);

	if (1 != fscanf(meshStream, "%d\n", &totalNbVertices)) {
		return false;
	}
	
	for (vi = 0; vi < totalNbVertices; ++vi) {
		float *co = mverts[vi].co;
		co[2] = 0.;
		if (3 != fscanf(meshStream, "%f %f %d\n", &co[0], &co[1], &boundaryId)) {
			return false;
		}
	}

	return true;
}

static bool ReadMeshInfo(FILE *meshStream, int *totalNbVertices, int *totalNbTriangles) {

	rewind(meshStream);

	if (!GotoSection(meshStream, "Vertices")) {
		return false;
	}

	if (1 != fscanf(meshStream, "%d\n", totalNbVertices)) {
		return false;
	}

	rewind(meshStream);

	if (!GotoSection(meshStream, "Triangles")) {
		return false;
	}

	if (1 != fscanf(meshStream, "%d\n", totalNbTriangles)) {
		return false;
	}

	return true;

}

static DerivedMesh *ReadMesh(const char *meshFileName) {

	FILE *meshStream = fopen(meshFileName, "r");
	DerivedMesh *mesh = NULL;
	int totalNbVertices, totalNbTriangles;

	if (!meshStream) {
		return NULL;
	}

	if (ReadMeshInfo(meshStream, &totalNbVertices, &totalNbTriangles)) {
		mesh = CDDM_new(totalNbVertices, 0, 0, totalNbTriangles * 3, totalNbTriangles);
		if (mesh && (!ReadVertices(meshStream, mesh) || !ReadTriangles(meshStream, mesh))) {
			mesh->release(mesh);
			mesh = NULL;
		}
	}

	fclose(meshStream);
	return mesh;
}

static DerivedMesh *generate_geometry(ShallowWaterModifierData *swmd)
{
	DerivedMesh *result = ReadMesh("/home/sb/Hacking/ssrb.github.com/assets/lyttelton/lyttelton.mesh");

	CDDM_calc_edges(result);

	// Maybe UV coord ?

	result->dirty |= DM_DIRTY_NORMALS;

	return result;
}

static bool LoadWaveComplexAmplitude(ShallowWaterModifierData *swmd, int numVerts)
{
	FILE *amplitudeStream = fopen("/home/sb/Hacking/ssrb.github.com/assets/lyttelton/lytteltonsol.txt", "r");
	int vi;
	swmd->imag = malloc(numVerts * sizeof(*swmd->imag));
	swmd->real = malloc(numVerts * sizeof(*swmd->real));

	for (vi = 0; vi < numVerts; ++vi) {
		fscanf(amplitudeStream, "%f%fi\n", &swmd->real[vi], &swmd->imag[vi]);
	}

	fclose(amplitudeStream);
	return true;
}

static DerivedMesh *do_shallow_water(ShallowWaterModifierData *swmd)
{
	
	DerivedMesh *mesh = generate_geometry(swmd);
	if (mesh && (!swmd->real || !swmd->imag))
	{
		int numVerts = mesh->getNumVerts(mesh);

		LoadWaveComplexAmplitude(swmd, numVerts);
	}

	return mesh;
}

static DerivedMesh *applyModifier(ModifierData *md, Object *ob,
                                  DerivedMesh *derivedData,
                                  ModifierApplyFlag UNUSED(flag))
{
	DerivedMesh *mesh = do_shallow_water((ShallowWaterModifierData *)md);
	return mesh ? mesh : derivedData;
}

static void initData(ModifierData *md)
{
	ShallowWaterModifierData *swmd = (ShallowWaterModifierData *)md;
	swmd->real = swmd->imag = NULL;
	swmd->time = 1.0;
}

static void freeData(ModifierData *md)
{
	ShallowWaterModifierData *swmd = (ShallowWaterModifierData *)md;
	free(swmd->real);
	free(swmd->imag);
}

ModifierTypeInfo modifierType_ShallowWater = {
	/* name */              "ShallowWater",
	/* structName */        "ShallowWaterModifierData",
	/* structSize */        sizeof(ShallowWaterModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_SupportsEditmode |
	                        eModifierTypeFlag_EnableInEditmode,
	/* copyData */          NULL,
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  NULL,
	/* freeData */          freeData,
	/* isDisabled */        NULL,
	/* updateDepgraph */    NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */  NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
