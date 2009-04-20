/*
 * Copyright 2008 by The University of Edinburgh
 *
 * See the file COPYING for the licence associated with this software.
 *
 * HTKFlatModelsThreading.cpp  -  based on HTKFlatModelsThreading, with added functions for used in a separate thread
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cassert>
#include <cstdlib>
#include "log_add.h"

#include "HTKFlatModelsThreading.h"
#include "LogFile.h"

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

bool HTKFlatModelsThreading::cachedOutput(int hmmInd, int stateInd, real* outp) {
    int gmmInd = hMMs[hmmInd].gmmInds[stateInd];
    int n = currFrame - fCacheT[gmmInd];
    if (n < fnBlock) {
        *outp = fCache[gmmInd*fnBlock+n];
        return true;
    } else {
        return false;
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
int HTKFlatModelsThreading::addQueue(int hmmInd, int stateInd) {
    int gmmInd = hMMs[hmmInd].gmmInds[stateInd];

    fQueue[fEnd].next = gmmInd;
    fEnd = gmmInd;
    fQueue[gmmInd].t = currFrame;
    return gmmInd;
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
