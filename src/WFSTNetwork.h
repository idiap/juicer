/*
 * Copyright 2004 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * Copyright 2008 by The University of Edinburgh
 *
 * Copyright 2008 by The University of Sheffield
 *
 * See the file COPYING for the licence associated with this software.
 */

#ifndef WFST_NETWORK_INC
#define WFST_NETWORK_INC

#include <cassert>
#include "general.h"
#include "WFSTGeneral.h"

// Changes Octavian
// For finding labels
#include <set>
#include <list>
#include <algorithm>

// Changes Octavian 20060325
using namespace std ;

/*
	Author:	Darren Moore (moore@idiap.ch)
	Date:		6 October 2004
	$Id: WFSTNetwork.h,v 1.14.4.1 2006/06/21 12:34:12 juicer Exp $
*/

/*
	Author: Octavian Cheng (ocheng@idiap.ch)
	Date:		6 June 2006
*/

namespace Juicer {


struct WFSTTransition
{
    int      id ;
    //int      fromState ;
    int      toState ;
    real     weight ;
    int      inLabel ;
    int      outLabel ;
};


struct WFSTState
{
    int      *trans ;    // indices of transitions out of this state
    int      label ;
    int      finalInd ;  // index into array of final state entries (-1 if not final)
    int      nTrans ;    // number of transitions out of this state
};


struct WFSTFinalState
{
   int      id ;
   real     weight ;
};

/**
 * WFST symbol set
 */
class WFSTAlphabet
{
public:
   WFSTAlphabet() ;
   WFSTAlphabet( const char *symbolsFilename ) ;
   virtual ~WFSTAlphabet() ;

   const char *getLabel( int index ) ;
   int getIndex( const char *label ) ;
   bool isAuxiliary( int index ) ;
   int getMaxLabel() { return maxLabel ; } ;
   // Changes 20060325
   int getNumLabels() { return nLabels ; } ;
   int getNumAuxSyms() { return nAux ; } ;
   void outputText() ;
   void write( FILE *fd , bool outputAux ) ;
   void writeBinary( FILE *fd ) ;
   void readBinary( FILE *fd ) ;

private:
   int   maxLabel ;
   int   nLabels ;
   int   nLabelsAlloc ;
   char  **labels ;

   int   nAux ;
   bool  *isAux ;

   bool  fromBinFile ;
};


// Changes Octavian 20060523
/** Enum variable for aux symbol removal */
typedef enum { REMOVEBOTH, REMOVEINPUT, NOTREMOVE } RemoveAuxOption ;

/**
 * Transducer
 */
class WFSTNetwork
{
public:
    WFSTNetwork() ;
    WFSTNetwork( real transWeightScalingFactor_, real insPenalty_ ) ;
    // Changes Octavian 20060523
    WFSTNetwork(
        const char *wfstFilename , const char *inSymsFilename=NULL ,
        const char *outSymsFilename=NULL ,
        real transWeightScalingFactor_=1.0 , real insPenalty_=0.0 ,
        RemoveAuxOption removeAuxOption=REMOVEBOTH
    ) ;
    virtual ~WFSTNetwork() ;

   int getInitState() { return initState ; } ;
   void getTransitions(
	 WFSTTransition *prev , int *nNext , WFSTTransition **next ) ;
    int getTransitions(
        WFSTTransition *prev, WFSTTransition **next
    );
   int getMaxOutTransitions() { return maxOutTransitions ; } ;
   int getNumTransitions() { return nTransitions ; } ;
   int getNumStates() { return nStates ; } ;
   int getWordEndMarker() { return wordEndMarker ; } ;
   int getNumTransitionsOfOneState ( int state )  {
       return states[state].nTrans ;
   }
   // Changes Octavian 20050325
   int getNumOutLabels() { return outputAlphabet->getNumLabels() ; } ;

   // Changes by Octavian
   bool isFinalState( int stateIndex ) {
       return states[stateIndex].finalInd >= 0;
   }
   real getFinalStateWeight( int stateIndex ) {
      return finalStates[states[stateIndex].finalInd].weight ;
   }
   const int *getTransitions( const WFSTTransition *prev , int *nNext ) ;
   WFSTTransition *getOneTransition( int transIndex ) ;
   int getInfoOfOneTransition(
	 const int gState, const int n, real *weight, int *toState,
	 int *outLabel ) ;
   // Changes Octavian 20060616
   int getInLabelOfOneTransition( const int gState , const int n ) ;

   // Changes Octavian 20060430
   int getTransID( int stateIndex, int nth ) {
      return states[stateIndex].trans[nth] ; } ;

   bool transGoesToFinalState( WFSTTransition *trans ) {
       assert(trans);
       return  states[trans->toState].finalInd >= 0;
   }
   real getFinalStateWeight( WFSTTransition *trans ) {
       return finalStates[states[trans->toState].finalInd].weight ;
   }

   void outputText( int type=0 ) ;
   void generateSequences( int maxSeqs=0 , bool logBase10=false ) ;
   void writeFSM(
	 const char *fsmFName , const char *inSymsFName ,
	 const char *outSymsFName ) ;
   virtual void writeBinary( const char *fname ) ;
   virtual void readBinary( const char *fname ) ;

   // Changes Octavian 20060616
   void printNumOutTransitions( const char *fname ) ;

   int               silMarker ;
   int               spMarker ;


protected:
   WFSTAlphabet      *inputAlphabet ;
   WFSTAlphabet      *outputAlphabet ;
   WFSTAlphabet      *stateAlphabet ;

   int               initState ;
   int               maxState ;

   int               nStates ;
   int               nStatesAlloc ;
   WFSTState         *states ;

   int               nFinalStates ;
   int               nFinalStatesAlloc ;
   WFSTFinalState    *finalStates ;

   int               nTransitions ;
   int               nTransitionsAlloc ;
   WFSTTransition    *transitions ;
   int               maxOutTransitions ;  // max outgoing transitions in any state

   bool              fromBinFile ;
   real              transWeightScalingFactor ;
   real              insPenalty ;
   int               wordEndMarker ;


   void initWFSTState( WFSTState *state ) ;
   void initWFSTTransition( WFSTTransition *transition ) ;
   void removeAuxiliarySymbols( bool markAuxOutputs=false ) ;
   // Changes Octavian 20060523
   void removeAuxiliaryInputSymbols ( bool markAuxOutputs=false ) ;
};


// Changes Octavian
/**
 * A network in which there is a label set associated with each transition
 */
class WFSTLabelPushingNetwork : public WFSTNetwork
{
public:
   // Changes Octavian
   typedef set<int> LabelSet ;

   WFSTLabelPushingNetwork() ;
   WFSTLabelPushingNetwork( real transWeightScalingFactor_ ) ;

   // Changes Octavian 20060523
   WFSTLabelPushingNetwork(
	 const char *wfstFilename , const char *inSymsFilename=NULL ,
	 const char *outSymsFilename=NULL ,
	 real transWeightScalingFactor_=1.0 ,
	 RemoveAuxOption removeAuxOption=REMOVEBOTH ) ;
   virtual ~WFSTLabelPushingNetwork() ;
   virtual void writeBinary( const char *fname ) ;
   virtual void readBinary( const char *fname ) ;

   const LabelSet *getOneLabelSet( int transIndex ) ;
   const LabelSet **getLabelArray() { return arc_outlabset_map ; } ;

   // Changes Octavian 20060616
   // Get the max number of outlabels from all label sets
   int getMaxOutLabels( const char *fname = NULL ) ;

#ifdef DEBUG
   void printLabelSet() ;
#endif

protected:
   // Changes Octavian 20060325
   typedef const LabelSet **ArcToLabelSetMap ;
   typedef enum { LOOPEND, NOTLOOPEND } LoopStatus ;

   // Data structure for storing output symbol label set
   // unique_outlabsets - a set of unique label sets
   // arc_outlabset_map - array which maps the transition index
   // to the label set
   // An output label set is a set of anticipated output symbols
   // If the transition has epsilon output and there is no non-eps
   // transitions on the path to the final state, the label set
   // will have just one label - NONPUSHING_OUTLABEL
   set<LabelSet> unique_outlabsets ;
   ArcToLabelSetMap arc_outlabset_map ;

   // Assign output label set to each transition
   // Main function which assigns output label set for each transition
   void assignOutlabsToTrans() ;

   // The following are helper functions for assignOutlabsToTrans()
   // Find a set of transitions in which we cannot decide its label set
   // Starting to do depth-first search from transIndex. Put "undecided"
   // transitions in undecidedTrans set
   const LabelSet* findUndecidedTrans(
	 int transIndex, bool *hasVisited, set<int>& undecidedTrans ) ;

   // This function iterates all undecided trans and find the start trans of
   // a loop and a new set of undecided trans.
   // newUndecidedTrans will store a set of undecided transitions.
   // loopStartTrans will store a set of transitions which is the start of
   // a loop.
   LoopStatus findLoopStartTrans(
	 int transIndex, bool *hasVisited, bool *hasReturned,
	 const LabelSet **returnLabelSet, set<int> &newUndecidedTrans,
	 set<int> &loopStartTrans ) ;

   // This function starts transversing from the start of a loop and collects
   // all states within the loop into loopStatesSet. See the cpp file.
   bool findLoop(
	 int transIndex, bool *hasVisited, const int currLoopStart,
	 const set<int> &loopStartTransSet, set<int> &loopStatesSet,
	 const set<int> &undecidedTransSet ) ;

   // This function combines loops that share some same states into one loop
   void combineLoop( list< set<int> > &loopList ) ;

   // This function assign a common label set for each transition in a loop.
   // Get a loop from the loop list. For each state of the loop, check if
   // it is dependent to other loop. If all the states are not dependent
   // on other loops, collect all the labels from all the set. Assign
   // the union to all the loop transitions.
   void assignOutlabsToLoops(
	 list< set<int> > &loopList , set<int> &undecidedTransSet ) ;

   // Return true if this transition is dependent on the other loops.
   // Otherwise return false.
   // returnLabelSet is the label set of this transition
   bool findOutlabsOfOneTransFromLoopState(
	 int transIndex, const list< set<int> >::iterator currLoop,
	 list< set<int> > &loopList , const LabelSet **returnLabelSet ,
	 set<int> &undecidedTransSet ) ;

};


// Changes by Octavian
/**
 * A network in which the input label is sorted
 */
class WFSTSortedInLabelNetwork : public WFSTNetwork
{
public:
   WFSTSortedInLabelNetwork() ;
   WFSTSortedInLabelNetwork( real transWeightScalingFactor_ ) ;
   // Changes Octavian 20060523
   WFSTSortedInLabelNetwork(
	 const char *wfstFilename , const char *inSymsFilename=NULL ,
	 const char *outSymsFilename=NULL ,
	 real transWeightScalingFactor_=1.0 ,
	 RemoveAuxOption removeAuxOption=REMOVEBOTH ) ;
   virtual ~WFSTSortedInLabelNetwork() {} ;

    /**
     * Searching for all states on an eps-only path with the accumuated weight
     */
   void getStatesOnEpsPath(
	 const int currGState, const real currEpsWeight,
	 const int currOutLabel, int *gStateArray, real *epsWeightArray,
	 int *outLabelArray, int *nGState, const int maxNGState ) ;

   // Changes Octavian 20060508
   int getNextStateOnEpsPath( int gState, real *backoffWeight );

   virtual void writeBinary( const char *fname ) {
      WFSTNetwork::writeBinary(fname) ; return ; };
   virtual void readBinary( const char *fname ) {
      WFSTNetwork::readBinary(fname) ; return ; };

protected:
    /**
     * A function which does binary search for inLabel on the array
     * which contains transitions coming from the same state.  The
     * return pointer is the insertion point.
     */
   int *binarySearchInLabel( int *start, const int length, const int inLabel ) ;
};


}

#endif
