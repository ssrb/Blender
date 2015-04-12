#include "MOD_modifiertypes.h"
#include "DNA_object_types.h"
#include "MEM_guardedalloc.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <math.h>

#ifdef _OPENMP
#define OMP_MIN_RES 18
#endif

static bool loadWaveComplexAmplitude(ShallowWaterModifierData *swmd, int numVerts)
{
	FILE *amplitudeStream = fopen(swmd->solution, "r");
	if (amplitudeStream) {
		int vi;
		swmd->imag = MEM_mallocN(numVerts * sizeof(*swmd->imag), __func__);
		swmd->real = MEM_mallocN(numVerts * sizeof(*swmd->real), __func__);
		for (vi = 0; vi < numVerts; ++vi) {
			if (2 != fscanf(amplitudeStream, "%f%fi\n", &swmd->real[vi], &swmd->imag[vi]))
			{
				MEM_SAFE_FREE(swmd->real);
				MEM_SAFE_FREE(swmd->imag);
				fclose(amplitudeStream);
				return false;
			}
		}
		fclose(amplitudeStream);
		return true;
	}
	return false;
}

static void applyAmplitude(ShallowWaterModifierData *swmd, float (*vertexCos)[3], int numVerts)
{
	int vi;
	double ct = cos(swmd->time), st = sin(swmd->time);
	#pragma omp parallel for private(vi) if (numVerts > OMP_MIN_RES)
	for (vi = 0; vi < numVerts; ++vi)
	{
		vertexCos[vi][2] += swmd->amplitude_multiplier * (swmd->real[vi] * ct - swmd->imag[vi] * st);
	}
}

static void doShallowWater(ShallowWaterModifierData *swmd, float (*vertexCos)[3], int numVerts)
{
	static bool failedToLoadOnce = false;
	if (!failedToLoadOnce && !swmd->real && !swmd->imag) {		
		failedToLoadOnce = !loadWaveComplexAmplitude(swmd, numVerts);
	}
	if (swmd->real && swmd->imag) {
		applyAmplitude(swmd, vertexCos, numVerts);
	}
}

static void initData(ModifierData *md)
{
	ShallowWaterModifierData *swmd = (ShallowWaterModifierData *)md;
	swmd->real = swmd->imag = NULL;
	swmd->time = 1.0;
	swmd->amplitude_multiplier = 0.0001;
	swmd->solution[0] = '\0';
}

static void freeData(ModifierData *md)
{
	ShallowWaterModifierData *swmd = (ShallowWaterModifierData *)md;
	MEM_SAFE_FREE(swmd->real);
	MEM_SAFE_FREE(swmd->imag);
}

static void copyData(ModifierData *src, ModifierData *dst)
{
	const ShallowWaterModifierData *swsrc = (const ShallowWaterModifierData *) src;
	ShallowWaterModifierData *swdst = (ShallowWaterModifierData *) dst;
	swdst->time = swsrc->time;
	swdst->amplitude_multiplier = swsrc->amplitude_multiplier;
	strncpy(swdst->solution, swsrc->solution, 1024);
	swdst->real = swdst->imag = NULL;
}

static void deformVerts(	struct ModifierData *md, struct Object *UNUSED(ob),
                			struct DerivedMesh *UNUSED(derivedData),
               			 	float (*vertexCos)[3], int numVerts,
                			ModifierApplyFlag UNUSED(flag))
{
	doShallowWater((ShallowWaterModifierData *)md, vertexCos, numVerts);
}

static void deformVertsEM(	struct ModifierData *md, struct Object *UNUSED(ob),
                  			struct BMEditMesh *UNUSED(editData), struct DerivedMesh *UNUSED(derivedData),
                  			float (*vertexCos)[3], int numVerts)
{
	doShallowWater((ShallowWaterModifierData *)md, vertexCos, numVerts);
}

static bool dependsOnNormals(ModifierData *UNUSED(md))
{
	return true;
}

ModifierTypeInfo modifierType_ShallowWater = {
	/* name */              "ShallowWater",
	/* structName */        "ShallowWaterModifierData",
	/* structSize */        sizeof(ShallowWaterModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_SupportsEditmode |
	                        eModifierTypeFlag_EnableInEditmode,
	/* copyData */          copyData,
	/* deformVerts */       deformVerts,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     deformVertsEM,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     NULL,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  NULL,
	/* freeData */          freeData,
	/* isDisabled */        NULL,
	/* updateDepgraph */    NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */  dependsOnNormals,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
