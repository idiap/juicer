/*
 * Copyright 2004 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#ifndef WFST_DECODER_INC
#define WFST_DECODER_INC

#include "general.h"
#include "WFSTNetwork.h"
#include "WFSTModel.h"
#include "WFSTLattice.h"
#include "Histogram.h"
#include "DecHypHistPool.h"

/*
	Author:  Darren Moore (moore@idiap.ch)
	Date:    14 October 2004
	$Id: WFSTDecoder.h,v 1.14.4.3 2006/10/13 01:42:00 juicer Exp $
*/

/*
	Author:	Octavian Cheng (ocheng@idiap.ch)
	Date:	7 June 2006		
*/


namespace Juicer {

/**
 * Basic decoder class.  This is the original static transducer
 * decoder by Darren.
 */
class WFSTDecoder
{
public:
   // Public Member Variables
   WFSTNetwork		   *network ;
   WFSTTransition    **transBuf ;

   // Hypothesis Management
   // Changes Octavian - Move to private
   /*
   WFSTModelPool     *modelPool ;
   */

   int               nActiveModels ;
   int               nActiveEmitHyps ;
   int               nActiveEndHyps ;

   int               nEmitHypsProcessed ;
   int               nEndHypsProcessed ;

   // Changes Octavian - Move to private
   /*
   WFSTModel         *activeModelsList ;
   */
   int               activeModelsLookupLen ;
   
   // Changes Octavian - Move to private
   /*
   WFSTModel         **activeModelsLookup ;
   */

   // Pruning
    real              emitPruneWin ;	
    real              phoneEndPruneWin ;	
    real              phoneStartPruneWin ;	
    int               maxEmitHyps ;

    real              currEmitPruneThresh ;
    real              currEndPruneThresh ;
    real              currStartPruneThresh ;

    real              bestEmitScore ;
    real              bestEndScore ;
    real bestStartScore;

   DecHypHist			*bestHypHist ;

   real              normaliseScore ;
   Histogram         *emitHypsHistogram ;

    // Constructors / destructor
    WFSTDecoder() ;
   
    // Changes Octavian
    WFSTDecoder(
        WFSTNetwork *network_ , Models *models_ ,
        real phoneStartPruneWin_, real emitPruneWin_, real phoneEndPruneWin_,
        int maxEmitHyps_ , bool doModelLevelOutput_ ,
        bool doLatticeGeneration_ , bool isStaticComposition_ = true
    ) ;
   
   virtual ~WFSTDecoder() ;

   // Public Methods
   // Changes Octavian - Change to virtual functions
    //PNG virtual DecHyp *decode( real **inputData , int nFrames_ ) ;
   // Changes Octavian - Change to virtual functions
   virtual void init() ;
   void processFrame( real *inputVec, int currFrame_ ) ;
   DecHyp *finish() ;

   bool modelLevelOutput() { return doModelLevelOutput ; } ;
   bool latticeGeneration() { return doLatticeGeneration ; } ;
   WFSTLattice *getLattice() { return lattice ; } ;
   void getBestHyp( int *numResultWords , int **resultWords , int **resultWordsTimes ) ;
   
/*
   void setNetwork( Network *network_ ) ;
*/

// Changes Octavian
//private:
protected:
   // Private Member Variables
   Models            *models ;
   
   // Changes by Octavian - move to private
   /*
   WFSTModel         *newActiveModelsList ;
   WFSTModel         *newActiveModelsListLastElem ;
   */
   
   // Changes
   DecHyp            *bestFinalHyp ;
   //DecHyp            bestFinalHyp ;
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

   // Private Methods
   // Changes
   virtual void processActiveModelsInitStates() ;
   virtual void processActiveModelsEmitStates() ;
   virtual void processActiveModelsEndStates() ;
   /*
   void processActiveModelsInitStates() ;
   void processActiveModelsEmitStates() ;
   void processActiveModelsEndStates() ;
   */

   // Changes
   /*
   void extendModelInitState( WFSTModel *model ) ;
   void processModelEmitStates( WFSTModel *model ) ;
   void extendModelEndState( DecHyp *endHyp , WFSTTransition *trans , 
                             WFSTTransition **nextTransBuf ) ;
			     */
   
   // Changes
   virtual void joinNewActiveModelsList() ;
   /*
   WFSTModel *getModel( WFSTTransition *trans ) ;
   void joinNewActiveModelsList() ;
   WFSTModel *returnModel( WFSTModel *model , WFSTModel *prevActiveModel ) ;
   */
   void reset() ;
   // Changes
   virtual void resetActiveHyps() ;
   virtual void checkActiveNumbers( bool checkPhonePrevHyps ) ;

   // Changes
   virtual void resetDecHyp( DecHyp* hyp ) ;
   virtual void registerLabel( DecHyp* hyp , int label ) ;
   /*
   void resetActiveHyps() ;
   void checkActiveNumbers( bool checkPhonePrevHyps ) ;
   */

   // Changes Octavian 20060523
   virtual void addLabelHist( DecHyp* hyp , int label ) ;

   // Changes
   virtual int addLatticeEntry( DecHyp *hyp , WFSTTransition *trans , int *fromState ) ;

private:
   // Changes by Octavian - moved from public
   WFSTModelPool     *modelPool ;
   WFSTModel         *activeModelsList ;
   WFSTModel         **activeModelsLookup ;
   // Changes by Octavian - moved from protected
   WFSTModel         *newActiveModelsList ;
   WFSTModel         *newActiveModelsListLastElem ;
   // Changes
   WFSTModel *getModel( WFSTTransition *trans ) ;
   WFSTModel *returnModel( WFSTModel *model , WFSTModel *prevActiveModel ) ;
   
   // Changes - moved from private
   void extendModelInitState( WFSTModel *model ) ;
   void processModelEmitStates( WFSTModel *model ) ;
   void extendModelEndState( DecHyp *endHyp , WFSTTransition *trans , 
                             WFSTTransition **nextTransBuf ) ;
};

}

#endif
