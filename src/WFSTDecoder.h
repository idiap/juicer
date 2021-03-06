/*
 * Copyright 2004 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * Copyright 2008 by The University of Sheffield
 *
 * Copyright 2008 by The University of Edinburgh
 *
 * See the file COPYING for the licence associated with this software.
 */

#ifndef WFST_DECODER_INC
#define WFST_DECODER_INC

#include "TracterObject.h"
#include "WFSTNetwork.h"
#include "WFSTModel.h"
#include "WFSTLattice.h"
#include "Histogram.h"
#include "DecHypHistPool.h"
#include "Decoder.h"

/*
	Author:  Darren Moore (moore@idiap.ch)
	Date:    14 October 2004
*/

/*
	Author:	Octavian Cheng (ocheng@idiap.ch)
	Date:	7 June 2006		
*/

#ifdef OPT_SINGLE_BEST
# define NO_BEST_START
# define NO_BEST_END
#endif

namespace Juicer
{

    /**
     * Basic decoder class.  This is the original static transducer
     * decoder by Darren.
     */
    class WFSTDecoder : public IDecoder,
                        public Tracter::Object
    {
    public:
        // Public Member Variables
        WFSTNetwork		   *network ;

        int               nActiveModels ;
        int               nActiveEmitHyps ;
        int               nActiveEndHyps ;
        int               nEmitHypsProcessed ;
        int               nEndHypsProcessed ;
        int               activeModelsLookupLen ;

        // Pruning
        real              emitPruneWin ;
        real              phoneEndPruneWin ;
        real              phoneStartPruneWin ;
        real              wordPruneWin ;
        int               maxEmitHyps ;

        real              bestEmitScore ;
        real              currEmitPruneThresh ;
#ifndef NO_BEST_END
        real              bestEndScore ;
#endif
        real              currEndPruneThresh ;
#ifndef NO_BEST_START
        real              bestStartScore;
#endif
        real              currStartPruneThresh ;
        real              currWordPruneThresh ;

        DecHypHist			*bestHypHist ;

        real              normaliseScore ;
        Histogram         *emitHypsHistogram ;

        // Constructors / destructor
        WFSTDecoder() ;
        WFSTDecoder(
            WFSTNetwork *network_ , IModels *models_ ,
            real phoneStartPruneWin_, real emitPruneWin_, real phoneEndPruneWin_,
            real wordPruneWin_,
            int maxEmitHyps_ , bool doModelLevelOutput_ ,
            bool doLatticeGeneration_ , bool isStaticComposition_ = true
        ) ;

        virtual ~WFSTDecoder() throw ();

        // Public Methods
        virtual void init() ;
        virtual void processFrame( real **inputVec, int currFrame_, int nFrames);
        virtual DecHyp *finish() ;

        bool modelLevelOutput() { return doModelLevelOutput ; } ;
        bool latticeGeneration() { return doLatticeGeneration ; } ;
        WFSTLattice *getLattice() { return lattice ; } ;
        void getBestHyp( int *numResultWords , int **resultWords , int **resultWordsTimes ) ;

        virtual void setMaxAllocModels(int maxAllocModels) {};

#ifdef PARTIAL_DECODING
        virtual void setPartialDecodeOptions(int traceInterval) {}
#endif

        /*
          void setNetwork( Network *network_ ) ;
        */

    protected:
        // Private Member Variables
        IModels           *models ;
        DecHyp            *bestFinalHyp ;
        bool              doModelLevelOutput ;

        // Hypothesis management
        DecHypHistPool    *decHypHistPool ;
        int               nFrames ;
        int               currFrame ;
        bool auxReplaced;  // Auxiliary symbols replaced a-priori

        int               totalActiveEmitHyps ;
        int               totalActiveEndHyps ;
        int               totalActiveModels ;
        int               totalProcEmitHyps ;
        int               totalProcEndHyps ;

        real              avgActiveEmitHyps ;
        real              avgActiveEndHyps ;
        real              avgActiveModels ;
        real              avgProcEmitHyps ;
        real              avgProcEndHyps ;

        // Lattice generation stuff
        bool              doLatticeGeneration ;
        bool              doLatticeDeadEndCleanup ;
        WFSTLattice       *lattice ;

        virtual void processActiveModelsInitStates() ;
        virtual void processActiveModelsEmitStates() ;
        virtual void processActiveModelsEndStates() ;

        virtual void joinNewActiveModelsList() ;
        void reset() ;

        virtual void resetActiveHyps() ;
        virtual void checkActiveNumbers( bool checkPhonePrevHyps ) ;

        /* zl:remove virtual functional call and only call resetDecHypHist
         * on non-NULL hist gives roughly 1.5% speed improvement for this
         * frequently called function in an informal test  */
        inline void resetDecHyp( DecHyp* hyp ) ;
        virtual void registerLabel( DecHyp* hyp , int label ) ;

        virtual void addLabelHist( DecHyp* hyp , int label ) ;

        virtual int addLatticeEntry( DecHyp *hyp , WFSTTransition *trans , int *fromState ) ;

    private:
        WFSTModelPool     *modelPool ;
        WFSTModel         *activeModelsList ;
        WFSTModel         **activeModelsLookup ;
        WFSTModel         *newActiveModelsList ;
        WFSTModel         *newActiveModelsListLastElem ;
        //WFSTModel *getModel( WFSTTransition *trans ) ;
        WFSTModel *getNewModel( WFSTTransition *trans );
        WFSTModel *returnModel( WFSTModel *model , WFSTModel *prevActiveModel ) ;

        void extendModelInitState( WFSTModel *model ) ;
        void processModelEmitStates( WFSTModel *model ) ;
        void extendModelEndState( DecHyp *endHyp , WFSTTransition *trans ) ;


    };

}

#endif
