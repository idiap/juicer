/*
 * Copyright 2006 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#ifndef WFST_OTFDECODER_INC
#define WFST_OTFDECODER_INC

#include "WFSTDecoder.h"
#include "WFSTModelOnTheFly.h"

// Changes Octavian 20060726
// If the following is defined, Caseiro's method is used - ie No label-
// matching (ie no control on non-coaccessible paths) and no weight 
// lookahead in the "suffix" region. See IDIAP RR-06-62 for details.
// #define CASEIRO

// Changes Octavian 20060809
// If defined, the duplication of G eps transitions is delayed until the
// token reaches the end of a non-eps-output transition in CoL.
// Otherwise, the duplication of G eps transitions is done when the 
// intersection between the anticipated output label set and G input 
// symbols is singleton.
#define DELAYPUSHING

// Changes Octavian 20060810
// If defined, the semiring used for weight pushing between anticipated
// output labels and G input symbols is a log semiring (ie semiring-add =
// log-add). Otherwise, the semiring is a tropical semiring (ie semiring-
// add = minimum cost (or maximum prob)).
#define LOG_ADD_LOOKAHEAD

// Changes Octavian 20060823
// If the following is defined, only the acoustic score will be put in the
// lattice. No LM score in the lattice.
// #define ACOUSTIC_SCORE_ONLY

namespace Juicer {


// ************************************
// Changes
// On-the-fly decoder

// Changes Octavian 20060325
#define DEAD_TRANS -4
#define FOLLOWON_TRANS -3
#define NOPUSHING_TRANS -2
#define UNDECIDED_TRANS -1

// Changes Octavian 20060415 20060417 20060508
#define CACHESIZEPERCENTAGE 0.0
#define CACHEMINSIZE 3000000
#define NUMBUCKETS 1024

/**
 * On the fly decoder by Octavian
 */
class WFSTOnTheFlyDecoder : public WFSTDecoder
{
public:
   // Public Member Variables
   WFSTSortedInLabelNetwork *gNetwork ;
      
   // Pruning
   // bestHypHist
   // emitHypsHistogram
   
   // Constructors / destructor
   WFSTOnTheFlyDecoder() ;
//PNG   WFSTOnTheFlyDecoder( 
//PNG	 WFSTNetwork *network_ , WFSTSortedInLabelNetwork *gNetwork_ ,
//PNG	 PhoneModels *phoneModels_ ,  real emitPruneWin_ , 
//PNG	 real phoneEndPruneWin_ ,  int maxEmitHyps_ ,
//PNG	 bool doPushingWeightAndLabel_ , bool doPushingWeightCache_ ) ;
   WFSTOnTheFlyDecoder( 
	 WFSTNetwork *network_ , WFSTSortedInLabelNetwork *gNetwork_ , 
	 Models *models_ ,  real emitPruneWin_ , 
	 real phoneEndPruneWin_ , int maxEmitHyps_ , 
	 bool doModelLevelOutput_ , bool doLatticeGeneration_ ,
	 bool doPushingWeightAndLabel_ , bool doPushingWeightCache_ ) ;
   virtual ~WFSTOnTheFlyDecoder() ;
   
   // Public Methods
   // Changes
   virtual DecHyp *decode( real **inputData , int nFrames_ ) ;
   // Changes
   virtual void init() ;

protected:
   // Hypothesis Management
   // Changes by Octavian
   WFSTModelOnTheFlyPool *modelOnTheFlyPool ;
   // Changes Octavian 20060627
   // DecHypPool *decHypPool ;
   
   WFSTModelOnTheFly *activeModelsOnTheFlyList ;
   WFSTModelOnTheFly **activeModelsOnTheFlyLookup ;
   WFSTModelOnTheFly *newActiveModelsOnTheFlyList ;
   WFSTModelOnTheFly *newActiveModelsOnTheFlyListLastElem ;
   
   // Changes
   virtual void processActiveModelsInitStates() ;
   virtual void processActiveModelsEmitStates() ;
   virtual void processActiveModelsEndStates() ;
   WFSTModelOnTheFly *getModel( WFSTTransition *trans ) ;
   WFSTModelOnTheFly *returnModel( WFSTModelOnTheFly *model , WFSTModelOnTheFly *prevActiveModel ) ;
   virtual void resetActiveHyps() ;
   virtual void registerLabel( DecHyp* hyp , int label ) ;
   virtual void joinNewActiveModelsList() ;
   virtual void checkActiveNumbers( bool checkPhonePrevHyps ) ;
   virtual void resetDecHyp( DecHyp* hyp ) ;
   // Changes Octavian 20060424 20060630
   void extendInitMap( 
	 WFSTModelOnTheFly *model, bool isPushed, 
	 NodeToDecHypOnTheFly *currNode, NodeToDecHypOnTheFly **helper ) ;
   void extendModelInitState( WFSTModelOnTheFly *model ) ;
   
   // Changes Octavian 20060531
   void extendDecHypOnTheFlyNonEndState(
	 DecHypOnTheFly *currHyp, int pushedGState, int matchedOutLabel, 
	 bool isPushedMap, int hmmStateIndex, WFSTModelOnTheFly *model, 
	 NodeToDecHypOnTheFly **helper ) ;
   void processModelEmitStates( WFSTModelOnTheFly *model ) ;

   // Changes Octavian 20060531
   // Changes Octavian 20060815
   void extendModelEndState( DecHypOnTheFly *endHyp , WFSTTransition *trans , 
                             WFSTTransition **nextTransBuf , bool isPushedMap,
			     int pushedGState, int thisMatchedOutLabel ) ;
   
   // Changes Octavian 20060523
   virtual void addLabelHist( DecHyp* hyp , int label ) ;

   // Changes
   // Lattice generation
   virtual int addLatticeEntry( DecHyp *hyp , WFSTTransition *trans , int *fromState ) ;

   // Enable/disable label and weight pushing
   bool doPushingWeightAndLabel ;
      
   // Find a set of next transitions in the C o L transducer where 
   // there is at least one label in its label set 
   // which is matched with the input labels in the G transducer.
   // Call this function when label pushing is used in this decoder
   // Arguments : prev (I) - current C o L transition
   // 		  nNext (O) - number of next non-dead CoL transitions
   // 		  next (O) - array of next non-dead CoL transitions
   // 		  gState (I) - current state number of the G transducer
   // 		  commonWeights (O) - array corresponding lookahead weights of
   // 		  	next C o L transitions
   //		  nextGState (O) - array corresponding lookahead G state of 
   //		  	next C o L transitions. Can have 4 possibilites: 
   //		  	- FOLLOWON_TRANS = this CoL trans has the same label
   //		  	  set as the prev CoL trans.
   //		  	- UNDECIDED_TRANS = this CoL trans has more than 1 
   //		  	  matches with the input symbols of the G transducer.
   //		  	- NOPUSHING_TRANS = this CoL trans has a
   //		  	  NONPUSHING_OUTLABEL. No pushing (lookahead) is 
   //		  	  allowed.
   //		  	- Actual lookahead G state = when there's only 1 match
   //		  	  with the input symbol of the G transducer.
   //		  nextOutLabel (O) - corresponding lookahead outLabel of next 
   //		  	C o L transitions
   //		  matchedOutLabel (O) - the output label in the CoL label set
   //		  	which matches with one of the input labels of the 
   //		  	G transducer.
   // For example: Suppose there are 3 CoL transitions after the current one.
   // Each has a label set. Suppose the first CoL transition has 5 outlabels 
   // in its label set which are matched with the inlabels of the gState.
   // The commonWeight of this transition will be the max of the LM weights 
   // of those 5 transitions in the G transducer. The nextGState of this 
   // transition will be unknown and set to UNDECIDED_TRANS because there are 
   // 5 possibilites and we don't know where it should go yet. The 
   // nextOutLabel will be UNDECIDED_OUTLABEL. The matchedOutLabel will be
   // UNDECIDED_OUTLABEL.
   //
   // Suppose the second transition has only 1 output label matched. The 
   // commonWeight will be the LM weight of that particular G transition. The 
   // nextGState will be the G state to which that G transition is pointing
   // to. The nextOutLabel will be the outLabel of that particular G 
   // transition. The matchedOutLabel will be the output label of CoL which
   // matches with the input label of the G transducer.
   //
   // Lastly, for the third CoL transition, suppose there are no 
   // intersections. Then it will not be one of the next CoL transitions and 
   // will not be stored. Hence, nNext will be 2. The "next" array will 
   // contain pointers to the 2 CoL transitions which have intersections. The
   // "commonWeights", "nextGState", "nextOutLabel" and "matchedOutLabel" 
   // will be the corresponding common weights, next G states, next 
   // outLabels and matched CoL outLabels of the 2 transitions.
   //
   // Exception: When a CoL transition is going towards the final state(s), 
   // label-pushing is switched off for that transition since we don't want 
   // the lookahead LM weights to be added to the hypotheses heading towards 
   // the final state(s). In this case, the corresponding nextGState, 
   // commonWeight, nextOutLabel and matchedOutLabel will be NOPUSHING_TRANS,
   // 0.0, UNDECIDED_OUTLABEL and UNDECIDED_OUTLABEL respectively.
   //
   // When the labelSet of the next transition is the same as the labelSet of
   // this transition, nextGState = FOLLOWON_TRANS. commonWeights, 
   // nextOutLabel and matchedOutLabel are not assigned. Do not try to read
   // their values.
   // Changes Octavian 20060531
   // Changes Octavian 20060726
   // Changes Octavian 20060828
#ifndef CASEIRO
   void findMatchedTransWithPushing( 
	 WFSTTransition *prev, int *nNext, WFSTTransition **next, 
	 int gState, real *commonWeights, int *nextGState, 
	 int *nextOutLabel, int *matchedOutLabel ) ;
#else
   void findMatchedTransWithPushing( 
	 WFSTTransition *prev, int *nNext, WFSTTransition **next, 
	 int gState, real *commonWeights, int *nextGState, 
	 int *nextOutLabel, int *matchedOutLabel, int nonRegisteredLabels, 
	 bool isPushed ) ;
#endif
  
   // This function finds matching transitions with the G transducer (similar
   // to findMatchedTransWithPushing). But the difference is it doesn't do any
   // lookahead. See the function for more detail.
   // Changes Octavian 20060531
   void findMatchedTransNoPushing( 
	 WFSTTransition *prev, int *nNext, WFSTTransition **next, 
	 int gState, real *commonWeights, int *nextGState, 
	 int *nextOutLabel, int *matchedOutLabel ) ;

   // Helper function for findMatchedTransWithPushing()
   // Intersection of one label set in CoL with all transitions of one state 
   // in the G transducer
   // Changes Octavian 20060531
   bool findIntersection( 
	 const WFSTLabelPushingNetwork::LabelSet *labelSet, const int gState, 
	 const int totalNTransInG , real *commonWeight, int *nextGState, 
	 int *nextOutLabel, int *matchedOutLabel ) ;

   // Binary search of one label in a range of G transitions - Only update 
   // weight, nextGState and nextOutLabel when a search is successful. 
   // Always output cloestTransInG regardless of the search result
   bool binarySearchGTrans( 
	 const int outLabelInCL, const int gState, const int startTransInG, 
	 const int nTransInG, real *weight, int *nextGState, 
	 int *closestTransInG, int *nextOutLabel ) ;
   
   // *************
   // Enable/disable cache for storing pushing weights   
   bool doPushingWeightCache ;
   
   // Cache for storing pushing weights
   class PushingWeightCache
   {
      public:
	 PushingWeightCache() ;
	 // Changes Octavian 20060325
	 PushingWeightCache( 
	       int numCoLTrans_, int numGStates_,
	       const WFSTLabelPushingNetwork::LabelSet **labelSetArray_, 
	       int numCoLOutSyms_, int nBucket_ ) ;
	 virtual ~PushingWeightCache() ;
	 // Changes Octavian 20060415
	 // void reset() ;
	 
	 // Changes Octavian 20060531
	 // Find a cache record.
	 bool findPushingWeight( 
	       int colTransIndex, int gStateIndex, int colOutLabel,  
	       real *foundWeight, int *foundNextGState, int *foundNextOutLabel,
	       int *foundMatchedOutLabel ) ;

	 // Changes Octavian 20060531
	 // Insert a new cache record
	 void insertPushingWeight( 
	       int colTransIndex, int gStateIndex, int colOutLabel,  
	       real newWeight, int newNextGState, int newNextOutLabel, 
	       int newMatchedOutLabel ) ;
	 
	 int numRecords() { return currRecords ; } ;

      private:
	 // Changes Octavian 20060415
	 // Changes Octavian 20060531
	 // Cache record - Each CoL label set will have a hash table storing 
	 // these records. A record stores the matching result of its 
	 // corresponding labelSet with a gState.
	 // - gState = State number of the G transducer
	 // - pushingWeight = max weights of matches (because it's a prob)
	 // - nextGState = the next G state. If the label set has a 
	 // NONPUSHING_OUTLABEL, it means that pushing is not allowed and 
	 // nextGState = NOPUSHING_TRANS. If there is no matching, nextGState
	 // = DEAD_TRANS. If there is more than 1 matches, nextGState = 
	 // UNDECIDED_TRANS. If there is only 1 match, nextGState = the next G
	 // state.
	 // - nextOutLabel = next output label from the G transducer. If it is
	 // a non-pushing label set or it has more than 1 match or no match, 
	 // then the nextOutLabel = UNDECIDED_OUTLABEL. Otherwise, 
	 // nextOutLabel is the output label of the matched transitions in 
	 // the G transducer.
	 // - matchedOutLabel = the label in the CoL label set which matches 
	 // with the input label of the G transducer. If it is a non-pushing 
	 // label set or more than one match or no match, matchedOutLabel = 
	 // UNDECIDED_OUTLABEL. Otherwise (ie only one match), matchedOutLabel
	 // will be the one which matches with the input label of the G 
	 // transducer.
	 // Other attributes: Just for maintaining the hash table and the 
	 // Least Recently Used (LRU) linked list.
	 // - prev and next: doubly linked list for LRU
	 // - hashprev and hashnext: doubly linked list for hash chain
	 // - bucket: a pointer pointing to the bucket where this record can 
	 // be found.
	 typedef struct _pushingWeightRecord  {
	    int gState ;
	    real pushingWeight ;
	    int nextGState ;
	    int nextOutLabel ;
	    int matchedOutLabel ;
	    struct _pushingWeightRecord *prev ;
	    struct _pushingWeightRecord *next ;
	    struct _pushingWeightRecord *hashprev ;
	    struct _pushingWeightRecord *hashnext ;
	    struct _pushingWeightRecord **bucket ;
	 } PushingWeightRecord ;

	 // Pushing weight record pool
	 PushingWeightRecord *recordPool ;
	 
	 // LRU
	 PushingWeightRecord *recordHead ;
	 PushingWeightRecord *recordTail ;
 
	 // Hash map buckets for all label sets
	 // bucketArray contains the buckets of all effecitve CoL transitions
	 // mapArray[n] points to the first bucket of the nth effective CoL 
	 // transition
	 PushingWeightRecord **bucketArray ;
	 PushingWeightRecord ***mapArray ;
	 int nBucket ;
	 int nBucketMinusOne ;
	
	 // Number of effective C o L trans
	 int numEffCoLTrans ;

	 // This array points to the hash table of a CoL transition. Will 
	 // only have an array when pushing is enabled. Each CoL transition
	 // will have an entry. It's possible to have several CoL transition
	 // pointing to a hash table because they have the same label set.
	 // The length of this array is number of CoL transitions.
	 // Will be NULL array if no pushing since the hash table is indexed
	 // with the output label of CoL transition, not by unique label set.
	 int *transLookup ; 

	 int numCoLTrans ;
	 int numGStates ;
	 int maxRecords ;
	 int currRecords ;
	 
	 void insert( int nthHashTable, int gStateIndex, PushingWeightRecord *newRecord ) ;
	 PushingWeightRecord *deletePushingWeight() ;

	 // Determine the number of buckets, which will be the next closest 
	 // 2^n after nBucket_
	 int calNBucket( int nBucket_ ) ;
	 PushingWeightRecord *find( int nthHashTable , int gStateIndex ) ;
	 int hash( int gStateIndex ) { return ( gStateIndex & nBucketMinusOne ) ; } ;
	 
   };

   // Cache storing the pushing weight
   PushingWeightCache *pushingWeightCache ;

   
};

}

#endif
