/*
 * Copyright 2006 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#include "WFSTOnTheFlyDecoder.h"
#include "WFSTLatticeOnTheFly.h"
#include "DecHypHistPool.h"
#include "log_add.h"
#include "LogFile.h"

/*
	Author:	 Octavian Cheng (ocheng@idiap.ch)
	Date:	 7 June 2006
*/

using namespace Torch;

namespace Juicer {

#define DEFAULT_DECHYPHISTPOOL_SIZE		5000
#define FRAMES_BETWEEN_LATTICE_PARTIAL_CLEANUP  2
#define FRAMES_BETWEEN_LATTICE_FULL_CLEANUP     10


// Changes
// WFSTOnTheFlyDecoder Implementation
WFSTOnTheFlyDecoder::WFSTOnTheFlyDecoder() : WFSTDecoder()
{
   gNetwork = NULL ;
   
   modelOnTheFlyPool = NULL ;
   // Changes Octavian 20060627
   // decHypPool = NULL ;
   activeModelsOnTheFlyList = NULL ;
   activeModelsOnTheFlyLookup = NULL ;
   newActiveModelsOnTheFlyList = NULL ;
   newActiveModelsOnTheFlyListLastElem = NULL ;

   doPushingWeightAndLabel = false ;
   doPushingWeightCache = false ;
   pushingWeightCache = NULL ;
}

//PNG
#if 0
WFSTOnTheFlyDecoder::WFSTOnTheFlyDecoder( 
      WFSTNetwork *network_ , WFSTSortedInLabelNetwork *gNetwork_ ,
      PhoneModels *phoneModels_ , real emitPruneWin_ , 
      real phoneEndPruneWin_ , int maxEmitHyps_ , 
      bool doPushingWeightAndLabel_ , bool doPushingWeightCache_ ) :
      WFSTDecoder( network_ , phoneModels_ , emitPruneWin_ , 
	    phoneEndPruneWin_ , maxEmitHyps_ , false )
{
   if ( (gNetwork = gNetwork_) == NULL )
      error("WFSTOnTheFlyDecoder::WFSTOnTheFlyDecoder - gNetwork_ is NULL") ;

   // Hypothesis management - WFSTOnTheFlyDecoder (1) and (2) 
   // First, allocate DecHypPool. A pool for holding hypotheses.
   // Changes Octavian 20060627
   // decHypPool = new DecHypPool( DEFAULT_DECHYPPOOL_SIZE, true ) ;
   
   // Second, allocate new data structure
   // Changes Octavian 20060627
   // modelOnTheFlyPool = new WFSTModelOnTheFlyPool( phoneModels, decHypHistPool, decHypPool ) ;
   modelOnTheFlyPool = new WFSTModelOnTheFlyPool( 
	 phoneModels, decHypHistPool ) ;
   
   activeModelsOnTheFlyList = NULL ;
   activeModelsOnTheFlyLookup = (WFSTModelOnTheFly **)malloc( activeModelsLookupLen * sizeof(WFSTModelOnTheFly *) ) ;
   
   for ( int i = 0 ; i < activeModelsLookupLen ; i++ )
      activeModelsOnTheFlyLookup[i] = NULL ;

   newActiveModelsOnTheFlyList = NULL ;
   newActiveModelsOnTheFlyListLastElem = NULL ;

   // Changes
   // Third, allocate bestFinalHyp
   if ( bestFinalHyp != NULL )
      error("WFSTOnTheFlyDecoder::WFSTOnTheFlyDecoder - bestFinalHyp != NULL") ;
   bestFinalHyp = new DecHypOnTheFly() ;
   DecHypHistPool::initDecHypOnTheFly( dynamic_cast<DecHypOnTheFly * >(bestFinalHyp), -1, -1 ) ;
   
   // Changes 20060325
   // Fourth, to get a pointer to the label set array if lookahead
   const WFSTLabelPushingNetwork::LabelSet **labelSetArray = NULL ;   
   doPushingWeightAndLabel = doPushingWeightAndLabel_ ;
   
   if ( doPushingWeightAndLabel )  {
      WFSTLabelPushingNetwork *tmpNetwork = dynamic_cast<WFSTLabelPushingNetwork* >(network) ;
      if ( tmpNetwork == NULL )  {
	 error("WFSTOnTheFlyDecoder::WFSTOnTheFlyDecoder - Not label-pushing network") ;
      }
      // Changes 20060325
      labelSetArray = tmpNetwork->getLabelArray() ;
   }
   
   // Changes Octavian
   // Fifth, allocate a cache if enabled
   doPushingWeightCache = doPushingWeightCache_ ;

   // Predefine a cache size
   if ( doPushingWeightCache )  {
      // Changes 20060325
      // Changes 20060415
      pushingWeightCache = new PushingWeightCache(
	    network->getNumTransitions() , gNetwork->getNumStates() , 
	    labelSetArray, network->getNumOutLabels(), NUMBUCKETS ) ;
   }
   else  {
      pushingWeightCache = NULL ;
   }

}
#endif

WFSTOnTheFlyDecoder::WFSTOnTheFlyDecoder( 
      WFSTNetwork *network_ , WFSTSortedInLabelNetwork *gNetwork_ ,
      Models *models_ , real emitPruneWin_ ,
      real phoneEndPruneWin_ , int maxEmitHyps_ ,
      bool doModelLevelOutput_ , bool doLatticeGeneration_ ,
      bool doPushingWeightAndLabel_ , bool doPushingWeightCache_ ) :
      WFSTDecoder( network_ , models_ , emitPruneWin_ , 
	    phoneEndPruneWin_ , maxEmitHyps_ , doModelLevelOutput_ ,
	    doLatticeGeneration_ , false )
{
   if ( (gNetwork = gNetwork_) == NULL )
      error("WFSTOnTheFlyDecoder::WFSTOnTheFlyDecoder(2) - gNetwork_ is NULL") ;

   // Hypothesis management - WFSTOnTheFlyDecoder (1) and (2) 
   // First, allocate DecHypPool
   // Changes Octavian 20060627
   // decHypPool = new DecHypPool( DEFAULT_DECHYPPOOL_SIZE, true ) ;
   
   // Second, allocate new data structure
   // Changes Octavian 20060627
   // modelOnTheFlyPool = new WFSTModelOnTheFlyPool( models, decHypHistPool, decHypPool ) ;
   modelOnTheFlyPool = new WFSTModelOnTheFlyPool( models, decHypHistPool ) ;

   activeModelsOnTheFlyList = NULL ;
   activeModelsOnTheFlyLookup = (WFSTModelOnTheFly **)malloc( activeModelsLookupLen * sizeof(WFSTModelOnTheFly *) ) ;
   
   for ( int i = 0 ; i < activeModelsLookupLen ; i++ )
      activeModelsOnTheFlyLookup[i] = NULL ;

   newActiveModelsOnTheFlyList = NULL ;
   newActiveModelsOnTheFlyListLastElem = NULL ;
   
   // Third, allocate bestFinalHyp
   // Changes
   if ( bestFinalHyp != NULL )
      error("WFSTOnTheFlyDecoder::WFSTOnTheFlyDecoder(2) - bestFinalHyp != NULL") ;
   bestFinalHyp = new DecHypOnTheFly() ;
   DecHypHistPool::initDecHypOnTheFly( dynamic_cast<DecHypOnTheFly * >(bestFinalHyp), -1, -1 ) ;
   
   // Changes
   // Lattice stuff
   if ( doLatticeGeneration_ )  {
#ifdef DEBUG
      if ( lattice == NULL )
	 error("WFSTOnTheFlyDecoder::WFSTOnTheFlyDecoder(2) - Base constructor does not create lattice") ;
#endif
      delete lattice ;
      lattice = new WFSTLatticeOnTheFly( network->getNumStates() , false, false ) ;
      lattice->enableDeadEndRemoval( FRAMES_BETWEEN_LATTICE_PARTIAL_CLEANUP , 
                                     FRAMES_BETWEEN_LATTICE_FULL_CLEANUP ) ;
      decHypHistPool->setLattice( lattice ) ;
   }
   
   // Changes 20060325
   // Get a pointer to the label set array if lookahead is enabled.
   const WFSTLabelPushingNetwork::LabelSet **labelSetArray = NULL ;   
 
   doPushingWeightAndLabel = doPushingWeightAndLabel_ ;

   if ( doPushingWeightAndLabel )  {
      WFSTLabelPushingNetwork *tmpNetwork = dynamic_cast<WFSTLabelPushingNetwork* >(network) ;
      if ( tmpNetwork == NULL )  {
	 error("WFSTOnTheFlyDecoder::WFSTOnTheFlyDecoder(2) - Not label-pushing network") ;
      }
      // Changes 20060325
      labelSetArray = tmpNetwork->getLabelArray() ;
   }

   // Changes
   // Allocate a cache if enabled.
   doPushingWeightCache = doPushingWeightCache_ ;

   if ( doPushingWeightCache )  {
      // Changes 20060325
      // Changes 20060415
      pushingWeightCache = new PushingWeightCache(
	    network->getNumTransitions() , gNetwork->getNumStates() ,
	    labelSetArray, network->getNumOutLabels(), NUMBUCKETS ) ;
   }
   else  {
      pushingWeightCache = NULL ;
   }
}


WFSTOnTheFlyDecoder::~WFSTOnTheFlyDecoder()
{
   // Changes Octavian 20060627
   // delete decHypPool ;
   delete modelOnTheFlyPool ;
   if ( activeModelsOnTheFlyLookup != NULL )
      free( activeModelsOnTheFlyLookup ) ;
   
   delete pushingWeightCache ; 
}


DecHyp *WFSTOnTheFlyDecoder::decode( real **inputData , int nFrames_ )
{
   // Bascially it's the same as WFSTDecoder::decode() at the moment 
   
   nFrames = nFrames_ ;
  
   // Initialise the decoder.
   // Should call WFSTOnTheFly::init()
   init() ;
  
   // process the inputs
   for ( int t=0 ; t<nFrames ; t++ )
   {
      currFrame = t ;
      processFrame( inputData[t] ) ;
   }

   avgActiveEmitHyps = (real)totalActiveEmitHyps / (real)nFrames ;
   avgActiveEndHyps = (real)totalActiveEndHyps / (real)nFrames ;
   avgActiveModels = (real)totalActiveModels / (real)nFrames ;
   avgProcEmitHyps = (real)totalProcEmitHyps / (real)nFrames ;
   avgProcEndHyps = (real)totalProcEndHyps / (real)nFrames ;

   LogFile::printf( "\nStatistics:\n  nFrames=%d\n  avgActiveEmitHyps=%.2f\n"
                    "  avgActiveEndHyps=%.2f\n  avgActiveModels=%.2f\n"
                    "  avgProcessedEmitHyps=%.2f\n  avgProcessedEndHyps=%.2f\n" ,
                    nFrames , avgActiveEmitHyps , avgActiveEndHyps , avgActiveModels , 
                    avgProcEmitHyps , avgProcEndHyps ) ;
   
   // Return the best final state hypothesis
   return finish() ;
}


void WFSTOnTheFlyDecoder::init()
{
   // Changes
   /*
   if ( bestFinalHyp != NULL )
      error("WFSTOnTheFlyDecoder::init - bestFinalHyp != NULL") ;
   
   bestFinalHyp = new DecHypOnTheFly();
   */
   DecHypHistPool::initDecHypOnTheFly( 
	 dynamic_cast<DecHypOnTheFly* >(bestFinalHyp) , -1 , -1 ) ;
   
   reset() ;
	
   // Reset the time
   currFrame = 0 ;
	
   // initialise a starting hypothesis.
   DecHypOnTheFly tmpHyp ;
   DecHypHistPool::initDecHypOnTheFly( &tmpHyp, -1 , -1 );

   // Changes - get a list of G states propagated from the initial state of 
   // the G transducer
   int multipleGState[MAXNUMDECHYPOUTLABELS] ;
   real multipleWeight[MAXNUMDECHYPOUTLABELS] ;
   int multipleOutLabel[MAXNUMDECHYPOUTLABELS] ;
   int multipleNGState = 0 ;

   gNetwork->getStatesOnEpsPath( 
	 gNetwork->getInitState(), 0.0, WFST_EPSILON, multipleGState, 
	 multipleWeight, multipleOutLabel, &multipleNGState, 
	 MAXNUMDECHYPOUTLABELS ) ;

   real accWeight = 0.0 ;
   int multipleOutLabelWithNoEPS[MAXNUMDECHYPOUTLABELS] ;
   int multipleNOutLabel = 0 ;

   for ( int i = 0 ; i < multipleNGState ; i++ )  {
      accWeight += multipleWeight[i] ;
      if ( multipleOutLabel[i] != WFST_EPSILON )  {
	 multipleOutLabelWithNoEPS[multipleNOutLabel] = multipleOutLabel[i] ;
	 multipleNOutLabel++ ;
      }
      
      tmpHyp.score = accWeight ;
      tmpHyp.acousticScore = 0.0 ;
      tmpHyp.lmScore = accWeight ;
      tmpHyp.currGState = multipleGState[i] ;
      tmpHyp.pushedWeight = 0.0 ;
      tmpHyp.nNextOutLabel = multipleNOutLabel ;
      for ( int j = 0 ; j < multipleNOutLabel ; j++ )  {
	 tmpHyp.nextOutLabel[j] = multipleOutLabelWithNoEPS[j] ;
      }

      // Extend the hypothesis into the initial states of the models
      // associated with the transitions out of the initial state.
      // Changes Octavian 20060531
      // Changes Octavian 20060815
      extendModelEndState( 
	    &tmpHyp , NULL , transBuf , false , multipleGState[i] , 
	    UNDECIDED_OUTLABEL ) ;

      // Changes
      //decHypHistPool->resetDecHypOnTheFly( &tmpHyp ) ;
      resetDecHyp( &tmpHyp ) ;
   }
   
   joinNewActiveModelsList() ;
}

void WFSTOnTheFlyDecoder::processActiveModelsInitStates()
{
   WFSTModelOnTheFly *model = activeModelsOnTheFlyList ;
   // Changes
   /*
   DecHyp *hyp ;
   */

   while ( model != NULL )
   {
      /*
      hyp = model->currHyps ;
      if ( hyp->score > LOG_ZERO )
      {
      */
      extendModelInitState( model ) ;
      /*
      }
      */
      
      model = model->next ;
   }
}


void WFSTOnTheFlyDecoder::processActiveModelsEmitStates()
{
   // For each phone in the activeModelsList, update the emitting state hypotheses
   // for the new frame.

#ifdef DEBUG
   checkActiveNumbers( false ) ;
#endif

   // Reset all of the (per-frame) active counts and best scores
   nActiveEmitHyps = 0 ;
   nActiveEndHyps = 0 ;
   nEmitHypsProcessed = 0 ;
   nEndHypsProcessed = 0 ;
   bestEmitScore = LOG_ZERO ;
   bestEndScore = LOG_ZERO ;
   bestHypHist = NULL ;

#ifdef DEBUG
   // Make sure 'frame' is in sync with phoneModels/models
   if ( phoneModels == NULL )
   {
      if ( currFrame != models->getCurrFrame() )
	 error("WFSTOnTheFlyDecoder::procActModEmitSts - currFrame != models->getCurrFrame()") ;
   }
   else
   {
      if ( currFrame != phoneModels->curr_t )
	 error("WFSTOnTheFlyDecoder::procActModEmitSts - currFrame != phoneModels->curr_t") ;
   }
#endif	

   // Changes
   WFSTModelOnTheFly *model = activeModelsOnTheFlyList ;
   WFSTModelOnTheFly *prevModel = NULL ;
   /*
   WFSTModel *model = activeModelsList ;
   WFSTModel *prevModel = NULL ;
   */
   while ( model != NULL )
   {
      processModelEmitStates( model ) ;

      // Check to see if pruning resulted in no emitting 
      //   states being extended.
      if ( model->nActiveHyps <= 0 )
      {
         model = returnModel( model , prevModel ) ;
      }
      else
      {
         prevModel = model ;
         model = model->next ;
      }
   }

#ifdef DEBUG
   checkActiveNumbers( false ) ;
#endif

}


/**
 * Look at the final state hypothesis for all models in the
 * active list.
 * If the end state is active, then we extend it to successor 
 * models as defined by the network.
 */
void WFSTOnTheFlyDecoder::processActiveModelsEndStates()
{

#ifdef DEBUG
   checkActiveNumbers( false ) ;
#endif

   // If we don't have any phone end hyps we don't need to go any further.
   if ( nActiveEndHyps <= 0 )
      return ;

   // Changes
   WFSTModelOnTheFly *model = activeModelsOnTheFlyList ;
   WFSTModelOnTheFly *prevModel = NULL ;
   
   while ( model != NULL )
   {
      // First, process unpushed hypothesis
      // Changes Octavian 20060627
      NodeToDecHypOnTheFly *node ;
      DecHypOnTheFly *decHypOnTheFly ;
      int gPushedState ;
      
      // Changes Octavian 20060417
#ifdef DEBUG
      int prevGPushedState = -1 ;
#endif
     
      // Changes Octavian 20060531 20060627
      /*
      while ( ( decHypOnTheFly = model->getLastStateHyp( false, &gPushedState, NULL ) ) != NULL )  {
      */
      while ( ( node = model->getLastStateHyp( false, &gPushedState, NULL ) ) != NULL )  {
	 // Changes Octavian 20060417
#ifdef DEBUG
	 if ( gPushedState <= prevGPushedState )
	    error ("WFSTOnTheFlyDecoder::processActiveModelsEndStates - Non-pushed map not in ascending order") ;

	 prevGPushedState = gPushedState ;
#endif
	 // Changes Octavian 20060627
	 decHypOnTheFly = &(node->hyp) ;

	 // Check if score is > LOG_ZERO and within currEndPruneThresh
	 if ( decHypOnTheFly->score > LOG_ZERO )  {
	    
	    if ( decHypOnTheFly->score > currEndPruneThresh )  {
	       nEndHypsProcessed++ ;
	       // Changes Octavian 20060531
	       // Changes Octavian 20060815
	       extendModelEndState( 
		     decHypOnTheFly, model->trans, transBuf, false, 
		     gPushedState, UNDECIDED_OUTLABEL ) ;
	    }
	   
	    // Deactive decHypOnTheFly
	    // Changes
	    //decHypHistPool->resetDecHypOnTheFly( decHypOnTheFly ) ;
	    resetDecHyp( decHypOnTheFly ) ;

	    // Free the element from the decHypPool
	    // Changes Octavian 20060627
	    // decHypPool->returnSingleHyp( decHypOnTheFly ) ;
	    model->pool->returnSingleNode( node ) ;

	    if ( (--nActiveEndHyps) < 0 )
	       error("WFSTOnTheFlyDecoder::processActiveModelsEndStates - nActiveEndHyps < 0") ;

	    // cf the implementation of WFSTDecoder
	    --(model->nActiveHyps) ;

	 }
#ifdef DEBUG
	 else  {
	    error("WFSTOnTheFlyDecoder::processActiveModelsEndStates - hyp < LOG_ZERO");
	 }
#endif
      }
      
      // Second, process the pushed map
      // Changes Octavian 20060417 20060531
#ifdef DEBUG
      prevGPushedState = -1 ;
      int prevMatchedOutLabel = -1 ;
#endif

      // Changes Octavian 20060531
      int matchedOutLabel ;
     
      // Changes Octavian 20060531 20060627
      /*
      while ( ( decHypOnTheFly = model->getLastStateHyp( true, &gPushedState, &matchedOutLabel ) ) != NULL )  {
      */
      while ( ( node = model->getLastStateHyp( true, &gPushedState, &matchedOutLabel ) ) != NULL )  {
	 // Changes Octavian 20060417
#ifdef DEBUG
	 // Changes Octavian 20060531
	 // The corresponding checking in unpushed case is <= since we do
	 // not expect same gState.
	 // For pushed case, it is allowed to have the same gPushedState,
	 // but the matchedOutLabel must be different.
	 if ( gPushedState < prevGPushedState )
	    error ("WFSTOnTheFlyDecoder::processActiveModelsEndStates - Pushed map not in ascending order") ;

	 // Changes Octavian 20060531
	 if (  ( gPushedState == prevGPushedState ) &&
	       ( matchedOutLabel <= prevMatchedOutLabel )  )
	    error ("WFSTOnTheFlyDecoder::processActiveModelsEndStates - matchedOutLabel not in ascending order") ;

	 prevGPushedState = gPushedState ;
	 // Changes Octavian 20060531
	 prevMatchedOutLabel = matchedOutLabel ;
#endif
	 // Changes Octavian 20060627
	 decHypOnTheFly = &(node->hyp) ;

	 // Check if score is > LOG_ZERO and within currEndPruneThresh
	 if ( decHypOnTheFly->score > LOG_ZERO )  {
	    
	    if ( decHypOnTheFly->score > currEndPruneThresh )  {
	       nEndHypsProcessed++ ;
	       // Changes Octavian 20060531
	       // Changes Octavian 20060815
	       extendModelEndState( 
		     decHypOnTheFly, model->trans, transBuf, true, 
		     gPushedState, matchedOutLabel ) ;
	    }
	   
	    // Deactive decHypOnTheFly
	    // Changes
	    //decHypHistPool->resetDecHypOnTheFly( decHypOnTheFly ) ;
	    resetDecHyp( decHypOnTheFly ) ;

	    // Free the element from the decHypPool
	    // Changes Octavian 20060627
	    // decHypPool->returnSingleHyp( decHypOnTheFly ) ;
	    model->pool->returnSingleNode( node ) ;

	    if ( (--nActiveEndHyps) < 0 )
	       error("WFSTOnTheFlyDecoder::processActiveModelsEndStates - nActiveEndHyps < 0") ;

	    // cf the implementation of WFSTDecoder
	    --(model->nActiveHyps) ;

	 }
#ifdef DEBUG
	 else  {
	    error("WFSTOnTheFlyDecoder::processActiveModelsEndStates - pushed hyp < LOG_ZERO");
	 }
#endif
      }
      
      // Third, check number of hyp
      if ( model->nActiveHyps == 0 )
      {
	 model = returnModel( model , prevModel ) ;
      }
      else
      {
	 prevModel = model ;
	 model = model->next ;
      }
   }

   // We have probably extended into new models that were
   //   not previously in the activeModelsList.
   // These models are currently being held in the newActiveModelsList.
   // Join this new list to the front of the activeModelsList,
   //   to form a new activeModelsList
   joinNewActiveModelsList() ;

#ifdef DEBUG
   checkActiveNumbers( false ) ;
#endif

}


WFSTModelOnTheFly *WFSTOnTheFlyDecoder::getModel( WFSTTransition *trans )
{
#ifdef DEBUG
   if ( trans == NULL )
      error("WFSTOnTheFlyDecoder::getModel - trans is NULL") ;
   if ( (trans->id < 0) || (trans->id >= activeModelsLookupLen) )
      error("WFSTOnTheFlyDecoder::getModel - trans->id out of range") ;
#endif

   // Do we already have an active model element for this transition ?
   if ( activeModelsOnTheFlyLookup[trans->id] == NULL )
   {
      // If we did not find a match, grab a new WFSTModel element
      //   from the pool and add to lookup table and temp active list.
      WFSTModelOnTheFly *model = modelOnTheFlyPool->getElem( trans ) ;
      model->next = newActiveModelsOnTheFlyList ;
      newActiveModelsOnTheFlyList = model ;
      if ( newActiveModelsOnTheFlyListLastElem == NULL )
         newActiveModelsOnTheFlyListLastElem = model ;

      activeModelsOnTheFlyLookup[trans->id] = model ;
      nActiveModels++ ;
   }
#ifdef DEBUG
   else
   {
      if ( activeModelsOnTheFlyLookup[trans->id]->trans != trans )
         error("WFSTOnTheFlyDecoder::getModel - activeModelsOnTheFlyLookup[...]->trans != trans") ;
   }
#endif

   return activeModelsOnTheFlyLookup[trans->id] ;

}


WFSTModelOnTheFly *WFSTOnTheFlyDecoder::returnModel( 
      WFSTModelOnTheFly *model , WFSTModelOnTheFly *prevActiveModel )
{
#ifdef DEBUG
   if ( model == NULL )
      error("WFSTOnTheFlyDecoder::returnModel - model is NULL") ;
   if ( activeModelsOnTheFlyLookup[model->trans->id] != model )
      error("WFSTOnTheFlyDecoder::returnModel - inconsistent entry in activeModelsOnTheFlyLookup") ;
#endif

   // Reset the entry in the active models lookup array.
   activeModelsOnTheFlyLookup[model->trans->id] = NULL ;
   
   // Return the model to the pool and remove it from the linked list of
   //   active models.
   if ( prevActiveModel == NULL )
   {
      // Model we are deactivating is at head of list.
      activeModelsOnTheFlyList = model->next ;
      modelOnTheFlyPool->returnElem( model ) ;
      --nActiveModels ;
      model = activeModelsOnTheFlyList ;
   }
   else
   {
      // Model we are deactivating is not at head of list.
      prevActiveModel->next = model->next ;
      modelOnTheFlyPool->returnElem( model ) ;
      --nActiveModels ;
      model = prevActiveModel->next ;
   }

   // Return the pointer to the next active model in the linked list.
   return model ;

}


// Should return all hyps in all the maps
void WFSTOnTheFlyDecoder::resetActiveHyps()
{
   nActiveEmitHyps = 0 ;
   nActiveEndHyps = 0 ;
   nEmitHypsProcessed = 0 ;
   nEndHypsProcessed = 0 ;
   
   activeModelsOnTheFlyList = NULL ;
   if ( activeModelsOnTheFlyLookup != NULL )
   {
      for ( int i=0 ; i<activeModelsLookupLen ; i++ )
      {
         if ( activeModelsOnTheFlyLookup[i] != NULL )
         {
	    // Changes Octavian
            modelOnTheFlyPool->returnElem( activeModelsOnTheFlyLookup[i] ) ;
            --nActiveModels ;
            activeModelsOnTheFlyLookup[i] = NULL ;
         }
      }
      
      if ( nActiveModels != 0 )
         error("WFSTOnTheFlyDecoder::resetActiveHyps - nActiveModels has unexpected value") ;
   }
   
   if ( modelOnTheFlyPool->numUsed() != 0 )
      error("WFSTOnTheFlyDecoder::resetActiveHyps - modelPool had nUsed != 0 after reset") ;

   // Changes
   // Check if the decHypPool is all freed
   // Changes Octavian 20060627
   /*
   if ( !(decHypPool->isAllFreed()) )
      error("WFSTOnTheFlyDecoder::resetActiveHyps - decHypPool still has hyp not freed") ;
      */
   
   newActiveModelsOnTheFlyList = NULL ;
   newActiveModelsOnTheFlyListLastElem = NULL ;

}

// Changes
void WFSTOnTheFlyDecoder::registerLabel( DecHyp* hyp , int label )
{
   DecHypHistPool::registerLabelOnTheFly( (DecHypOnTheFly *) hyp ) ;
}

void WFSTOnTheFlyDecoder::joinNewActiveModelsList()
{
   if ( newActiveModelsOnTheFlyList == NULL )
      return ;

   newActiveModelsOnTheFlyListLastElem->next = activeModelsOnTheFlyList ;
   activeModelsOnTheFlyList = newActiveModelsOnTheFlyList ;

   newActiveModelsOnTheFlyList = newActiveModelsOnTheFlyListLastElem = NULL ;
}


// Changes - This function doesn't check the bool variable
void WFSTOnTheFlyDecoder::checkActiveNumbers( bool checkPhonePrevHyps )
{
   // Make sure that our record of the number of active phone hyps is accurate.
   int cnt=0 , i ;
   WFSTModelOnTheFly *model ;

   for ( i=0 ; i<activeModelsLookupLen ; i++ )
   {
      model = activeModelsOnTheFlyLookup[i] ;
      if ( model != NULL )
      {
         if ( model->nActiveHyps <= 0 )
            error("WFSTOnTheFlyDecoder::checkActiveNumbers - active model has nActiveHyps <= 0") ;
         ++cnt ;
      }
   }

   if ( cnt != nActiveModels )
      error("WFSTOnTheFlyDecoder::checkActiveNumbers - unexpected nActiveModels %d %d",cnt,nActiveModels);

   // compare result to one obtained by going through the activeModelsList
   cnt = 0 ;
   model = activeModelsOnTheFlyList ;
   while ( model != NULL )
   {
      ++cnt ;
      model = model->next ;
   }
      
   if ( cnt != nActiveModels )
   {
      error("WFSTOnTheFlyDecoder::checkActiveNumbers - unexpected active model count in list %d %d" , 
            cnt , nActiveModels ) ;
   }
}

void WFSTOnTheFlyDecoder::resetDecHyp( DecHyp* hyp )
{
   decHypHistPool->resetDecHypOnTheFly( (DecHypOnTheFly *) hyp ) ;
}

// Changes Octavian 20060424 20060531 20060630
// Similar to WFSTModelOnTheFlyPool::iterInitMap()
/*
void WFSTOnTheFlyDecoder::extendInitMap( WFSTModelOnTheFly *model, bool isPushed, NodeToDecHypOnTheFlyMap *currNode, NodeToDecHypOnTheFly **helper )
*/
void WFSTOnTheFlyDecoder::extendInitMap( WFSTModelOnTheFly *model, bool isPushed, NodeToDecHypOnTheFly *currNode, NodeToDecHypOnTheFly **helper )
{
   // Terminating case
   // Case 1: currNode == NULL (ie the root)
   if ( currNode == NULL )
      return ;

   // Left first
   if ( currNode->ptr.left != NULL )  {
#ifdef DEBUG
      // Changes Octavian 20060531
      if ( currNode->ptr.left->gState > currNode->gState )
	 error("WFSTOnTheFlyDecoder::extendInitMap - left > curr") ;
      // Changes Octavian 20060531
      if (  ( currNode->ptr.left->gState == currNode->gState ) && 
	    ( currNode->ptr.left->matchedOutLabel >= currNode->matchedOutLabel ) )
	 error("WFSTOnTheFlyDecoder::extendInitMap - left.matchedOutLabel") ;
#endif
      extendInitMap( model, isPushed, currNode->ptr.left, helper ) ;
   }

   // currNode
   // Changes Octavian 20060627
   DecHypOnTheFly *currHyp = &(currNode->hyp) ;
   int pushedGState = currNode->gState ;
   // Changes Octavian 20060531
   int matchedOutLabel = currNode->matchedOutLabel ;

   // Changes Octavian 20060531
   extendDecHypOnTheFlyNonEndState( 
	 currHyp, pushedGState, matchedOutLabel, isPushed, 0, model, helper ) ;
   resetDecHyp( currHyp ) ;

   // Changes Octavian 20060627
   // decHypPool->returnSingleHyp( currHyp ) ;
   
   --(model->nActiveHyps) ;

   // Right last
   if ( currNode->right != NULL )  {
#ifdef DEBUG
      // Changes Octavian 20060531
      if ( currNode->right->gState < currNode->gState )
	 error("WFSTOnTheFlyDecoder:extendInitMap - right < curr") ;
      // Changes Octavian 20060531
      if ( ( currNode->right->gState == currNode->gState ) &&
	   ( currNode->right->matchedOutLabel <= currNode->matchedOutLabel ) )
	 error("WFSTOnTheFlyDecoder::extendInitMap - right.matchedOutLabel") ;
#endif
      extendInitMap( model, isPushed, currNode->right, helper ) ;
   }

   // Remove the currNode
   // Changes Octavian 20060630
   // model->mapPool->returnSingleNode( currNode ) ;
   model->pool->returnSingleNode( currNode ) ;
   return ;
}


void WFSTOnTheFlyDecoder::extendModelInitState( WFSTModelOnTheFly *model )
{
   // Changes Octavian 20060417
   // Helper array for speeding up Viterbi
   // helper[n] will point to a node at where sequent hyp merging should be
   // started.
   NodeToDecHypOnTheFly **helper = NULL ;
   helper = model->createHelper() ;
   model->assignHelper( helper, false ) ;
  
   // Changes Octavian 20060424
   extendInitMap( model, false, model->initMapNotPushed, helper ) ;
   model->initMapNotPushed = NULL ;

   // Changes Octavian 20060417
   if ( helper != NULL )  {
      model->assignHelper( helper, true ) ;
   }
   
   // Changes Octavian 20060424
   extendInitMap( model, true, model->initMapPushed, helper ) ;
   model->initMapPushed = NULL ;

   // Changes Octavian 20060417
   delete [] helper ;

}


// This function extends one particular hyp in the one state 
// (a non-end state) to the other state(s)
// destInitStMap is the destination map at the HMM state 0. When the new 
// score of a hyp is calculated (by adding the transition prob), it is 
// compared with the prev hyp located at destInitStMap[sucInd]. Replace 
// hyp in destInitStMap[suc] if newScore is greater.
// currHyp is the hyp. If propagating from emit state, make sure the 
// emission prob has been added to the score and acoustic score.
// pushedGState is the G state that has been possibly pushed due to 
// label-pushing
// isPushedMap indicates whether this currHyp is from the pushed map of 
// the init state
// hmmStateIndex is the index of the HMM state where currHyp is located
// model is the WFSTModelOnTheFly

// Changes Octavian 20060417
// Changes Octavian 20060531
void WFSTOnTheFlyDecoder::extendDecHypOnTheFlyNonEndState( 
      DecHypOnTheFly *currHyp, int pushedGState, int matchedOutLabel, 
      bool isPushedMap, int hmmStateIndex, WFSTModelOnTheFly *model, 
      NodeToDecHypOnTheFly **helper )
{
#ifdef DEBUG
   if ( currHyp->score <= LOG_ZERO )
      error("WFSTOnTheFlyDecoder::extendDecHypOnTheFlyNonEndState - hyp's score <= LOG_ZERO") ;
   if (( !isPushedMap ) && ( pushedGState != currHyp->currGState ))
      error("WFSTOnTheFlyDecoder::extendDecHypOnTheFlyNonEndState - pushedGState != currGState in notPushed map") ;
   if (( hmmStateIndex < 0 ) || ( hmmStateIndex >= (model->hmm->n_states - 1) )) 
      error("WFSTOnTheFlyDecoder::extendDecHypOnTheFlyNonEndState - hmmStateIndex out of range") ;
#endif
   
   //DecodingHMMState *initSt = model->hmm->states ;
   // currHypSt is the pointer to the HMM state where the currHyp is located
   DecodingHMMState *currHypSt = model->hmm->states + hmmStateIndex ;
   
   // This bool variable determines if the hyp is in the init state of the 
   // HMM. Need this for different treatments between propagating hyp from 
   // init st and hyp from emit st.
   bool isInitSt = ( hmmStateIndex == 0 ) ;

   // Changes Octavian 20060417
#ifdef DEBUG
   if ( isInitSt == true )  {
      if ( helper == NULL )
	 error("WFSTOnTheFlyDecoder::extendDecHypOnTheFlyNonEndState - helper is NULL but it's init state") ;
   }
   else  {
      if ( helper != NULL )
	 error("WFSTOnTheFlyDecoder::extendDecHypOnTheFlyNonEndState - helper is not NULL for non-init state") ;
   }
#endif
   
   DecHypOnTheFly *destHyp ;
   // Changes Octavian 20060627
   bool isNew ;

   for ( int i = 0 ; i < currHypSt->n_sucs ; i++ )  {
      int sucInd = currHypSt->sucs[i] ;
      real newScore = currHyp->score + currHypSt->suc_probs[i] ;
      real acousticScore = currHyp->acousticScore + currHypSt->suc_probs[i] ;


      if ( isInitSt == true )
	 // Changes Octavian 20060531
	 // Changes Octavian 20060627
	 destHyp = model->findCurrEmitStateHypsFromInitSt( 
	       sucInd, isPushedMap, pushedGState, matchedOutLabel, helper,
	       &isNew ) ;
      else
	 // Changes Octavian 20060531
	 // Changes Octavian 20060627
	 destHyp = model->findCurrEmitStateHypsFromEmitSt( 
	       sucInd, isPushedMap, pushedGState, matchedOutLabel, &isNew ) ;
      
      // DecodingHMMState *sucSt = initSt + sucInd ;
      DecodingHMMState *sucSt = (model->hmm->states) + sucInd ;

      real destScore ;

      // Changes Octavian 20060627
      /*
      if ( destHyp != NULL )  {
      */
      if ( isNew == false )  {
	 // Have found a hyp in the dest map
	 destScore = destHyp->score ;
#ifdef DEBUG
	 if ( destScore <= LOG_ZERO )
	    error("WFSTOnTheFlyDecoder::extendDecHypOnTheFlyNonEndState - dest hyp's score <= LOG_ZERO") ;
#endif
	 if ( newScore <= destScore )  {
	    // New hyp is not better. Continue the next state transition
	    continue ;
	 }
	 
      }
      else  {
	 // No hyp in the dest map
	 destScore = LOG_ZERO ;
	 
	 // Changes Octavian 20060627
	 // destHyp = (DecHypOnTheFly *) decHypPool->getSingleHyp() ;
	 
	 // Check active?

	 // Update count
	 model->nActiveHyps++ ;
	 if ( sucSt->type == DHS_EMITTING )
	    nActiveEmitHyps++ ;
	 else if ( sucSt->type == DHS_PHONE_END )  {
	    // Changes
	    if ( isInitSt == true )  {
	       error("WFSTOnTheFlyDecoder::extendDecHypOnTheFlyNonEndSt - tee models not supported at the moment") ;
	    }
	    else  {
	       nActiveEndHyps++ ;
	    }
	 }
	 else
	    error("WFSTOnTheFlyDecoder::extendDecHypOnTheFlyNonEndSt - suc st of init had bad type") ;
	
	 // Changes Octavian 20060627
	 /*
	 if ( isInitSt == true )
	    // Changes Octavian 20060531
	    model->putCurrEmitStateHypsFromInitSt( 
		  destHyp, sucInd, isPushedMap, pushedGState, 
		  matchedOutLabel, helper ) ;
	 else
	    // Changes Octavian 20060531
	    model->putCurrEmitStateHypsFromEmitSt( 
		  destHyp, sucInd, isPushedMap, pushedGState, 
		  matchedOutLabel ) ;
		  */
      }

      // These are the common function calls for both newScore>destScore and 
      // newly created dest hyp. This function will also release any hist 
      // elem if destHyp is the only hyp holding it. 
      // ie it will call resetDecHyp()
      decHypHistPool->extendDecHypOnTheFly(
	    currHyp, destHyp, newScore, acousticScore, currHyp->lmScore, 
	    currHyp->currGState, currHyp->pushedWeight, currHyp->nextOutLabel,
	    currHyp->nNextOutLabel ) ;

      // Put into the pruning histogram
      if ( (emitHypsHistogram != NULL) && (sucSt->type == DHS_EMITTING) ) {
	 // Add the new score (and perhaps remove the old) from the emitting
	 //    state hypothesis scores histogram.
	 emitHypsHistogram->addScore( newScore , destScore ) ;
	 //emitHypsHistogram->addScore( newScore , oldScore ) ;
      }

      // Update our best emitting state scores and if the successor state was
      // a phone end then process that now.
      if ( (sucSt->type == DHS_EMITTING) && (newScore > bestEmitScore) )  {
	 bestEmitScore = newScore ;
	 bestHypHist = destHyp->hist ;
	 //bestHypHist = currHyps[sucInd].hist ;
      }
      else if ( sucSt->type == DHS_PHONE_END )  {
	 // Changes
	 if ( isInitSt == true )  {
	    error("WFSTOnTheFlyDecoder::extendDecHypOnTheFlyNonEndSt - tee models not supported at the moment") ;
	 }
	 else  {
	    if( newScore > bestEndScore )
	       bestEndScore = newScore ;
	 }
      }
   }
   // End of for loop - End of processing all state transitions
}

void WFSTOnTheFlyDecoder::processModelEmitStates( WFSTModelOnTheFly *model )
{
   // Changes Octavian 20060417
   // Flip the prevHyps and currHyps.
   NodeToDecHypOnTheFlyList *prevHypsNotPushed = model->currHypsNotPushed ;
   NodeToDecHypOnTheFlyList *prevHypsPushed = model->currHypsPushed ;
   model->currHypsNotPushed = model->prevHypsNotPushed ;
   model->currHypsPushed = model->prevHypsPushed ;
   model->prevHypsNotPushed = prevHypsNotPushed ;
   model->prevHypsPushed = prevHypsPushed ;

#ifdef DEBUG
   // Changes Octavian 20060417
   // Check all maps (pushed and not pushed) of currHypsMap are empty
   for ( int i = 0 ; i < model->hmm->n_states - 1 ; i++ )  {
      
      if (  ( model->currHypsNotPushed[i].head != NULL ) || 
	    ( model->currHypsNotPushed[i].tail != NULL )  )  {
	 error("WFSTOnTheFlyDecoder::processModelEmitStates - currHypsNotPushed is not empty") ;
      }
      
      if (  ( model->currHypsPushed[i].head != NULL ) || 
	    ( model->currHypsPushed[i].tail != NULL )  )  {
	 error("WFSTOnTheFlyDecoder::processModelEmitStates - currHypsPushed is not empty") ;
      }

   }

   // Changes Octavian 20060417
   // Check the number of emitting hypotheses
   int cntNumActiveHyps = 0 ;
   // Changes Octavian 20060424
   if (  ( model->initMapNotPushed != NULL ) ||
	 ( model->initMapPushed != NULL )  )
      error("WFSTOnTheFlyDecoder::processModelEmitStates - init maps are not empty") ;
   
   for ( int i = 0 ; i < model->hmm->n_states - 1 ; i++ )  {
      NodeToDecHypOnTheFly *tmp = model->prevHypsNotPushed[i].head ;
      while ( tmp != NULL )  {
	 cntNumActiveHyps++ ;
	 // Changes Octavian 20060630
	 tmp = tmp->ptr.next ;
      }

      tmp = model->prevHypsPushed[i].head ;
      while ( tmp != NULL )  {
	 cntNumActiveHyps++ ;
	 // Changes Octavian 20060630
	 tmp = tmp->ptr.next ;
      }
   }

   if ( cntNumActiveHyps != model->nActiveHyps )
      error("WFSTOnTheFlyDecoder::processModelEmitStates - old number of active hyps do not match") ;

   if (  ( model->prevHypsNotPushed[model->hmm->n_states-2].head != NULL ) ||
	 ( model->prevHypsNotPushed[model->hmm->n_states-2].tail != NULL ) )
      error("WFSTOnTheFlyDecoder::processModelEmitStates - prevHypsNotPushed[end] is not empty") ;

   if (  ( model->prevHypsPushed[model->hmm->n_states-2].head != NULL ) ||
	 ( model->prevHypsPushed[model->hmm->n_states-2].tail != NULL )  )
      error("WFSTOnTheFlyDecoder::processModelEmitStates - prevHypsPushed[end] is not empty") ;

#endif

   // Reset the count of active hyps
   model->nActiveHyps = 0 ;

#ifdef DEBUG
   for ( int i = 1 ; i < (model->hmm->n_states-1) ; i++ )  {
      if ( model->hmm->states[i].type != DHS_EMITTING )
	 error("WFSTOnTheFlyDecoder::procModelEmitSts - unexpected state type") ;
   }
#endif
   
   DecHypOnTheFly *fromHyp ;
   // Changes Octavian 2006027
   NodeToDecHypOnTheFly *node ;
   int fromHMMState ;
   int pushedGState ;
   // Changes Octavian 20060531
   int matchedOutLabel ;

#ifdef DEBUG
   int prevPushedGState = 0 ;
#endif

   // Process unpushed states
   // Changes Octavian 20060531 20060627
   /*
   while ( ( fromHyp = model->getNextEmitStateHyps( false, &pushedGState, &matchedOutLabel, &fromHMMState ) ) != NULL )  {
   */
   while ( ( node = model->getNextEmitStateHyps( false, &pushedGState, &matchedOutLabel, &fromHMMState ) ) != NULL )  {
      // Changes Octavian 20060627
      fromHyp = &(node->hyp) ;

#ifdef DEBUG
      if ( fromHyp->score <= LOG_ZERO )
	 error("WFSTOnTheFlyDecoder::processModelEmitStates - hyp <= LOG_ZERO") ;
      if ( pushedGState < prevPushedGState )
	 error("WFSTOnTheFlyDecoder::processModelEmitStates - pushedGState < prevPushedGState non-pushed map") ;
      prevPushedGState = pushedGState ;
#endif

      fromHyp->score -= normaliseScore ;
      if ( fromHyp->score > currEmitPruneThresh )  {
	 nEmitHypsProcessed++ ;
	 // Changes Octavian 20060417
	 //DecodingHMMState *fromSt = model->hmm->states + i ;
	 DecodingHMMState *fromSt = model->hmm->states + fromHMMState ;
	
	 // 1. Calculate its emission probability.
	 real emisProb ;
//PNG	 if ( phoneModels == NULL )
	    emisProb = models->calcOutput( fromSt->emis_prob_index ) ;
//PNG	 else
//PNG	    emisProb = phoneModels->calcEmissionProb( fromSt->emis_prob_index ) ;

	 // Add the emisProb to the hyp
	 fromHyp->score += emisProb ;
	 fromHyp->acousticScore += emisProb ;
	
	 // Changes Octavian 20060417 20060531
	 // 2. Evaluate transitions to successor states.
	 extendDecHypOnTheFlyNonEndState( 
	       fromHyp, pushedGState, matchedOutLabel, false, fromHMMState, 
	       model, NULL ) ;
      }
      
      // 3. Finish extending this hyp
      // Reset and Free
      // Changes
      //decHypHistPool->resetDecHypOnTheFly( fromHyp ) ;

      resetDecHyp( fromHyp ) ;

      // Changes Octavian 20060627
      // decHypPool->returnSingleHyp( fromHyp ) ;
      model->pool->returnSingleNode( node ) ;
     
   }
   
   // End of iterating all hyp in a state

#ifdef DEBUG
   prevPushedGState = 0 ;
#endif

   // Process pushed states
   // Changes Octavian 20060531 20060627
   /*
   while ( ( fromHyp = model->getNextEmitStateHyps( true, &pushedGState, &matchedOutLabel, &fromHMMState ) ) != NULL )  {
   */
   while ( ( node = model->getNextEmitStateHyps( true, &pushedGState, &matchedOutLabel, &fromHMMState ) ) != NULL )  {
      // Changes Octavian 2006027
      fromHyp = &(node->hyp) ;

#ifdef DEBUG
      if ( fromHyp->score <= LOG_ZERO )
	 error("WFSTOnTheFlyDecoder::processModelEmitStates - hyp <= LOG_ZERO") ;
      if ( pushedGState < prevPushedGState )
	 error("WFSTOnTheFlyDecoder::processModelEmitStates - pushedGState < prevPushedGState pushed map") ;
      prevPushedGState = pushedGState ;
#endif

      fromHyp->score -= normaliseScore ;
      if ( fromHyp->score > currEmitPruneThresh )  {
	 nEmitHypsProcessed++ ;

	 // Changes Octavian 20060417
	 //DecodingHMMState *fromSt = model->hmm->states + i ;
	 DecodingHMMState *fromSt = model->hmm->states + fromHMMState ;
	 
	 // 1. Calculate its emission probability.
	 real emisProb ;
//PNG	 if ( phoneModels == NULL )
	    emisProb = models->calcOutput( fromSt->emis_prob_index ) ;
//PNG	 else
//PNG	    emisProb = phoneModels->calcEmissionProb( fromSt->emis_prob_index ) ;

	 // Add the emisProb to the hyp
	 fromHyp->score += emisProb ;
	 fromHyp->acousticScore += emisProb ;
	
	 // Changes Octavian 20060417 20060531
	 // 2. Evaluate transitions to successor states.
	 extendDecHypOnTheFlyNonEndState( 
	       fromHyp, pushedGState, matchedOutLabel, true, fromHMMState, 
	       model, NULL ) ;
      }

      // 3. Finish extending this hyp
      // Reset and Free
      // Changes
      //decHypHistPool->resetDecHypOnTheFly( fromHyp ) ;
      resetDecHyp( fromHyp ) ;
      
      // Changes Octavian 20060627
      // decHypPool->returnSingleHyp( fromHyp ) ;
      model->pool->returnSingleNode( node ) ;
   }

   // End of iterating all hyp in a state

#ifdef DEBUG
   // Changes Octavian 20060417
   // Check if all maps (pushed and unpushed) are empty in prevHyps
   for ( int i = 0 ; i < model->hmm->n_states - 1 ; i++ )  {
      if (  ( model->prevHypsNotPushed[i].head != NULL ) ||
	    ( model->prevHypsNotPushed[i].tail != NULL )  )
	 error("WFSTOnTheFlyDecoder::processModelEmitStates - prevHypsNotPushed is not empty") ;
      if (  ( model->prevHypsPushed[i].head != NULL ) ||
	    ( model->prevHypsPushed[i].tail != NULL )  )
	 error("WFSTOnTheFlyDecoder::processModelEmitStates - prevHypsPushed is not empty") ;
   }

   // Check the number of active hyps
   int newCntNumActiveHyps = 0 ;
   for ( int i = 0 ; i < model->hmm->n_states - 1 ; i++ )  {
      NodeToDecHypOnTheFly *tmp = model->currHypsNotPushed[i].head ;
      while ( tmp != NULL )  {
	 newCntNumActiveHyps++ ;
	 // Changes Octavian 20060630
	 tmp = tmp->ptr.next ;
      }

      tmp = model->currHypsPushed[i].head ;
      while ( tmp != NULL )  {
	 newCntNumActiveHyps++ ;
	 // Changes Octavian 20060630
	 tmp = tmp->ptr.next ;
      }
   }

   if ( newCntNumActiveHyps != model->nActiveHyps )
      error("WFSTOnTheFlyDecoder::processModelEmitStates - newCntNumActiveHyps incorrect") ;

#endif

}

// Extend a hyp from current transition to subsequent transitions
// endHyp is the hyp reaching the end state of the current transition
// trans is the current transition
// nextTransBuf is a buffer for storing a set of subsequent transitions
// isPushedMap indicates whether this hyp is from the pushedMap or 
// notPushedMap
// 	- pushedMap means the output label has been pushed. The hyp can be 
// 	  compared using the anticipated G state, which is not the actual G 
// 	  state that the hyp is located.
// 	- notPushedMap means that the output label has not been decided yet. 
// 	  The hyp is compared using the G state where the hyp is located.
// The pair (pushedGState, thisMatchedOutLabel) is the key of either the 
// pushedMap or notPushedMap.
void WFSTOnTheFlyDecoder::extendModelEndState(
      DecHypOnTheFly *endHyp , WFSTTransition *trans , 
      WFSTTransition **nextTransBuf , bool isPushedMap , 
      int pushedGState , int thisMatchedOutLabel )
{
#ifdef DEBUG
   if ( endHyp == NULL )
      error("WFSTOnTheFlyDecoder::extendModelEndState - endHyp == NULL") ;
   if ( endHyp->score <= LOG_ZERO )
      error("WFSTOnTheFlyDecoder::extendModelEndState - score <= LOG_ZERO") ;
   // Changes
   // 20060423
   
   if ( endHyp->score <= currEndPruneThresh )
      error("WFSTOnTheFlyDecoder::extendModelEndState - score <= currEndPruneThresh") ;

#endif
   
   int lattToState=-1 , lattFromState=-1 ;

   // True if we want to directly assign commonWeight to hyp. False if we 
   // want to add only the difference
   bool isDirectAssign = false ;

   // Before propagation, perform lattice generation and label registration, 
   // etc.
   if ( trans != NULL )  {
      // Non-initial case
#ifdef DEBUG
      if ( trans->outLabel == network->getWordEndMarker() )
         error("WFSTOnTheFlyDecoder::extendModelEndState - out label is word-end marker");
#endif

      // Reset variables responsible for label-pushing, etc, if the current 
      // trans has an output label
      if ( trans->outLabel != WFST_EPSILON )  {
	 // Checking - isPushed must be true
	 if ( isPushedMap != true )
	    error("WFSTOnTheFlyDecoder::extendModelEndState - isPushed = false while outLabel != eps. Trans ID = %d", trans->id ) ;
	 
	 endHyp->currGState = pushedGState ;
	 endHyp->pushedWeight = 0.0 ;
	 isPushedMap = false ;
	 // Changes Octavian 20060531
	 thisMatchedOutLabel = UNDECIDED_OUTLABEL ;
	 isDirectAssign = true ;
      }

      // Lattice generation - Put the outLabel(s) to the lattice
      if ( doLatticeGeneration )  {
	 lattToState = addLatticeEntry( endHyp , trans , &lattFromState ) ;
      }

      // Model level output or Word level output
      if ( doModelLevelOutput )  {
	 // If the hyp has some output labels, put them in the history. The 
	 // second argument is ignored by the function for on-the-fly hyp.
         if ( endHyp->nNextOutLabel > 0 )  {
	    // Changes Octavian 20060523
	    // decHypHistPool->addLabelHistToDecHyp( endHyp , endHyp->nextOutLabel[0] ) ;
	    addLabelHist( endHyp, endHyp->nextOutLabel[0] ) ;
         }

	 // If the transition has a inLabel, put the inLabel in the history.
         if ( (trans->inLabel != WFST_EPSILON) && (trans->inLabel != network->getWordEndMarker()) )  {
            decHypHistPool->addHistToDecHyp( 
		  endHyp , trans->inLabel , endHyp->score , 
		  currFrame , endHyp->acousticScore , endHyp->lmScore );
         }
      }
      else  {
	 // If the hyp has some output labels, put them in the non-registered
	 // (NR) list of the hyp.
	 // The second argument is ignored for on-the-fly hypothesis
         if ( endHyp->nNextOutLabel > 0 ) {
	    // Changes
	    /*
            DecHypHistPool::registerLabel( endHyp, endHyp->nextOutLabel[0] );
	    */
	    registerLabel( endHyp, endHyp->nextOutLabel[0] ) ;
         }

	 // If word-end marker is encountered, register the first outLabel 
	 // in the NR queue.
         if ( trans->inLabel == network->getWordEndMarker() ) {
            decHypHistPool->registerEnd( 
		  endHyp, endHyp->score, currFrame, endHyp->acousticScore, 
		  endHyp->lmScore );
         }
      }
#ifdef DEBUG
      if ( doLatticeGeneration )
      {
         if ( (endHyp->hist != NULL) && (endHyp->hist->type == LATTICEDHHTYPE) )
            error("WFSTOnTheFlyDecoder::extendModelEndState - unexpected LATTICEDHHTYPE history found") ;
      }
#endif

      // If this transition goes to a final state of both transducers, then 
      // check to see if this is the best final state hypothesis.
      if (  ( network->transGoesToFinalState( trans ) ) && 
	    ( gNetwork->isFinalState( endHyp->currGState ) ))  {
         real fScore = network->getFinalStateWeight(trans) + 
	    gNetwork->getFinalStateWeight(endHyp->currGState);

         if ( doLatticeGeneration )  {
#ifdef ACOUSTIC_SCORE_ONLY
            lattice->addFinalState( lattToState , 0.0 ) ;
#else
            lattice->addFinalState( lattToState , fScore ) ;
#endif
         }

         // But first add on the final state weight.
	 // Changes
         if ( (endHyp->score + fScore) > bestFinalHyp->score )  {
            decHypHistPool->extendDecHypOnTheFly( 
		  endHyp ,(DecHypOnTheFly *) bestFinalHyp , 
		  endHyp->score+fScore , endHyp->acousticScore , 
		  endHyp->lmScore+fScore , endHyp->currGState , 
		  endHyp->pushedWeight, endHyp->nextOutLabel , 
		  endHyp->nNextOutLabel ) ;
         }
      }
      
      // Put a lattice history at the tail of hist linked list
      if ( doLatticeGeneration )  {
         // Add lattice history to this end hyp, for quick access to the 
	 // previous lattice state, as well as the score.
#ifdef ACOUSTIC_SCORE_ONLY
	 decHypHistPool->addLatticeHistToDecHyp( 
	       endHyp , lattToState , 
	       endHyp->acousticScore ) ; 
#else
         decHypHistPool->addLatticeHistToDecHyp( 
	       endHyp , lattToState , 
	       endHyp->acousticScore + endHyp->lmScore ) ; 
#endif
         
         // Register that there is a potential transition going out from
	 // lattToState. This is so that there is correct behaviour when 
	 // endHyp gets deactivated after being extended to init states.
         lattice->registerActiveTrans( lattToState ) ;
      }
   }
   else  {
      // Initial case
#ifdef DEBUG
      if ( currFrame != 0 )
         error("WFSTOnTheFlyDecoder::extModelEndState - currFrame==0 when trans==NULL assumption wrong") ;
#endif

      if ( doLatticeGeneration )  {
         // Add lattice history to this end hyp, for quick access to the 
	 // previous lattice state, as well as the score.
         //decHypHistPool->addLatticeHistToDecHyp( endHyp, lattice->getInitState(), endHyp->score ) ;
         
	 // Changes - Add a history of score 0.0 with lattice init state to 
	 // endhyp
	 // Not using endHyp->score because it can contain back-off weights 
	 // of G
	 decHypHistPool->addLatticeHistToDecHyp( 
	       endHyp, lattice->getInitState(), 0.0 ) ;

         // Register that there is a potential transition going out from 
	 // lattToState.
         // This is so that there is correct behaviour when endHyp gets 
	 // deactivated after being extended to init states.
         lattice->registerActiveTrans( lattice->getInitState() ) ;
      }

      // Changes
      // If the end hyp has either backoff-weights and/or output labels 
      // before even entering CoL transducer, register them. This could 
      // happen if G transducer has epsilon arcs from its initial state.

      // Do lattice generation first
      if ( doLatticeGeneration )  {
	 lattToState = addLatticeEntry( endHyp , trans , &lattFromState ) ;
      }

      // Then register output labels if any
      if ( doModelLevelOutput )  {
	 if ( endHyp->nNextOutLabel > 0 )
	    addLabelHist( endHyp, endHyp->nextOutLabel[0] ) ;
	 // Changes Octavian 20060523
	 // decHypHistPool->addLabelHistToDecHyp( endHyp , endHyp->nextOutLabel[0] ) ;
      }
      else  {
	 if ( endHyp->nNextOutLabel > 0 )
	    registerLabel( endHyp, endHyp->nextOutLabel[0] ) ;
	    // Changes
	    //DecHypHistPool::registerLabel( endHyp, endHyp->nextOutLabel[0] ) ;
      }
      
#ifdef DEBUG
      if ( doLatticeGeneration )  {
	 if ( (endHyp->hist != NULL) && (endHyp->hist->type == LATTICEDHHTYPE) )
	    error("WFSTOnTheFlyDecoder::extendModelEndState - unexpected LATTICEDHHTYPE history found (2)") ;
      }
#endif

      if (( network->isFinalState( network->getInitState() ) ) && 
	    ( gNetwork->isFinalState( endHyp->currGState ) ))  {
	 real fScore = network->getFinalStateWeight( network->getInitState() ) + 
	    gNetwork->getFinalStateWeight( endHyp->currGState ) ;

	 if ( doLatticeGeneration )  {
#ifdef ACOUSTIC_SCORE_ONLY
	    lattice->addFinalState( lattToState , 0.0 ) ;
#else
	    lattice->addFinalState( lattToState , fScore ) ;
#endif
	 }
	 
	 // But first add on the final state weight.
	 // Changes
	 if ( (endHyp->score + fScore) > bestFinalHyp->score )  {
	    decHypHistPool->extendDecHypOnTheFly( 
		  endHyp , (DecHypOnTheFly *) bestFinalHyp , endHyp->score+fScore ,
		  endHyp->acousticScore , endHyp->lmScore+fScore,
		  endHyp->currGState, endHyp->pushedWeight, 
		  endHyp->nextOutLabel, endHyp->nNextOutLabel ) ;
	 }
      }

      // Put a lattice history at the tail of hist linked list
      if ( doLatticeGeneration )  {
#ifdef ACOUSTIC_SCORE_ONLY
	 decHypHistPool->addLatticeHistToDecHyp( endHyp , lattToState , 
	       endHyp->acousticScore ) ;
#else
	 decHypHistPool->addLatticeHistToDecHyp( endHyp , lattToState , 
	       endHyp->acousticScore + endHyp->lmScore ) ;
#endif
	 
	 lattice->registerActiveTrans( lattToState ) ;
      }
   }

   // Reset nextOutLabel and nNextOutLabel before propagation
   endHyp->nNextOutLabel = 0 ;

   // Check. If isPushedMap == false and the pushedGState and the hyp's 
   // GState don't match, error.
#ifdef DEBUG
   if (( isPushedMap == false ) && ( pushedGState != endHyp->currGState )) 
      error("WFSTOnTheFlyDecoder::extendModelEndState - Not-pushed status but GState don't match") ;
#endif

   // Two different schemes of hypothesis propagation
#ifdef DELAYPUSHING
   // Start Propagation
   // Allocating a buffer if there is none.
   bool ownNextTransBuf=false ;
   int nNextTrans = 0 ;
   if ( nextTransBuf == NULL )  {
      // We need to allocate our own memory to store next transitions results.
      nextTransBuf = new WFSTTransition*[network->getMaxOutTransitions()] ;
      ownNextTransBuf = true ;
   }
   
   // Do we split the propagation in two parts? - doPushingWeightAndLabel

   // Allocate some memory for commonWeights, nextGState, nextOutLabel and
   // matchedOutLabel. They are the corresponding values for each of the next
   // transitions.
   // Changes Octavian 20060531
   // THESE ARE FOR STORING MATCHING LABELS.
   int networkMaxOutTrans = network->getMaxOutTransitions() ;
   real *commonWeight = new real[networkMaxOutTrans] ;
   int *nextGState = new int[networkMaxOutTrans] ;
   int *nextOutLabel = new int[networkMaxOutTrans] ;
   // Changes Octavian 20060531
   int *matchedOutLabel = new int[networkMaxOutTrans] ;

   // Changes Octavian 20060824
   // Some extra variables for storing muliple G states, their weights
   // and their output labels - make it generalised
   // Multiple G states could happen on the epsilon path of the G transducer
   // To simplify implementation, currently set the size of array to 
   // MAXNUMDECHYPOUTLABELS, which corresponds to the number of output labels 
   // which a hyp can hold.
   // THESE ARE FOR EPS ARCS IN G TRANSDUCER
   int multipleGState[MAXNUMDECHYPOUTLABELS] ;
   real multipleWeight[MAXNUMDECHYPOUTLABELS] ;
   int multipleOutLabel[MAXNUMDECHYPOUTLABELS] ;
   int multipleNGState = 0 ;

   // Changes Octavian 20060824
   // isDirectAssign is true when this transition has an non-eps output labels
   if ( isDirectAssign == true )  {
      gNetwork->getStatesOnEpsPath( 
	    endHyp->currGState, 0.0, WFST_EPSILON, 
	    multipleGState, multipleWeight, multipleOutLabel, 
	    &multipleNGState, MAXNUMDECHYPOUTLABELS ) ;
   }
   else  {
      // multipleGState - Here it's not on the eps path.
      multipleGState[0] = pushedGState ;
      multipleWeight[0] = 0.0 ;
      multipleOutLabel[0] = WFST_EPSILON ;
      multipleNGState = 1 ;
   }

   // An array storing only non-eps output labels from eps path
   int multipleOutLabelWithNoEPS[MAXNUMDECHYPOUTLABELS] ;
   // Each element in the arrays records the number of non-eps labels at that
   // state
   int multipleNOutLabelAtPosition[MAXNUMDECHYPOUTLABELS] ;
   // Number of non-eps output labels on the eps path
   int multipleNOutLabel = 0 ;
   multipleNOutLabelAtPosition[0] = 0 ;
   
   // Accumulate eps weight and non-eps output labels
   for ( int i = 1 ; i < multipleNGState ; i++ )  {
      multipleWeight[i] = multipleWeight[i-1] + multipleWeight[i] ;
      if ( multipleOutLabel[i] != WFST_EPSILON )  {
	 multipleOutLabelWithNoEPS[multipleNOutLabel] = multipleOutLabel[i] ;
	 multipleNOutLabel++ ;
      }
      multipleNOutLabelAtPosition[i] = multipleNOutLabel ;
   }

   // Duplicate hyp if on eps path
   for ( int j = multipleNGState - 1 ; j >= 0 ; j-- )  {
      nNextTrans = 0 ;
      DecHypOnTheFly *toPropagate = NULL ;
      DecHypOnTheFly tmpHypForEpsPath ;
      if ( j == 0 )  {
	 toPropagate = endHyp ;
      }
      else  {
	 // Only enter this when just entering a new prefix region AND G has
	 // eps-input transitions (back-off)
	 tmpHypForEpsPath.hist = NULL ;
	 decHypHistPool->extendDecHypOnTheFly(
	       endHyp, &tmpHypForEpsPath, 
	       endHyp->score + multipleWeight[j], 
	       endHyp->acousticScore, 
	       endHyp->lmScore + multipleWeight[j], 
	       multipleGState[j], endHyp->pushedWeight, 
	       multipleOutLabelWithNoEPS, multipleNOutLabelAtPosition[j] ) ;
	 toPropagate = &tmpHypForEpsPath ;
      }

      // Can be used for multiple GState in eps path
      pushedGState = multipleGState[j] ;
      
      // Get a set of subsequent transitions
      // Changes. Can enable or disable label/weight pushing
      if ( doPushingWeightAndLabel )  {
	 // Changes Octavian 20060531
	 // Changes Octavian 20060726
#ifndef CASEIRO
	 findMatchedTransWithPushing(
	       trans, &nNextTrans, nextTransBuf, toPropagate->currGState, 
	       commonWeight, nextGState, nextOutLabel, matchedOutLabel ) ;
#else
	 findMatchedTransWithPushing( 
	       trans, &nNextTrans, nextTransBuf, toPropagate->currGState, 
	       commonWeight, nextGState, nextOutLabel, matchedOutLabel,
	       toPropagate->nLabelsNR, isPushedMap ) ;
#endif
      }
      else  {
	 // Changes Octavian 20060531
	 findMatchedTransNoPushing(
	       trans, &nNextTrans, nextTransBuf, toPropagate->currGState, 
	       commonWeight, nextGState, nextOutLabel, matchedOutLabel ) ;
      }
      
      // destIsPushedMap indicates whether the hyp is put in a pushedMap or 
      // notPushedMap of next trans.
      // destMatchedOutLabel is the CoL output label of the next CoL 
      // transition which matches with one of the input symbols of the G 
      // transducer. If the next transition is a FOLLOWON_TRANS, 
      // NOPUSHING_TRANS or non-sigleton-intersection trans, 
      // destMatchedOutLabel = UNDECIDED_OUTLABEL. Otherwise, if there's 
      // only a sigleton intersection,
      // destMatchedOutLabel = the actual matching label.
      // THESE FIVE ATTRIBUTES NEED TO BE DEFINED FOR EACH CASE
      bool destIsPushedMap ;
      // Changes Octavian 20060824
      int destPushedGState ;
      // Changes Octavian 20060531
      int destMatchedOutLabel ;
      // Changes Octavian 20060824
      int destOutLabel ;
      // Changes Octavian 20060824
      real incPushedWeight ;
 
      // Iterate all next transitions
      for ( int i = 0 ; i < nNextTrans ; i++ )  {
	       
	 // Octavian 20060325 - add one more case
	 // First case: FOLLOWON_TRANS
	 // The label set of this next trans is the same as the current trans
	 // No need to add weight, change status, etc
      
	 if ( nextGState[i] == FOLLOWON_TRANS )  {
	    // Because it's a FOLLOWON_TRANS. The following are set by 
	    // findMatchedTransWithPushing()
	    commonWeight[i] = toPropagate->pushedWeight ;
	 
	    // nextOutLabel is not used.
	    destIsPushedMap = isPushedMap ;

	    // Changes Octavian 20060824
	    destPushedGState = pushedGState ;
	 
	    // Changes Octavian 20060531
	    destMatchedOutLabel = thisMatchedOutLabel ;

	    // Changes Octavian 20060824
	    destOutLabel = WFST_EPSILON ;

	    // Changes Octavian 20060824
	    incPushedWeight = 0.0 ;

	    // Changes Octavian 20060824
	    /*
	    multipleNGState = 1 ;
	    multipleGState[0] = pushedGState ;
	    multipleWeight[0] = 0.0 ;
	    multipleOutLabel[0] = WFST_EPSILON ;
	    */
      
	 }
	 else if ( nextGState[i] == UNDECIDED_TRANS )  {
	    // Second case: nextGState[i] = UNDECIDED_TRANS 
	    // i.e. non-singleton intersection
#ifdef DEBUG
	    // isPushed must be false for this to happen
	    if ( isPushedMap == true )
	       error("WFSTOnTheFlyDecoder::extendModelEndState - isPushedMap == true for UNDECIDED_TRANS") ;
	    // More checking. The transition should have no output labels
	    if ( nextTransBuf[i]->outLabel != WFST_EPSILON )
	       error("WFSTOnTheFlyDecoder::extendModelEndState - should have no outLabel being a UNDECIDED_TRANS") ;

	    // Changes Octavian 20060531
	    if ( thisMatchedOutLabel != UNDECIDED_OUTLABEL )
	       error("WFSTOnTheFlyDecoder::extendModelEndState - thisMatchedOutLabel != UNDECIDED_OUTLABEL for UNDECIDED_TRANS") ;
	 
	    // Changes Octavian 20060531
	    if ( matchedOutLabel[i] != UNDECIDED_OUTLABEL )
	       error("WFSTOnTheFlyDecoder::extendModelEndState - matchedOutputLabel != UNDECIDED_OUTLABEL for UNDECIDED_TRANS") ;
#endif
	    // Calculate the increment weight (commonWeight - pushed weight)
	    if ( isDirectAssign == true )
	       incPushedWeight = commonWeight[i] ;
	    else
	       incPushedWeight = commonWeight[i] - toPropagate->pushedWeight ;

	    // Other variables concerning the next transition
	    destIsPushedMap = false ;
	 
	    // Changes Octavian 20060824
	    destPushedGState = pushedGState ;
	 
	    // Changes Octavian 20060531
	    destMatchedOutLabel = UNDECIDED_OUTLABEL ;
	 
	    // Changes Octavian 20060824
	    destOutLabel = WFST_EPSILON ;
	 
	    // because the G state does not change. No need to propagate G.
	    // Changes Octavian 20060824
	    /*
	       multipleNGState = 1 ;
	       multipleGState[0] = pushedGState ;
	       multipleWeight[0] = incPushedWeight ;
	       multipleOutLabel[0] = WFST_EPSILON;
	       */
	 }
#ifdef DEBUG
	 else if ( nextGState[i] <= DEAD_TRANS )  {
	    error("WFSTOnTheFlyDecoder::extendModelEndState - nextGState[i] <= DEAD_TRANS") ;
	 }
#endif
	 else if ( nextGState[i] == NOPUSHING_TRANS )  {
	    // Third case: No-pushing transitions. Appear before the final 
	    // state of CoL transducer
#ifdef DEBUG
	    // isPushed must be false for this to happen
	    if ( isPushedMap == true )
	       error("WFSTOnTheFlyDecoder::extendModelEndState - isPushedMap == true for NOPUSHING_TRANS") ;
	 
	    // Pushed weight of the endHyp must be 0.0 
	    // Because if next tran is the first NOPUSHING_TRANS, this trans 
	    // must have an output label, which means the 
	    // endHyp->pushedWeight has been reset to 0.0.	 
	    // If this tran is a NOPUSHING_TRANS, then endHyp->pushedWeight 
	    // must be also 0.0 because commonWeight for NOPUSHING_TRANS is 
	    // 0.0.
	    // Because pushedWeight for NOPUSHING_TRANS is 0.0, it should be 
	    // OK when they encounter either singleton or non-sigleton trans 
	    // later because the pushed weight being 0 will have no effect on
	    // the subsequent common weight.
	 
	    if ( commonWeight[i] != 0.0 )
	       error("WFSTOnTheFlyDecoder::extendModelEndState - commonWeight for NOPUSHING_TRANS != 0.0") ;
	    
	    if ( toPropagate->pushedWeight != 0.0 )
	       error("WFSTOnTheFlyDecoder::extendModelEndState - endHyp->pushedWeight != 0.0 for NOPUSHING_TRANS") ;
	    
	    // More checking. The transition should have no output labels
	    if ( nextTransBuf[i]->outLabel != WFST_EPSILON )
	       error("WFSTOnTheFlyDecoder::extendModelEndState - should have no outLabel being a NOPUSHING_TRANS") ;
	    
	    // Changes Octavian 20060531
	    if ( thisMatchedOutLabel != UNDECIDED_OUTLABEL )
	       error("WFSTOnTheFlyDecoder::extendModelEndState - thisMatchedOutLabel != UNDECIDED_OUTLABEL for NOPUSHING_TRANS") ;
	    
	    // Changes Octavian 20060531
	    if ( matchedOutLabel[i] != UNDECIDED_OUTLABEL )
	       error("WFSTOnTheFlyDecoder::extendModelEndState - matchedOutLabel != UNDECIDED_OUTLABEL for NOPUSHING_TRANS") ;
#endif
	    destIsPushedMap = false ;
	    
	    // Changes Octavian 20060824
	    destPushedGState = pushedGState ;
	 
	    // Changes Octavian 20060531
	    destMatchedOutLabel = UNDECIDED_OUTLABEL ;

	    // Changes Octavian 20060824
	    destOutLabel = WFST_EPSILON ;

	    // Changes Octavian 20060824
	    incPushedWeight = 0.0 ;

	    // Changes Octavian 20060824
	    /*
	       multipleNGState = 1 ;
	       multipleGState[0] = pushedGState ;
	       multipleWeight[0] = 0.0 ;
	       multipleOutLabel[0] = WFST_EPSILON ;
	       */
	 }
	 else  {
	    // Fourth case: has singleton intersection
	    if ( isPushedMap == false )  {
	       // The endHyp is from an unpushed map. So this is the first 
	       // time encountering a singleton intersection
#ifdef DEBUG
	       // Changes Octavian 20060531
	       if ( thisMatchedOutLabel != UNDECIDED_OUTLABEL )
		  error("WFSTOnTheFlyDecoder::extendModelEndState - thisMatchedOutLabel != UNDECIDED_OUTLABEL for first-time singleton") ;
	    
	       // Changes Octavian 20060531
	       if ( matchedOutLabel[i] == UNDECIDED_OUTLABEL )
		  error("WFSTOnTheFlyDecoder::extendModelEndState - matchedOutLabel[i] == UNDECIDED_OUTLABEL for first-time singleton") ;
#endif
	       // Calculate increment weight
	       if ( isDirectAssign == true )
		  incPushedWeight = commonWeight[i] ;
	       else
		  incPushedWeight = commonWeight[i] - toPropagate->pushedWeight ;
	       // Assign other variables
	       destIsPushedMap = true ;

	       // Changes Octavian 20060824
	       destPushedGState = nextGState[i] ;
	    
	       // Changes Octavian 20060531
	       destMatchedOutLabel = matchedOutLabel[i] ;

	       // Changes Octavian 20060824
	       destOutLabel = nextOutLabel[i] ;

	       // *********************************
	       // Changes Octavian 20060508
	       // Changes Octavian 20060523
	       // Changes Octavian 20060824
	       /*gNetwork->getStatesOnEpsPath( 
		     nextGState[i], incPushedWeight, 
		     nextOutLabel[i], multipleGState, 
		     multipleWeight, multipleOutLabel, 
		     &multipleNGState, MAXNUMDECHYPOUTLABELS ) ;
		     */
	       // **********************************
	    }
	    else  {
	       // Already pushed
#ifdef DEBUG
	       // Checking - commonWeight should be equal to the 
	       // hyp->pushedWeight
	       if ( toPropagate->pushedWeight != commonWeight[i] )
		  error("WFSTOnTheFlyDecoder::extendModelEndState - pushedWeight not equal for already pushed hyp") ;

	       // Changes Octavian 20060531
	       if ( thisMatchedOutLabel == UNDECIDED_OUTLABEL )
		  error("WFSTOnTheFlyDecoder::extendModelEndState - thisMatchedOutLabel == UNDECIDED_OUTLABEL for already-pushed trans") ;
	       
	       // Changes Octavian 20060531
	       if ( thisMatchedOutLabel != matchedOutLabel[i] )
		  error("WFSTOnTheFlyDecoder::extendModelEndState - thisMatchedOutLabel != matchedOutLabel[i] for already-pushed trans") ;
#endif
	    
	       // Changes Octavian 20060824
	       incPushedWeight = 0.0 ;
	       destIsPushedMap = true ;
	    
	       // Changes Octavian 20060824
	       destPushedGState = pushedGState ;
	    
	       // Changes Octavian 20060531
	       destMatchedOutLabel = thisMatchedOutLabel ;
	    
	       // Changes Octavian 20060824
	       destOutLabel = WFST_EPSILON ;
	    
	       // Changes Octavian 20060824
	       /*
		  multipleNGState = 1 ;
		  multipleGState[0] = pushedGState ;
		  multipleWeight[0] = 0.0 ;
		  multipleOutLabel[0] = WFST_EPSILON ;
		  */
	    }
	 }
	 // End of the four cases (follow-on, singleton, no-pushing and 
	 // non-singleton)
      
	 // Because some output labels are epsilon, they cannot be stored 
	 // inside the hyp.Otherwise it will register additional epsilon 
	 // labels when word-end marker is encountered.      
	 // Need to clean up the multipleOutLabel array      
	 // Changes Octavian 20060824
	 /*
	  * int multipleOutLabelWithNoEPS[MAXNUMDECHYPOUTLABELS] ;
	  * int multipleNOutLabel = 0 ;
	  * */
      
	 // Changes Octavian 20060824
	 int incOutLabel = 0 ;
	 if ( destOutLabel != WFST_EPSILON )  {
	    // Put the destOutLabel on the toPropagate Hyp without i
	    // ncrementing the count. So that position can be re-used.
	    if ( toPropagate->nNextOutLabel == MAXNUMDECHYPOUTLABELS )
	       error("WFSTOnTheFlyDecoder::extendModelEndState - full outlabels. Try to increase that amount by re-defining that macro") ;
	    toPropagate->nextOutLabel[toPropagate->nNextOutLabel] = destOutLabel ;
	    incOutLabel = 1 ;
	 }
      
	 // Propagation starts here
	 // First case - if next transitions has no model
	 if (  (nextTransBuf[i]->inLabel == WFST_EPSILON) ||
	       (nextTransBuf[i]->inLabel == network->getWordEndMarker()) )  {
	    DecHypOnTheFly tmp ;
	    tmp.hist = NULL ;

	    decHypHistPool->extendDecHypOnTheFly(
		  toPropagate, &tmp, 
		  toPropagate->score + incPushedWeight + nextTransBuf[i]->weight, 
		  toPropagate->acousticScore, 
		  toPropagate->lmScore + incPushedWeight + nextTransBuf[i]->weight, 
		  toPropagate->currGState, commonWeight[i], 
		  toPropagate->nextOutLabel, 
		  toPropagate->nNextOutLabel + incOutLabel ) ;
	    
	    extendModelEndState( 
		  &tmp, nextTransBuf[i], NULL, destIsPushedMap, 
		  destPushedGState, destMatchedOutLabel ) ;
	    
	    // Changes
	    //decHypHistPool->resetDecHypOnTheFly( &tmp ) ;
	    resetDecHyp( &tmp ) ;
	 }
	 else  {
	    // Second case - has actually a model
	    WFSTModelOnTheFly *nextModel = getModel( nextTransBuf[i] ) ;
	    real newScore = toPropagate->score + incPushedWeight + nextTransBuf[i]->weight ;
	    
	    DecHypOnTheFly *destHyp ;
	    // Changes Octavian 20060627
	    // DecHypOnTheFly **destHypInMap ;
	 
	    bool isNew ;
	 
	    if ( destIsPushedMap == false )  {
	       // Changes Octavian 20060531
	       // Changes Octavian 20060627
	       /*
	       destHypInMap = nextModel->findInitStateHyp( 
		     nextModel->initMapNotPushed, false, 
		     multipleGState[j], destMatchedOutLabel, &isNew ) ;
		     */
	       // Changes Octavian 20060824
	    
	       /*
		* destHyp = nextModel->findInitStateHyp(
		  nextModel->initMapNotPushed, false, 
		  multipleGState[j], destMatchedOutLabel, &isNew ) ;
		  */
	       destHyp = nextModel->findInitStateHyp(
		     nextModel->initMapNotPushed, false, 
		     destPushedGState, destMatchedOutLabel, &isNew ) ;
	    }
	    else  {
	       // Changes Octavian 20060531
	       // Changes Octavian 20060627
	       /*
	       destHypInMap = nextModel->findInitStateHyp( 
		     nextModel->initMapPushed, true, 
		     multipleGState[j], destMatchedOutLabel, &isNew ) ;
		     */
	       // Changes Octavian 20060824
	       /*
	       destHyp = nextModel->findInitStateHyp( 
		     nextModel->initMapPushed, true, 
		     multipleGState[j], destMatchedOutLabel, &isNew ) ;
		     */
	       destHyp = nextModel->findInitStateHyp( 
		     nextModel->initMapPushed, true, 
		     destPushedGState, destMatchedOutLabel, &isNew ) ;
	    }
	    
	    if ( isNew == false )  {
	       // Found the hyp
#ifdef DEBUG
	       if ( destHyp->score <= LOG_ZERO )
		  error("WFSTOnTheFlyDecoder::extendModelEndState - destHyp->score <= LOG_ZERO") ;
#endif
	       // Changes Octavian 20060824
	       // This continue refers to the for loop iterating all the 
	       // nextTrans
	       if ( newScore <= destHyp->score )
		  continue ;
	    }
	    else  {
	       // Not found
	       // Get a piece of memory for a hyp from pool
	       // Changes Octavian 20060627
	       // destHyp = (DecHypOnTheFly*) decHypPool->getSingleHyp() ;
	       // *destHypInMap = destHyp ;
	       nextModel->nActiveHyps++ ;
	    }

	    // Changes Octavian 20060824
	    /*
	     * decHypHistPool->extendDecHypOnTheFly(
	       endHyp, destHyp, newScore, endHyp->acousticScore, 
	       endHyp->lmScore + accWeight + nextTransBuf[i]->weight,
	       endHyp->currGState, commonWeight[i], 
	       multipleOutLabelWithNoEPS, multipleNOutLabel ) ;
	       */
	    decHypHistPool->extendDecHypOnTheFly(
		  toPropagate, destHyp, newScore, toPropagate->acousticScore, 
		  toPropagate->lmScore + incPushedWeight + nextTransBuf[i]->weight,
		  toPropagate->currGState, commonWeight[i], 
		  toPropagate->nextOutLabel, toPropagate->nNextOutLabel + incOutLabel ) ;
	 }
      }
      // End of iterating all transitions

      // Only reset if we have duplicated the transition
      if ( j != 0 )
	 resetDecHyp( toPropagate ) ;
   
   }
   
   if ( ownNextTransBuf )
      delete [] nextTransBuf ;

   delete [] commonWeight ;
   delete [] nextGState ;
   delete [] nextOutLabel ;
   // Changes Octavian 20060531
   delete [] matchedOutLabel ;

#else   
   // Start Propagation
   // Allocating a buffer if there is none.
   bool ownNextTransBuf=false ;
   int nNextTrans = 0 ;
   if ( nextTransBuf == NULL )  {
      // We need to allocate our own memory to store next transitions results.
      nextTransBuf = new WFSTTransition*[network->getMaxOutTransitions()] ;
      ownNextTransBuf = true ;
   }
   
   // Do we split the propagation in two parts? - doPushingWeightAndLabel

   // Allocate some memory for commonWeights, nextGState, nextOutLabel and
   // matchedOutLabel. They are the corresponding values for each of the next
   // transitions.
   // Changes Octavian 20060531
   int networkMaxOutTrans = network->getMaxOutTransitions() ;
   real *commonWeight = new real[networkMaxOutTrans] ;
   int *nextGState = new int[networkMaxOutTrans] ;
   int *nextOutLabel = new int[networkMaxOutTrans] ;
   // Changes Octavian 20060531
   int *matchedOutLabel = new int[networkMaxOutTrans] ;

   // Get a set of subsequent transitions
   // Changes. Can enable or disable label/weight pushing
   if ( doPushingWeightAndLabel )  {
      // Changes Octavian 20060531
      // Changes Octavian 20060726
#ifndef CASEIRO
      findMatchedTransWithPushing( 
	    trans, &nNextTrans, nextTransBuf, endHyp->currGState, 
	    commonWeight, nextGState, nextOutLabel, matchedOutLabel ) ;
#else
      findMatchedTransWithPushing( 
	    trans, &nNextTrans, nextTransBuf, endHyp->currGState, 
	    commonWeight, nextGState, nextOutLabel, matchedOutLabel,
	    endHyp->nLabelsNR, isPushedMap ) ;
#endif
   }
   else  {
      // Changes Octavian 20060531
      findMatchedTransNoPushing( 
	    trans, &nNextTrans, nextTransBuf, endHyp->currGState, 
	    commonWeight, nextGState, nextOutLabel, matchedOutLabel ) ;
   }
 
   // Some extra variables for storing muliple G states, their weights
   // and their output labels - make it generalised
   // Multiple G states could happen on the epsilon path of the G transducer
   // To simplify implementation, currently set the size of array to 
   // MAXNUMDECHYPOUTLABELS, which corresponds to the number of output labels 
   // which a hyp can hold.
   // destIsPushedMap indicates whether the hyp is put in a pushedMap or 
   // notPushedMap of next trans.
   // destMatchedOutLabel is the CoL output label of the next CoL transition
   // which matches with one of the input symbols of the G transducer. If the
   // next transition is a FOLLOWON_TRANS, NOPUSHING_TRANS or 
   // non-sigleton-intersection trans, destMatchedOutLabel = 
   // UNDECIDED_OUTLABEL. Otherwise, if there's only a sigleton intersection,
   // destMatchedOutLabel = the actual matching label.
   int multipleGState[MAXNUMDECHYPOUTLABELS] ;
   real multipleWeight[MAXNUMDECHYPOUTLABELS] ;
   int multipleOutLabel[MAXNUMDECHYPOUTLABELS] ;
   int multipleNGState ;
   
   bool destIsPushedMap ;
   // Changes Octavian 20060531
   int destMatchedOutLabel ;
  
   // Iterate all next transitions
   for ( int i = 0 ; i < nNextTrans ; i++ )  {
      // Reset the count of multiple out labels
      multipleNGState = 0 ;
      real incPushedWeight ;

      // Octavian 20060325 - add one more case
      // First case: FOLLOWON_TRANS
      // The label set of this next trans is the same as the current trans
      // No need to add weight, change status, etc
      if ( nextGState[i] == FOLLOWON_TRANS )  {
	 // Because it's a FOLLOWON_TRANS. The following are set by 
	 // findMatchedTransWithPushing()
	 commonWeight[i] = endHyp->pushedWeight ;
	 // nextOutLabel is not used.

	 destIsPushedMap = isPushedMap ;
	 // Changes Octavian 20060531
	 destMatchedOutLabel = thisMatchedOutLabel ;

	 multipleNGState = 1 ;
	 multipleGState[0] = pushedGState ;
	 multipleWeight[0] = 0.0 ;
	 multipleOutLabel[0] = WFST_EPSILON ;
      }
      else if ( nextGState[i] == UNDECIDED_TRANS )  {
	 // Second case: nextGState[i] = UNDECIDED_TRANS 
	 // i.e. non-singleton intersection
#ifdef DEBUG
	 // isPushed must be false for this to happen
	 if ( isPushedMap == true )
	    error("WFSTOnTheFlyDecoder::extendModelEndState - isPushedMap == true for UNDECIDED_TRANS") ;
	 // More checking. The transition should have no output labels
	 if ( nextTransBuf[i]->outLabel != WFST_EPSILON )
	    error("WFSTOnTheFlyDecoder::extendModelEndState - should have no outLabel being a UNDECIDED_TRANS") ;

	 // Changes Octavian 20060531
	 if ( thisMatchedOutLabel != UNDECIDED_OUTLABEL )
	    error("WFSTOnTheFlyDecoder::extendModelEndState - thisMatchedOutLabel != UNDECIDED_OUTLABEL for UNDECIDED_TRANS") ;
	 
	 // Changes Octavian 20060531
	 if ( matchedOutLabel[i] != UNDECIDED_OUTLABEL )
	    error("WFSTOnTheFlyDecoder::extendModelEndState - matchedOutputLabel != UNDECIDED_OUTLABEL for UNDECIDED_TRANS") ;
#endif
	 // Calculate the increment weight (commonWeight - pushed weight)
	 if ( isDirectAssign == true )
	    incPushedWeight = commonWeight[i] ;
	 else
	    incPushedWeight = commonWeight[i] - endHyp->pushedWeight ;

	 // Other variables concerning the next transition
	 destIsPushedMap = false ;
	 // Changes Octavian 20060531
	 destMatchedOutLabel = UNDECIDED_OUTLABEL ;
	
	 // because the G state does not change. No need to propagate G.
	 multipleNGState = 1 ;
	 multipleGState[0] = pushedGState ;
	 multipleWeight[0] = incPushedWeight ;
	 multipleOutLabel[0] = WFST_EPSILON;
      }
#ifdef DEBUG
      else if ( nextGState[i] <= DEAD_TRANS )  {
	 error("WFSTOnTheFlyDecoder::extendModelEndState - nextGState[i] <= DEAD_TRANS") ;
      }
#endif
      else if ( nextGState[i] == NOPUSHING_TRANS )  {
	 // Third case: No-pushing transitions. Appear before the final state
	 // of CoL transducer
#ifdef DEBUG
	 // isPushed must be false for this to happen
	 if ( isPushedMap == true )
	    error("WFSTOnTheFlyDecoder::extendModelEndState - isPushedMap == true for NOPUSHING_TRANS") ;
	 // Pushed weight of the endHyp must be 0.0 
	 // Because if next tran is the first NOPUSHING_TRANS, this trans must
	 // have an output label, which means the endHyp->pushedWeight has 
	 // been reset to 0.0.
	 // If this tran is a NOPUSHING_TRANS, then endHyp->pushedWeight must
	 // be also 0.0 because commonWeight for NOPUSHING_TRANS is 0.0.
	 // Because pushedWeight for NOPUSHING_TRANS is 0.0, it should be OK 
	 // when they encounter either singleton or non-sigleton trans later
	 // because the pushed weight being 0 will have no effect on the 
	 // subsequent common weight.
	 if ( commonWeight[i] != 0.0 )
	    error("WFSTOnTheFlyDecoder::extendModelEndState - commonWeight for NOPUSHING_TRANS != 0.0") ;
	 if ( endHyp->pushedWeight != 0.0 )
	    error("WFSTOnTheFlyDecoder::extendModelEndState - endHyp->pushedWeight != 0.0 for NOPUSHING_TRANS") ;
	 // More checking. The transition should have no output labels
	 if ( nextTransBuf[i]->outLabel != WFST_EPSILON )
	    error("WFSTOnTheFlyDecoder::extendModelEndState - should have no outLabel being a NOPUSHING_TRANS") ;

	 // Changes Octavian 20060531
	 if ( thisMatchedOutLabel != UNDECIDED_OUTLABEL )
	    error("WFSTOnTheFlyDecoder::extendModelEndState - thisMatchedOutLabel != UNDECIDED_OUTLABEL for NOPUSHING_TRANS") ;
	 
	 // Changes Octavian 20060531
	 if ( matchedOutLabel[i] != UNDECIDED_OUTLABEL )
	    error("WFSTOnTheFlyDecoder::extendModelEndState - matchedOutLabel != UNDECIDED_OUTLABEL for NOPUSHING_TRANS") ;
#endif
	 destIsPushedMap = false ;
	 // Changes Octavian 20060531
	 destMatchedOutLabel = UNDECIDED_OUTLABEL ;

	 multipleNGState = 1 ;
	 multipleGState[0] = pushedGState ;
	 multipleWeight[0] = 0.0 ;
	 multipleOutLabel[0] = WFST_EPSILON ;
      }
      else  {
	 // Fourth case: has singleton intersection
	 if ( isPushedMap == false )  {
	    // The endHyp is from an unpushed map. So this is the first time 
	    // encountering a singleton intersection
#ifdef DEBUG
	    // Changes Octavian 20060531
	    if ( thisMatchedOutLabel != UNDECIDED_OUTLABEL )
	       error("WFSTOnTheFlyDecoder::extendModelEndState - thisMatchedOutLabel != UNDECIDED_OUTLABEL for first-time singleton") ;

	    // Changes Octavian 20060531
	    if ( matchedOutLabel[i] == UNDECIDED_OUTLABEL )
	       error("WFSTOnTheFlyDecoder::extendModelEndState - matchedOutLabel[i] == UNDECIDED_OUTLABEL for first-time singleton") ;
#endif
	    // Calculate increment weight
	    if ( isDirectAssign == true )
	       incPushedWeight = commonWeight[i] ;
	    else
	       incPushedWeight = commonWeight[i] - endHyp->pushedWeight ;

	    // Assign other variables
	    destIsPushedMap = true ;
	    // Changes Octavian 20060531
	    destMatchedOutLabel = matchedOutLabel[i] ;

	    // Changes Octavian 20060508
	    // Changes Octavian 20060523
	    gNetwork->getStatesOnEpsPath( 
		     nextGState[i], incPushedWeight, 
		     nextOutLabel[i], multipleGState, 
		     multipleWeight, multipleOutLabel, 
		     &multipleNGState, MAXNUMDECHYPOUTLABELS ) ;
	 }
	 else  {
	    // Already pushed
#ifdef DEBUG
	    // Checking - commonWeight should be equal to the 
	    // hyp->pushedWeight
	    if ( endHyp->pushedWeight != commonWeight[i] )
	       error("WFSTOnTheFlyDecoder::extendModelEndState - pushedWeight not equal for already pushed hyp") ;

	    // Changes Octavian 20060531
	    if ( thisMatchedOutLabel == UNDECIDED_OUTLABEL )
	       error("WFSTOnTheFlyDecoder::extendModelEndState - thisMatchedOutLabel == UNDECIDED_OUTLABEL for already-pushed trans") ;

	    // Changes Octavian 20060531
	    if ( thisMatchedOutLabel != matchedOutLabel[i] )
	       error("WFSTOnTheFlyDecoder::extendModelEndState - thisMatchedOutLabel != matchedOutLabel[i] for already-pushed trans") ;
#endif
	    destIsPushedMap = true ;
	    // Changes Octavian 20060531
	    destMatchedOutLabel = thisMatchedOutLabel ;

	    multipleNGState = 1 ;
	    multipleGState[0] = pushedGState ;
	    multipleWeight[0] = 0.0 ;
	    multipleOutLabel[0] = WFST_EPSILON ;
	 }
      }
      // End of the four cases (follow-on, singleton, no-pushing and 
      // non-singleton)
     
      // Because some output labels are epsilon, they cannot be stored inside
      // the hyp.Otherwise it will register additional epsilon labels when 
      // word-end marker is encountered.
      // Need to clean up the multipleOutLabel array
      int multipleOutLabelWithNoEPS[MAXNUMDECHYPOUTLABELS] ;
      int multipleNOutLabel = 0 ;
      
      // Propagation starts here
      // First case - if next transitions has no model
      if (  (nextTransBuf[i]->inLabel == WFST_EPSILON) || 
	    (nextTransBuf[i]->inLabel == network->getWordEndMarker()) )  {

	 real accWeight = 0.0 ;
	 for ( int j = 0 ; j < multipleNGState ; j++ )  {
	    accWeight += multipleWeight[j] ;
	    
	    // Check if the current outLabel is epsilon. If not, register 
	    // in a new array. Filter out the epsilon labels.
	    if ( multipleOutLabel[j] != WFST_EPSILON )  {
	       multipleOutLabelWithNoEPS[multipleNOutLabel] = multipleOutLabel[j] ;
	       multipleNOutLabel++ ;
	    }
	    
	    DecHypOnTheFly tmp ;
	    tmp.hist = NULL ;

	    decHypHistPool->extendDecHypOnTheFly(
		  endHyp, &tmp, 
		  endHyp->score + accWeight + nextTransBuf[i]->weight, 
		  endHyp->acousticScore, 
		  endHyp->lmScore + accWeight + nextTransBuf[i]->weight, 
		  endHyp->currGState, commonWeight[i], 
		  multipleOutLabelWithNoEPS, multipleNOutLabel ) ;
	    
	    // Changes Octavian 20060531
	    extendModelEndState( 
		  &tmp, nextTransBuf[i], NULL, destIsPushedMap, 
		  multipleGState[j], destMatchedOutLabel ) ;
	    
	    // Changes
	    //decHypHistPool->resetDecHypOnTheFly( &tmp ) ;
	    resetDecHyp( &tmp ) ;
	 }
      }
      else  {
	 // Second case - has actually a model
	 WFSTModelOnTheFly *nextModel = getModel( nextTransBuf[i] ) ;
	 real accWeight = 0.0 ;
	 
	 for ( int j = 0 ; j < multipleNGState ; j++ )  {
	    accWeight += multipleWeight[j] ;
	    
	    // Check if the current outLabel is epsilon. 
	    // If not, register in a new array. Filter out epsilon labels.
	    if ( multipleOutLabel[j] != WFST_EPSILON )  {
	       multipleOutLabelWithNoEPS[multipleNOutLabel] = multipleOutLabel[j] ;
	       multipleNOutLabel++ ;
	    }
	    
	    real newScore = endHyp->score + accWeight + nextTransBuf[i]->weight ;

	    DecHypOnTheFly *destHyp ;
	    // Changes Octavian 20060627
	    // DecHypOnTheFly **destHypInMap ;
	    bool isNew ;
	   
	    if ( destIsPushedMap == false )  {
	       // Changes Octavian 20060531
	       // Changes Octavian 20060627
	       /*
	       destHypInMap = nextModel->findInitStateHyp( 
		     nextModel->initMapNotPushed, false, 
		     multipleGState[j], destMatchedOutLabel, &isNew ) ;
		     */
	       destHyp = nextModel->findInitStateHyp(
		     nextModel->initMapNotPushed, false, 
		     multipleGState[j], destMatchedOutLabel, &isNew ) ;
	    }
	    else  {
	       // Changes Octavian 20060531
	       // Changes Octavian 20060627
	       /*
	       destHypInMap = nextModel->findInitStateHyp( 
		     nextModel->initMapPushed, true, 
		     multipleGState[j], destMatchedOutLabel, &isNew ) ;
		     */
	       destHyp = nextModel->findInitStateHyp( 
		     nextModel->initMapPushed, true, 
		     multipleGState[j], destMatchedOutLabel, &isNew ) ;
	    }

	    // Changes Octavian 20060627
	    /*
	    if ( isNew == false )
	       destHyp = *destHypInMap ;
	    else
	       destHyp = NULL ;
	    */
	   
	    // Changes Octavian 20060627
	    /*
	    if ( destHyp != NULL )  {
	    */
	    if ( isNew == false )  {
	       // Found the hyp
#ifdef DEBUG
	       if ( destHyp->score <= LOG_ZERO )
		  error("WFSTOnTheFlyDecoder::extendModelEndState - destHyp->score <= LOG_ZERO") ;
#endif
	       if ( newScore <= destHyp->score )
		  continue ;
	    }
	    else  {
	       // Not found
	       // Get a piece of memory for a hyp from pool
	       // Changes Octavian 20060627
	       // destHyp = (DecHypOnTheFly*) decHypPool->getSingleHyp() ;
	       // *destHypInMap = destHyp ;
	       
	       nextModel->nActiveHyps++ ;
	    }

	    decHypHistPool->extendDecHypOnTheFly(
		  endHyp, destHyp, newScore, endHyp->acousticScore, 
		  endHyp->lmScore + accWeight + nextTransBuf[i]->weight,
		  endHyp->currGState, commonWeight[i], 
		  multipleOutLabelWithNoEPS, multipleNOutLabel ) ;
	    
	 }
      }
   }
   if ( ownNextTransBuf )
      delete [] nextTransBuf ;

   delete [] commonWeight ;
   delete [] nextGState ;
   delete [] nextOutLabel ;
   // Changes Octavian 20060531
   delete [] matchedOutLabel ;
#endif

}

// Changes Octavian 20060523
void WFSTOnTheFlyDecoder::addLabelHist( DecHyp* hyp , int label )
{
   decHypHistPool->addLabelHistToDecHypOnTheFly( (DecHypOnTheFly *)hyp , label ) ;
}

// Similar to the base class function except when calling lattice->addEntry(), it uses a different 
// version. 
int WFSTOnTheFlyDecoder::addLatticeEntry( DecHyp *hyp_ , WFSTTransition *trans , int *fromState )
{
   // Changes Octavian 20060508
   DecHypOnTheFly *hyp = (DecHypOnTheFly *) hyp_ ;
   
   if ( hyp == NULL )
      error("WFSTOnTheFlyDecoder::addLatticeEntry - hyp is not DecHypOnTheFly") ;
   
   if ( doLatticeGeneration == false )
      error("WFSTOnTheFlyDecoder::addLatticeEntry - doLatticeGeneration == false") ;
      
   // The first element in the history linked list for endHyp should be a lattice
   //   entry containing the lattice WFST state we want to use as the from state.
   if ( (hyp->hist == NULL) || (hyp->hist->type != LATTICEDHHTYPE) )
      error("WFSTOnTheFlyDecoder::addLatticeEntry - no lattice history found") ;

   LatticeDecHypHist *hist = (LatticeDecHypHist *)(hyp->hist) ;

   // Changes Octavian 20060823
#ifdef ACOUSTIC_SCORE_ONLY
   real newScore = hyp->acousticScore - hist->accScore ;
#else
   real newScore = hyp->acousticScore + hyp->lmScore - hist->accScore ;
#endif
   
   int toState ;

#ifdef DEBUG
   if ( hyp->nNextOutLabel < 0 )
      error("WFSTOnTheFlyDecoder::addLatticeEntry - nNextOutLabel < 0") ;
   if (( trans == NULL ) && ( currFrame != 0 ))
      error("WFSTOnTheFlyDecoder::addLatticeEntry - trans == NULL but currFrame != 0") ;
#endif
   
   // Changes
   if ( hyp->nNextOutLabel == 0 )  {
      // No output label. Put epsilon to output
      int tmpLabel = WFST_EPSILON ;
      if ( trans != NULL )  {
	 toState = lattice->addEntry( hist->latState , trans->toState , hyp->currGState, trans->inLabel , 
	       &tmpLabel , 1 , newScore ) ;
      }
      else  {
	 // Before propagating to the CoL transducer, there are already some 
	 // outLabels and/or back-off weights from the G transducer
	 toState = lattice->addEntry( hist->latState , network->getInitState() , hyp->currGState, 
	       WFST_EPSILON , &tmpLabel , 1 , newScore ) ;
      }
   }
   else  {
      if ( trans != NULL )  {
	 toState = lattice->addEntry( hist->latState , trans->toState , hyp->currGState, trans->inLabel , 
	       hyp->nextOutLabel , hyp->nNextOutLabel , newScore ) ;
      }
      else  {
	 // Before propagating to the CoL transducer, there are already some outLabels and/or back-off
	 // weights from the G transducer
	 toState = lattice->addEntry( hist->latState , network->getInitState() , hyp->currGState, 
	       WFST_EPSILON , hyp->nextOutLabel , hyp->nNextOutLabel , newScore ) ;
      }
   }
   /*
   toState = lattice->addEntry( hist->latState , trans->toState , trans->inLabel , trans->outLabel , 
   newScore ) ;
   */
                                
   if ( fromState != NULL )
      *fromState = hist->latState ;

   // Remove the lattice history that we no longer need
   hyp->hist = hist->prev ;
   if ( --(hist->nConnect) == 0 )
   {
      decHypHistPool->returnSingleElem( (DecHypHist *)hist ) ;
      // the new hyp->hist->nConnect does not change
   }
   else if ( hyp->hist != NULL )
   {
      hyp->hist->nConnect++ ;
   }

   return toState ;

}
	 
// Notice only "next" and "nextGState" is assigned to values when 
// nextGState[i] is FOLLOWON_TRANS.
// commonWeights, nextOutLabel and matchedOutLabel are set by the caller of
// this function .
// Changes Octavian 20060531
// Changes Octavian 20060726 
// - nonRegisteredLabels = number of output labels pending for word-end
// marker
// - isPushed = true if the output label is pushed and the hyp is still in the
// prefix region (ie before the arc having the actual output labels)
// Changes Octavian 20060828
#ifndef CASEIRO
void WFSTOnTheFlyDecoder::findMatchedTransWithPushing( WFSTTransition *prev, int *nNext, WFSTTransition **next, int gState, real *commonWeights, int *nextGState, int *nextOutLabel, int *matchedOutLabel )
#else
void WFSTOnTheFlyDecoder::findMatchedTransWithPushing( WFSTTransition *prev, int *nNext, WFSTTransition **next, int gState, real *commonWeights, int *nextGState, int *nextOutLabel, int *matchedOutLabel, int nonRegisteredLabels, bool isPushed )
#endif
{
   
#ifdef DEBUG
   if ( nNext == NULL )
      error("WFSTOnTheFlyDecoder::findMatchedTransWithPushing - nNext == NULL");
   if ( next == NULL )
      error("WFSTOnTheFlyDecoder::findMatchedTransWithPushing - next == NULL") ;
   if (( gState < 0 ) || ( gState >= gNetwork->getNumStates() ))
      error("WFSTOnTheFlyDecoder::findMatchedTransWithPushing - gState < 0") ;
   if ( commonWeights == NULL )
      error("WFSTOnTheFlyDecoder::findMatchedTransWithPushing - commonWeights == NULL") ;
   if ( nextGState == NULL )
      error("WFSTOnTheFlyDecoder::findMatchedTransWithPushing - nextGState == NULL") ;
   if ( nextOutLabel == NULL )
      error("WFSTOnTheFlyDecoder::findMatchedTransWithPushing - nextOutLabel == NULL") ;
   // Changes Octavian 20060531
   if ( matchedOutLabel == NULL )
      error("WFSTOnTheFlyDecoder::findMatchedTransWithPushing - matchedOutLabel== NULL") ;
#endif

#ifdef CASEIRO
   // Changes Octavian 20060726
   if ( nonRegisteredLabels > 1 )
      error("WFSTOnTheFlyDecoder::findMatchedTransWithPushing - nonRegisteredLabels > 1") ;
   bool isSuffix = (( nonRegisteredLabels == 1 ) && ( isPushed == false )) ;
#endif
   
   // Get an array of indices of next transitions from the C o L transducer
   int nTransInCL ;
   const int *transInCLNetwork = network->getTransitions( prev , &nTransInCL ) ;
   
   // Get the number of transitons in the G transducer
   int nTransInG = gNetwork->getNumTransitionsOfOneState( gState ) ;

   // For each transition in the C o L transducer
   // Get its LabelSet (ie tagging list)
   // Find the intersection of the LabelSet with the input labels of the 
   // G transducer
  
   *nNext = 0 ;

#ifdef DEBUG
   if ( nTransInG < 0 )
      error("WFSTOnTheFlyDecoder::findMatchedTransWithPushing - nTransInG < 0") ;
#endif
  
   // Changes Octavian 20060523
   // if ( nTransInG == 0 )
   // return ;
      
   // Changes Octavian 20060325
   WFSTLabelPushingNetwork *networkOTF = dynamic_cast<WFSTLabelPushingNetwork *>( network ) ;
   if ( networkOTF == NULL )
      error("WFSTOnTheFlyDecoder::findMatchedTransWithPushing - network is not on-the-fly network") ;
   const WFSTLabelPushingNetwork::LabelSet *labelSetPrevTrans ;
   if ( prev == NULL )
      labelSetPrevTrans = NULL ;
   else
      labelSetPrevTrans = networkOTF->getOneLabelSet( prev->id ) ;

   for ( int i = 0 ; i < nTransInCL ; i++ )  {
      // Changes Octavian 20060726
#ifdef CASEIRO
      if ( isSuffix )  {
	 next[*nNext] = network->getOneTransition( transInCLNetwork[i] ) ;
	 nextGState[*nNext] = FOLLOWON_TRANS ;
	 (*nNext)++ ;
	 continue ;
      }
#endif
      // Changes Octavian 20060325
      // First, only search for common weights if the label set are different
      const WFSTLabelPushingNetwork::LabelSet *labelSet = networkOTF->getOneLabelSet(transInCLNetwork[i]) ;

      // Changes Octavian 20060726
#ifndef CASEIRO
      if ( ( prev != NULL ) && ( prev->outLabel == WFST_EPSILON ) && ( labelSet == labelSetPrevTrans ) )  {
#else
      if ( ( prev != NULL ) && ( prev->inLabel != network->getWordEndMarker() ) && ( labelSet == labelSetPrevTrans ) )  {
#endif
	 // Notice only "next" and "nextGState" is assigned to values. 
	 next[*nNext] = network->getOneTransition( transInCLNetwork[i] ) ;
	 nextGState[*nNext] = FOLLOWON_TRANS ;
	 (*nNext)++ ;
	 continue ;
      }
      
      real tmpWeight = 0.0 ;
      int tmpNextGState = -1 ;
      // Change
      int tmpNextOutLabel = -1 ;
      // Changes Octavian 20060531
      int tmpMatchedOutLabel = -1 ;

      // Second, go to the cache if there's a record
      if ( doPushingWeightCache )  {
	 // Label and weight pushing - dont care the third argument
	 // Changes Octavian 20060531
	 bool isFound = pushingWeightCache->findPushingWeight(
	       transInCLNetwork[i], gState, WFST_EPSILON, 
	       &tmpWeight, &tmpNextGState, &tmpNextOutLabel,
	       &tmpMatchedOutLabel ) ;

	 if ( isFound )  {
#ifdef DEBUG
	    if ( tmpNextGState < DEAD_TRANS )
	       error("WFSTOnTheFlyDecoder::findMatchedTransWithPushing - tmpNextGState out of range") ;
#endif
	    if ( tmpNextGState != DEAD_TRANS )  {
	       next[*nNext] = network->getOneTransition( transInCLNetwork[i] ) ;
	       commonWeights[*nNext] = tmpWeight ;
	       nextGState[*nNext] = tmpNextGState ;
	       nextOutLabel[*nNext] = tmpNextOutLabel ;
	       // Changes Octavian 20060531
	       matchedOutLabel[*nNext] = tmpMatchedOutLabel ;
	       (*nNext)++ ;
	    }
	    continue ;   
	 }
      }

      // Third, check if the label set is a non-pushing label set.
      // If labelSet has a NONPUSHING_OUTLABEL, it means this CoL transition 
      // cannot be label-pushed. This kind of transitions appears before the 
      // final state(s) of the CoL transducer, as we don't want weight pushing
      // for hypotheses which are going towards the final state(s). The reason
      // we don't want to do weight pushing before a final state is because 
      // we don't want to push the weights which are supposed to be after the
      // final state.
      if ( labelSet->find( NONPUSHING_OUTLABEL ) != labelSet->end() )  {
#ifdef DEBUG
	 if ( labelSet->size() != 1 )
	    error("WFSTOnTheFlyDecoder::findMatchedTransWithPushing - size != 1 but NONPUSHING_OUTLABEL is found") ;
#endif

	 next[*nNext] = network->getOneTransition( transInCLNetwork[i] ) ;
	 commonWeights[*nNext] = 0.0 ;
	 nextGState[*nNext] = NOPUSHING_TRANS ;
	 nextOutLabel[*nNext] = UNDECIDED_OUTLABEL ;
	 // Changes Octavian 20060531
	 matchedOutLabel[*nNext] = UNDECIDED_OUTLABEL ;
	 (*nNext)++ ;
	 
	 if ( doPushingWeightCache )  {
	    // Changes 20060325 - Label pushing. 
	    // Don't care about the third argument
	    // Changes Octavian 20060531
	    pushingWeightCache->insertPushingWeight(
		  transInCLNetwork[i], gState, WFST_EPSILON, 0.0, 
		  NOPUSHING_TRANS, UNDECIDED_OUTLABEL, UNDECIDED_OUTLABEL ) ;
	 }
	 continue ;
      }

      // Changes Octavian 20060523
      // Fourth, here it means the next C o L transition is neither
      // a FOLLOWON_TRANS nor a NOPUSHING_TRANS.
      // In addition, we have no record about it in the cache.
      // We need to find (look-ahead) the details of the next transition
      bool hasIntersection ;
      if ( nTransInG != 0 )  {
	 // Changes Octavian 20060531
	 hasIntersection = findIntersection( 
	       labelSet, gState, nTransInG, &tmpWeight, &tmpNextGState, 
	       &tmpNextOutLabel, &tmpMatchedOutLabel ) ;
      }
      else  {
	 hasIntersection = false ;
      }
     
      // Here we have the result of lookahead. Cache it if we have a cache.
      if ( doPushingWeightCache )  {
	 if ( hasIntersection )  {
	    // Changes 20060325 - Label pushing. 
	    // Don't care about the third argument
	    // Changes Octavian 20060531
	    pushingWeightCache->insertPushingWeight(
		  transInCLNetwork[i], gState, WFST_EPSILON, 
		  tmpWeight, tmpNextGState, tmpNextOutLabel, 
		  tmpMatchedOutLabel ) ;
	 }
	 else  {
	    // Changes 20060325 - Label pushing - 
	    // Don't care about the third argrument
	    // Changes Octavian 20060531
	    pushingWeightCache->insertPushingWeight(
		  transInCLNetwork[i], gState, WFST_EPSILON, 
		  0.0, DEAD_TRANS, UNDECIDED_OUTLABEL, UNDECIDED_OUTLABEL ) ;
	 }
      }

      // Finally, if we have an intersection (ie at least one intersection),
      // output the relevant infomation to appropriate array.
      if ( hasIntersection )  {
	 next[*nNext] = network->getOneTransition( transInCLNetwork[i] ) ;
	 commonWeights[*nNext] = tmpWeight ;
	 nextGState[*nNext] = tmpNextGState ;
	 nextOutLabel[*nNext] = tmpNextOutLabel ;
	 // Changes Octavian 20060531
	 matchedOutLabel[*nNext] = tmpMatchedOutLabel ;
	 (*nNext)++ ;
      }      
   }
}


// Changes Octavian 20060523
// Changes Octavian 20060531
void WFSTOnTheFlyDecoder::findMatchedTransNoPushing( WFSTTransition *prev, int *nNext, WFSTTransition **next, int gState, real *commonWeights, int *nextGState, int *nextOutLabel, int *matchedOutLabel )
{
#ifdef DEBUG
   if ( nNext == NULL )
      error("WFSTOnTheFlyDecoder::findMatchedTransNoPushing - nNext == NULL");
   if ( next == NULL )
      error("WFSTOnTheFlyDecoder::findMatchedTransNoPushing - next == NULL") ;
   if (( gState < 0 ) || ( gState >= gNetwork->getNumStates() ))
      error("WFSTOnTheFlyDecoder::findMatchedTransNoPushing - gState < 0") ;
   if ( commonWeights == NULL )
      error("WFSTOnTheFlyDecoder::findMatchedTransNoPushing - commonWeights == NULL") ;
   if ( nextGState == NULL )
      error("WFSTOnTheFlyDecoder::findMatchedTransNoPushing - nextGState == NULL") ;
   if ( nextOutLabel == NULL )
      error("WFSTOnTheFlyDecoder::findMatchedTransNoPushing - nextOutLabel == NULL") ;
   // Changes Octavian 20060531
   if ( matchedOutLabel == NULL )
      error("WFSTOnTheFlyDecoder::findMatchedTransNoPushing - matchedOutLabel == NULL") ;
#endif
   
   // Get an array of indices of next transitions from the C o L transducer
   int nTransInCL ;
   const int *transInCLNetwork = network->getTransitions( prev , &nTransInCL ) ;
   
   // Get the number of transitons in the G transducer
   int nTransInG = gNetwork->getNumTransitionsOfOneState( gState ) ;

   // For each transition in the C o L transducer
   // Get its LabelSet (ie tagging list)
   // Find the intersection of the LabelSet with the input labels of the 
   // G transducer
  
   *nNext = 0 ;

#ifdef DEBUG
   if ( nTransInG < 0 )
      error("WFSTOnTheFlyDecoder::findMatchedTransNoPushing - nTransInG < 0") ;
#endif
  
   // Changes Octavian 20060523
   // if ( nTransInG == 0 )
   // return ;

   for ( int i = 0 ; i < nTransInCL ; i++ )  {
      WFSTTransition *nextPotentialCLTrans = network->getOneTransition( transInCLNetwork[i] ) ;
      int outLabelCL = nextPotentialCLTrans->outLabel ;

      // Check whether if the output label is eps or not.
      // If eps, free entry.
      // If non-eps, see if there's a match between this outLabel with one
      // of the input labels in the G transducer.
      if ( outLabelCL == WFST_EPSILON )  {
	 // First case: eps output label = free entry
	 // No LM weight to be added
	 next[*nNext] = nextPotentialCLTrans ;
	 commonWeights[*nNext] = 0.0 ;
	 nextGState[*nNext] = UNDECIDED_TRANS ;
	 nextOutLabel[*nNext] = UNDECIDED_OUTLABEL ;
	 // Changes Octavian 20060531
	 matchedOutLabel[*nNext] = UNDECIDED_OUTLABEL ;
	 (*nNext)++ ;
      }
      else  {
	 // Second case: non-eps output label = Check if there's a match.
	 // Changes Octavian 20060523
	 // If the next C o L trans has an output label, but 
	 // the current G state does not have any transitions,
	 // no need to propagate further.
	 if ( nTransInG == 0 )
	    continue ;
	 
	 // Has an output label on the CoL transition
	 // Check if it is a valid output label for the current G state 
	 real tmpWeight = 0.0 ;
	 int tmpNextGState = -1 ;
	 int tmpClosestTransInG = -1 ;
	 int tmpNextOutLabel = -1 ;
	 // Changes Octavian 20060531
	 int tmpMatchedOutLabel = -1 ;

	 // First, use cache if allowed
	 if ( doPushingWeightCache )  {
	    // Changes 20060325
	    // Changes Octavian 20060531
	    bool isFoundInCache = pushingWeightCache->findPushingWeight( 
		  transInCLNetwork[i], gState, outLabelCL, &tmpWeight, 
		  &tmpNextGState, &tmpNextOutLabel, &tmpMatchedOutLabel ) ;

	    if ( isFoundInCache )  {
#ifdef DEBUG
	       if ( tmpNextGState < DEAD_TRANS )
		  error("WFSTOnTheFlyDecoder::findMatchedTransNoPushing - tmpNextGState out of range") ;
#endif
	       if ( tmpNextGState != DEAD_TRANS )  {
		  next[*nNext] = nextPotentialCLTrans ;
		  commonWeights[*nNext] = tmpWeight ;
		  nextGState[*nNext] = tmpNextGState ;
		  nextOutLabel[*nNext] = tmpNextOutLabel ;
		  // Changes Octavian 20060531
		  matchedOutLabel[*nNext] = tmpMatchedOutLabel ;
		  (*nNext)++ ;
	       }
	       continue ;
	    }
	 }

	 // Second, if not found in cache, do actual matching.
	 bool isFound = binarySearchGTrans( 
	       outLabelCL, gState, 0, nTransInG, &tmpWeight, &tmpNextGState,
	       &tmpClosestTransInG, &tmpNextOutLabel ) ;

	 // Changes Octavian 20060531
	 if ( isFound )
	    tmpMatchedOutLabel = outLabelCL ;
	 // Maybe we don't need the else statement
	 /*
	 else
	    tmpNextOutLabel = UNDECIDED_OUTLABEL ;
	 */


	 // Register to cache if allowed
	 if ( doPushingWeightCache )  {
	    if ( isFound )  {
	       // Changes 20060325
	       // Changes Octavian 20060531
	       pushingWeightCache->insertPushingWeight(
		     transInCLNetwork[i], gState, outLabelCL, 
		     tmpWeight, tmpNextGState, tmpNextOutLabel,
		     tmpMatchedOutLabel ) ;
	    }
	    else  {
	       // Changes 20060325
	       // Changes Octavian 20060531
	       pushingWeightCache->insertPushingWeight(
		     transInCLNetwork[i], gState, outLabelCL, 
		     0.0, DEAD_TRANS, UNDECIDED_OUTLABEL, 
		     UNDECIDED_OUTLABEL ) ;
	    }
	 }

	 // Finally, if there's a match, put the result in the output array
	 if ( isFound )  {
	    next[*nNext] = nextPotentialCLTrans ;
	    commonWeights[*nNext] = tmpWeight ;
	    nextGState[*nNext] = tmpNextGState ;
	    nextOutLabel[*nNext] = tmpNextOutLabel ;
	    // Changes Octavian 20060531
	    matchedOutLabel[*nNext] = tmpMatchedOutLabel ;
	    (*nNext)++ ;
	 }
      }
   }
}


// Changes Octavian 20060531
bool WFSTOnTheFlyDecoder::findIntersection( const WFSTLabelPushingNetwork::LabelSet *labelSet, const int gState, const int totalNTransInG , real *commonWeight, int *nextGState, int *nextOutLabel, int *matchedOutLabel )
{
   int numMatched = 0 ;
   WFSTLabelPushingNetwork::LabelSet::iterator labelIterInCL ;
   int startTransInG = 0 ;
   int nTransInG = totalNTransInG ;
   int closestTransInG = 0 ;
   bool isFound ;

   real tmpWeight = 0.0 ;
   int tmpNextGState = -1 ;
   int tmpNextOutLabel = -1 ;
   // Changes Octavian 20060531
   int tmpMatchedOutLabel = -1 ;

   // Changes Octavian 20060328
   // real minWeight = 0.0 ;
   // Changes Octavian 20060810
#ifndef LOG_ADD_LOOKAHEAD
   real maxWeight = LOG_ZERO ;
#else
   real logSumWeight = LOG_ZERO ;
#endif

#ifdef DEBUG
   if ( labelSet->empty() )
      error("WFSTOnTheFlyDecoder::findIntersection - labelSet is empty") ;
   int tmpInLabel ;
#endif
 
   // *****************************
   // Changes Octavian 20060616
   /*
   for ( labelIterInCL = labelSet->begin() ; labelIterInCL != labelSet->end() ; labelIterInCL++ )  {
   */
   labelIterInCL = labelSet->begin() ;
   WFSTLabelPushingNetwork::LabelSet::iterator endIter = labelSet->end() ;
   while ( labelIterInCL != endIter )  { 
#ifdef DEBUG
      if ( labelIterInCL == labelSet->begin() )  {
	 tmpInLabel = *labelIterInCL ;
      }
      else  {
	 if ( *labelIterInCL <= tmpInLabel )
	    error("WFSTOnTheFlyDecoder::findIntersection - labelSet is not in ascending order") ;
	 tmpInLabel = *labelIterInCL ;
      }

      if ( (*labelIterInCL) <= 0 )
	 error("WFSTOnTheFlyDecoder::findIntersection - inLabel <= 0") ;
#endif
      // *******************************

      // For each label in the set, try to find a match in G.
      isFound = binarySearchGTrans( 
	    *labelIterInCL, gState, startTransInG, nTransInG, &tmpWeight, 
	    &tmpNextGState, &closestTransInG, &tmpNextOutLabel ) ;

      if ( isFound )  {
	 if ( numMatched == 0 )  {
	    // Changes Octavian 20060328
	    //minWeight = tmpWeight ;
	    // Changes Octavian 20060810
#ifndef LOG_ADD_LOOKAHEAD
	    maxWeight = tmpWeight ;
#else
	    logSumWeight = tmpWeight ;
#endif
	 }
	 else  {
	    // Changes Octavian 20060328
	    // Changes Octavian 20060810
#ifndef LOG_ADD_LOOKAHEAD
	    if ( tmpWeight > maxWeight )
	       maxWeight = tmpWeight ;
#else
	    logSumWeight = logAdd( logSumWeight, tmpWeight ) ;
#endif
	    /*
	    if ( tmpWeight < minWeight )
	       minWeight = tmpWeight ;
	    */
	 }
	 // Changes Octavian 20060531
	 tmpMatchedOutLabel = *labelIterInCL ;
	 numMatched++ ;
      }

      startTransInG = closestTransInG + 1 ;
      // ************
      // Changes Octavian 20060617
      //nTransInG = totalNTransInG - startTransInG ;
      // ************
      if ( startTransInG >= totalNTransInG )
	 break ;

      // ***********************
      // Changes Octavian 20060616 20060617
      int gInLabel = gNetwork->getInLabelOfOneTransition( 
	    gState, startTransInG ) ;
      labelIterInCL = labelSet->lower_bound( gInLabel ) ;
      if ( *labelIterInCL == gInLabel )  {
	 gNetwork->getInfoOfOneTransition( 
	       gState, startTransInG, &tmpWeight, &tmpNextGState, 
	       &tmpNextOutLabel ) ;
	 
	 if ( numMatched == 0 )  {
	    // Changes Octavian 20060328
	    //minWeight = tmpWeight ;
	    // Changes Octavian 20060810
#ifndef LOG_ADD_LOOKAHEAD
	    maxWeight = tmpWeight ;
#else
	    logSumWeight = tmpWeight ;
#endif
	 }
	 else  {
	    // Changes Octavian 20060328
	    // Changes Octavian 20060810
#ifndef LOG_ADD_LOOKAHEAD
	    if ( tmpWeight > maxWeight )
	       maxWeight = tmpWeight ;
#else
	    logSumWeight = logAdd( logSumWeight, tmpWeight ) ;
#endif
	    /*
	    if ( tmpWeight < minWeight )
	       minWeight = tmpWeight ;
	    */
	 }
	 // Changes Octavian 20060531
	 tmpMatchedOutLabel = *labelIterInCL ;
	 numMatched++ ;

	 // Changes Octavian 20060617
	 labelIterInCL++ ;
      }
      // Changes Octavian 20060617
      startTransInG++ ;
      nTransInG = totalNTransInG - startTransInG ;
      if ( startTransInG >= totalNTransInG )
	 break ;
      // ************************
   }

   bool returnStatus ;

   if ( numMatched == 0 )  {
      *commonWeight = 0.0 ;
      *nextGState = DEAD_TRANS ;
      *nextOutLabel = UNDECIDED_OUTLABEL ;
      // Changes Octavian 20060531
      *matchedOutLabel = UNDECIDED_OUTLABEL ;
      returnStatus = false ;
      // return false ;
   }
#ifdef DEBUG
   else if ( numMatched < 0 )  {
      error("WFSTOnTheFlyDecoder::findIntersection - numMatched < 0") ;
   }
#endif
   else if ( numMatched == 1 )  {
      // Changes Octavian 20060328
      // *commonWeight = minWeight ;
      // Changes Octavian 20060810
#ifndef LOG_ADD_LOOKAHEAD
      *commonWeight = maxWeight ;
#else
      *commonWeight = logSumWeight ;
#endif
      *nextGState = tmpNextGState ;
      *nextOutLabel = tmpNextOutLabel ;
      // Changes Octavian 20060531
      *matchedOutLabel = tmpMatchedOutLabel ;
      returnStatus = true ;
      // return true ;
   }
   else  {
      // Changes Octavian 20060328
      // *commonWeight = minWeight ;
      // Changes Octavian 20060810
#ifndef LOG_ADD_LOOKAHEAD
      *commonWeight = maxWeight ;
#else
      *commonWeight = logSumWeight ;
#endif
      *nextGState = UNDECIDED_TRANS ;
      *nextOutLabel = UNDECIDED_OUTLABEL ;
      // Changes Octavian 20060531
      *matchedOutLabel = UNDECIDED_OUTLABEL ;
      returnStatus = true ;
      // return true ;
   }
   return returnStatus ;
}


bool WFSTOnTheFlyDecoder::binarySearchGTrans( const int outLabelInCL, const int gState, const int startTransInG, const int nTransInG, real *weight, int *nextGState, int *closestTransInG, int *nextOutLabel )
{
   real weightInG ;
   int toState ;
   int inLabelInG ;
   int outLabelInG ;
   
   // Terminating case
   if ( nTransInG == 1 )  {
      inLabelInG = gNetwork->getInfoOfOneTransition( gState, startTransInG, &weightInG, &toState, &outLabelInG ) ;

      if ( outLabelInCL == inLabelInG )  {
	 *weight = weightInG ;
	 *nextGState = toState ;
	 *closestTransInG = startTransInG ;
	 *nextOutLabel = outLabelInG ;
	 return true ;
      }
      else if ( outLabelInCL < inLabelInG )  {
	 *closestTransInG = startTransInG - 1;
	 return false ;
      }
      else  {
	 *closestTransInG = startTransInG  ;
	 return false ;
      }
   }

   int middle = nTransInG / 2 ;
   inLabelInG = gNetwork->getInfoOfOneTransition( gState, startTransInG + middle , &weightInG, &toState, &outLabelInG ) ;

   if ( outLabelInCL == inLabelInG )  {
      *weight = weightInG ;
      *nextGState = toState ;
      *closestTransInG = startTransInG + middle ;
      *nextOutLabel = outLabelInG ;
      return true ;
   }
   else if ( outLabelInCL < inLabelInG )  {
      return binarySearchGTrans( outLabelInCL, gState, startTransInG, middle, weight, nextGState, closestTransInG, nextOutLabel ) ;
   }
   else  {

      if ( nTransInG == 2 )  {
	 *closestTransInG = startTransInG + middle ;
	 return false ;
      }
      else  {
	 return binarySearchGTrans( outLabelInCL, gState, startTransInG + middle + 1, nTransInG - middle - 1 , weight, nextGState, closestTransInG, nextOutLabel ) ;
      }

   }

}


// Pushing weight cache implementation
// Changes Octavian 20060415
WFSTOnTheFlyDecoder::PushingWeightCache::PushingWeightCache()
{
   // Changes Octavian 20060415
   recordPool = NULL ;

   recordHead = NULL ;
   recordTail = NULL ;
   
   // Changes Octavian 20060415
   bucketArray = NULL ;
   mapArray = NULL ;
   nBucket = 0 ;
   nBucketMinusOne = -1 ;
  
   // Changes Octavian 20060325
   numEffCoLTrans = 0 ;
   transLookup = NULL ;
   
   numCoLTrans = 0 ;
   numGStates = 0 ;
   maxRecords = 0 ;
   currRecords = 0 ;
}

WFSTOnTheFlyDecoder::PushingWeightCache::PushingWeightCache( int numCoLTrans_, int numGStates_, const WFSTLabelPushingNetwork::LabelSet **labelSetArray_ , int numCoLOutSyms_ , int nBucket_ )
{
#ifdef DEBUG
   if ( numCoLTrans_ <= 0 )
      error("PushingWeightCache::PushingWeightCache (2) - numCoLTrans <= 0") ;
#endif
   
   numCoLTrans = numCoLTrans_ ;
   numGStates = numGStates_ ;
   
   // Changes 20060325
   // Changes 20060415
   if ( labelSetArray_ == NULL )  {
      // No label and weight pushing
      // Effective CoL transitions will be the same as the number of CoL out 
      // symbols
      numEffCoLTrans = numCoLOutSyms_ ;
      transLookup = NULL ;
   }
   else  {
      // Has label and weight pushing
      transLookup = new int[numCoLTrans] ;
      map< const WFSTLabelPushingNetwork::LabelSet* , int > labelToTransMap ;
      numEffCoLTrans = 0 ;
      
      for ( int i = 0 ; i < numCoLTrans ; i++ )  {
#ifdef DEBUG
	 if ( labelSetArray_[i] == NULL )
	    error("PushingWeightCache::PushingWeightCache (2) - labelSetArray[i] == NULL ") ;
#endif
	 pair< map<const WFSTLabelPushingNetwork::LabelSet*, int>::iterator , bool > status = 
	    labelToTransMap.insert( pair< const WFSTLabelPushingNetwork::LabelSet*, int>( labelSetArray_[i], i ) ) ;

	 if ( status.second == true )  {
	    transLookup[i] = numEffCoLTrans ; 
	    numEffCoLTrans++ ;
	 }
	 else  {
	    transLookup[i] = transLookup[(*(status.first)).second] ;
	 }
      }
   }
   
   // Changes 20060415
   nBucket = calNBucket( nBucket_ ) ;
   nBucketMinusOne = nBucket - 1 ;
   bucketArray = new PushingWeightRecord*[ numEffCoLTrans * nBucket ] ;
   for ( int i = 0 ; i < (numEffCoLTrans*nBucket) ; i++ )  {
      bucketArray[i] = NULL ;
   }
   
   mapArray = new PushingWeightRecord**[numEffCoLTrans] ;
   for ( int i = 0 ; i < numEffCoLTrans ; i++ )  {
      mapArray[i] = bucketArray + i * nBucket ;
   }
   
   recordHead = NULL ;
   recordTail = NULL ;
   int tempMaxRecords = (int) (((real)numEffCoLTrans)*((real)numGStates)*CACHESIZEPERCENTAGE) ;
   maxRecords = (tempMaxRecords > CACHEMINSIZE)?tempMaxRecords:CACHEMINSIZE ;
   recordPool = new PushingWeightRecord[maxRecords] ;
   currRecords = 0 ;

}

WFSTOnTheFlyDecoder::PushingWeightCache::~PushingWeightCache()
{
   // Changes Octavian 20060415
   // reset() ;
   delete [] recordPool ;
   delete [] bucketArray ;
   
   // Changes Octavian 20060325
   delete [] transLookup ;
   delete [] mapArray ;
}

/*
void WFSTOnTheFlyDecoder::PushingWeightCache::reset()
{
   // Changes 20060325
   for ( int i = 0 ; i < numEffCoLTrans ; i++ )  {
      
      //delete mapArray[i] ;
      //mapArray[i] = NULL ;
      
      mapArray[i].clear() ;
   }
   

   PushingWeightRecord *tmpRec = recordHead ;
   while ( tmpRec != NULL )  {
      recordHead = recordHead->next ;
      delete ( tmpRec ) ;
      tmpRec = recordHead ;
   }
   recordTail = NULL ;
   currRecords = 0 ;

}
*/

// Changes Octavian 20060531
bool WFSTOnTheFlyDecoder::PushingWeightCache::findPushingWeight( int colTransIndex, int gStateIndex, int colOutLabel, real *foundWeight, int *foundNextGState, int *foundNextOutLabel, int *foundMatchedOutLabel )
{
#ifdef DEBUG
   if (( colTransIndex < 0 ) || ( colTransIndex >= numCoLTrans ))
      error("PushingWeightCache::findPushingWeight - colTransIndex out of range") ;
   if (( gStateIndex < 0 ) || ( gStateIndex >= numGStates ))
      error("PushingWeightCache::findPushingWeight - gStateIndex out of range") ;
#endif

   // Changes Octavian 20060415
   PushingWeightRecord *foundRecord = NULL ;
   if ( transLookup == NULL )  {
      foundRecord = find( colOutLabel, gStateIndex ) ;
      if ( foundRecord == NULL )
	 return false ;      
   }
   else  {
      foundRecord = find( transLookup[colTransIndex], gStateIndex ) ;
      if ( foundRecord == NULL )
	 return false ;
   }

   *foundWeight = foundRecord->pushingWeight ;
   *foundNextGState = foundRecord->nextGState ;
   *foundNextOutLabel = foundRecord->nextOutLabel ;
   // Changes Octavian 20060531
   *foundMatchedOutLabel = foundRecord->matchedOutLabel ;
   
   if ( foundRecord != recordTail )  {
      if ( foundRecord == recordHead )  {
	 recordHead = foundRecord->next ;
	 recordHead->prev = NULL ;
      }
      else  {
	 foundRecord->prev->next = foundRecord->next ;
	 foundRecord->next->prev = foundRecord->prev ;
      }
      foundRecord->prev = recordTail ;
      foundRecord->next = NULL ;
      recordTail->next = foundRecord ;
      recordTail = foundRecord ;
   }

   return true ;
   
}

// Changes Octavian 20060531
void WFSTOnTheFlyDecoder::PushingWeightCache::insertPushingWeight( int colTransIndex, int gStateIndex, int colOutLabel, real newWeight, int newNextGState, int newNextOutLabel, int newMatchedOutLabel )
{
#ifdef DEBUG
   if (( colTransIndex < 0 ) || ( colTransIndex >= numCoLTrans ))
      error("PushingWeightCache::insertPushingWeight - colTransIndex out of range") ;
   if (( gStateIndex < 0 ) || ( gStateIndex >= numGStates ))
      error("PushingWeightCache::insertPushingWeight - gStateIndex out of range") ;
#endif

   // Changes 20060325
   PushingWeightRecord *newRecord = NULL ;
   
   if ( currRecords == maxRecords )  {
      newRecord = deletePushingWeight() ;	 
   }
   else  {
      newRecord = recordPool + currRecords ;
   }

   // Changes 20060325
   if ( transLookup == NULL )  {
      insert( colOutLabel, gStateIndex, newRecord ) ;
   }
   else  {
      insert( transLookup[colTransIndex], gStateIndex, newRecord ) ;
   }

   // Changes 20060415
   // The insert() function will assign gState, hashprev, hashnext and bucket 
   newRecord->pushingWeight = newWeight ;
   newRecord->nextGState = newNextGState ;
   newRecord->nextOutLabel = newNextOutLabel ;
   // Changes Octavian 20060531
   newRecord->matchedOutLabel = newMatchedOutLabel ;

   if ( recordTail == NULL )  {
      recordHead = recordTail = newRecord ;
      newRecord->prev = NULL ;
      newRecord->next = NULL ;
   }
   else  {
      recordTail->next = newRecord ;
      newRecord->prev = recordTail ;
      newRecord->next = NULL ;
      recordTail = newRecord ;
   }

   currRecords++ ;

}

	 
// Changes 20060415
void WFSTOnTheFlyDecoder::PushingWeightCache::insert( int nthHashTable, int gStateIndex, WFSTOnTheFlyDecoder::PushingWeightCache::PushingWeightRecord *newRecord )
{
   PushingWeightRecord **bucket = mapArray[nthHashTable] + hash(gStateIndex) ;
  
   // First record
   if ( *bucket == NULL )  {
      *bucket = newRecord ;
      newRecord->hashprev = NULL ;
      newRecord->hashnext = NULL ;
   }
   else  {
      newRecord->hashprev = NULL ;
      newRecord->hashnext = *bucket ;
      (*bucket)->hashprev = newRecord ;
      *bucket = newRecord ;
   }

   newRecord->gState = gStateIndex ;
   newRecord->bucket = bucket ;
}

// Changes 20050325
WFSTOnTheFlyDecoder::PushingWeightCache::PushingWeightRecord *
WFSTOnTheFlyDecoder::PushingWeightCache::deletePushingWeight()
{
#ifdef DEBUG
   if ( recordHead == NULL )
      error("PushingWeightCache::deletePushingWeight - recordHead is NULL") ;
#endif
   
   PushingWeightRecord *toDel = recordHead ;
   recordHead = toDel->next ;
   
   currRecords-- ;

   if ( recordHead == NULL )  {
      recordTail = NULL ;
   }
   else  {
      recordHead->prev = NULL ;
   }

   // Changes 20060415
   if ( toDel->hashprev == NULL )  {
      *(toDel->bucket) = toDel->hashnext ;
      if ( toDel->hashnext != NULL )
	 toDel->hashnext->hashprev = NULL ;
   }
   else  {
      toDel->hashprev->hashnext = toDel->hashnext ;
      if ( toDel->hashnext != NULL )  
	 toDel->hashnext->hashprev = toDel->hashprev ;      
   }

   // Changes 20060325
   return toDel ;
}

int WFSTOnTheFlyDecoder::PushingWeightCache::calNBucket( int nBucket_ )
{
#ifdef DEBUG
   if ( nBucket_ < 0 )
      error("PushingWeightCache::calNBucket - input bucket number < 0") ;
#endif
   
   int nextpow2 = 2 ;
   while ( nBucket_ > 2 )  {
      nBucket_ /= 2 ;
      nextpow2 *= 2 ;
   }

   return nextpow2 ;
}

WFSTOnTheFlyDecoder::PushingWeightCache::PushingWeightRecord *
WFSTOnTheFlyDecoder::PushingWeightCache::find( int nthHashTable , int gStateIndex )
{
   PushingWeightRecord *record = *( mapArray[nthHashTable] + hash( gStateIndex ) ) ;

   while ( record != NULL )  {
      if ( record->gState == gStateIndex )
	 break ;
      record = record->hashnext ;
   }
   
   return record ;
}

/*
Changes for bestFinalHyp as ptrs
Change for DecHypOnTheFly -- check relative functions
... Tracing from WFSTDecoder->decode (virtual at the moment)
virtual init() 	-> reset() (non-virtual)
   			-> resetActiveHyps -- virtual
			-> emitHypsHistogram class - Not changed
			-> lattice - Not changed
			-> resetDecHyp - Created an virtual function
			-> Does not reset pushing cache
   		-> initDecHyp	-- Changed!
		-> extendModelEndState -- Changed
		-> joinNewActiveModelsList()
   		-> resetDecHyp  -- Created an virtual function
   
   processFrame()	-> resetDecHyp - Created an virtual function
   			-> processActiveModelsInitStates - virtual
			-> newFrame and setInputVector - not changed
			-> lattice->newFrame
			-> emitHypsHistgram stuff - not changed
			-> processActiveModelsEmitStates - virtual
			-> processActiveModelsEndStates - virtual

   finish()		-> resetAciveHyps - virtual
   			-> lattice stuff - not changed
			-> isActiveHyp - not changed
*/
}
