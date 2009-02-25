/*
 * Copyright 2008 by The University of Edinburgh
 *
 * See the file COPYING for the licence associated with this software.
 *
 * HTKFlatModels.cpp  -  store GMM parameters in flat arrays for fast
 * computation, derived from HTKModels.
 * by Zhang Le
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cassert>
#include <cstdlib>
#include "log_add.h"

#ifdef HAVE_INTEL_IPP
#include <ippcore.h>
#include <ipps.h>
#include <ippsr.h>
#endif

#include "HTKFlatModels.h"


using namespace Torch;

namespace Juicer {

HTKFlatModels::HTKFlatModels() {
    fBuffer = NULL;
}

HTKFlatModels::~HTKFlatModels() {
#ifdef HAVE_INTEL_IPP
    ippFree(fBuffer);
#else
    free(fBuffer);
#endif
}

void HTKFlatModels::readBinary( const char *fName ) {
    HTKModels::readBinary(fName);
    init();
}
void HTKFlatModels::Load( const char *htkModelsFName , bool removeInitialToFinalTransitions_ ) {
    HTKModels::Load(htkModelsFName, removeInitialToFinalTransitions_);
    init();
}

void HTKFlatModels::init()
{
#ifdef HAVE_INTEL_IPP
    ippStaticInit();
    int cpuMhz;
    ippGetCpuFreqMhz(&cpuMhz);
    const IppLibraryVersion* version = ippsGetLibVersion();
    printf("Optimised code using INTEL Performance Primitives Lib [%s, %s]\n", version->Name, version->Version);
    printf("CPU runs at %d Mhz\n", cpuMhz);
#endif

    // now create our own GMM parameter structure from loaded structures from
    // HTKModels
    int nMaxGmmComp = 0;
    for (int i = 0; i < nMixtures; ++i) {
        if (mixtures[i].nComps > nMaxGmmComp)
            nMaxGmmComp = mixtures[i].nComps;
    }

    fnGaussians = nMaxGmmComp*nMixtures;
#ifdef OPTIMISE_ALIGN4
    fvecSize4=(vecSize+3)&(~3);
    fnMixtures4 = (nMixtures+3)&(~3);
    fnGaussians4 = (fnGaussians+3)&(~3);
#else
    fvecSize4 = vecSize;
    fnMixtures4 = nMixtures;
    fnGaussians4 = fnGaussians;
#endif

    int model_len=sizeof(FMixture)*fnMixtures4+sizeof(real)*(fnGaussians4+ 
            fnGaussians*fvecSize4+fnGaussians*fvecSize4);
    printf("HTKFlatModels allocated %.2f MB for flat parameters\n", model_len/(1024.*1024));
#ifdef HAVE_INTEL_IPP
    fBuffer = ippMalloc(model_len);
#else
    fBuffer = malloc(model_len);
#endif
    if (!fBuffer)
        error("fail to allocate memory for HTKFlatModels");
    fMixtures     = (FMixture*)fBuffer;
    fDets    = (real*)(fMixtures+fnMixtures4);
    fMeans      = fDets+fnGaussians4;
    fVars       = fMeans+fnGaussians*fvecSize4;

    for (int i = 0; i < nMixtures; ++i) {
        fMixtures[i].compNum = mixtures[i].nComps;
        fMixtures[i].compInd = i*nMaxGmmComp;
    }

    // all structures created, now copy the parameters over
    for (int i = 0; i < nMixtures; ++i) {
        Mixture* mix = mixtures + i;
        real* means_new = fMean(i);
        real* vars_new = fVar(i);
        for (int j = 0; j < mix->nComps; ++j) {
            real* means_old_j = meanVecs[mix->meanVecInds[j]].means;
            real* means_new_j = means_new+j*fvecSize4;
            real* vars_old_j = varVecs[mix->varVecInds[j]].vars;
            real* vars_new_j = vars_new+j*fvecSize4;
            for (int k = 0; k < vecSize; ++k) {
                means_new_j[k] = means_old_j[k];
                vars_new_j[k] = 1.0/vars_old_j[k]; // actually stores inversed variance
            }
            // Det = -0.5*(d log2pi + log(var))
            fDet(i)[j] = varVecs[mix->varVecInds[j]].sumLogVarPlusNObsLog2Pi;
        }
    }

    // since the GMM parameters here are not shared, add log weights to dets
    // as part of the pre-computation
    for (int i = 0; i < nGMMs; ++i) {
        real* logCompWeights = gMMs[i].logCompWeights;
        Mixture* mix = mixtures + gMMs[i].mixtureInd;
        // printf("add log weights of GMM %d Mixture %d\n", i, gMMs[i].mixtureInd);
        for (int j = 0; j < mix->nComps; ++j) {
            fDet(i)[j] += logCompWeights[j];
        }
    }

}

real HTKFlatModels::calcOutput( int hmmInd , int stateInd )
{
#ifdef DEBUG
   if ( (hmmInd < 0) || (hmmInd >= nHMMs) )
      error("HTKModels::calcOutput - hmmInd out of range") ;
   if ( (stateInd < 0) || (stateInd >= hMMs[hmmInd].nStates) )
      error("HTKModels::calcOutput - stateInd out of range") ;
   if ( hMMs[hmmInd].gmmInds[stateInd] < 0 )
      error("HTKModels::calcOutput - no gmm associated with stateInd") ;
#endif

   if ( hybridMode )
   {
#ifdef DEBUG
      if ( hmmInd != hMMs[hmmInd].gmmInds[stateInd] )
         error("HTKModels::calcOutput - unexpected gmmInds value") ;
#endif
      return ( currInput[hmmInd] - logPriors[hmmInd] ) ;
   }
   else
      return calcGMMOutput( hMMs[hmmInd].gmmInds[stateInd] ) ;
}

real HTKFlatModels::calcOutput( int gmmInd ) { // new version of GMM obversion calculation
#ifdef DEBUG
   if ( hybridMode )
   {
      if ( (gmmInd < 0) || (gmmInd >= nHMMs) )
         error("HTKModels::calcOutput(2) - gmmInd out of range") ;
   }
   else
   {
      if ( (gmmInd < 0) || (gmmInd >= nGMMs) )
         error("HTKModels::calcOutput(2) - gmmInd out of range") ;
   }
#endif

   if ( hybridMode )
   {
      //printf("%.3f %.3f\n",currInput[gmmInd],logPriors[gmmInd]);fflush(stdout);
      return ( currInput[gmmInd] - logPriors[gmmInd] ) ;
   }
   else
      return calcGMMOutput( gmmInd ) ;
}
    

real HTKFlatModels::calcGMMOutput( int gmmInd )
{
    if ( currGMMOutputs[gmmInd] <= LOG_ZERO ) {
        // real* logCompWeights = gMMs[gmmInd].logCompWeights;
        real logProb = LOG_ZERO;
        real *means=fMean(gmmInd);
        real *vars=fVar(gmmInd);
        real *dets=fDet(gmmInd);
        int nMix = fMixtures[gmmInd].compNum;

#ifdef HAVE_INTEL_IPP
        ippsLogGaussMixture_32f_D2(currInput,means,vars,nMix,fvecSize4, vecSize, dets, &logProb);
#else
        for (int i = 0; i < nMix; ++i) {
            real* m = means;
            real* v = vars;
            const real* x=currInput;
            real sumxmu = 0.0;
            for (int j = 0; j < vecSize; ++j) {
                real xmu = *(x++) - *(m++); 
                sumxmu += xmu*xmu* *v++;
            }
            means += fvecSize4;
            vars += fvecSize4;
            logProb = logAdd(logProb , -0.5*sumxmu +dets[i]) ;

        }
#endif
        currGMMOutputs[gmmInd] = logProb;
        return logProb;
    }
    return currGMMOutputs[gmmInd] ;
}

}; // namespace juicer
