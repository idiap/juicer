/*
 * Copyright 2006 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#ifndef WFST_LATTICEONTHEFLY_INC
#define WFST_LATTICEONTHEFLY_INC

#include "general.h"
#include "WFSTGeneral.h"
#include "WFSTLattice.h"

// Changes Octavian
#include <map>
using namespace std ;

/*
 	Author:		Octavian Cheng (ocheng@idiap.ch)
	Date:		7 June 2006
*/

namespace Juicer {

/**
 * For on-the-fly decoder
 */
class WFSTLatticeOnTheFly : public WFSTLattice
{
public:
   WFSTLatticeOnTheFly( 
	 int nStatesInDecNet_ , bool wfsaMode_ , bool projectInput_ ) ;
   virtual ~WFSTLatticeOnTheFly() ;
   virtual int addEntry( 
	 int lattFromState , int netToState , int inLabel , int outLabel , 
	 real score ) ;
   virtual int addEntry( 
	 int lattFromState , int netToState , int gState, int inLabel , 
	 const int *outLabel, int nOutLabel, real score ) ;

protected:
   // This map is for mapping a net state to a latt state
   // In static network, in each time frame, the map coverts a state in the
   // transducer to a lattice state in the lattice.
   // i.e. transducer state => lattice state
   // In dynamic (on-the-fly) decoding, in each time frame, the map coverts
   // a pair of transducer states, which consists of a CoL state and a G 
   // state, to a lattice state.
   // i.e. (CoL state, G state) => lattice state
   // In the following declaration: map< int, map <int, int> > , 
   // The first int is the CoL net state; 
   // The second int is G the net state; 
   // The third int is the latt state.
   // Changes 20050328
   typedef map< int, int > _InnerMap ;
   typedef map< int, _InnerMap > DecNetStateToLattStateMapOnTheFly ;
 
   // Changes
   DecNetStateToLattStateMapOnTheFly decNetStateToLattStateMapOnTheFly ;
   
   virtual void resetDecNetStateToLattStateMap() ;

   void insertInnerMap( _InnerMap &innerMap, int gState, int lattToState ) ;
   void insertOuterMap( int netToState, _InnerMap &innerMap ) ;
   void checkLattToState( int netToState, int gState, int lattToState ) ;
   
} ;

}

#endif
