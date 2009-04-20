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
 * WFSTDecoderLiteThreading.cpp  -  WFST decoder based on token-passing principle that uses a separate 
 * thread to handle GMM calculation. It can only handle HMMs with one to-exit transition, not counting tee transitions. 
 * Which is the case for all left-to-right HMMs commonly used in speech recognition.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cassert>

#include "LogFile.h"
#include "WFSTDecoderLiteThreading.h"

using namespace std;
using namespace Torch;

namespace Juicer {
    const Token nullToken = {LOG_ZERO, LOG_ZERO, LOG_ZERO, NULL};


WFSTDecoderLiteThreading::WFSTDecoderLiteThreading(WFSTNetwork* network_ , 
        Models *models_ ,
        real phoneStartPruneWin_,
        real emitPruneWin_, 
        real phoneEndPruneWin_,
        real wordPruneWin_,
        int maxEmitHyps_ 
    ) 
    :WFSTDecoderLite(network_,models_, phoneStartPruneWin_, emitPruneWin_,phoneEndPruneWin_,wordPruneWin_,maxEmitHyps_)
{
    threadHMMModels = (HTKFlatModelsThreading*)hmmModels;

    // check if all HMM's has only one transition to exit state (not including tee transitions)
    singleToExitTransitionModel = true;
    for (int i = 0; i < hmmModels->getNumHMMs(); ++i) {
        int n = hmmModels->getNumStates(i);
        SEIndex* se = hmmModels->getSEIndex(n-1);
        if (!(se->start == n-2 && se->end == n-1)) {
            singleToExitTransitionModel = false;
            break;
        }
    }
    if (singleToExitTransitionModel == false) {
        error("WFSTDecoderLiteThreading can not deal with HMMs with more than one to-exit transition (not counting tee transition).");
        exit(-1);
    }

    int nGMMs = 0;
    for (int i = 0; i < threadHMMModels->getNumHMMs(); ++i)
        nGMMs += threadHMMModels->getNumStates(i);
    waitStateQueue.resize(nGMMs);
    waitGMMs = new int[nGMMs];

    LogFile::printf("WFSTDecoderLiteThreading (nGMMs = %d) initialised successfully\n", nGMMs);
}

WFSTDecoderLiteThreading::~WFSTDecoderLiteThreading() {
    delete[] waitGMMs;
}


void WFSTDecoderLiteThreading::doHMMInternalPropagation() {
        nActiveEmitHyps = 0;
        nActiveEndHyps = 0;
        nEmitHypsProcessed = 0;
        nEndHypsProcessed = 0;

        bestEmitScore = LOG_ZERO; /* bestEmitScore & bestEndScore will be updated in HMMInternalPropagation() */
#ifndef OPT_SINGLE_BEST
        bestEndScore = LOG_ZERO;
#endif

        waiting = -1;
        for (int i = 0; i < waitStateQueue.size(); ++i)
            waitStateQueue[i].clear();
        gmmRequested = gmmQueued = gmmCalced = 0;

        NetInst* prevInst = NULL;
        NetInst* inst = activeNetInstList;
        while (inst) {
            // language model pruning
            Token* entry = inst->states;
            if (entry->score > LOG_ZERO && entry->score < currStartPruneThresh) {
                *entry = nullToken;
                --inst->nActiveHyps;
            }

            //TODO: assert(inst->nActiveHyps > 0);
            if (inst->nActiveHyps > 0) 
                HMMInternalPropagationPass1(inst);

            // post-emitting pruning
            assert(inst->nActiveHyps >= 0);
            if (inst->nActiveHyps == 0) {
                inst = returnNetInst(inst, prevInst);
            } else {
                prevInst = inst;
                inst = inst->next;
            }
        }

        totalActiveEmitHyps += nActiveEmitHyps;
        totalActiveEndHyps += nActiveEndHyps;
        totalProcEmitHyps += nEmitHypsProcessed;


    {
        // process all hmm emitting states in queue
        int i = 0;
        int j = -1;
        while (waiting > j) {
            j = threadHMMModels->nReadyStates();
            if ((j >= waiting) || (j > (i+5))) {
                for(;i<=j;i++){
                    real outp;
                    int gmmInd = waitGMMs[i];
                    bool rc = threadHMMModels->cachedOutput(gmmInd, &outp);
                    assert (rc == true);
                    for (int k = 0; k < waitStateQueue[gmmInd].size(); ++k) {
                        WaitState ws = waitStateQueue[gmmInd][k];
                        Token* res = ws.inst->states + ws.state;
                        res->score += outp;
                        res->acousticScore += outp;
                        if (emitHypsHistogram) {
                            emitHypsHistogram->addScore(res->score, LOG_ZERO);
                        }
                        if (res->score > bestEmitScore)
                            bestEmitScore = res->score;

                        // process exit state
                        int N_1 = ws.inst->nStates-1;
                        if (ws.state == N_1-1) {
                            Token* exit = res+1;
                            assert(exit == &ws.inst->states[N_1]);
                            *exit = *res;
                            real** trP = threadHMMModels->getTransMat(ws.inst->hmmIndex);
                            exit->score += trP[ws.state][N_1];
                            exit->acousticScore += trP[ws.state][N_1];
#ifndef OPT_SINGLE_BEST
                            if (exit->score > bestEndScore)
                                bestEndScore = cur->score;
#else
                            // exit->score is always smaller than res->score
                            // so no point to compare with bestEmitScore in this case
                            // if (exit->score > bestEmitScore)
                            //     bestEmitScore = exit->score;
#endif
                            ++ws.inst->nActiveHyps;
                            ++nActiveEndHyps;
                        }
                    }
                }
            } else {
            }
        }
        threadHMMModels->sync();
    }
}

// internal propagation: scan all emitting states and add states to wait queue 
void WFSTDecoderLiteThreading::HMMInternalPropagationPass1(NetInst* inst) {
    int N_1 = inst->nStates - 1; // nStates-1 is the only way nStates will be used here
    Token* res;
    Token* cur;
    real** trP = threadHMMModels->getTransMat(inst->hmmIndex);
    SEIndex* se = threadHMMModels->getSEIndex(inst->hmmIndex);

    inst->nActiveHyps = 0;
    
    // <<Token propagation of emitting states>>
    {
        // scan emitting states first
        res = tokenBuf+1;
        for (int j = 1 ; j < N_1; ++j, ++res) {
            int i = se[j].start;
            int endi = se[j].end;
            cur = &inst->states[i];
            
            // assume a transition from i to j
            *res = *cur;
            
            res->score += trP[i][j];
            res->acousticScore += trP[i][j];
            
            // then compare with all other possible incoming transitions
            for (++i, ++cur; i < endi; ++i, ++cur) {
                score_t tmpScore = cur->score + trP[i][j];
                if (tmpScore > res->score) {
                    *res = *cur;
                    res->score = tmpScore;
                    res->acousticScore += trP[i][j];
                }
            }
            // add output probability if above emitting threshold
            res->score -= normaliseScore;
            if (res->score > currEmitPruneThresh) {
                ++nEmitHypsProcessed;
                ++gmmRequested;

                real outp;
                if (threadHMMModels->cachedOutput(inst->hmmIndex, j, &outp)) {
                    // real outp = threadHMMModels->calcOutput(inst->hmmIndex, j);
                    res->score += outp;
                    res->acousticScore += outp;
                    if (emitHypsHistogram) {
                        emitHypsHistogram->addScore(res->score, LOG_ZERO);
                    }
                    if (res->score > bestEmitScore)
                        bestEmitScore = res->score;
                    ++gmmCalced;

                    // pass to exit state
                    if (j == N_1-1) {
                        Token* exit = &inst->states[N_1];
                        *exit = *res;
                        exit->score += trP[j][N_1];
                        exit->acousticScore += trP[j][N_1];
#ifndef OPT_SINGLE_BEST
                        if (exit->score > bestEndScore)
                            bestEndScore = cur->score;
#else
                        // exit->score is always smaller than res->score
                        // so no point to compare with bestEmitScore in this case
                        // if (exit->score > bestEmitScore)
                        //     bestEmitScore = exit->score;
#endif
                        ++inst->nActiveHyps;
                        ++nActiveEndHyps;
                    }
                } else {
                    // this state has not been calculated, added to queue
                    int gmmInd = threadHMMModels->gmmInd(inst->hmmIndex, j);
                    // wait_stat[waiting]=(inst,j);
                    if (waitStateQueue[gmmInd].empty()) {
                        waiting++;
                        waitGMMs[waiting] = gmmInd;
                        threadHMMModels->addQueue(inst->hmmIndex, j); 
                    }
                    WaitState ws;
                    ws.inst = inst;
                    ws.state = j;
                    waitStateQueue[gmmInd].push_back(ws);
                    ++gmmQueued;
                }
            } else {
                // pruned away
                *res = nullToken;
            }
        }

        // in singleToExitTransitionModel res is the only emitting state that goes to exit state
        if ((res-1)->score <= LOG_ZERO) {
            inst->states[N_1] = nullToken;
        }
        
        // copy all result tokens back to the instance, the entry token (tokenBuf[0]) is nullToken
        // ready for next propagation
        res = tokenBuf;
        cur = inst->states;
        for (int i = 0; i < N_1; ++i) {
            if (res->score > LOG_ZERO) {
                ++inst->nActiveHyps;
                ++nActiveEmitHyps;
            }
            *cur++ = *res++;
        }
    }
}
};

