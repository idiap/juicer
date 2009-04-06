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
 * WFSTDecoderLite.cpp  -  WFST decoder based on token-passing principle
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cassert>

#include <log_add.h>
#include "LogFile.h"
#include "WFSTDecoderLite.h"

#define MEMORY_POOL_REALLOC_AMOUNT 5000

using namespace std;
using namespace Torch;

namespace Juicer {
    const Token nullToken = {LOG_ZERO, LOG_ZERO, LOG_ZERO, NULL};


WFSTDecoderLite::WFSTDecoderLite(WFSTNetwork* network_ , 
        Models *models_ ,
        real phoneStartPruneWin_,
        real emitPruneWin_, 
        real phoneEndPruneWin_,
        real wordPruneWin_,
        int maxEmitHyps_ 
    ) 
{
    network = network_;
    hmmModels = models_;

    maxEmitHyps = maxEmitHyps_;
    emitPruneWin = emitPruneWin_;

    phoneEndPruneWin = phoneEndPruneWin_;
    phoneStartPruneWin = phoneStartPruneWin_;
    wordPruneWin = wordPruneWin_;

    LogFile::printf("WFSTDecoderLite initialised with:\n");
    LogFile::printf("\tmaxEmitHyps = %d\n",maxEmitHyps);
    emitPruneWin_ == LOG_ZERO ? LogFile::printf("\temitPruneWin = LOG_ZERO\n"):LogFile::printf("\temitPruneWin = %f\n", emitPruneWin_);
    phoneStartPruneWin_ == LOG_ZERO ? LogFile::printf("\tphoneStartPruneWin = LOG_ZERO\n"):LogFile::printf("\tphoneStartPruneWin = %f\n", phoneStartPruneWin_);
    phoneEndPruneWin_ == LOG_ZERO ? LogFile::printf("\tphoneEndPruneWin = LOG_ZERO\n"):LogFile::printf("\tphoneEndPruneWin = %f\n", phoneEndPruneWin_);
    wordPruneWin_ == LOG_ZERO ? LogFile::printf("\twordPruneWin = LOG_ZERO\n"):LogFile::printf("\twordPruneWin = %f\n", wordPruneWin_);
    

    setMaxAllocModels(10); // default is to keep 10% NetInsts out of all possible NetInsts

    if (maxEmitHyps > 0) {
        if (emitPruneWin > 0.0)
            emitHypsHistogram = new Histogram(1, -emitPruneWin - 800.0, 200.0);
        else
            emitHypsHistogram = new Histogram(1, -1000.0, 200.0);
    } else
        emitHypsHistogram = NULL;

    // initialise memory pools

    maxNStates = 0;
    for (int i = 0; i < hmmModels->getNumHMMs(); ++i)
        if (hmmModels->getNumStates(i) > maxNStates)
            maxNStates = hmmModels->getNumStates(i);
    nStatePools = maxNStates;

    pathPool = new BlockMemPool(sizeof(Path), MEMORY_POOL_REALLOC_AMOUNT);
    stateNPools = new BlockMemPool*[maxNStates];
    --stateNPools; /* address from 1 to maxNStates */
    for (int i = 1; i <= maxNStates; ++i)
        stateNPools[i] = new BlockMemPool(
            sizeof(NetInst)  + sizeof(Token)*i,
            MEMORY_POOL_REALLOC_AMOUNT/2
        );

    nAllocInsts = 0;

    dhhPool = NULL;   
    bestDecHyp = NULL;


    this->tokenBuf = new Token[maxNStates];
    this->tokenBuf[0] = nullToken; /* states other than entry will be overwritten during decoding */

    resetPathLists();

    activeNetInstList = NULL;
    newActiveNetInstList = NULL;
    newActiveNetInstListLastElem = NULL;

#ifdef PARTIAL_DECODING
    partialTraceInterval = 0;
#endif
}

WFSTDecoderLite::~WFSTDecoderLite() {
    delete[] tokenBuf;
    for (int i = 1; i <= nStatePools; ++i)
        delete stateNPools[i];
    ++stateNPools;
    delete[] stateNPools;
    delete pathPool;

    delete dhhPool;
    delete bestDecHyp;

    delete emitHypsHistogram;
}

// start recognition for a new utterance, so initialise per-utterance 
// resources here
void WFSTDecoderLite::recognitionStart() {
    // reset per-utterance stats
    this->currFrame = 0;
    bestFinalToken = nullToken;

    // we do this free memory stuff at the start rather then at the finish
    // because the returned best token from recognitionFinish() will still use the memory for path back tracking

    //  <<Free per-utterance memory>>
    {

        NetInst* inst = activeNetInstList;
        while (inst) {
            inst->nActiveHyps = 0;
            for (int i = 0; i < inst->nStates; ++i)
                inst->states[i] = nullToken;
            inst = inst->next;
        }
        activeNetInstList = NULL;
        assert(newActiveNetInstList == NULL);

        // free all paths
        pathPool->purge_memory();
        resetPathLists();

        if (nAllocInsts > maxAllocModels) {
            network->resetTransitionHooks();
            for (int i = 1; i <= maxNStates; ++i)
                stateNPools[i]->purge_memory();
            nAllocInsts = 0;
        }


        // clear memory left from last utterance
        delete dhhPool;
        dhhPool = NULL;

        delete bestDecHyp;
        bestDecHyp = NULL;

#ifdef PARTIAL_DECODING
        partialPaths.clear();
#endif

    } // end of <<Free per-utterance memory>>

    // reset global pruning stats
    if (emitHypsHistogram)
        emitHypsHistogram->reset();

    normaliseScore = 0.0;

    bestEmitScore = LOG_ZERO;  
#ifndef OPT_SINGLE_BEST
    bestStartScore = LOG_ZERO;
    bestEndScore = LOG_ZERO;
#endif

    currStartPruneThresh = LOG_ZERO;
    currEndPruneThresh = LOG_ZERO;
    currWordPruneThresh = LOG_ZERO;
    currEmitPruneThresh = LOG_ZERO;

    lastPathCollectFrame = -1;

#ifdef PARTIAL_DECODING
    lastPartialTraceFrame = -1;
#endif

    totalActiveModels = 0;
    totalActiveEmitHyps = 0;
    totalActiveEndHyps = 0;
    totalProcEmitHyps = 0;
    totalProcEndHyps = 0;

    nActiveInsts = 0;
    nActiveEmitHyps = 0;
    nActiveEndHyps = 0;
    nEmitHypsProcessed = 0;
    nEndHypsProcessed = 0;

    // create an empty token and propogate it into the network's entry transitions
    Token tmp;
    tmp.score = 0.0;
    tmp.acousticScore = 0.0;
    tmp.lmScore = 0.0;
    tmp.path = NULL;
    propagateToken(&tmp, NULL);
    joinNewActiveInstList();
}

DecHyp* WFSTDecoderLite::recognitionFinish() {
    LogFile::printf(
            "\nStatistics:\n  nFrames=%d\n  avgActiveEmitHyps=%.2f\n"
            "  avgActiveEndHyps=%.2f\n  avgActiveModels=%.2f\n"
            "  avgProcessedEmitHyps=%.2f\n  avgProcessedEndHyps=%.2f\n" ,
            currFrame+1,
            ((real)totalActiveEmitHyps)/(currFrame+1), 
            ((real)totalActiveEndHyps)/(currFrame+1),
            ((real)totalActiveModels)/ (currFrame+1),
            ((real)totalProcEmitHyps)/(currFrame+1),
            ((real)totalProcEndHyps)/(currFrame+1)
            ) ;

    Token best = bestFinalToken; 

#ifdef PARTIAL_DECODING
    // perform one more partial tracing from the best token
    if (partialTraceInterval > 0) {
        if (best.score > LOG_ZERO) {
            // only trace back if there is a token survrived
            traceWinningPaths(best.path);
        }
        LogFile::printf("Partial paths recovered at frames: ");
        for (vector<Path*>::iterator it = partialPaths.begin(); it != partialPaths.end(); ++it) {
            LogFile::printf("%03d ", (*it)->frame);
        }
        LogFile::printf("\n");
    }
#endif

    // the per utterance memory is not freed now, as the best token returned still use them
    // they will be cleared the next time recognitionStart() is called
    // <<Convert best token to DecHyp structure>>
    {
        if (best.score == LOG_ZERO) {
            assert(best.path == NULL);
            fprintf(stderr, "WARNING: no token survived at the end of decoding\n");
            return NULL;
        } else {
            assert(dhhPool == NULL);
            dhhPool = new BlockMemPool(sizeof(DecHypHist) , 100) ;
            bestDecHyp = new DecHyp();
            DecHypHist* oldHist = NULL;
            Path* p = best.path;
            while (p != NULL) {
                DecHypHist *tmp ;

                tmp = (DecHypHist *)(dhhPool->getElem()) ;
                tmp->type = DHHTYPE ;
                tmp->nConnect = 1 ;
                tmp->prev = NULL ;

                tmp->state = p->label;
                tmp->time = p->frame;
                tmp->score = p->score;
                tmp->acousticScore = p->acousticScore;
                tmp->lmScore = p->lmScore;

                if (oldHist != NULL) {
                    oldHist->prev = tmp;
                } else {
                    // need to update the last hypothesis as the best hypothesis can contain
                    // added final transition weights
                    tmp->lmScore = best.lmScore;
                    tmp->acousticScore = best.acousticScore;
                    tmp->score = best.score;

                    bestDecHyp->score = best.score;
                    bestDecHyp->lmScore = best.lmScore;
                    bestDecHyp->acousticScore = best.acousticScore;
                    bestDecHyp->hist = tmp;
                }
                oldHist = tmp;

                p = p->prev;
            }
            return bestDecHyp;
        }
    } // end of <<Convert best token to DecHyp structure>>
}

void WFSTDecoderLite::processFrame(real **inputVec, int frame_, int nFrames_) {
    // fprintf(stderr, "processing frame %d\n", currFrame); fflush(stderr);

    currFrame = frame_;

    hmmModels->newFrame(currFrame, inputVec, nFrames_); 
    bestFinalToken = nullToken; 


    //    <<Update start & emit pruning thresholds>>
    {

        normaliseScore = (bestEmitScore > LOG_ZERO ? bestEmitScore : 0.0);
        if (emitHypsHistogram) {

            currEmitPruneThresh = emitHypsHistogram->calcThresh(maxEmitHyps);
            currEmitPruneThresh -= normaliseScore;
            if (emitPruneWin > 0.0 && currEmitPruneThresh < -emitPruneWin)
                currEmitPruneThresh = -emitPruneWin;

            emitHypsHistogram->reset();
        } else {
            currEmitPruneThresh = (emitPruneWin > 0.0 ? -emitPruneWin : LOG_ZERO);
        }

#ifndef OPT_SINGLE_BEST
        currStartPruneThresh = (phoneStartPruneWin > 0.0 ? (bestStartScore - phoneStartPruneWin) : LOG_ZERO);
#else
        currStartPruneThresh = (phoneStartPruneWin > 0.0 ? (bestEmitScore - phoneStartPruneWin) : LOG_ZERO);
#endif
    } // end of <<Update start & emit pruning thresholds>>

    // <<Do hmm internal propagation for each active inst>>
    {

        nActiveEmitHyps = 0;
        nActiveEndHyps = 0;
        nEmitHypsProcessed = 0;
        nEndHypsProcessed = 0;

        bestEmitScore = LOG_ZERO; /* bestEmitScore & bestEndScore will be updated in HMMInternalPropagation() */
#ifndef OPT_SINGLE_BEST
        bestEndScore = LOG_ZERO;
#endif

        NetInst* prevInst = NULL;
        NetInst* inst = activeNetInstList;
        while (inst) {
            // language model pruning
            Token* entry = inst->states;
            if (entry->score > LOG_ZERO && entry->score < currStartPruneThresh) {
                *entry = nullToken;
                --inst->nActiveHyps;
            }

            HMMInternalPropagation(inst);

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


    } // end of <<Do hmm internal propagation for each active inst>>

    // Update end pruning thresholds
    // bestEndScore has now been updated during hmm internal propagation
#ifndef OPT_SINGLE_BEST
    currEndPruneThresh = (phoneEndPruneWin > 0.0 ? (bestEndScore - phoneEndPruneWin) : LOG_ZERO) ;
    currWordPruneThresh = (wordPruneWin > 0.0 ? (bestEndScore - wordPruneWin) : LOG_ZERO) ;
#else
    currEndPruneThresh = (phoneEndPruneWin > 0.0 ? (bestEmitScore - phoneEndPruneWin) : LOG_ZERO) ;
    currWordPruneThresh = (wordPruneWin > 0.0 ? (bestEmitScore - wordPruneWin) : LOG_ZERO) ;
#endif

    // <<Do external propagation (exit states) for each active inst (inc. tee and eplison transition)>>
    {
        nEndHypsProcessed = 0;
#ifndef OPT_SINGLE_BEST
        bestStartScore = LOG_ZERO; /* bestStartScore will be updated in propagateToken */
#endif

        NetInst* prevInst = NULL;
        NetInst* inst = activeNetInstList;
        while (inst) {
            WFSTTransition* trans = inst->trans;
            Token* exit_tok = &inst->states[inst->nStates - 1];
            if (exit_tok->score > LOG_ZERO) {
                // VW - word based pruning
                // Use a different purning threshold when a word is emitted
                if (inst->trans->outLabel == WFST_EPSILON) {
                    if (exit_tok->score > currEndPruneThresh) {
                        ++nEndHypsProcessed;
                        propagateToken(exit_tok, trans);
                    }
                } else {
                    if (exit_tok->score > currWordPruneThresh) {
                        ++nEndHypsProcessed;
                        propagateToken(exit_tok, trans);
                    }
                }

                *exit_tok = nullToken; // de-active exit_tok
                assert(inst->nActiveHyps >= 0);
                if (--inst->nActiveHyps == 0) {
                    inst = returnNetInst(inst, prevInst);
                } else {
                    prevInst = inst;
                    inst = inst->next;
                }
            } else {
                prevInst = inst;
                inst = inst->next;
            }
        }

        totalProcEndHyps += nEndHypsProcessed;

        joinNewActiveInstList();
        totalActiveModels += nActiveInsts;
    }

    // path collection
    // To speed up token assginment, the unused paths are collocted in a 
    // separate pass.
    {
        assert(nPath >= 0);
        real pathRatio = ((real)nPath)/nPathNew; /* # of last remained paths / # of newly created paths */

        if ((pathRatio > 12. && nPath > 10000) || (currFrame - lastPathCollectFrame > 100)) {
            // printf("pathRatio = %f, nPath = %d, nPathNew = %d\n", pathRatio, nPath, nPathNew);
            collectPaths();
#ifdef PARTIAL_DECODING
            // trace every partialTraceInterval frames
            if (partialTraceInterval > 0 && (currFrame - lastPartialTraceFrame > partialTraceInterval))
                tracePartialPath();
#endif
        }
    }
}

// internal propagation passes tokens within an HMM, tee transition is not 
// dealt with here
void WFSTDecoderLite::HMMInternalPropagation(NetInst* inst) {
    int N_1 = inst->nStates - 1; // nStates-1 is the only way nStates will be used here
    Token* res;
    Token* cur;
    real** trP = hmmModels->getTransMat(inst->hmmIndex);
    SEIndex* se = hmmModels->getSEIndex(inst->hmmIndex);
    
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
                real outp = hmmModels->calcOutput(inst->hmmIndex, j);
                res->score += outp;
                res->acousticScore += outp;
                if (emitHypsHistogram) {
                    emitHypsHistogram->addScore(res->score, LOG_ZERO);
                }
                if (res->score > bestEmitScore)
                    bestEmitScore = res->score;
    
            } else {
                // pruned away
                *res = nullToken;
            }
        }
        
        // copy all result tokens back to the instance, the entry token (tokenBuf[0]) is nullToken
        // ready for next propagation
        inst->nActiveHyps = 0;
        res = tokenBuf;
        cur = inst->states;
        for (int i = 0; i < N_1; ++i) {
            if (res->score > LOG_ZERO)
                ++inst->nActiveHyps;
            *cur++ = *res++;
        }
        nActiveEmitHyps += inst->nActiveHyps;
        
    } // end of <<Token propagation of emitting states>>

    // <<Token propagation of exit states>>
    // note the SEIndex does not include tee transition, which are dealt 
    // elsewhere
    {
        int i = se[N_1].start;
        int endi = se[N_1].end;
        
        res = &inst->states[N_1];
        cur = &inst->states[i];
        
        // assume a transition from i to exit
        *res = *cur;
        
        res->score += trP[i][N_1];
        res->acousticScore += trP[i][N_1];
        
        // compare with all other possible to-exit transitions
        for (++i, ++cur; i < endi; ++i, ++cur) {
            score_t tmpScore = cur->score + trP[i][N_1];
            if (tmpScore > res->score) {
                *res = *cur;
                res->score = tmpScore;
                res->acousticScore += trP[i][N_1];
    
                // non-emit states can not have higher score so no need to compare with bestEmitScore
            }
        }
        
        if (res->score <= LOG_ZERO) {
            *res = nullToken;
       } else {
#ifndef OPT_SINGLE_BEST
            if (res->score > bestEndScore)
                bestEndScore = res->score;
#else
            if (res->score > bestEmitScore)
                bestEmitScore = res->score;
#endif
            ++inst->nActiveHyps;
            ++nActiveEndHyps;
       }
       
       
    } // end of <<Token propagation of exit states>>
}

// pass token to a list of WFSTTransition following |trans|, tee transition 
// and eplison transion will be dealt with here
// new NetInst will be created and attached to new transition.
// this function also record word boundary from the trans argument if it's not
// NULL
void WFSTDecoderLite::propagateToken(Token* tok, WFSTTransition* trans) {
    assert(tok->score > LOG_ZERO);
    if (trans != NULL) {
        // for non-NULl transition do:
        // 1. Add a new path to |tok| if at word boundary
        {
            if (trans->outLabel != WFST_EPSILON) {
                Path* p;
                p = createNewNoRefPath();
                p->frame = currFrame;
                p->score = tok->score;
                p->lmScore = tok->lmScore;
                p->acousticScore = tok->acousticScore;
                p->label = trans->outLabel;
                p->prev = tok->path;
                if (p->prev != NULL)
                    refPath(p->prev);
                tok->path = p;
            }
        }
        // 2. Update |bestFinalToken| if trans reaches the final state
        {
            if (network->transGoesToFinalState(trans)) {
                real weight = network->getFinalStateWeight(trans);
                if (tok->score + weight > bestFinalToken.score) {
                    bestFinalToken = *tok;
                    bestFinalToken.score += weight;
                    bestFinalToken.lmScore += weight;
                }
            }
        }
    }

    // now retrieve the list of next transitions following |trans|
    int nTrans;
    WFSTTransition* transList;
    nTrans = network->getTransitions(trans, &transList);

    // <<Pass |tok| to each |trans| in |transList|>>
    {
        for (int iTrans = 0;  iTrans < nTrans ;  ++iTrans) {
            WFSTTransition* trans = transList + iTrans;
            if (trans->inLabel == WFST_EPSILON /* || trans->inLabel == network->getWordEndMarker() */ ) {
                // there is no instance attached to an epsilon transition
                // the token is simply passed on
                Token tmp = *tok;
                tmp.score += trans->weight;
                tmp.lmScore += trans->weight;
                if (tmp.score > currEndPruneThresh) 
                    propagateToken(&tmp, trans);
            } else {
                // normal transition, pass tok to the entry state of it's attached instance
                // create new NetInst if neccssary
                NetInst* inst = (NetInst*)trans->hook;
                if (inst == NULL) {
                    inst = attachNetInst(trans);
                } else  {
                    if (inst->nActiveHyps == 0) {
                        // this inst is reused for the 1st time
                        // put it into newActiveNetInstList
                        inst->next = newActiveNetInstList;
                        newActiveNetInstList = inst;
                        if (newActiveNetInstListLastElem == NULL)
                            newActiveNetInstListLastElem = inst;
                        ++nActiveInsts;
                    }
                }

                // pass token to entry state
                Token* res = inst->states;
                score_t newScore = tok->score + trans->weight;

                if (newScore > res->score) {

                    if (res->score <= LOG_ZERO)
                        ++inst->nActiveHyps;

                    *res = *tok;
                    res->score = newScore;
                    res->lmScore += trans->weight;

                    if (newScore > bestEmitScore)
                        bestEmitScore = newScore;

#ifndef OPT_SINGLE_BEST
                    if (newScore > bestStartScore)
                        bestStartScore = newScore;
#else
                    if (newScore > bestEmitScore)
                        bestEmitScore = newScore;
#endif
                }

                if (inst->teeWeight > LOG_ZERO) {
                    real teeWeight = inst->teeWeight;
                    newScore += teeWeight;
                    // if there is a tee transition, pass the token on
                    // and propagate it to next transitions
                    Token tmp = *tok;
                    tmp.score = newScore;
                    tmp.acousticScore += teeWeight;
                    tmp.lmScore += trans->weight;
                    if (inst->trans->outLabel != WFST_EPSILON) {
                        if ( newScore > currWordPruneThresh )
                            propagateToken(&tmp, trans);
                    } else {
                        if (newScore > currEndPruneThresh) 
                            propagateToken(&tmp, trans);
                    }
                } // handle teeWeight

            }
        }
    } // end <<Pass |tok| to each |trans| in |transList|>>
}

// newly created path is added to the front of noRefList
Path* WFSTDecoderLite::createNewNoRefPath() {
    Path* p = (Path*)pathPool->malloc();
    p->link = noRefList.link;
    p->knil = (Path*)&noRefList;
    p->link->knil = p->knil->link = p;
    p->directlyUsedByToken = false;
    p->refCount = 0;
#ifdef PARTIAL_DECODING
    p->jointCount = 0;
#endif
    ++nPath;
    return p;
}

void WFSTDecoderLite::destroyPath(Path* p) {
    p->link->knil = p->knil;
    p->knil->link = p->link; // remove from List
    pathPool->free(p);
    --nPath;
}

// increase the reference count and move the path to the front of yesRefList if necessary
void WFSTDecoderLite::refPath(Path* p) {
    if (p->refCount == 0) {
        p->link->knil = p->knil;
        p->knil->link = p->link; // remove from noRefList
        p->link = yesRefList.link; // insert to the front of yesRefList
        p->knil = &yesRefList;
        p->link->knil = p->knil->link = p;
    }
    ++p->refCount;
}

// move path back to *the tail* of noRefList
void WFSTDecoderLite::deRefPath(Path* p) {
    assert(p->refCount > 0);
    --p->refCount;
    if (p->refCount == 0) {
        // move to noRefList
        p->link->knil = p->knil;
        p->knil->link = p->link; // remove from yesRefList
        
        assert(noRefListTail.link == NULL);
        assert(yesRefListTail.link == NULL);
        
        p->link = (Path*)&noRefListTail;
        p->knil = noRefListTail.knil;
        noRefListTail.knil = p->knil->link = p;
    }
}

// this will move the path to the front of the yesRefList
void WFSTDecoderLite::movePathYesRefList(Path* p) {
    // detach from old list
    p->link->knil = p->knil;
    p->knil->link = p->link;
    
    // insert to the front of yesRefList
    p->link = yesRefList.link;
    p->knil = (Path*)&yesRefList;
    p->link->knil = p->knil->link = p;
}

// this will move the path to the tail of the yesRefList
void WFSTDecoderLite::movePathYesRefListTail(Path* p) {
    // detach from old list
    p->link->knil = p->knil;
    p->knil->link = p->link;
    
    // insert to the tail of yesRefList
    p->link = (Path*)&yesRefListTail;
    p->knil = yesRefListTail.knil;
    yesRefListTail.knil = p->knil->link = p;
}

void WFSTDecoderLite::resetPathLists() {

    noRefList.link = (Path*)&noRefListTail;
    noRefListTail.knil = (Path*)&noRefList;
    noRefListTail.link = noRefList.knil = NULL;
    noRefListTail.refCount = -2;

    yesRefList.link = (Path*)&yesRefListTail;
    yesRefListTail.knil = (Path*)&yesRefList;
    yesRefListTail.link = yesRefList.knil = NULL;
    yesRefListTail.refCount = -2;
    
    nPath = nPathNew = 0;

}

void WFSTDecoderLite::collectPaths() {

    // before each collection, the directlyUsedByToken of each path should be false
    // first scan all tokens in all instances and mark the directly referred paths
    NetInst* inst = activeNetInstList;
    while (inst) {
        int n = inst->nStates;
        Token* tok = inst->states;
        for (int i = 0; i < n; ++tok, ++i) {
            Path* path = tok->path;
            if (path && path->directlyUsedByToken == false) {
                path->directlyUsedByToken = true;
                if (path->refCount > 0) {
                        movePathYesRefList(path); // force move the path to the front of the yes list
                }
            }
        }
        inst = inst->next;
    }

    // now all directly referred path has directlyUsedByToken == true
    // all in-directly referred path are in yesRefList (via refPath(), or the above scan)
    // so we free all other paths in noRefList
    for (Path* path = noRefList.link; path->link != NULL;) {
        if (path->directlyUsedByToken == false) {
            if (path->prev)
                deRefPath(path->prev); // this will move the prev path to the tail of noRefList to be processed in this loop
            Path* p = path;
            path = path->link;
            destroyPath(p);
        } else {
            // reset directlyUsedByToken for next collection
            path->directlyUsedByToken = false;
            path = path->link;
        }
    }

    // reset directlyUsedByToken in yesRefList for next collection
    // note all such paths have been moved to the front part of the list
    for (Path* path = yesRefList.link; path->link != NULL; path = path->link) {
        if (path->directlyUsedByToken == false)
            break; // all the rest paths having directlyUsedByToken == false too
        path->directlyUsedByToken = false;
    }


    nPathNew = nPath;
    lastPathCollectFrame = currFrame;
}

// attach a NetInst to non-eplison transition, and add it to the *front* 
// active inst list, so it will be processed at the next frame
NetInst* WFSTDecoderLite::attachNetInst(WFSTTransition* trans) {
    assert(trans->hook == NULL);
    
    int hmmIndex = trans->inLabel - 1;
    int n = hmmModels->getNumStates(hmmIndex);
    NetInst* inst = (NetInst*)stateNPools[n]->malloc();
    inst->hmmIndex = hmmIndex;
    inst->nStates = n;
    for (int i = 0; i < n; ++i)
        inst->states[i] = nullToken;
    trans->hook = inst;
    inst->trans = trans;
    inst->teeWeight = hmmModels->getTeeLogProb(hmmIndex);
    inst->nActiveHyps = 0;
    inst->next = newActiveNetInstList;
    newActiveNetInstList = inst;
    if (newActiveNetInstListLastElem == NULL)
        newActiveNetInstListLastElem = inst;
    ++nActiveInsts;
    ++nAllocInsts;
    return inst;
}


NetInst* WFSTDecoderLite::returnNetInst(NetInst* inst, NetInst* prevInst) {
    // Return the model to the pool and remove it from the linked list of
    //   active models.
    if ( prevInst == NULL ) {
        // Model we are deactivating is at head of list.
        activeNetInstList = inst->next;
        for (int i = 0; i < inst->nStates; ++i)
            inst->states[i] = nullToken;
        inst = activeNetInstList;
    } else {
        // Model we are deactivating is not at head of list.
        prevInst->next = inst->next;
        for (int i = 0; i < inst->nStates; ++i)
            inst->states[i] = nullToken;
        inst = prevInst->next;
    }

    --nActiveInsts;
    // Return the pointer to the next active model in the linked list.
    return inst ;
}

void WFSTDecoderLite::joinNewActiveInstList() {
    if (newActiveNetInstList == NULL)
        return;
    newActiveNetInstListLastElem->next = activeNetInstList;
    activeNetInstList = newActiveNetInstList;
    newActiveNetInstList = newActiveNetInstListLastElem = NULL;
}

void WFSTDecoderLite::setMaxAllocModels(int maxAllocModels_) {
    assert(maxAllocModels_ > 0);
    if (maxAllocModels_ < 100) {
        // it's a percentage number
        maxAllocModels = network->getNumTransitions()*maxAllocModels_/100;
    } else if (maxAllocModels_ >= 100 && maxAllocModels_ < 8000) {
        // it's a memory limit in MB
        maxAllocModels = maxAllocModels_*1024*1024/(sizeof(NetInst)+sizeof(Token)*nStatePools);
    } else {
        // it's just a limit
        maxAllocModels = maxAllocModels_;
    }
    // printf("WFSTDecoderLite.cpp: maxAllocModels = %d, maxAllocModel_ = %d\n", maxAllocModels, maxAllocModel_);
}

#ifdef PARTIAL_DECODING
// return true if found a converged path during the call
bool WFSTDecoderLite::tracePartialPath() {
    // Idealy this function should be called immediately after collectPaths()
    // so that we do not waste time on resetting paths to be reclaimed
    bool found = false;
    Path* lastTracedPath = partialPaths.empty() ? NULL : (*partialPaths.rbegin());
    int lastTracedFrame = lastTracedPath ? lastTracedPath->frame : -1;

    // step 1: reset jointCount for tracing
    for (Path* path = yesRefList.link; path->link != NULL; path = path->link) {
        if (path->frame > lastTracedFrame)
            path->jointCount = 0;
    }
    for (Path* path = noRefList.link; path->link != NULL; path = path->link) {
        if (path->frame > lastTracedFrame) {
            path->jointCount = 0;
        }
    }

    // step 2: go through all open hypothesises and try to find a path that all paths converge into
    NetInst* inst = activeNetInstList;
    while (!found && inst) {
        // as all tokens within a NetInst have the same path history
        // so we only need to trace from the first valid one
        Token* tok = inst->states;
        Path* path = tok->path;
        while (path == NULL) {
            ++tok;
            path = tok->path;
        }

        // backtrace from this path
        while (path != NULL)  {
            if (path->frame > lastTracedFrame && ++path->jointCount == nActiveInsts) {
                found = true;
                // there may be other winning paths leading to this one but can not
                // be discovered by the algorithm, let's backtrace to find them out
                traceWinningPaths(path);
                break;
            }
            path = path->prev;
        }
        inst = inst->next;
    }

    lastPartialTraceFrame = currFrame;
    return found;
}

// trace winning paths from |path| (including those can not be discovered by tracePartialPath())
// up to the last traced path, and added them to partialPaths in order
void WFSTDecoderLite::traceWinningPaths(Path* path) {
    assert(path);

    Path* lastTracedPath = partialPaths.empty() ? NULL : (*partialPaths.rbegin());
    vector<Path*> paths;

    if (path == lastTracedPath) return;

    paths.push_back(path);
    while (lastTracedPath != path->prev) {
        paths.push_back(path->prev);
        path = path->prev;
    }
    for (vector<Path*>::reverse_iterator it = paths.rbegin(); it != paths.rend(); ++it) {
        partialPaths.push_back(*it);
    }
}

void WFSTDecoderLite::setPartialDecodeOptions(int traceInterval) {
    assert(traceInterval >= 0);
    partialTraceInterval = traceInterval;
    LogFile::printf("WFSTDecoderLite::partialTraceInterval = %d frames\n", partialTraceInterval);
}
#endif

};

