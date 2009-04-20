/*
 * Copyright 2008 by The University of Edinburgh
 *
 * See the file COPYING for the licence associated with this software.
 */

/*
 * vi:ts=4:tw=78:shiftwidth=4:expandtab
 * vim600:fdm=marker
 *
 * HTKFlatModelsThreading.h  -  based on HTKFlatModels, with added functions for used in a separate thread
 *
 */

#ifndef _HTKFLATMODELSTHREADING_H
#define _HTKFLATMODELSTHREADING_H

#include "HTKFlatModels.h"

namespace Juicer {
    typedef struct FProbQueue_{
        int   t;
        int   next;
    } FProbQueue;

    class HTKFlatModelsThreading : public HTKFlatModels {
        public:
            HTKFlatModelsThreading();
            virtual ~HTKFlatModelsThreading();
            void init();

            bool cachedOutput(int hmmInd, int stateInd, real* outp);
            bool cachedOutput(int gmmInd, real* outp);
            int addQueue(int hmmInd, int stateInd);
            int nReadyStates() { return fCounter; }
            void sync() { fCounter = -1; }
            int gmmInd(int hmmInd, int stateInd) { return hMMs[hmmInd].gmmInds[stateInd]; }
            void calcStates();
            void stop() { fRunning = false; }

        private:
            FProbQueue* fQueue;
            int  fStart;
            int  fEnd;
            int  fCounter;
            bool fRunning;
    };

}; // namespace juicer
#endif /* ifndef _HTKFLATMODELSTHREADING_H */

