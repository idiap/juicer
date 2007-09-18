/*
 * Copyright 2005 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#ifndef WFST_LATTICE_INC
#define WFST_LATTICE_INC

#include "general.h"
#include "WFSTGeneral.h"

/*
	Author :		Darren Moore (moore@idiap.ch)
	Date :		16 June 2005
	$Id: WFSTLattice.h,v 1.3.4.1 2006/06/21 12:34:12 juicer Exp $
*/

namespace Juicer {


struct WFSALatticeTrans
{
   int   fromState ;
   int   toState ;
   int   label ;
   real  weight ;
};


struct WFSTLatticeTrans
{
   int   fromState ;
   int   toState ;
   int   inLabel ;
   int   outLabel ;
   real  weight ;
};


struct WFSTLatticeFinalState
{
   int   state ;
   real  weight ;
};


/**
 * Lattice
 */
class WFSTLattice
{
public:
   // Changes
   // WFSTLattice( int nStatesInDecNet_ , bool wfsaMode_ , bool projectInput_ ) ;
   WFSTLattice( 
	 int nStatesInDecNet_ , bool wfsaMode_ , bool projectInput_ , 
	 bool allocMap_ = true ) ;
   virtual ~WFSTLattice() ;

   void enableDeadEndRemoval( int nFramesBetweenPartialDeadEndRemoval_=10 , 
                              int nFramesBetweenFullDeadEndRemoval_=25 ) ;
   int reset() ;
   // Changes
   virtual int addEntry( 
	 int lattFromState , int netToState , int inLabel , int outLabel , 
	 real score ) ;
   virtual int addEntry( 
	 int lattFromState , int netToState , int gState, int inLabel , 
	 const int *outLabel, int nOutLabel, real score ) ;
   
   void addFinalState( int state_ , real weight_ ) ;
   void newFrame( int frame_ ) ;
   int getInitState() { return initState ; } ;
   void writeLatticeFSM( const char *outFName ) ;
   void registerActiveTrans( int fromState ) ;
   void registerInactiveTrans( int fromState ) ;

   void printLogInfo() ;
   int getStateNumOutTrans( int state ) { return stateNumOutTrans[state] ; } ;
   void removeDeadEndTransitions( bool doFullRemoval=false ) ;

// Changes 
//private:
protected:
   bool                    wfsaMode ;     // If true, use WFSALatticeTrans structures
   bool                    projectInput ; // When wfsaMode true, if true store input labels,
                                          //   otherwise store output labels.

   int                     nTrans ;
   int                     nTransAlloc ;
   WFSALatticeTrans        *wfsaTrans ;
   WFSTLatticeTrans        *wfstTrans ;
   int                     initState ;
   int                     currFrame ;

   int                     nFinalStates ;
   int                     nFinalStatesAlloc ;
   WFSTLatticeFinalState   *finalStates ;

   int                     nStates ;   // current number of states in the lattice FSM
   int                     nStatesAlloc ;
   short                   *stateNumOutTrans ;  // number of transitions out from each state
   bool                    *stateIsFinal ;
   
   int                     nStatesInDecNet ;
   // Changes - moved to private
   // int                     *decNetStateToLattStateMap ;   // for the current frame

   bool                    doDeadEndRemoval ;
   int                     totalTransAdded ;
   int                     avgTransPerFrame ;                     
   int                     nFramesBetweenPartialDeadEndRemoval ;
   int                     nFramesBetweenFullDeadEndRemoval ;
   int                     nTransPrevFrame ;
   int                     nTransPrevRemoval ;

   void removeWFSTDeadEndTransitions( int fromTrans , int toTrans ) ;
   void removeWFSADeadEndTransitions( int fromTrans , int toTrans ) ;

   // Changes
   virtual void resetDecNetStateToLattStateMap() ;

   // Changes
   // Helper function for addEntry()
   void addEntryBeforeMapping() ; 	   
   void addEntryAfterMapping( 
	 int lattFromState, int lattToState, int inLabel,
	 int outLabel, real score ) ;
   
private:
   // Changes
   int                     *decNetStateToLattStateMap ;   // for the current frame
};


}

#endif
