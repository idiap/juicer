/*
 * Copyright 2008 by The University of Edinburgh
 *
 * See the file COPYING for the licence associated with this software.
 *
 * HTKFlatModelsThreading.cpp  -  based on HTKFlatModels, with added functions for 2-thread decoding
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cassert>
#include <cstdlib>
#include "log_add.h"

#include "HTKFlatModelsThreading.h"
#include "LogFile.h"

#ifdef HAVE_INTEL_IPP
#include <ippcore.h>
#include <ipps.h>
#include <ippsr.h>
#endif

using namespace Torch;

namespace Juicer {

HTKFlatModelsThreading::HTKFlatModelsThreading() {
    fQueue = NULL;
    fCounter = -1;
    fStart = fEnd = 0;
    fRunning = true;
}

HTKFlatModelsThreading::~HTKFlatModelsThreading() {
    delete[] fQueue;
}

void HTKFlatModelsThreading::init()
{
    HTKFlatModels::init();

    fQueue = new FProbQueue[nGMMs];
    for(int i = 0; i < nGMMs; i++){
        fQueue[i].next=-1;
        fQueue[i].t=-1;
    }
}

bool HTKFlatModelsThreading::cachedOutput(int gmmInd, real* outp) {
    int n = currFrame - fCacheT[gmmInd];
    if (n < fnBlock) {
        *outp = fCache[gmmInd*fnBlock+n];
        return true;
    } else {
        return false;
    }
}

real HTKFlatModelsThreading::readOutput(int gmmInd) {
    int n = currFrame - fCacheT[gmmInd];
    if (n < fnBlock) {
        return fCache[gmmInd*fnBlock+n];
    } else {
        // the output of gmmInd is supposed to be calculated in thread2, but not due to
        // a very rare sync problem (get skipped in fQueue for whatever
        // reason), and a simple fix is to re-calculate it
         return this->calcSingleFrameGMMOutput(gmmInd);
    }
}

// GMM calculation (the same as real HTKFlatModels::calcGMMOutput( int gmmInd ))
// without updating the cache to avoid possible sync problem in thread 2. As
// thread 2 is dedicated to GMM calculation and can update cache at any time.
real HTKFlatModelsThreading::calcSingleFrameGMMOutput(int gmmInd )
{
    // compute GMM outp for current frame only
    real *dets=fDet(gmmInd);
    int nMix = fMixtures[gmmInd].compNum;
    real *means=fMean(gmmInd);
    real *vars=fVar(gmmInd);
    real logProb = LOG_ZERO;
#ifdef HAVE_INTEL_IPP
    ippsLogGaussMixture_32f_D2(currInputData[0],means,vars,nMix,fvecSize4, vecSize, dets, &logProb);
#else
    for (int i = 0; i < nMix; ++i) {
        real* m = means;
        real* v = vars;
        const real* x=currInputData[0];
        real sumxmu = 0.0;
        for (int j = 0; j < vecSize; ++j) {
            real xmu = *(x++) - *(m++); 
            sumxmu += xmu*xmu* *v++;
        }
        means += fvecSize4;
        vars += fvecSize4;
        logProb = HTKFlatModels::logAdd(logProb , -0.5*sumxmu +dets[i]) ;
    }
#endif

    return logProb;
}

void HTKFlatModelsThreading::addQueue(int gmmInd) {

    assert(fQueue[fEnd].next == -1);
    fQueue[fEnd].next = gmmInd;
    fEnd = gmmInd;
    fQueue[gmmInd].t = currFrame;
}

void HTKFlatModelsThreading::calcStates(){
   while(fRunning){
      if(fQueue[fStart].t>=0){
         this->calcOutput(fStart);
         fQueue[fStart].t=-1;
         fCounter++;
      }

      if(fStart!=fEnd){
         int n=fStart;
         fStart=fQueue[n].next;
         fQueue[n].next=-1;
      } else {
          // break;
      }
   }
   return;
}

}; // namespace juicer
