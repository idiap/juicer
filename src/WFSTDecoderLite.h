/*
 * Copyright 2009 by The University of Edinburgh
 *
 * Copyright 2009 by Idiap Research Institute
 *                   http://www.idiap.ch
 *
 * Copyright 2009 by The University of Sheffield
 *
 * See the file COPYING for the licence associated with this software.
 */

/*
 * vi:ts=4:tw=78:shiftwidth=4:expandtab
 * vim600:fdm=marker
 *
 * WFSTDecoderLite.h  -  WFST decoder based on token-passing principle
 *
 * University of Edinburgh
 * Begin       : 03-Nov-2008
 * 
 */

#ifndef _WFSTDECODERLITE_H
#define _WFSTDECODERLITE_H

#include "general.h"

#include <vector>

#include "WFSTNetwork.h"
#include "Models.h"
#include "BlockMemPool.h"
#include "DecHypHistPool.h"
#include "WFSTDecoder.h"

using namespace std;

namespace Juicer {

#ifdef USE_DOUBLE_SCORE
    typedef double score_t;
#else
    typedef real score_t;
#endif

    typedef struct Path_ {
        struct Path_* prev; /* previous path */
        
        struct Path_* link; /* linked node for yesRefList and noRefList */
        struct Path_* knil;
        
        int frame;
        score_t score;  /* use score_t for higher precision, will be normalised (-bestEmitScore) at each frame  */
        real acousticScore; /* un-normalised acoustic score */
        real lmScore;
        int label;          /* output symbol id */
        int refCount;               /* counts this path is referred by other path's prev field */
        bool directlyUsedByToken;   /* normally false, used during path collection */
#ifdef PARTIAL_DECODING
        int jointCount;   /* how many paths go via this one */
#endif
    } Path;

    typedef struct Token_ {
        score_t score;  /* use score_t for higher precision, will be normalised (-bestEmitScore) at each frame  */
        score_t acousticScore; /* un-normalised real acoustic score */
        real lmScore; 
        Path* path; 
    } Token;

    /* a NetInst is attached to each non-eplison transition */
    typedef struct NetInst_ {
        struct NetInst_* next;

        int hmmIndex;
        int nStates;
        int nActiveHyps;
        WFSTTransition* trans;  // the transition this inst is attached to
#ifdef OPT_INST_TEE
        real teeWeight;
#endif
        Token states[];         // states[nStates] including non-emitting entry and exit states
                                // NetInst + states[n] are allocated by stateNPools
    } NetInst;
    
    
    class WFSTDecoderLite : public WFSTDecoder {
        public:
            WFSTDecoderLite(WFSTNetwork*network_ , 
                Models *models_ ,
                real phoneStartPruneWin_,
                real emitPruneWin_, 
                real phoneEndPruneWin_,
                real wordPruneWin_,
                int maxEmitHyps_ );

            virtual ~WFSTDecoderLite();

            // compatipable with WFSTDecoder interface
            void init() {recognitionStart();}
            DecHyp* finish() {return recognitionFinish();}

            void recognitionStart();
            void processFrame(real **inputVec, int currFrame_, int nFrames);
            DecHyp* recognitionFinish();
            
            void setMaxAllocModels(int maxAllocModels);
            // although most variables are the same as in WFSTDecoder,
            // they are re-declared here instead of inheriting from WFSTDecoder,
            // just in case this may replace WFSTDecoder completely
        private:
            // essential variables for decoding
            // need to be reset for each utterance
            Token* tokenBuf;      /* temp updated tokens */
            Path noRefList;       /* list of paths that has no reference (refCount is 0) */
            Path noRefListTail;   
            Path yesRefList;      /* Path with refCount > 0 */
            Path yesRefListTail;  

            NetInst* activeNetInstList;
            NetInst* newActiveNetInstList;
            NetInst* newActiveNetInstListLastElem;
            
            int currFrame;
            int nPath;           /* # of all paths in lists */
            int nPathNew;        /* # of all paths since last collection */
            Token bestFinalToken; /* store best token as there may be no inst (hence token) attached to a final epsilon transition */

            // pruning resources
            Histogram    *emitHypsHistogram;
            real emitPruneWin; /* main beam pruning threshold, 0.0 to disable */
            real phoneEndPruneWin;
            real phoneStartPruneWin;
            real wordPruneWin;
            int maxEmitHyps;   /* top N instances threshold, 0 to disable */

            real bestEmitScore; /* hightest score of each frame */
#ifndef OPT_SINGLE_BEST
            real bestStartScore;
            real bestEndScore;
#endif
            real normaliseScore; /* highest normalised emitting score from last frame */

            real currStartPruneThresh;
            real currEndPruneThresh;
            real currWordPruneThresh;
            real currEmitPruneThresh;

            int maxAllocModels; // free up all NetInsts when this limit is reached

            // statistics
            int lastPathCollectFrame; 

            int totalActiveModels;
            int totalActiveEmitHyps;
            int totalActiveEndHyps;
            int totalProcEmitHyps;
            int totalProcEndHyps;

            int nActiveEmitHyps;
            int nActiveEndHyps;
            int nActiveInsts;

            int nEmitHypsProcessed;
            int nEndHypsProcessed;

            int nAllocInsts;
            
            // other resources
            Models* hmmModels;     
            WFSTNetwork* network;  
            int maxNStates;
            
            // memory resources
            BlockMemPool* pathPool;
            BlockMemPool** stateNPools;
            
            int nStatePools;
            
            /* compatible with WFSTDecoder interface */
            BlockMemPool   *dhhPool;
            DecHyp*        bestDecHyp;

            // functions
            Path* createNewNoRefPath();
            void collectPaths();
            void refPath(Path* p);
            void deRefPath(Path* p);
            void destroyPath(Path* p);
            void movePathYesRefList(Path* p);
            void movePathYesRefListTail(Path* p);
            void resetPathLists();
            void HMMInternalPropagation(NetInst* inst);
            void propagateToken(Token* tok, WFSTTransition* trans);
            NetInst* attachNetInst(WFSTTransition* trans);
            void joinNewActiveInstList();
            NetInst* returnNetInst(NetInst* inst, NetInst* prevInst);

#ifdef PARTIAL_DECODING
            vector<Path*> partialPaths; // list of joint Path node in the hypothesis network
            bool tracePartialPath();        // return true if found such node
            void traceWinningPaths(Path* path);
            int lastPartialTraceFrame;
            int partialTraceInterval;   // in terms of frames
        public:
            void setPartialDecodeOptions(int traceInterval);
#endif


    };
};
#endif /* ifndef _WFSTDECODERLITE_H */
