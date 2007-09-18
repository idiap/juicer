/*
 * Copyright 2005 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#include "WFSTLattice.h"
#include "LogFile.h"

/*
	Author :		Darren Moore (moore@idiap.ch)
	Date :		16 June 2005
	$Id: WFSTLattice.cpp,v 1.3.4.1 2006/06/21 12:34:13 juicer Exp $
*/

/*
	Author:		Octavian Cheng (ocheng@idiap.ch)
	Date:		7 June 2006
*/


using namespace Torch;

namespace Juicer {

#define WFSTLATTICETRANSREALLOCAMOUNT           50000
#define WFSTLATTICEMAXTRANS                     5000000
#define WFSTLATTICEFINALSTATESREALLOCAMOUNT     1000
#define WFSTLATTICEMAXFINALSTATES               50000
#define WFSTLATTICESTATESREALLOCAMOUNT		10000
#define WFSTLATTICEMAXSTATES			500000

// Changes
//WFSTLattice::WFSTLattice( int nStatesInDecNet_ , bool wfsaMode_ , bool projectInput_ )
WFSTLattice::WFSTLattice( int nStatesInDecNet_ , bool wfsaMode_ , bool projectInput_ , bool allocMap_ )
{
   nStatesInDecNet = nStatesInDecNet_ ;
   wfsaMode = wfsaMode_ ;
   projectInput = projectInput_ ;

   nTrans = 0 ;
   nTransAlloc = 0 ;
   wfsaTrans = NULL ;
   wfstTrans = NULL ;
   initState = -1 ;
   currFrame = -1 ;
   
   nFinalStates = 0 ;
   nFinalStatesAlloc = 0 ;
   finalStates = NULL ;

   nStates = 0 ;
   nStatesAlloc = 0 ;
   stateNumOutTrans = NULL ;
   stateIsFinal = NULL ;

   // Changes
   if ( allocMap_ )
      decNetStateToLattStateMap = new int[nStatesInDecNet] ;
   else
      decNetStateToLattStateMap = NULL ;

   doDeadEndRemoval = false ;
   totalTransAdded = 0 ;
   avgTransPerFrame = 0 ;
   nFramesBetweenPartialDeadEndRemoval = 1 ;
   nFramesBetweenFullDeadEndRemoval = 1 ;
   nTransPrevFrame = 0 ;
   nTransPrevRemoval = 0 ;
}


WFSTLattice::~WFSTLattice()
{
   if ( wfsaTrans != NULL )
      free( wfsaTrans ) ;
   if ( wfstTrans != NULL )
      free( wfstTrans ) ;
   if ( finalStates != NULL )
      free( finalStates ) ;

   if ( stateNumOutTrans != NULL )
      free( stateNumOutTrans ) ;
   if ( stateIsFinal != NULL )
      free( stateIsFinal ) ;

   delete [] decNetStateToLattStateMap ;
}


void WFSTLattice::enableDeadEndRemoval( int nFramesBetweenPartialDeadEndRemoval_ ,
                                        int nFramesBetweenFullDeadEndRemoval_ )
{
   doDeadEndRemoval = true ;
   if ( (nFramesBetweenPartialDeadEndRemoval = nFramesBetweenPartialDeadEndRemoval_) == 0 )
      nFramesBetweenPartialDeadEndRemoval = 100000000 ;
   if ( (nFramesBetweenFullDeadEndRemoval = nFramesBetweenFullDeadEndRemoval_) == 0 )
      nFramesBetweenFullDeadEndRemoval = 100000000 ;
}


int WFSTLattice::reset()
{
   if ( nTransAlloc > WFSTLATTICEMAXTRANS )
   {
      nTransAlloc = WFSTLATTICEMAXTRANS ;
      if ( wfsaMode )
         wfsaTrans = (WFSALatticeTrans *)realloc( wfsaTrans, nTransAlloc*sizeof(WFSALatticeTrans));
      else
         wfstTrans = (WFSTLatticeTrans *)realloc( wfstTrans, nTransAlloc*sizeof(WFSTLatticeTrans));
   }

   if ( nFinalStatesAlloc > WFSTLATTICEMAXFINALSTATES )
   {
      nFinalStatesAlloc = WFSTLATTICEMAXFINALSTATES ;
      finalStates = (WFSTLatticeFinalState *)realloc( finalStates , 
                                 nFinalStatesAlloc*sizeof(WFSTLatticeFinalState) ) ;
   }

	if ( nStatesAlloc > WFSTLATTICEMAXSTATES )
	{
		nStatesAlloc = WFSTLATTICEMAXSTATES ;
		stateNumOutTrans = (short *)realloc( stateNumOutTrans , nStatesAlloc*sizeof(short) ) ;
		stateIsFinal = (bool *)realloc( stateIsFinal , nStatesAlloc*sizeof(bool) ) ;
	}
	else if ( nStatesAlloc == 0 )
	{
		nStatesAlloc = WFSTLATTICESTATESREALLOCAMOUNT ;
		stateNumOutTrans = (short *)realloc( stateNumOutTrans , nStatesAlloc*sizeof(short) ) ;
		stateIsFinal = (bool *)realloc( stateIsFinal , nStatesAlloc*sizeof(bool) ) ;
	}
	
   nTrans = 0 ;
   nFinalStates = 0 ;
   nStates = 1 ;
   initState = 0 ;
	stateNumOutTrans[initState] = 0 ;
   stateIsFinal[initState] = false ;
   currFrame = -1 ;

   // Changes
   resetDecNetStateToLattStateMap() ;
   /*
   for ( int i=0 ; i<nStatesInDecNet ; i++ )
   {
      decNetStateToLattStateMap[i] = -1 ;
   }
   */

   totalTransAdded = 0 ;
   avgTransPerFrame = 0 ;
   nTransPrevFrame = 0 ;
   nTransPrevRemoval = 0 ;

   return initState ;
}


int WFSTLattice::addEntry( int lattFromState , int netToState , int inLabel , int outLabel , 
                           real score )
{
#ifdef DEBUG
   if ( (lattFromState < 0) || (lattFromState >= nStates) )
      error("WFSTLattice::addEntry - lattFromState out of range") ;
   if ( (netToState < 0) || (netToState >= nStatesInDecNet) )
      error("WFSTLattice::addEntry - netToState %d out of range" , netToState ) ;
#endif
/*
   int lattToState ;
	if ( nStates == nStatesAlloc )
	{
		nStatesAlloc += WFSTLATTICESTATESREALLOCAMOUNT ;
		stateNumOutTrans = (short *)realloc( stateNumOutTrans , nStatesAlloc*sizeof(short) ) ;
		stateIsFinal = (bool *)realloc( stateIsFinal , nStatesAlloc*sizeof(bool) ) ;
	}
  
*/
   // Changes
   addEntryBeforeMapping() ;
   int lattToState ;
   
   if ( decNetStateToLattStateMap[netToState] < 0 )
   {
      stateNumOutTrans[nStates] = 0 ;
      stateIsFinal[nStates] = false ;
      lattToState = nStates++ ;
      decNetStateToLattStateMap[netToState] = lattToState ;
   }
   else
      lattToState = decNetStateToLattStateMap[netToState] ;

   // Changes
   addEntryAfterMapping( 
	 lattFromState, lattToState, inLabel, outLabel, score ) ;
   /*
   if ( nTrans == nTransAlloc )
   {
      nTransAlloc += WFSTLATTICETRANSREALLOCAMOUNT ;
      if ( wfsaMode )
      {
         wfsaTrans = (WFSALatticeTrans *)realloc( wfsaTrans, nTransAlloc*sizeof(WFSALatticeTrans));
      }
      else
      {
         wfstTrans = (WFSTLatticeTrans *)realloc( wfstTrans, nTransAlloc*sizeof(WFSTLatticeTrans));
      }
   }

   if ( wfsaMode )
   {
      wfsaTrans[nTrans].fromState = lattFromState ;
      wfsaTrans[nTrans].toState = lattToState ;
      if ( projectInput )
         wfsaTrans[nTrans].label = inLabel ;
      else
         wfsaTrans[nTrans].label = outLabel ;
      wfsaTrans[nTrans].weight = score ;
   }
   else
   {
      wfstTrans[nTrans].fromState = lattFromState ;
      wfstTrans[nTrans].toState = lattToState ;
      wfstTrans[nTrans].inLabel = inLabel ;
      wfstTrans[nTrans].outLabel = outLabel ;
      wfstTrans[nTrans].weight = score ;
   }

   nTrans++ ;

#ifdef DEBUG
   if ( lattToState == initState )
      error("WFSTLattice::addEntry - lattToState %d == initState %d, nStates=%d" , 
            lattToState , initState , nStates ) ;
#endif
*/
   return lattToState ;
}

int WFSTLattice::addEntry( int lattFromState , int netToState , int gState, int inLabel , const int *outLabel, int nOutLabel, real score )
{
   error("WFSTLattice::addEntry(2) - base class should not call this function") ;
   // Keep compiler happy
   return -1 ;
}

void WFSTLattice::addFinalState( int state_ , real weight_ )
{
#ifdef DEBUG
   if ( (state_ < 0) || (state_ >= nStates) )
      error("WFSTLattice::addFinalState - state_ out of range") ;
#endif

   if ( stateIsFinal[state_] )
      return ;

   if ( nFinalStates == nFinalStatesAlloc )
   {
      nFinalStatesAlloc += WFSTLATTICEFINALSTATESREALLOCAMOUNT ;
      finalStates = (WFSTLatticeFinalState *)realloc( finalStates , 
                                    nFinalStatesAlloc*sizeof(WFSTLatticeFinalState) ) ;
   }

   finalStates[nFinalStates].state = state_ ;
   finalStates[nFinalStates].weight = weight_ ;
   nFinalStates++ ;

   stateIsFinal[state_] = true ;
}


void WFSTLattice::newFrame( int frame )
{
   int i ;
   
   if ( frame != (currFrame+1) )
      error("WFSTLattice::newFrame - unexpected new frame - old=%d new=%d",currFrame,frame) ;
   currFrame = frame ;

   // Changes
   resetDecNetStateToLattStateMap() ;
   /*
   for ( i=0 ; i<nStatesInDecNet ; i++ )
   {
      decNetStateToLattStateMap[i] = -1 ;
   }
   */

   // reset the final states array
   for ( i=0 ; i<nFinalStates ; i++ )
   {
      stateIsFinal[finalStates[i].state] = false ;
   }
   nFinalStates = 0 ;

#ifdef DEBUG
   for ( i=0 ; i<nStates ; i++ )
   {
      if ( stateIsFinal[i] )
         error("WFSTLattice::newFrame - stateIsFinal[i] is true after reset");
   }
#endif

   if ( currFrame > 0 )
   {
      totalTransAdded += (nTrans - nTransPrevFrame) ;
      avgTransPerFrame = (int)((real)totalTransAdded / (real)currFrame + 0.5) ;
   
      if ( doDeadEndRemoval && ((currFrame % nFramesBetweenPartialDeadEndRemoval) == 0) )
      {
         removeDeadEndTransitions() ;
      }
      
      if ( doDeadEndRemoval && ((currFrame % nFramesBetweenFullDeadEndRemoval) == 0) )
      {
         removeDeadEndTransitions( true ) ;
      }
   }
   nTransPrevFrame = nTrans ;
}


void WFSTLattice::writeLatticeFSM( const char *outFName )
{
   FILE *fd ;
   int i ;

   if ( (fd = fopen( outFName , "wb" )) == NULL )
      error("WFSTLattice::writeLatticeFSM - error opening outFName for writing") ;

   if ( wfsaMode )
   {
      // Write the WFSA transitions
      for ( i=0 ; i<nTrans ; i++ )
      {
         fprintf( fd , "%d %d %d %.3f\n" , wfsaTrans[i].fromState , wfsaTrans[i].toState ,
                                           wfsaTrans[i].label , -(wfsaTrans[i].weight) ) ;
      }
   }
   else
   {
      // Write the WFST transitions
      for ( i=0 ; i<nTrans ; i++ )
      {
         fprintf( fd , "%d %d %d %d %.3f\n" , wfstTrans[i].fromState , wfstTrans[i].toState ,
                                              wfstTrans[i].inLabel , wfstTrans[i].outLabel , 
                                              -(wfstTrans[i].weight) ) ;
      }
   }

   // Write final states
   for ( i=0 ; i<nFinalStates ; i++ )
   {
      fprintf( fd , "%d %.3f\n" , finalStates[i].state , -(finalStates[i].weight) ) ;
   }

   fclose(fd) ;
}


void WFSTLattice::registerActiveTrans( int fromState )
{
   // This is to keep a record of the number of potential transitions that are outgoing
   //   from a particular state (potential meaning that the transitions have not yet been
   //   added to the lattice). This method is called by the decoder for the hypothesis
   //   in the initial state of each HMM after all intermodel transitions have been
   //   evaluated.

#ifdef DEBUG
   if ( (fromState < 0) || (fromState >= nStates) )
      error("WFSTLattice::registerOutputTrans - fromState out of range") ;
#endif

   stateNumOutTrans[fromState]++ ;
}


void WFSTLattice::registerInactiveTrans( int fromState )
{
   // If a hypothesis in an emitting state is pruned, or a hypothesis at the end of a model
   //   is pruned prior to evaluating inter-model transitions, then we register with the
   //   lattice that its lattice fromState has 1 less outgoing transition.

#ifdef DEBUG
   if ( (fromState < 0) || (fromState >= nStates) )
      error("WFSTLattice::registerPrunedTransition - fromState out of range") ;
#endif

   stateNumOutTrans[fromState]-- ;

#ifdef DEBUG
   if ( stateNumOutTrans[fromState] < 0 )
      error("WFSTLattice::registerPrunedTransition - stateNumOutTrans[fromState] < 0") ;
#endif
}


void WFSTLattice::printLogInfo()
{
   int totalOut=0 ;
   for ( int i=0 ; i<nStates ; i++ )
   {
      totalOut += stateNumOutTrans[i] ;
   }

   LogFile::printf("WFSTLattice: totalOutTrans=%d nStates=%d nTrans=%d avgTransPerFrame=%d\n" , 
                   totalOut , nStates , nTrans , avgTransPerFrame ) ;
}


void WFSTLattice::removeDeadEndTransitions( bool doFullRemoval )
{
   int fromTrans , toTrans ;

   if ( doFullRemoval )
   {
      fromTrans = 0 ;
      toTrans = nTransPrevFrame - 1 ;
   }
   else
   {
      fromTrans = (int)(0.6 * nTransPrevRemoval + 0.5) ;
      toTrans = nTransPrevFrame - 1 ;
   }

   if ( fromTrans < 0 )
      fromTrans = 0 ;
   if ( toTrans < 0 )
      toTrans = 0 ;
   
   if ( wfsaMode )
      removeWFSADeadEndTransitions( fromTrans , toTrans ) ;
   else
      removeWFSTDeadEndTransitions( fromTrans , toTrans ) ;

   nTransPrevRemoval = nTrans ;
   nTransPrevFrame = nTrans ;
}


void WFSTLattice::removeWFSTDeadEndTransitions( int fromTrans , int toTrans )
{
#ifdef DEBUG
   if ( (fromTrans < 0) || (toTrans < fromTrans) || (toTrans >= nTrans) )
      error("WFSTLattice::removeWFSTDeadEndTransitions - fromTrans or toTrans out of range") ;
#endif

   int i , newNTrans=nTrans ;
   for ( i=toTrans ; i>=fromTrans ; i-- )
   {
      if ( (stateNumOutTrans[wfstTrans[i].toState] == 0) && 
           (stateIsFinal[wfstTrans[i].toState] == false) )
      {
         // This transition goes nowhere - remove
         stateNumOutTrans[wfstTrans[i].fromState]-- ;
#ifdef DEBUG
         if ( stateNumOutTrans[wfstTrans[i].fromState] < 0 )
            error("WFSTLattice::remWFSTDeadEndTrans - stateNumOutTrans[wfstTrans[i].fromState]<0");
#endif
         wfstTrans[i].fromState = -1 ;
         newNTrans-- ;
      }
   }

   if ( newNTrans != nTrans )
   {
      // Find location of first transition that is invalid
      int ind = -1 ;
      for ( i=fromTrans ; i<=toTrans ; i++ )
      {
         if ( wfstTrans[i].fromState < 0 )
         {
            ind = i ;
            break ;
         }
      }

#ifdef DEBUG
      if ( ind < 0 )
         error("WFSTLattice::remWFSTDeadEndTrans - ind < 0") ;
#endif

      // Shuffle down remaining transitions (including those above toTrans) to remove invalid ones.
      for ( i=(ind+1) ; i<nTrans ; i++ )
      {
         if ( wfstTrans[i].fromState >= 0 )
         {
            wfstTrans[ind++] = wfstTrans[i] ;
         }
      }
      
      if ( ind != newNTrans )
         error("WFSTLattice::removeWFSTDeadEndTransitions - ind != newNTrans") ;
      nTrans = ind ;
   }
}

void WFSTLattice::removeWFSADeadEndTransitions( int fromTrans , int toTrans )
{
   error("WFSTLattice::removeWFSADeadEndTransitions - not implemented yet") ;
}

// Changes
void WFSTLattice::resetDecNetStateToLattStateMap()
{
   for ( int i=0 ; i<nStatesInDecNet ; i++ )
   {
      decNetStateToLattStateMap[i] = -1 ;
   }
}

// Changes
void WFSTLattice::addEntryBeforeMapping()
{
   if ( nStates == nStatesAlloc )
   {
      nStatesAlloc += WFSTLATTICESTATESREALLOCAMOUNT ;
      stateNumOutTrans = (short *)realloc( 
	    stateNumOutTrans , nStatesAlloc*sizeof(short) ) ;
      stateIsFinal = (bool *)realloc( 
	    stateIsFinal , nStatesAlloc*sizeof(bool) ) ;
   }
}

// Changes
void WFSTLattice::addEntryAfterMapping( 
	 int lattFromState, int lattToState, int inLabel, int outLabel, real score )
{
   if ( nTrans == nTransAlloc )
   {
      nTransAlloc += WFSTLATTICETRANSREALLOCAMOUNT ;
      if ( wfsaMode )
      {
         wfsaTrans = (WFSALatticeTrans *)realloc( wfsaTrans, nTransAlloc*sizeof(WFSALatticeTrans));
      }
      else
      {
         wfstTrans = (WFSTLatticeTrans *)realloc( wfstTrans, nTransAlloc*sizeof(WFSTLatticeTrans));
      }
   }

   if ( wfsaMode )
   {
      wfsaTrans[nTrans].fromState = lattFromState ;
      wfsaTrans[nTrans].toState = lattToState ;
      if ( projectInput )
         wfsaTrans[nTrans].label = inLabel ;
      else
         wfsaTrans[nTrans].label = outLabel ;
      wfsaTrans[nTrans].weight = score ;
   }
   else
   {
      wfstTrans[nTrans].fromState = lattFromState ;
      wfstTrans[nTrans].toState = lattToState ;
      wfstTrans[nTrans].inLabel = inLabel ;
      wfstTrans[nTrans].outLabel = outLabel ;
      wfstTrans[nTrans].weight = score ;
   }

   nTrans++ ;

#ifdef DEBUG
   if ( lattToState == initState )
      error("WFSTLattice::addEntryAfterMapping - lattToState %d == initState %d, nStates=%d" , 
            lattToState , initState , nStates ) ;
#endif
}

}
