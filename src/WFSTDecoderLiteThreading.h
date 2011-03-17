/*
 * Copyright 2009 by The University of Edinburgh
 *
 * Copyright 2009 by Idiap Research Institute
 *                   http://www.idiap.ch
 *
 * Copyright 2009 by The University of Sheffield
 *
 * See the file COPYING for the licence associated with this software.
 *
 * WFSTDecoderLiteThreading.h  -  WFST decoder based on token-passing principle that uses a separate 
 * thread to handle GMM calculation
 * 
 */

#ifndef _WFSTDECODERLITETHREADING_H
#define _WFSTDECODERLITETHREADING_H

#include "WFSTDecoderLite.h"
#include "HTKFlatModelsThreading.h"

using namespace std;

namespace Juicer
{
    typedef struct WaitState_ {
        NetInst* inst;
        int state;
    } WaitState;


    class WFSTDecoderLiteThreading : public WFSTDecoderLite
    {
    public:
        WFSTDecoderLiteThreading(
            WFSTNetwork*network_ ,
            IModels *models_ ,
            real phoneStartPruneWin_,
            real emitPruneWin_,
            real phoneEndPruneWin_,
            real wordPruneWin_,
            int maxEmitHyps_
        );

        virtual ~WFSTDecoderLiteThreading() throw ();

    protected:
        HTKFlatModelsThreading* threadHMMModels;
        bool singleToExitTransitionModel;
        vector<vector<WaitState> > waitStateQueue;
        int waiting;
        int* waitGMMs;
        unsigned long gmmRequested;
        unsigned long gmmCalced;
        unsigned long gmmQueued;

        void doHMMInternalPropagation(); // overload WFSTDecoderLite::doHMMInternalPropagation()
        void HMMInternalPropagationPass1(NetInst* inst);
    };
};
#endif /* ifndef _WFSTDECODERLITETHREADING_H */
