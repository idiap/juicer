/*
 * Copyright 2006 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#include "WFSTLatticeOnTheFly.h"
#include "LogFile.h"

/*
	Author:		Octavian Cheng (ocheng@idiap.ch)
	Date:		7 June 2006
*/


using namespace Torch;

namespace Juicer {


// Changes
// WFSTLatticeOnTheFly Implementation
WFSTLatticeOnTheFly::WFSTLatticeOnTheFly( int nStatesInDecNet_ , bool wfsaMode_ , bool projectInput_ ) : WFSTLattice ( nStatesInDecNet_ , wfsaMode_ , projectInput_ , false ) 
{}

WFSTLatticeOnTheFly::~WFSTLatticeOnTheFly() {}

int WFSTLatticeOnTheFly::addEntry( int lattFromState , int netToState , int inLabel , int outLabel , real score )
{
   // For safety
   error("WFSTLatticeOnTheFly::addEntry - on-the-fly lattice should not call this function") ;
   // Keep compiler happy
   return -1 ;
}

// A list of outLabels is possible when eps transitions in the G transducer 
// also have outLabels, although for gramgen it will not be the case.
// Keep it generalised.
int WFSTLatticeOnTheFly::addEntry( int lattFromState , int netToState , int gState, int inLabel , const int *outLabel , int nOutLabel , real score )
{
#ifdef DEBUG
   if ( (lattFromState < 0) || (lattFromState >= nStates) )
      error("WFSTLatticeOnTheFly::addEntry - lattFromState out of range") ;
   if ( (netToState < 0) || (netToState >= nStatesInDecNet) )
      error("WFSTLatticeOnTheFly::addEntry - netToState %d out of range" , netToState ) ;
   if ( nOutLabel <= 0 )
      error("WFSTLatticeOnTheFly::addEntry - nOutLabel <= 0") ;
#endif
   
   int lattToState ;
  
   // Just one output label
   if ( nOutLabel == 1 )  {
      addEntryBeforeMapping() ;
  
      // Find the lattToState if already created
      // Changes
      DecNetStateToLattStateMapOnTheFly::iterator netToStateMapIter ; 
      netToStateMapIter = decNetStateToLattStateMapOnTheFly.find( netToState ) ;
      if ( netToStateMapIter != decNetStateToLattStateMapOnTheFly.end() )  {
	 // CoL state matched
	 // Changes
	 _InnerMap &gStateMap = (*netToStateMapIter).second ;
	 _InnerMap::iterator gStateMapIter = gStateMap.find( gState ) ;
	 if ( gStateMapIter != gStateMap.end() )  {
	    // G state also matched
	    lattToState = (*gStateMapIter).second ;
	 }
	 else  {
	    // G state not found
	    stateNumOutTrans[nStates] = 0 ;
	    stateIsFinal[nStates] = false ;
	    lattToState = nStates++ ;
	    
	    insertInnerMap( gStateMap, gState, lattToState ) ;
	 }
      }
      else  {
	 // CoL state not found
	 stateNumOutTrans[nStates] = 0 ;
	 stateIsFinal[nStates] = false ;
	 lattToState = nStates++ ;

	 // Changes
	 _InnerMap gStateMap ;
	 insertInnerMap( gStateMap, gState, lattToState ) ;
	 insertOuterMap( netToState, gStateMap ) ;
      }
      
      addEntryAfterMapping( 
	    lattFromState, lattToState, inLabel, outLabel[0], score ) ;

#ifdef DEBUG
      // Check if insertion is successful in the map
      checkLattToState( netToState, gState, lattToState ) ;
#endif

   }
   else  {
      // More than 1 outlabels
      // First, check if lattToState has already existed
      bool isFound = false ;
      
      // Changes
      DecNetStateToLattStateMapOnTheFly::iterator netToStateMapIter ; 
      _InnerMap *gStateMap ;
      _InnerMap::iterator gStateMapIter ;

      netToStateMapIter = decNetStateToLattStateMapOnTheFly.find( netToState ) ;
      if ( netToStateMapIter != decNetStateToLattStateMapOnTheFly.end() )  {
	 // CoL state matched
	 gStateMap = &( (*netToStateMapIter).second ) ;
	 gStateMapIter = gStateMap->find( gState ) ;
	 if ( gStateMapIter != gStateMap->end() )  {
	    // G state also matched
	    isFound = true ;
	    lattToState = (*gStateMapIter).second ;
	 }
      }

      // If the final lattToState has already existed
      if ( isFound )  {
	 int tmpLattFromState = lattFromState ;
	 int tmpLattToState ;
	 int i ;
	
	 // Loop to the second last outLabels
	 for ( i = 0 ; i < nOutLabel - 1 ; i++ )  {
	    addEntryBeforeMapping() ;
	    // Create new states for in-between arcs
	    stateNumOutTrans[nStates] = 0 ;
	    stateIsFinal[nStates] = false ;
	    tmpLattToState = nStates++ ;
	    
	    if ( i == 0 )
	       addEntryAfterMapping(
		     tmpLattFromState, tmpLattToState, inLabel, outLabel[i], 
		     score) ;
	    else
	       addEntryAfterMapping(
		     tmpLattFromState, tmpLattToState, WFST_EPSILON, 
		     outLabel[i], 0.0 ) ;

	    tmpLattFromState = tmpLattToState ;
	 }

	 // Link to the lattToState (the found one)
	 addEntryAfterMapping(
	       tmpLattFromState, lattToState, WFST_EPSILON, outLabel[i], 
	       0.0 ) ;

      }
      else  {
	 int tmpLattFromState = lattFromState ;
	 int tmpLattToState ;
	 
	 // If the final lattToState has not yet existed
	 for ( int i = 0 ; i < nOutLabel ; i++ )  {
	    addEntryBeforeMapping() ;

	    stateNumOutTrans[nStates] = 0 ;
	    stateIsFinal[nStates] = false ;
	    tmpLattToState = nStates++ ;

	    if ( i == 0 )
	       addEntryAfterMapping( 
		     tmpLattFromState, tmpLattToState, inLabel, outLabel[i], 
		     score) ;
	    else
	       addEntryAfterMapping( 
		     tmpLattFromState, tmpLattToState, WFST_EPSILON, 
		     outLabel[i], 0.0 ) ;
	    
	    tmpLattFromState = tmpLattToState ;
	 }

	 lattToState = tmpLattToState ;

	 // Now add the lattToState to map
	 if ( netToStateMapIter != decNetStateToLattStateMapOnTheFly.end() )  {
	    // CoL state has been found
#ifdef DEBUG
	    if ( gStateMapIter != gStateMap->end() ) 
	       error("WFSTLatticeOnTheFly::addEntry - both maps exist") ;
#endif
	    insertInnerMap( *gStateMap, gState, lattToState ) ;
	 }
	 else  {
	    // CoL Not Found
	    _InnerMap newGStateMap ;

	    insertInnerMap( newGStateMap, gState, lattToState ) ;
	    insertOuterMap( netToState, newGStateMap ) ;
	 }

#ifdef DEBUG
	 checkLattToState( netToState, gState, lattToState ) ;
#endif

      }

   }
   
   return lattToState ;
}

void WFSTLatticeOnTheFly::resetDecNetStateToLattStateMap()
{
   decNetStateToLattStateMapOnTheFly.clear() ;
}

void WFSTLatticeOnTheFly::insertInnerMap( _InnerMap &innerMap, int gState, int lattToState )
{
   pair< _InnerMap::iterator, bool > status =
      innerMap.insert( pair< int, int >( gState, lattToState ) ) ;

      
#ifdef DEBUG
   if ( status.second == false )  
      error("WFSTLatticeOnTheFly::insertInnerMap - insertion failed") ;
#endif

}


void WFSTLatticeOnTheFly::insertOuterMap( int netToState, _InnerMap &innerMap )
{
   // Changes
   pair< DecNetStateToLattStateMapOnTheFly::iterator, bool > status = 
      decNetStateToLattStateMapOnTheFly.insert(pair<int, _InnerMap >(netToState,innerMap)) ;
	 
#ifdef DEBUG
   if ( status.second == false )
      error("WFSTLatticeOnTheFly::insertOuterMap - insertion failed") ;
#endif
}

	 
// Check if the entry (netToState, gState, lattToState) is inserted
void WFSTLatticeOnTheFly::checkLattToState( int netToState, int gState, int lattToState )
{
   // Changes
   DecNetStateToLattStateMapOnTheFly::iterator firstMapIter = 
      decNetStateToLattStateMapOnTheFly.find( netToState ) ;
 
   if ( firstMapIter == decNetStateToLattStateMapOnTheFly.end() )
      error("WFSTLatticeOnTheFly::checkLattToState - first map check failed") ;

   // Changes
   _InnerMap &secondMap = (*firstMapIter).second ;
   _InnerMap::iterator secondMapIter = secondMap.find( gState ) ;
   if ( secondMapIter == secondMap.end() )
      error("WFSTLatticeOnTheFly::checkLattToState - second map check failed") ;

   if ( (*secondMapIter).second != lattToState )
      error("WFSTLatticeOnTheFly::checkLattToState - lattToState not matched") ;

}

}
