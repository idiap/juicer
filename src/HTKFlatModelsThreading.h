/*
 * Copyright 2008 by The University of Edinburgh
 *
 * See the file COPYING for the licence associated with this software.
 */

/*
 * vi:ts=4:tw=78:shiftwidth=4:expandtab
 * vim600:fdm=marker
 *
 * HTKFlatModelsThreading.h  -  based on HTKFlatModels, with added functions for 2-thread decoding
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

            bool cachedOutput(int gmmInd, real* outp);
            real readOutput(int gmmInd);
            void addQueue(int gmmInd);
            int nReadyStates() { assert(fCounter >= -1); return fCounter; }
            void sync() { fCounter = -1; }
            int gmmInd(int hmmInd, int stateInd) { return hMMs[hmmInd].gmmInds[stateInd]; }
            void calcStates();
            void stop() { fRunning = false; }

        private:
            real calcSingleFrameGMMOutput(int gmmInd );
            FProbQueue* fQueue;
            int  fStart;
            int  fEnd;
            int  fCounter;
            bool fRunning;
    };

}; // namespace juicer
#endif /* ifndef _HTKFLATMODELSTHREADING_H */

