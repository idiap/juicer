/*
 * vi:ts=4:tw=78:shiftwidth=4:expandtab
 * vim600:fdm=marker
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

    printf("WFSTDecoderLite (maxEmitHyps = %d, emit/p_start/p_end/w_end PruneWin (%f/%f/%f/%f)\n", maxEmitHyps, emitPruneWin_, phoneStartPruneWin_, phoneEndPruneWin_, wordPruneWin_);

    if (maxEmitHyps > 0) {
        if (emitPruneWin > 0.0)
            emitHypsHistogram = new Histogram(1, -emitPruneWin - 800.0, 200.0);
        else
            emitHypsHistogram = new Histogram(1, -1000.0, 200.0);
    } else
        emitHypsHistogram = NULL;

    // initialise memory pools
    {
        maxNStates = 0;
        for (int i = 0; i < hmmModels->getNumHMMs(); ++i)
            if (hmmModels->getNumStates(i) > maxNStates)
                maxNStates = hmmModels->getNumStates(i);
        nStatePools = maxNStates;

        pathPool = new BlockMemPool(sizeof(Path), MEMORY_POOL_REALLOC_AMOUNT);
        netInstPool = new BlockMemPool(sizeof(NetInst), MEMORY_POOL_REALLOC_AMOUNT/2);
        stateNPools = new BlockMemPool*[maxNStates];
        --stateNPools; /* address from 1 to maxNStates */
        for (int i = 1; i <= maxNStates; ++i)
            stateNPools[i] = new BlockMemPool(sizeof(Token)*i, MEMORY_POOL_REALLOC_AMOUNT/2);

        dhhPool = NULL;   
        bestDecHyp = NULL;
    }

    this->tokenBuf = new Token[maxNStates];
    this->tokenBuf[0] = nullToken; /* states other than entry will be overwritten during decoding */

    resetPathLists();

}

WFSTDecoderLite::~WFSTDecoderLite() {
    delete[] tokenBuf;
    for (int i = 1; i <= nStatePools; ++i)
        delete stateNPools[i];
    ++stateNPools;
    delete[] stateNPools;
    delete pathPool;
    delete netInstPool;

    delete dhhPool;
    delete bestDecHyp;

    if (emitHypsHistogram)
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
        // as destroyNetInst() is not used to free instances,
        // need to manually reset trans->hook
        for(list<NetInst*>::iterator it = netInstList.begin(); it != netInstList.end(); ++it) {
            (*it)->trans->hook = NULL;
        }
        netInstList.clear();
        netInstPool->purge_memory();

        // free all paths
        pathPool->purge_memory();
        resetPathLists();

        // free all tokens
        for (int i = 1; i <= maxNStates; ++i)
            stateNPools[i]->purge_memory();

        if (dhhPool) {
            delete dhhPool;
            dhhPool = NULL;
        }

        if (bestDecHyp) {
            delete bestDecHyp;
            bestDecHyp = NULL;
        }

    } // end of <<Free per-utterance memory>>

    // reset global pruning stats
    if (emitHypsHistogram)
        emitHypsHistogram->reset();

    normaliseScore = 0.0;

    bestEmitScore = LOG_ZERO;  
    bestStartScore = LOG_ZERO;
    bestEndScore = LOG_ZERO;

    currStartPruneThresh = LOG_ZERO;
    currEndPruneThresh = LOG_ZERO;
    currWordPruneThresh = LOG_ZERO;
    currEmitPruneThresh = LOG_ZERO;

    lastPathCollectFrame = 0;

    totalActiveModels = 0;
    totalActiveEmitHyps = 0;
    totalActiveEndHyps = 0;
    totalProcEmitHyps = 0;
    totalProcEndHyps = 0;

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
}

DecHyp* WFSTDecoderLite::recognitionFinish() {
    Token best = nullToken;
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
       
    best = bestFinalToken;

//    printf("\n");
//    printf("%03d, best score:%f, best acousticScore: %f, best lmScore:%f\n", currFrame, best.score, best.acousticScore, best.lmScore);

    // the per utterance memory is not freed now, as the best token returned still use them
    // they will be cleared the next time recognitionStart() is called
    // <<Convert best token to DecHyp structure>>
    {
        if (best.score == LOG_ZERO) {
            fprintf(stderr, "WARNING: no token survived at the end of decoding\n");
            return NULL;
        } else {
            assert(dhhPool == NULL);
            dhhPool = new BlockMemPool(sizeof(DecHypHist) , 100) ;
            bestDecHyp = new DecHyp();
            DecHypHist* oldHist = NULL;
            Path* p = best.path;
            int i = 0;
            while (p != NULL) {
                DecHypHist *tmp ;
            
                tmp = (DecHypHist *)(dhhPool->getElem()) ;
               tmp->type = DHHTYPE ;
               tmp->nConnect = 1 ;
               tmp->prev = NULL ;
              
               //printf("DEBUG_PATH path[%03d] score = %.15f\n", i++, p->acousticScore);
               tmp->state = p->label;
               tmp->score = p->score;
               tmp->time = p->frame;
               tmp->acousticScore = p->acousticScore;
               tmp->lmScore = p->lmScore;
               
               if (oldHist != NULL) {
                   oldHist->prev = tmp;
               } else {
                    // need to update the last hypothesis as the best hypothesis can contain
                    // added final transition weights
                    tmp->lmScore = best.lmScore;
                    tmp->score = best.score;
                    tmp->acousticScore = best.acousticScore;
                    
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

void WFSTDecoderLite::processFrame(real* inputVec, int frame_) {
    // fprintf(stderr, "processing frame %d\n", frame_);

    currFrame = frame_;

    // TODO: a better caching mechanism
    hmmModels->newFrame(currFrame, inputVec); // clear GMM cache
    bestFinalToken = nullToken; 


    //    <<Update start & emit pruning thresholds>>
    {

        normaliseScore = (bestEmitScore > LOG_ZERO ? bestEmitScore : 0.0);
        if (emitHypsHistogram) {

            currEmitPruneThresh = emitHypsHistogram->calcThresh(maxEmitHyps);
            // printf("%03d, emitHypsHistogram->calcThresh(%d) = %f\n", currFrame, maxEmitHyps, currEmitPruneThresh);
            currEmitPruneThresh -= normaliseScore;
            if (emitPruneWin > 0.0 && currEmitPruneThresh < -emitPruneWin)
                currEmitPruneThresh = -emitPruneWin;

            emitHypsHistogram->reset();
        } else {
            currEmitPruneThresh = (emitPruneWin > 0.0 ? -emitPruneWin : LOG_ZERO);
        }

        currStartPruneThresh = (phoneStartPruneWin > 0.0 ? (bestStartScore - phoneStartPruneWin) : LOG_ZERO);
        // printf("%03d, bestEmitScore = %f\n", currFrame, bestEmitScore);
        // printf("%03d, normaliseScore = %f, currEmitPruneThresh = %f\n", currFrame, normaliseScore, currEmitPruneThresh);
    } // end of <<Update start & emit pruning thresholds>>

    // <<Do hmm internal propagation for each active inst>>
    {

        nActiveEmitHyps = 0;
        nActiveEndHyps = 0;
        nEmitHypsProcessed = 0;
        nEndHypsProcessed = 0;

        bestEmitScore = LOG_ZERO; /* bestEmitScore & bestEndScore will be updated in HMMInternalPropagation() */
        bestEndScore = LOG_ZERO;

        int nPruned = 0;
        int nInst = netInstList.size();
        list<NetInst*>::iterator endOfOldList = netInstList.end();
        for(list<NetInst*>::iterator it = netInstList.begin(); it != endOfOldList;) {
            NetInst* inst = *it;

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
                it = netInstList.erase(it); 
                destroyNetInst(inst);
                ++nPruned;
            } else {
                ++it;
            }
        }

        totalActiveEmitHyps += nActiveEmitHyps;
        totalActiveEndHyps += nActiveEndHyps;
        totalProcEmitHyps += nEmitHypsProcessed;

#if 0
        int nActiveTokens = 0;

        for(list<NetInst*>::iterator it = netInstList.begin(); it != netInstList.end();++it) {
            NetInst* inst = *it;
            for (int i = 0; i < inst->nStates-1; ++i) {
                if ((inst->states[i]).score > LOG_ZERO)
                    ++nActiveTokens;
            }
        }
        printf("%03d, active tokens = %d\n", currFrame, nActiveTokens);
#endif


        // printf("%03d, %d/%d = %d pruned after HMM internal propagation\n", currFrame, nPruned, nInst, netInstList.size());

    } // end of <<Do hmm internal propagation for each active inst>>

    //printf("%03d, n(Active/Emit/End)Hyps = (%d/%d/%d)\n", currFrame, nActiveEmitHyps + nActiveEndHyps, nActiveEmitHyps, nActiveEndHyps);

#if 0
    if (emitHypsHistogram)
        printf("%03d, histogram->count: %d\n", currFrame, emitHypsHistogram->count);
#endif

    // Update end pruning thresholds
    // bestEndScore has now been updated during hmm internal propagation
    currEndPruneThresh = (phoneEndPruneWin > 0.0 ? (bestEndScore - phoneEndPruneWin) : LOG_ZERO) ;
    currWordPruneThresh = (wordPruneWin > 0.0 ? (bestEndScore - wordPruneWin) : LOG_ZERO) ;
    //printf("%03d, currEndPruneThresh = %f\n", currFrame, currEndPruneThresh);

    // <<Do external propagation (exit states) for each active inst (inc. tee and eplison transition)>>
    {
        nEndHypsProcessed = 0;
        int nEnd1 = 0;
        int nEnd2 = 0;
        int nPruned = 0;

        bestStartScore = LOG_ZERO; /* bestStartScore will be updated in propagateToken */
        list<NetInst*>::iterator endOfOldList = netInstList.end();
        for(list<NetInst*>::iterator it = netInstList.begin(); it != endOfOldList; ) {
            NetInst* inst = *it;
            WFSTTransition* trans = inst->trans;
            Token* exit_tok = inst->states + inst->nStates - 1;
            if (exit_tok->score > LOG_ZERO) {
                // VW - word based pruning
                // Use a different purning threshold when a word is emitted
                if (inst->trans->outLabel == WFST_EPSILON) {
                    if (exit_tok->score > currEndPruneThresh) {
                        ++nEndHypsProcessed;
                        ++nEnd1;
                        propagateToken(exit_tok, trans);
                    }
                } else {
                    if (exit_tok->score > currWordPruneThresh) {
                        ++nEndHypsProcessed;
                        ++nEnd2;
                        propagateToken(exit_tok, trans);
                    }
                }

                *exit_tok = nullToken; // de-active exit_tok
                assert(inst->nActiveHyps >= 0);
                if (--inst->nActiveHyps == 0) {
                    it = netInstList.erase(it); 
                    destroyNetInst(inst);
                    ++nPruned;
                } else {
                    ++it;
                }
            } else
                ++it;
        }


        //printf("%03d, nEnd1=%d, nEnd2=%d\n", currFrame, nEnd1, nEnd2);
        // printf("%03d, nPruned=%d\n", currFrame, nPruned);

        totalProcEndHyps += nEndHypsProcessed;
    }

    //printf("%03d, nEndHypsProcessed = %d\n", currFrame, nEndHypsProcessed);
    //printf("%03d, nActiveModels = %d\n", currFrame, netInstList.size());
    totalActiveModels += netInstList.size();

    // path collection
    // To speed up token assginment, the unused paths are collocted in a 
    // separate pass.
    {
        assert(nPath >= 0);
        real pathRatio = ((real)nPath)/nPathNew; /* # of newly created paths / # of last remained paths */

        if (pathRatio > 10. && nPath > 4096 || (currFrame - lastPathCollectFrame > 100)) {
            // printf("%03d, nPath/nPathNew = (%d/%d) = %f\n", currFrame, nPath, nPathNew, pathRatio);
            // printf("%03d, %d paths since last collection\n", currFrame, nPath - nPathNew);
            collectPaths();
            lastPathCollectFrame = currFrame;
        }
    }
}

// internal propagation passes tokens within an HMM, tee transition is not 
// dealt with here
void WFSTDecoderLite::HMMInternalPropagation(NetInst* inst) {
    int N = inst->nStates;
    Token* res;
    Token* cur;
    Token tmp;
    real** trP = hmmModels->getTransMat(inst->hmmIndex);
    SEIndex* se = hmmModels->getSEIndex(inst->hmmIndex);
    
    // <<Token propagation of emitting states>>
    {
        // scan emitting states first
        res = tokenBuf+1;
        for (int j = 1 ; j < N-1; ++j, ++res) {
            int i = se[j].start;
            int endi = se[j].end;
            cur = inst->states + i;
            
            // assume a transition from i to j
            *res = *cur;
            
            res->score += trP[i][j];
            res->acousticScore += trP[i][j];
            
            // then compare with all other possible incoming transitions
            for (++i, ++cur; i < endi; ++i, ++cur) {
                double tmpScore = cur->score + trP[i][j];
                if (tmpScore > res->score) {
                    *res = *cur;
                    res->score = tmpScore;
                    res->acousticScore += trP[i][j];
                }
            }
            // add output probability if above emitting threshold
            //assert(res->score > LOG_ZERO);
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
        for (int i = 0; i < N-1; ++i) {
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
        int i = se[N-1].start;
        int endi = se[N-1].end;
        
        res = inst->states + N-1;
        cur = inst->states + i;
        
        // assume a transition from i to exit
        *res = *cur;
        
        res->score += trP[i][N-1];
        res->acousticScore += trP[i][N-1];
        
        // compare with all other possible to-exit transitions
        for (++i, ++cur; i < endi; ++i, ++cur) {
            double tmpScore = cur->score + trP[i][N-1];
            if (tmpScore > res->score) {
                *res = *cur;
                res->score = tmpScore;
                res->acousticScore += trP[i][N-1];
    
                // non-emit states can not have higher score so no need to compare with bestEmitScore
            }
        }
        
        // pruning?
        if (res->score <= LOG_ZERO) {
            *res = nullToken;
       } else {
            if (res->score > bestEndScore)
                bestEndScore = res->score;
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
                if (trans->hook == NULL) {
                    attachNetInst(trans);
                }
                NetInst* inst = (NetInst*)trans->hook;

                // pass token to entry state
                Token* res = inst->states;
                double newScore = tok->score + trans->weight;

                if (newScore > res->score) {

                    if (res->score <= LOG_ZERO)
                        ++inst->nActiveHyps;

                    *res = *tok;
                    res->score = newScore;
                    res->lmScore += trans->weight;

                    if (newScore > bestEmitScore)
                        bestEmitScore = newScore;

                    if (newScore > bestStartScore)
                        bestStartScore = newScore;
                }

                real teeWeight = hmmModels->getTeeLogProb(inst->hmmIndex);
                //teeWeight = LOG_ZERO;
                if (teeWeight > LOG_ZERO) {
                    newScore += teeWeight;
                    // if there is a tee transition, pass the token on
                    // and propagate it to next transitions
                    Token tmp = *tok;
                    tmp.score = newScore;
                    tmp.acousticScore += teeWeight;
                    tmp.lmScore += trans->weight;
                    if (trans->inLabel == network->silMarker || trans->inLabel == network->spMarker ) {
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
    ++nPath;
    return p;
}

void WFSTDecoderLite::destroyPath(Path* p) {
    p->link->knil = p->knil;
    p->knil->link = p->link; // remove from List
    pathPool->free(p);
    --nPath;
}

// increase the reference count and move the path to yesRefList if necessary
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

// move path back to *the tail* of noRefList if necessary
void WFSTDecoderLite::deRefPath(Path* p) {
    assert(p->refCount > 0);
    --p->refCount;
    if (p->refCount == 0) {
        // move to noRefList
        p->link->knil = p->knil;
        p->knil->link = p->link; // remove from yesRefList
        
        /*
        p->link = noRefList.link; // insert to the front of noRefList
        p->knil = (Path*)&noRefList;
        p->link->knil = p->knil->link = p;
        */
        assert(noRefListTail.link == NULL);
        assert(yesRefListTail.link == NULL);
        
        p->link = (Path*)&noRefListTail;
        p->knil = noRefListTail.knil;
        noRefListTail.knil = p;
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
    for(list<NetInst*>::iterator it = netInstList.begin(); it != netInstList.end(); ++it) {
        NetInst* inst = *it;
        int n = inst->nStates;
        Token* tok = inst->states;
        for (int i = 0; i < n; ++tok, ++i) {
            Path* path = tok->path;
            if (path && path->directlyUsedByToken == false) {
                path->directlyUsedByToken = true;
                if (path->refCount > 0)
                    movePathYesRefList(path); // force move the path to the front of the yes list
            }
        }
    }
    

    // now all directly referred path has directlyUsedByToken == true
    // all in-directly referred path are in yesRefList    
    // so we free all other paths in noRefList
    for (Path* path = noRefList.link; path->link != NULL;) {
        if (path->directlyUsedByToken == false) {
            if (path->prev)
                deRefPath(path->prev); // this will move the prev path to the tail of noRefList to be processed in this loop if necessary
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
    
}

// attach a NetInst to non-eplison transition, and add it to the *front* 
// active inst list, so it will be processed at the next frame
void WFSTDecoderLite::attachNetInst(WFSTTransition* trans) {
    assert(trans->hook == NULL);
    
    NetInst* inst = (NetInst*)netInstPool->malloc();
    inst->hmmIndex = trans->inLabel - 1;
    int n = hmmModels->getNumStates(inst->hmmIndex);
    inst->nStates = n;
    inst->states = (Token*)stateNPools[n]->malloc();
    for (int i = 0; i < n; ++i)
        inst->states[i] = nullToken;
    trans->hook = inst;
    inst->trans = trans;
    inst->nActiveHyps = 0;
    netInstList.push_front(inst);
    
    //push_back to the end of netInstList will give slightly different result, not clear why
    //netInstList.push_back(inst);
}

// path is not freed here, and will be freed in collectPath()
void WFSTDecoderLite::destroyNetInst(NetInst* inst) {
    inst->trans->hook = NULL;
    stateNPools[inst->nStates]->free(inst->states);
    netInstPool->free(inst);
}

};

