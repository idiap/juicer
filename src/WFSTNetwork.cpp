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

#include <assert.h>

#include "WFSTNetwork.h"
#include "log_add.h"

using namespace Torch;

/*
	Author:	Darren Moore (moore@idiap.ch)
	Date:		6 October 2004
	$Id: WFSTNetwork.cpp,v 1.14.4.1 2006/06/21 12:34:13 juicer Exp $
*/

/*
 	Author: Octavian Cheng (ocheng@idiap.ch)
	Date:		7 June 2006
*/

namespace Juicer {


//****** WFSTAlphabet Implementation ******


WFSTAlphabet::WFSTAlphabet()
{
   maxLabel = -1 ;
   nLabels = 0 ;
   nLabelsAlloc = 0 ;
   labels = NULL ;
   nAux = 0 ;
   isAux = NULL ;
   fromBinFile = false ;
}


WFSTAlphabet::WFSTAlphabet( const char *symbolsFilename )
{
   FILE *fd ;
   char *line , *symStr ;
   int symID , i ;

   maxLabel = -1 ;
   nLabels = 0 ;
   nLabelsAlloc = 0 ;
   labels = NULL ;
   nAux = 0 ;
   isAux = NULL ;

   if ( symbolsFilename == NULL )
      error("WFSTAlphabet::WFSTAlphabet - symbolsFilename is NULL") ;

   if ( (fd = fopen( symbolsFilename , "rb" )) == NULL )
      error("WFSTAlphabet::WFSTAlphabet - error opening symbols filename %s" , symbolsFilename ) ;

   line = new char[10000] ;
   symStr = new char[10000] ;
   while ( (line = fgets( line , 10000 , fd )) != NULL )
   {
      if ( sscanf( line , "%s %d" , symStr , &symID ) != 2 )
         continue ;

      if ( symID >= nLabelsAlloc )
      {
         labels = (char **)realloc( labels , (symID+1000)*sizeof(char *) ) ;
         for ( i=nLabelsAlloc ; i<(symID+1000) ; i++ )
            labels[i] = NULL ;
         nLabelsAlloc = symID + 1000 ;
      }

      if ( labels[symID] != NULL )
         error("WFSTAlphabet::WFSTAlphabet - duplicate symbol detected for %s" , symStr ) ;

      labels[symID] = new char[strlen(symStr)+1] ;
      strcpy( labels[symID] , symStr ) ;

      nLabels++ ;
      if ( symID > maxLabel )
         maxLabel = symID ;
   }

   // Now allocate memory for isAux and do a pass through labels to determine
   //   which are auxiliary symbols.
   isAux = new bool[maxLabel+1] ;
   for ( i=0 ; i<=maxLabel ; i++ )
   {
      // An auxiliary symbol has first char as '#'
      if ( (labels[i] != NULL) && (labels[i][0] == '#') )
      {
         isAux[i] = true ;
         nAux++ ;
      }
      else
         isAux[i] = false ;
   }

   delete [] line ;
   fclose( fd ) ;

   fromBinFile = false ;
}


WFSTAlphabet::~WFSTAlphabet()
{
   int i ;

   if ( labels != NULL )
   {
      for ( i=0 ; i<nLabelsAlloc ; i++ )
         delete [] labels[i] ;
      if ( fromBinFile )
         delete [] labels ;
      else
         free( labels ) ;
   }
   delete [] isAux ;
}


const char *WFSTAlphabet::getLabel( int index )
{
   if ( (index < 0) || (index > maxLabel) )
   {
      error("WFSTAlphabet::getLabel - index out of range") ;
      return NULL ;
   }

   if ( labels[index] == NULL )
   {
      error("WFSTAlphabet::getLabel - labels[%d] is NULL" , index ) ;
      return NULL ;
   }

   return labels[index] ;
}


int WFSTAlphabet::getIndex( const char *label )
{
   if ( (label == NULL) || (label[0] == '\0') )
      return -1 ;

   for ( int i=0 ; i<=maxLabel ; i++ )
   {
      if ( labels[i] == NULL )
         continue ;
      if ( strcmp( label , labels[i] ) == 0 )
         return i ;
   }

   return -1 ;
}


bool WFSTAlphabet::isAuxiliary( int index )
{
   if ( (index < 0) || (index > maxLabel) )
   {
      error("WFSTAlphabet::isAuxiliary - index out of range") ;
      return false ;
   }
   if ( labels[index] == NULL )
   {
      error("WFSTAlphabet::isAuxiliary - labels[%d] is NULL" , index ) ;
      return false ;
   }

   return isAux[index] ;
}


void WFSTAlphabet::outputText()
{
   int i ;
   printf("****** START WFSTAlphabet ******\n") ;
   printf("maxLabel=%d nLabels=%d nLabelsAlloc=%d nAux=%d\n" ,
           maxLabel , nLabels , nLabelsAlloc , nAux ) ;
   for ( i=0 ; i<=maxLabel ; i++ )
   {
      if ( labels[i] != NULL )
         printf("%-25s %d\n" , labels[i] , i ) ;
   }
   printf("****** END WFSTAlphabet ******\n") ;
}


void WFSTAlphabet::write( FILE *fd , bool outputAux )
{
   for ( int i=0 ; i<=maxLabel ; i++ )
   {
      if ( labels[i] == NULL )
         continue ;
      if ( (outputAux == false) && isAuxiliary(i) )
         continue ;

      writeFSMSymbol( fd , labels[i] , i ) ;
   }
}


void WFSTAlphabet::writeBinary( FILE *fd )
{
   // 0. Write ID
   char id[5] ;
   strcpy( id , "JWAL" ) ;
   fwrite( (int *)id , sizeof(int) , 1 , fd ) ;

   // 1. Write maxLabel, nLabels
   fwrite( &maxLabel , sizeof(int) , 1 , fd ) ;
   fwrite( &nLabels , sizeof(int) , 1 , fd ) ;

   // 2. Write labels array
   if ( maxLabel >= 0 )
   {
      int i , len ;
      for ( i=0 ; i<=maxLabel ; i++ )
      {
         if ( labels[i] == NULL )
         {
            len = 0 ;
            fwrite( &len , sizeof(int) , 1 , fd ) ;
         }
         else
         {
            len = strlen(labels[i]) + 1 ;
            fwrite( &len , sizeof(int) , 1 , fd ) ;
            fwrite( labels[i] , sizeof(char) , len , fd ) ;
         }
      }

      // 3. Write nAux and isAux array
      fwrite( &nAux , sizeof(int) , 1 , fd ) ;
      fwrite( isAux , sizeof(bool) , maxLabel+1 , fd ) ;
   }
}


void WFSTAlphabet::readBinary( FILE *fd )
{
   // 0. Read ID
   char id[5] ;
   if ( fread( (int *)id , sizeof(int) , 1 , fd ) != 1 )
      error("WFSTAlphabet::readBinary - error reading ID") ;
   id[4] = '\0' ;
   if ( strcmp( id , "JWAL" ) != 0 )
      error("WFSTAlphabet::readBinary - invalid ID") ;

   // 1. Read maxLabel, nLabels
   if ( fread( &maxLabel , sizeof(int) , 1 , fd ) != 1 )
      error("WFSTAlphabet::readBinary - error reading maxLabel") ;
   if ( fread( &nLabels , sizeof(int) , 1 , fd ) != 1 )
      error("WFSTAlphabet::readBinary - error reading nLabels") ;

   if ( maxLabel >= 0 )
   {
      // 2. Allocate and read labels array
      nLabelsAlloc = maxLabel + 1 ;
      labels = new char*[nLabelsAlloc] ;
      int i , len ;
      for ( i=0 ; i<=maxLabel ; i++ )
      {
         if ( fread( &len , sizeof(int) , 1 , fd ) != 1 )
            error("WFSTAlphabet::readBinary - error reading label length") ;
         if ( len > 0 )
         {
            labels[i] = new char[len] ;
            if ( (int)fread( labels[i] , sizeof(char) , len , fd ) != len )
               error("WFSTAlphabet::readBinary - error reading label string") ;
            if ( labels[i][len-1] != '\0' )
               error("WFSTAlphabet::readBinary - last char of label string was not nul") ;
         }
         else
            labels[i] = NULL ;
      }

      // 3. Read nAux and isAux array
      if ( fread( &nAux , sizeof(int) , 1 , fd ) != 1 )
         error("WFSTAlphabet::readBinary - error reading nAux") ;
      isAux = new bool[maxLabel+1] ;
      if ( (int)fread( isAux , sizeof(bool) , (maxLabel+1) , fd ) != (maxLabel+1) )
         error("WFSTAlphabet::readBinary - error reading isAux array") ;
   }

   fromBinFile = true ;
}


//****** WFSTNetwork Implementation ******

WFSTNetwork::WFSTNetwork()
{
   inputAlphabet = NULL ;
   outputAlphabet = NULL ;
   stateAlphabet = NULL ;

   initState = -1 ;
   maxState = -1 ;

   nStates = 0 ;
   nStatesAlloc = 0 ;
   states = NULL ;

   nFinalStates = 0 ;
   nFinalStatesAlloc = 0 ;
   finalStates = NULL ;

   nTransitions = 0 ;
   nTransitionsAlloc = 0 ;
   transitions = NULL ;
   maxOutTransitions = 0 ;

   fromBinFile = false ;
   transWeightScalingFactor = 1.0 ;
   insPenalty = 0.0 ;
   wordEndMarker = -1 ;
   silMarker = -1;
   spMarker = -1;
}


WFSTNetwork::WFSTNetwork( real transWeightScalingFactor_ , real insPenalty_ )
{
   inputAlphabet = NULL ;
   outputAlphabet = NULL ;
   stateAlphabet = NULL ;

   initState = -1 ;
   maxState = -1 ;

   nStates = 0 ;
   nStatesAlloc = 0 ;
   states = NULL ;

   nFinalStates = 0 ;
   nFinalStatesAlloc = 0 ;
   finalStates = NULL ;

   nTransitions = 0 ;
   nTransitionsAlloc = 0 ;
   transitions = NULL ;
   maxOutTransitions = 0 ;

   fromBinFile = false ;
   transWeightScalingFactor = transWeightScalingFactor_ ;
   insPenalty = insPenalty_;
   wordEndMarker = -1 ;
   silMarker = -1;
   spMarker = -1;
}

// Changes Octavian 20060523
// Adding a variable for remove aux symbols
// removeAuxOption:
// REMOVEBOTH => 	remove aux symbols from both alphabet.
// REMOVEINPUT =>	remove aux symbols only in the input alphabet
// NOTREMOVE =>		Not remove any aux symbols
// wordEndMarker will still be the max of both alphabet + 1

WFSTNetwork::WFSTNetwork(
	const char *wfstFilename , const char *inSymsFilename ,
	const char *outSymsFilename ,
    real transWeightScalingFactor_ , real insPenalty_ ,
	RemoveAuxOption removeAuxOption
	)
{
   inputAlphabet = NULL ;
   outputAlphabet = NULL ;
   stateAlphabet = NULL ;

   initState = -1 ;
   maxState = -1 ;

   nStates = 0 ;
   nStatesAlloc = 0 ;
   states = NULL ;

   nFinalStates = 0 ;
   nFinalStatesAlloc = 0 ;
   finalStates = NULL ;

   nTransitions = 0 ;
   nTransitionsAlloc = 0 ;
   transitions = NULL ;
   maxOutTransitions = 0 ;

   transWeightScalingFactor = transWeightScalingFactor_ ;
   insPenalty = insPenalty_ ;
   wordEndMarker = -1 ;
   silMarker = -1;
   spMarker = -1;

   FILE *fd ;

   if ( (fd = fopen( wfstFilename , "rb" )) == NULL )
      error("WFSTNetwork::WFSTNetwork - error opening wfstFilename") ;

   int cnt , i ;
   int from , to , in , out , final , maxOutLab=-1 , maxInLab=-1;
   float weight ;
   char *line ;

   line = new char[10000] ;
   while ( fgets( line , 10000 , fd ) != NULL )
   {
      if ( (cnt = sscanf( line , "%d %d %d %d %f" , &from , &to , &in , &out , &weight )) != 5 )
      {
         if ( (cnt = sscanf( line , "%d %d %d %d" , &from , &to , &in , &out )) != 4 )
         {
            if ( (cnt = sscanf( line , "%d %f" , &final , &weight )) != 2 )
            {
               if ( (cnt = sscanf( line , "%d" , &final )) != 1 )
               {
                  // Blank line or line with invalid format - just go to next line.
                  continue ;
               }
               else
                  weight = 0.0 ;
            }

            // Final state line
            if ( nFinalStates == nFinalStatesAlloc )
            {
               finalStates = (WFSTFinalState *)realloc( finalStates ,
                                        (nFinalStatesAlloc+1000)*sizeof(WFSTFinalState) ) ;
               nFinalStatesAlloc += 1000 ;
            }

            finalStates[nFinalStates].id = final ;
            finalStates[nFinalStates].weight = (real)(-weight * transWeightScalingFactor) ;
            nFinalStates++ ;
            continue ;
         }
         else
            weight = 0.0 ;
      }

      if ( (from < 0) || (to < 0) || (in < 0) || (out < 0) )
         error("WFSTNetwork::WFSTNetwork - something < 0. %d %d %d %d",from,to,in,out) ;

      if ( initState < 0 )
         initState = from ;   // init state is source state in first line of file

      if ( from > maxState )
         maxState = from ;
      if ( to > maxState )
         maxState = to ;

      // Add the new transition to the array of transitions
      if ( nTransitions == nTransitionsAlloc )
      {
         nTransitionsAlloc += 1000000 ;
         transitions = (WFSTTransition *)realloc(
             transitions , nTransitionsAlloc * sizeof(WFSTTransition)
         ) ;
         if ( transitions == NULL )
            error("WFSTNetwork::WFSTNetwork - transitions realloc failed") ;
         for ( i=nTransitions ; i<nTransitionsAlloc ; i++ )
            initWFSTTransition( transitions + i ) ;
      }

      transitions[nTransitions].id = nTransitions ;
      //transitions[nTransitions].fromState = from ;
      transitions[nTransitions].toState = to ;
      transitions[nTransitions].inLabel = in ;
      transitions[nTransitions].outLabel = out ;

      // FSM weights are -ve log
      transitions[nTransitions].weight = (real)(-weight * transWeightScalingFactor ) ;

      // This is implemented as a word insertion penalty as is normal.
      // It could conceivably also be a model insertion penalty.
      if (out > 0)
          transitions[nTransitions].weight += insPenalty;

      ++nTransitions ;

      if ( in > maxInLab ) {
         maxInLab = in;
      }
      if ( out > maxOutLab ) {
         maxOutLab = out;
      }

      // Make sure we have state entries for both from and to states in the
      // new transition.
      if ( maxState >= nStatesAlloc )
      {
          // Was running out of memory for large transducers...
          //int nAlloc = maxState + 1000;
          int nAlloc = maxState * 2;
          states = (WFSTState *)realloc(
              states, nAlloc * sizeof(WFSTState)
          );
          assert(states);
          for ( i=nStatesAlloc ; i<nAlloc ; i++ )
              initWFSTState( states + i ) ;
          nStatesAlloc = nAlloc ;
      }

      // Initialise the from and to state labels if not already done.
      if ( states[from].label < 0 )
         states[from].label = from ;
      else if ( states[from].label != from )
         error("WFSTNetwork::WFSTNetwork - from state %d label mismatch" , from ) ;

      if ( states[to].label < 0 )
         states[to].label = to ;
      else if ( states[to].label != to )
         error("WFSTNetwork::WFSTNetwork - to state %d label mismatch" , to ) ;

      // Add the new transition index to the list in the from state
      int ind = (states[from].nTrans)++ ;
      states[from].trans = (int *)realloc( states[from].trans , states[from].nTrans*sizeof(int) ) ;
      states[from].trans[ind] = nTransitions - 1 ;
      if ( states[from].nTrans > maxOutTransitions )
         maxOutTransitions = states[from].nTrans ;
   }

   for ( i=0 ; i<nFinalStates ; i++ )
   {
      if ( (finalStates[i].id < 0) || (finalStates[i].id > maxState) )
         error("WFSTNetwork::WFSTNetwork - finalState[%d].id out of range" , i ) ;
      if ( states[finalStates[i].id].label < 0 )
         error("WFSTNetwork::WFSTNetwork - finalState[%d] state label < 0" , i ) ;

      states[finalStates[i].id].finalInd = i ;
   }

   // Now create the input and/or output alphabets
   if ( inSymsFilename != NULL )
   {
      inputAlphabet = new WFSTAlphabet( inSymsFilename ) ;
      if ( maxInLab > inputAlphabet->getMaxLabel() )
         error("WFSTNetwork::WFSTNetwork - maxInLab > inputAlphabet->getMaxLabel()");
      maxInLab = inputAlphabet->getMaxLabel();
   }
   if ( outSymsFilename != NULL )
   {
      outputAlphabet = new WFSTAlphabet( outSymsFilename ) ;
      if ( maxOutLab > outputAlphabet->getMaxLabel() ) {
         error("WFSTNetwork::WFSTNetwork - maxOutLab=%d > outputAlphabet->getMaxLabel()=%d",
               maxOutLab, outputAlphabet->getMaxLabel() );
      }
      maxOutLab = outputAlphabet->getMaxLabel();
   }

   wordEndMarker = maxInLab + 1;
   if ( wordEndMarker <= maxOutLab ) {
      wordEndMarker = maxOutLab + 1;
   }

   // **********************************
   // Changes Octavian 20060523
   switch ( removeAuxOption )  {
      case REMOVEBOTH:
	 if ( (inputAlphabet != NULL) && (outputAlphabet != NULL) )
	 {
	    // Remove auxiliary symbols from the WFST (replace with epsilon)
	    removeAuxiliarySymbols( true ) ;
	 }
	 break ;
      case REMOVEINPUT:
	 if ( inputAlphabet != NULL )
	 {
	    removeAuxiliaryInputSymbols( true ) ;
	 }
	 break ;
      case NOTREMOVE:
	 break ;
      default:
	 error ("WFSTNetwork::WFSTNetwork - Invalid removeAuxOption") ;
	 break ;
   }
   /*
   if ( (inputAlphabet != NULL) && (outputAlphabet != NULL) )
   {
      // Remove auxiliary symbols from the WFST (replace with epsilon)
      removeAuxiliarySymbols( true ) ;
   }
   */
   // *************************************

   delete [] line ;
   fclose( fd ) ;

   fromBinFile = false ;
   nStates = (maxState+1) ;

   // VW: record sil and sp indices for word end pruning
   for ( i=0 ; i<inputAlphabet->getNumLabels() ; i++ ){
	   if ( strcmp( inputAlphabet->getLabel( i ) , "sil" ) == 0 )
	   {
		   silMarker = i ;
	   }
	   if ( strcmp( inputAlphabet->getLabel( i ) , "sp" ) == 0 )
	   {
		   spMarker = i ;
	   }
   }
   //printf( "sil index = %d\n sp index  = %d\n" , silMarker , spMarker );
}


WFSTNetwork::~WFSTNetwork()
{
   int i ;

   if ( states != NULL )
   {
      for ( i=0 ; i<=maxState ; i++ )
      {
         if ( states[i].trans != NULL )
         {
            if ( fromBinFile )
               delete [] states[i].trans ;
            else
               free( states[i].trans ) ;
         }
      }

      if ( fromBinFile )
         delete [] states ;
      else
         free( states ) ;
   }

   if ( transitions != NULL )
   {
      if ( fromBinFile )
         delete [] transitions ;
      else
         free( transitions ) ;
   }

   if ( finalStates != NULL )
   {
      if ( fromBinFile )
          delete [] finalStates ;
      else
          free( finalStates ) ;
   }

   delete inputAlphabet ;
   delete outputAlphabet ;
   delete stateAlphabet ;
}


void WFSTNetwork::getTransitions(
    WFSTTransition *prev , int *nNext , WFSTTransition **next
)
{
#ifdef DEBUG
   if ( nNext == NULL )
      error("WFSTNetwork::getTransitions - nNext is NULL") ;
   if ( next == NULL )
      error("WFSTNetwork::getTransitions - next is NULL") ;
#endif

   int state ;
   if ( prev == NULL )
      state = initState ;
   else
      state = prev->toState ;

#ifdef DEBUG
   if ( (state < 0) || (state > maxState) )
      error("WFSTNetwork::getTransitions - state out of range") ;
#endif

   int n = states[state].nTrans ;
   int* trans = states[state].trans;
   for ( int i=0 ; i<n ; i++ )
   {
#ifdef DEBUG
      if ( (states[state].trans[i] < 0) || (states[state].trans[i] >= nTransitions) )
         error("WFSTNetwork::getTransitions - states[state].trans[i] invalid") ;
#endif
      next[i] = transitions + trans[i];
   }
   *nNext = n;
}

/**
 * Alternative getTransitions() that just returns a pointer rather
 * than copy an array of pointers.
 */
int WFSTNetwork::getTransitions(
    WFSTTransition *prev, WFSTTransition **next
)
{
    int state ;
    if ( prev == NULL )
        state = initState ;
    else
        state = prev->toState ;
    if (states[state].nTrans > 0)
        *next = transitions + states[state].trans[0] ;
    return states[state].nTrans ;
}

#if 0
// Changes by Octavian
int WFSTNetwork::getNumTransitionsOfOneState ( int state )
{
#ifdef DEBUG
   if (( state < 0 ) || ( state >= nStates ))  {
      error("WFSTNetwork::getNumTransitionsOfOneState - state out of range") ;
   }
#endif
   return states[state].nTrans ;
}

// Changes
bool WFSTNetwork::isFinalState( int stateIndex )
{
#ifdef DEBUG
   if (( stateIndex < 0 ) || ( stateIndex >= nStates ))
      error("WFSTNetwork::isFinalState - state out of range") ;
#endif
   if ( states[stateIndex].finalInd < 0 )
      return false ;
   else
      return true ;
}

// Changes
real WFSTNetwork::getFinalStateWeight( int stateIndex )
{
#ifdef DEBUG
   if (( stateIndex < 0 ) || ( stateIndex >= nStates ))
      error("WFSTNetwork::getFinalStateWeight - stateIndex out of range") ;
#endif
   int ind = states[stateIndex].finalInd ;

#ifdef DEBUG
   if ( ind < 0 )
   {
      warning("WFSTNetwork::getFinalStateWeight - called for non-final state") ;
      return LOG_ZERO ;
   }
   else
#endif
   {
      return finalStates[ind].weight ;
   }
}
#endif

// Changes by Octavian
const int *WFSTNetwork::getTransitions( const WFSTTransition *prev , int *nNext )
{
#ifdef DEBUG
   if ( nNext == NULL )
      error("WFSTNetwork::getTransitions (2) - nNext is NULL") ;
#endif

   int state ;
   if ( prev == NULL )
      state = initState ;
   else
      state = prev->toState ;

#ifdef DEBUG
   if ( (state < 0) || (state > maxState) )
      error("WFSTNetwork::getTransitions (2) - state out of range") ;
#endif

   *nNext = states[state].nTrans ;
   return states[state].trans ;

}

// Changes by Octavian
WFSTTransition *WFSTNetwork::getOneTransition( int transIndex )
{
#ifdef DEBUG
   if (( transIndex < 0 ) || ( transIndex >= nTransitions ))
      error("WFSTNetwork::getOneTransition - transIndex out of range") ;
#endif
   return ( transitions + transIndex ) ;
}

// Changes by Octavian
int WFSTNetwork::getInfoOfOneTransition( const int gState, const int n, real *weight, int *toState, int *outLabel )
{

#ifdef DEBUG
   if (( gState < 0 ) || ( gState >= nStates ))
      error("WFSTNetwork::getInfoOfOneTransition - gState out of range") ;

   int nTrans = states[gState].nTrans ;
   if (( n < 0 ) || ( n >= nTrans ))
      error("WFSTNetwork::getInfoOfOneTransition - n = %d out of range (state = %d)", n , gState ) ;
#endif

   int transIndex = states[gState].trans[n] ;

   *weight = transitions[transIndex].weight ;
   *toState = transitions[transIndex].toState ;
   *outLabel = transitions[transIndex].outLabel ;
   return ( transitions[transIndex].inLabel ) ;
}

// Changes Octavian 20060616
int WFSTNetwork::getInLabelOfOneTransition( const int gState , const int n )
{
#ifdef DEBUG
   if (( gState < 0 ) || ( gState >= nStates ))
      error("WFSTNetwork::getInLabelOfOneTransition - gState out of range") ;

   int nTrans = states[gState].nTrans ;
   if (( n < 0 ) || ( n >= nTrans ))
      error("WFSTNetwork::getInLabelOfOneTransition - n = %d out of range (state = %d)", n , gState ) ;
#endif

   int transIndex = states[gState].trans[n] ;
   return ( transitions[transIndex].inLabel ) ;
}


#if 0
//zl: inlined
bool WFSTNetwork::transGoesToFinalState( WFSTTransition *trans )
{
#ifdef DEBUG
   if ( trans == NULL )
      error("WFSTNetwork::transGoesToFinalState - trans is NULL") ;
   if ( trans != ( transitions + trans->id) )
      error("WFSTNetwork::transGoesToFinalState - unexpected trans") ;
#endif

   if ( states[trans->toState].finalInd < 0 )
      return false ;
   else
      return true ;
}


real WFSTNetwork::getFinalStateWeight( WFSTTransition *trans )
{
#ifdef DEBUG
   if ( trans == NULL )
      error("WFSTNetwork::getFinalStateWeight - trans is NULL") ;
   if ( trans != ( transitions + trans->id) )
      error("WFSTNetwork::getFinalStateWeight - unexpected trans") ;
#endif

   int ind = states[trans->toState].finalInd ;
#ifdef DEBUG
   if ( ind < 0 )
   {
      warning("WFSTNetwork::getFinalStateWeight - called for non-final state") ;
      return LOG_ZERO ;
   }
   else
#endif
   {
      return finalStates[ind].weight ;
   }
}
#endif


void WFSTNetwork::outputText( int type )
{
   // If 'type' == 0, output FSM
   // If 'type' == 1, output input symbols (if defined)
   // If 'type' == 2, output output symbols (if defined)

   int i , j ;

   if ( type == 0 )
   {
      // Print transitions out of initial state first
      for ( j=0 ; j<states[initState].nTrans ; j++ )
      {
         WFSTTransition *trans = transitions + states[initState].trans[j] ;
         printf("%-10d %-10d %-10d %-10d %0.3f\n" , states[initState].label ,
               trans->toState , trans->inLabel , trans->outLabel , -(trans->weight) ) ;
      }

      // Print all other transitions
      for ( i=0 ; i<=maxState ; i++ )
      {
         if ( i == initState )
            continue ;

         if ( states[i].label >= 0 )
         {
            for ( j=0 ; j<states[i].nTrans ; j++ )
            {
               WFSTTransition *trans = transitions + states[i].trans[j] ;
               printf("%-10d %-10d %-10d %-10d %0.3f\n" , states[i].label ,
                     trans->toState , trans->inLabel , trans->outLabel , -(trans->weight) ) ;
            }
         }
      }

      // Print final states
      for ( i=0 ; i<nFinalStates ; i++ )
      {
         printf("%d %0.3f\n" , finalStates[i].id , finalStates[i].weight ) ;
      }
   }
   else if ( (type == 1) && (inputAlphabet != NULL) )
   {
      inputAlphabet->outputText() ;
   }
   else if ( (type == 2) && (outputAlphabet != NULL) )
   {
      outputAlphabet->outputText() ;
   }
}


void WFSTNetwork::generateSequences( int maxSeqs , bool logBase10 )
{
   if ( maxSeqs <= 0 )
      maxSeqs = 5 ;

   // Allocate memory to hold transition results
   int nTrans ;
   WFSTTransition **trans = new WFSTTransition*[maxOutTransitions] ;

   // Seed the random number generator.
   srand( time(NULL) ) ;

   int ind=0 ;
   WFSTTransition *tran = NULL ;
   real totalWeight=0.0 ;

   real ln10 = log(10.0) ;

   for ( int i=0 ; i<maxSeqs ; i++ )
   {
      totalWeight = 0.0 ;
      tran = NULL ;
      while ( 1 )
      {
         getTransitions( tran , &nTrans , trans ) ;
         if ( nTrans == 0 )
         {
            if ( tran == NULL )
               error("WFSTNetwork::genSeqs - no transition out of initial state") ;
            if ( states[tran->toState].finalInd < 0 )
               error("WFSTNetwork::genSeqs - state had no sucs but was not final") ;
         }

         if ( (tran != NULL) && (states[tran->toState].finalInd >= 0) )
         {
            // Generate a random index into trans with option to interpret state as final
            ind = (int)( (float)rand() / RAND_MAX * (nTrans+1) ) ;
            if ( ind >= nTrans )
            {
               // Take the final state option
               totalWeight += finalStates[states[tran->toState].finalInd].weight ;
               if ( logBase10 )
               {
                  printf("** (%.3f) %.3f\n" ,
                        finalStates[states[tran->toState].finalInd].weight / ln10 ,
                        totalWeight / ln10 ) ;
               }
               else
               {
                  printf("** (%.3f) %.3f\n" , finalStates[states[tran->toState].finalInd].weight ,
                        totalWeight ) ;
               }
               break ;
            }
         }
         else
         {
            ind = (int)( (float)rand() / RAND_MAX * nTrans ) ;
            if ( ind == nTrans )
               ind-- ;
         }

         tran = trans[ind] ;

         // Output the transition details
         // Input label.
         if ( tran->inLabel != 0 )    // i.e. non-epsilon
         {
            if ( tran->inLabel == wordEndMarker ) {
               printf("# ");
            } else {
               if ( inputAlphabet != NULL )
                  printf("%s" , inputAlphabet->getLabel( tran->inLabel ) ) ;
               else
                  printf("%d" , tran->inLabel ) ;
            }
         }

         // Output label if non-epsilon
         if ( tran->outLabel != 0 )   // i.e. non-epsilon
         {
            if ( tran->outLabel == wordEndMarker ) {
               printf("[#]");
            } else {
               if ( outputAlphabet != NULL )
                  printf("[%s]" , outputAlphabet->getLabel( tran->outLabel ) ) ;
               else
                  printf("[%d]" , tran->outLabel ) ;
            }
         }

         // Add transition weight to total weight (even if it is an epsilon transition)
         if ( logBase10 )
            printf("(%.2f) " , tran->weight/ln10 ) ;
         else
            printf("(%.2f) " , tran->weight ) ;
         totalWeight += tran->weight ;
      }
   }

   delete [] trans ;
}


void WFSTNetwork::writeFSM( const char *fsmFName , const char *inSymsFName ,
                            const char *outSymsFName )
{
/*
   if ( inputAlphabet == NULL )
      error("WFSTNetwork::writeFSM - inputAlphabet is NULL") ;
   if ( outputAlphabet == NULL )
      error("WFSTNetwork::writeFSM - outputAlphabet is NULL") ;

   FILE *fsmFD , *inSymsFD , *outSymsFD ;

   // ** FSM file **
   // Open the FSM output file
   if ( (fsmFD = fopen( fsmFName , "wb" )) == NULL )
      error("WFSTNetwork::writeFSM - error opening FSM output file") ;

   // Write arcs
   int i ;
   for ( i=0 ; i<nTransitions ; i++ )
   {
      if ( inputAlphabet->isAuxiliary( transitions[i].inLabel ) )
         error("WFSTNetwork::writeFSM - auxiliary input label detected") ;
      if ( outputAlphabet->isAuxiliary( transitions[i].outLabel ) )
         error("WFSTNetwork::writeFSM - auxiliary output label detected") ;

      writeFSMTransition( fsmFD , transitions[i].fromState , transitions[i].toState ,
                          transitions[i].inLabel , transitions[i].outLabel ,
                          transitions[i].weight ) ;
   }

   // Write final states
   for ( i=0 ; i<nFinalStates ; i++ )
   {
      writeFSMFinalState( fsmFD , finalStates[i].id , finalStates[i].weight ) ;
   }

   // Close FSM file
   fclose( fsmFD ) ;

   // ** Input symbols file **
   // Open file
   if ( (inSymsFD = fopen( inSymsFName , "wb" )) == NULL )
      error("WFSTNetwork::writeFSM - error opening input symbols output file") ;

   // Write
   inputAlphabet->write( inSymsFD , false ) ;

   // Close file
   fclose( inSymsFD ) ;

   // ** Output symbols file **
   // Open file
   if ( (outSymsFD = fopen( outSymsFName , "wb" )) == NULL )
      error("WFSTNetwork::writeFSM - error opening output symbols output file") ;

   // Write
   outputAlphabet->write( outSymsFD , false ) ;

   // Close file
   fclose( outSymsFD ) ;
*/
}


void WFSTNetwork::writeBinary( const char *fname )
{
   // If we have scaled all transition weights, unscale them before we write
   // them to binary file.

    // Yet to propagate the insertion penalty
    assert(insPenalty == 0.0);

   if ( transWeightScalingFactor != 1.0 )
   {
      for ( int i=0 ; i<nTransitions ; i++ )
      {
         transitions[i].weight /= transWeightScalingFactor ;
      }
   }

   FILE *fd ;
   if ( (fd = fopen( fname , "wb" )) == NULL )
      error("WFSTNetwork::writeBinary - error opening output file") ;

   char id[5] ;
   strcpy( id , "JWNT" ) ;

   // 1. Write ID, initState , maxState, nStates, maxOutTransitions,
   // wordEndMarker
   fwrite( (int *)id , sizeof(int) , 1 , fd ) ;
   fwrite( &initState , sizeof(int) , 1 , fd ) ;
   fwrite( &maxState , sizeof(int) , 1 , fd ) ;
   fwrite( &nStates , sizeof(int) , 1 , fd ) ;
   fwrite( &maxOutTransitions , sizeof(int) , 1 , fd ) ;
   fwrite( &wordEndMarker , sizeof(int) , 1 , fd ) ;

   // 2. Write states array
   int i ;
   for ( i=0 ; i<=maxState ; i++ )
   {
      fwrite( &(states[i].label) , sizeof(int) , 1 , fd ) ;
      fwrite( &(states[i].finalInd) , sizeof(int) , 1 , fd ) ;
      fwrite( &(states[i].nTrans) , sizeof(int) , 1 , fd ) ;
      if ( states[i].nTrans > 0 )
         fwrite( states[i].trans , sizeof(int) , states[i].nTrans , fd ) ;
   }

   // 3. Write nFinalStates and finalStates array.
   fwrite( &nFinalStates , sizeof(int) , 1 , fd ) ;
   if ( nFinalStates > 0 )
      fwrite( finalStates , sizeof(WFSTFinalState) , nFinalStates , fd ) ;

   // 4. Write nTransitions and transitions array.
   fwrite( &nTransitions , sizeof(int) , 1 , fd ) ;
   if ( nTransitions > 0 )
      fwrite( transitions , sizeof(WFSTTransition) , nTransitions , fd ) ;

   // 5. Write inputAlphabet and outputAlphabet
   bool haveAlphabet ;
   if ( inputAlphabet == NULL )
   {
      haveAlphabet = false ;
      fwrite( &haveAlphabet , sizeof(bool) , 1 , fd ) ;
   }
   else
   {
      haveAlphabet = true ;
      fwrite( &haveAlphabet , sizeof(bool) , 1 , fd ) ;
      inputAlphabet->writeBinary( fd ) ;
   }

   if ( outputAlphabet == NULL )
   {
      haveAlphabet = false ;
      fwrite( &haveAlphabet , sizeof(bool) , 1 , fd ) ;
   }
   else
   {
      haveAlphabet = true ;
      fwrite( &haveAlphabet , sizeof(bool) , 1 , fd ) ;
      outputAlphabet->writeBinary( fd ) ;
   }

   // 6. Write the ID (again).
   fwrite( (int *)id , sizeof(int) , 1 , fd ) ;

   fclose( fd ) ;

   // If we have unscaled all transition weights for writing to binary file,
   //   re-scale them back to their original values.
   if ( transWeightScalingFactor != 1.0 )
   {
      for ( int i=0 ; i<nTransitions ; i++ )
      {
         transitions[i].weight *= transWeightScalingFactor ;
      }
   }
}


void WFSTNetwork::readBinary( const char *fname )
{
   FILE *fd ;
   if ( (fd = fopen( fname , "rb" )) == NULL )
      error("WFSTNetwork::readBinary - error opening input file") ;

   char id[5] ;

   // 1. Read ID, initState , maxState, nStates, maxOutTransitions
   if ( fread( (int *)id , sizeof(int) , 1 , fd ) != 1 )
      error("WFSTNetwork::readBinary - error reading ID") ;
   id[4] = '\0' ;
   if ( strcmp( id , "JWNT" ) != 0 )
      error("WFSTNetwork::readBinary - invalid ID") ;
   if ( fread( &initState , sizeof(int) , 1 , fd ) != 1 )
      error("WFSTNetwork::readBinary - error reading initState") ;
   if ( fread( &maxState , sizeof(int) , 1 , fd ) != 1 )
      error("WFSTNetwork::readBinary - error reading maxState") ;
   if ( fread( &nStates , sizeof(int) , 1 , fd ) != 1 )
      error("WFSTNetwork::readBinary - error reading nStates") ;
   if ( fread( &maxOutTransitions , sizeof(int) , 1 , fd ) != 1 )
      error("WFSTNetwork::readBinary - error reading maxOutTransitions") ;
   if ( fread( &wordEndMarker , sizeof(int) , 1 , fd ) != 1 )
      error("WFSTNetwork::readBinary - error reading wordEndMarker") ;

   // 2. Allocate and read the states array
   nStatesAlloc = maxState + 1 ;
   states = new WFSTState[nStatesAlloc] ;
   int i ;
   for ( i=0 ; i<=maxState ; i++ )
   {
      if ( fread( &(states[i].label) , sizeof(int) , 1 , fd ) != 1 )
         error("WFSTNetwork::readBinary - error reading states[i].label") ;
      if ( fread( &(states[i].finalInd) , sizeof(int) , 1 , fd ) != 1 )
         error("WFSTNetwork::readBinary - error reading states[i].finalInd") ;
      if ( fread( &(states[i].nTrans) , sizeof(int) , 1 , fd ) != 1 )
         error("WFSTNetwork::readBinary - error reading states[i].nTrans") ;
      if ( states[i].nTrans > 0 )
      {
         states[i].trans = new int[states[i].nTrans] ;
         if ( (int)fread( states[i].trans, sizeof(int), states[i].nTrans, fd ) != states[i].nTrans )
            error("WFSTNetwork::readBinary - error reading states[i].trans array") ;
      }
      else
         states[i].trans = NULL ;
   }

   // 3. Read nFinalStates and finalStates array.
   if ( fread( &nFinalStates , sizeof(int) , 1 , fd ) != 1 )
      error("WFSTNetwork::readBinary - error reading nFinalStates") ;
   if ( nFinalStates > 0 )
   {
      finalStates = new WFSTFinalState[nFinalStates] ;
      if ( (int)fread( finalStates, sizeof(WFSTFinalState), nFinalStates, fd ) != nFinalStates )
         error("WFSTNetwork::readBinary - error reading finalStates array") ;
   }
   else
      finalStates = NULL ;

   // 4. Read nTransitions and transitions array.
   if ( fread( &nTransitions , sizeof(int) , 1 , fd ) != 1 )
      error("WFSTNetwork::readBinary - error reading nTransitions") ;
   if ( nTransitions > 0 )
   {
      transitions = new WFSTTransition[nTransitions] ;
      if ( (int)fread( transitions, sizeof(WFSTTransition), nTransitions, fd ) != nTransitions )
         error("WFSTNetwork::readBinary - error reading transitions array") ;
   }
   else
      transitions = NULL ;

   // 5. Read inputAlphabet and outputAlphabet
   bool haveAlphabet ;
   if ( fread( &haveAlphabet , sizeof(bool) , 1 , fd ) != 1 )
      error("WFSTNetwork::readBinary - error reading inputAlphabet haveAlphabet") ;
   if ( haveAlphabet )
   {
      inputAlphabet = new WFSTAlphabet() ;
      inputAlphabet->readBinary( fd ) ;
   }
   else
      inputAlphabet = NULL ;

   if ( fread( &haveAlphabet , sizeof(bool) , 1 , fd ) != 1 )
      error("WFSTNetwork::readBinary - error reading outputAlphabet haveAlphabet") ;
   if ( haveAlphabet )
   {
      outputAlphabet = new WFSTAlphabet() ;
      outputAlphabet->readBinary( fd ) ;
   }
   else
      outputAlphabet = NULL ;

   // 6. Read the ID field
   if ( fread( (int *)id , sizeof(int) , 1 , fd ) != 1 )
      error("WFSTNetwork::readBinary - error reading ID (2)") ;
   id[4] = '\0' ;
   if ( strcmp( id , "JWNT" ) != 0 )
      error("WFSTNetwork::readBinary - invalid ID (2)") ;

   fclose( fd ) ;

   fromBinFile = true ;

   // Scaled all transition weights if a scaling factor is specified
   if ( transWeightScalingFactor != 1.0 )
   {
      for ( int i=0 ; i<nTransitions ; i++ )
      {
         transitions[i].weight *= transWeightScalingFactor ;
      }
   }
}


// Change Octavian 20060616
void WFSTNetwork::printNumOutTransitions( const char *fname )
{
   FILE *fptr ;

   if ( (fptr = fopen( fname, "w" )) == NULL )
      error("WFSTNetwork::printNumOutTransitions - Cannot open file") ;

   for ( int i = 0 ; i < nStates ; i++ )  {
      fprintf( fptr, "%d, ", states[i].nTrans ) ;
   }

   fclose( fptr ) ;

}


void WFSTNetwork::initWFSTState( WFSTState *state )
{
#ifdef DEBUG
   if ( state == NULL )
      error("WFSTNetwork::initWFSTState - state is NULL") ;
#endif

   state->label = -1 ;
   state->finalInd = -1 ;
   state->nTrans = 0 ;
   state->trans = NULL ;
}

void WFSTNetwork::initWFSTTransition( WFSTTransition *transition )
{
#ifdef DEBUG
   if ( transition == NULL )
      error("WFSTNetwork::initWFSTTransition - transition is NULL") ;
#endif

   transition->id = -1 ;
   //transition->fromState = -1 ;
   transition->toState = -1 ;
   transition->inLabel = -1 ;
   transition->outLabel = -1 ;
   transition->weight = LOG_ZERO ;
}

// Changes Octavian 20060523
// If any changes appear in this function, consider changing it in
// removeAuxiliaryInputSymbols() as well.
void WFSTNetwork::removeAuxiliarySymbols( bool markAux )
{
   // If markAux is true then auxiliary input and output labels are replaced
   // with wordEndMarker, which will indicate word-end to the decoding engine.
   if ( markAux && (wordEndMarker < 0) )
      error("WFSTNetwork::removeAuxiliarySymbols - markAux true, but wordEndMarker < 0") ;

   // Can only do this if we have input and output alphabets
   if ( inputAlphabet == NULL )
      error("WFSTNetwork::removeAuxiliarySymbols - inputAlphabet is NULL") ;
   if ( outputAlphabet == NULL )
      error("WFSTNetwork::removeAuxiliarySymbols - outputAlphabet is NULL") ;

   // Iterate through all transitions.
   // Whenever we encounter an input or output label that is an auxiliary
   // symbol, replace it with epsilon.

   for ( int i=0 ; i<nTransitions ; i++ )
   {
      if ( inputAlphabet->isAuxiliary( transitions[i].inLabel ) ) {
         if ( markAux ) {
            transitions[i].inLabel = wordEndMarker ;
         } else {
            transitions[i].inLabel = WFST_EPSILON ;
         }
      }

      if ( outputAlphabet->isAuxiliary( transitions[i].outLabel ) ) {
         if ( markAux ) {
            transitions[i].outLabel = wordEndMarker ;
         } else {
            transitions[i].outLabel = WFST_EPSILON ;
         }
      }
   }
}

// Changes Octavian 20060523
// This funciton will remove auxiliary input symbols only
void WFSTNetwork::removeAuxiliaryInputSymbols( bool markAux )
{
   // If markAux is true then auxiliary input and output labels are replaced
   // with wordEndMarker, which will indicate word-end to the decoding engine.
   if ( markAux && (wordEndMarker < 0) )
      error("WFSTNetwork::removeAuxiliaryInputSymbols - markAux true, but wordEndMarker < 0") ;

   // Can only do this if we have input and output alphabets
   if ( inputAlphabet == NULL )
      error("WFSTNetwork::removeAuxiliaryInputSymbols - inputAlphabet is NULL") ;
   /*
   if ( outputAlphabet == NULL )
      error("WFSTNetwork::removeAuxiliaryInputSymbols - outputAlphabet is NULL") ;
   */

   // Iterate through all transitions.
   // Whenever we encounter an input or output label that is an auxiliary
   // symbol, replace it with epsilon.

   for ( int i=0 ; i<nTransitions ; i++ )
   {
      if ( inputAlphabet->isAuxiliary( transitions[i].inLabel ) ) {
         if ( markAux ) {
            transitions[i].inLabel = wordEndMarker ;
         } else {
            transitions[i].inLabel = WFST_EPSILON ;
         }
      }

      /*
      if ( outputAlphabet->isAuxiliary( transitions[i].outLabel ) ) {
         if ( markAux ) {
            transitions[i].outLabel = wordEndMarker ;
         } else {
            transitions[i].outLabel = WFST_EPSILON ;
         }
      }
      */
   }

}


//****** WFSTLabelPushingNetwork Implementation ******
// Changes 20060325
WFSTLabelPushingNetwork::WFSTLabelPushingNetwork() : WFSTNetwork()
{
   arc_outlabset_map = NULL ;
}

// Changes 20060325
WFSTLabelPushingNetwork::WFSTLabelPushingNetwork( real transWeightScalingFactor_ ) : WFSTNetwork( transWeightScalingFactor_ , 0.0 )
{
   arc_outlabset_map = NULL ;
}

// Changes Octavian 20060523
WFSTLabelPushingNetwork::WFSTLabelPushingNetwork(
		const char *wfstFilename , const char *inSymsFilename ,
		const char *outSymsFilename , real transWeightScalingFactor_ ,
		RemoveAuxOption removeAuxOption )
                : WFSTNetwork(  wfstFilename , inSymsFilename ,
				outSymsFilename , transWeightScalingFactor_ ,
				removeAuxOption )
{
   if (( nStates == 0 )||( nTransitions == 0 ))
      return;

   assignOutlabsToTrans() ;
   //printLabelSet() ;
}

// Changes 20060325
WFSTLabelPushingNetwork::~WFSTLabelPushingNetwork()
{
   delete [] arc_outlabset_map ;
   arc_outlabset_map = NULL ;
}


void WFSTLabelPushingNetwork::writeBinary( const char *fname )
{
   WFSTNetwork::writeBinary(fname) ;
}


void WFSTLabelPushingNetwork::readBinary( const char *fname )
{
   WFSTNetwork::readBinary(fname) ;
   assignOutlabsToTrans() ;
   // printLabelSet() ;
}


const WFSTLabelPushingNetwork::LabelSet *WFSTLabelPushingNetwork::getOneLabelSet( int transIndex )
{

#ifdef DEBUG
   if ((transIndex < 0 ) || (transIndex >= nTransitions))
      error("WFSTLabelPushingNetwork::getOneLabelSet - transIndex out of range") ;
   if ( arc_outlabset_map == NULL )
      error("WFSTLabelPushingNetwork::getOneLabelSet - arc_outlabset_map == NULL") ;
#endif
   // Changes 20060325
   return arc_outlabset_map[transIndex] ;

}

// Changes Octavian 20060616
int WFSTLabelPushingNetwork::getMaxOutLabels( const char *fname )
{
   set<LabelSet>::iterator iter ;
   int maxLabels = -1 ;

   //**********
   FILE *fptr = NULL ;

   if ( fname != NULL )  {
      if ( (fptr = fopen( fname, "w")) == NULL )
	 error("WFSTLabelPushingNetwork::getMaxOutLabels - cannot open file") ;
   }
   //**********

   for ( iter = unique_outlabsets.begin() ;
	 iter != unique_outlabsets.end() ;
	 iter++ )  {
      int numLabels = (*iter).size() ;

      //*******
      if ( fptr != NULL )
	 fprintf( fptr, "%d, ", numLabels ) ;
      //*******

      if ( maxLabels < numLabels )
	 maxLabels = numLabels ;
   }

   //**********
   if ( fptr != NULL )
      fclose( fptr ) ;
   //**********

   return maxLabels ;
}


#ifdef DEBUG
void WFSTLabelPushingNetwork::printLabelSet()
{
   if ( arc_outlabset_map != NULL )  {
      for ( int i = 0 ; i < nTransitions ; i++ )  {
	 const LabelSet *labSet = arc_outlabset_map[i] ;
	 print("Trans: %d \n ", i ) ;

	 if ( labSet != NULL )  {
	    for ( LabelSet::iterator labSetIter = labSet->begin() ;
		  labSetIter != labSet->end() ;
		  labSetIter++ )  {
	       print("%d ", (*labSetIter) ) ;
	    }
	 }

	 print("\n*****************\n") ;
      }
   }
   /*
   for ( ArcToLabelSetMap::iterator iter = arc_outlabset_map.begin() ;
	 iter != arc_outlabset_map.end() ;
	 iter++ )  {
      const LabelSet *labSet = (*iter).second ;
      print("Trans: %d \n ", (*iter).first ) ;
      for ( LabelSet::iterator labSetIter = labSet->begin() ;
	    labSetIter != labSet->end() ;
	    labSetIter++ )  {
	 print("%d ", (*labSetIter) ) ;
      }

      print("\n*****************\n") ;
   }
   */
}
#endif

// The main function which assigns output label set to each transition
void WFSTLabelPushingNetwork::assignOutlabsToTrans()
{
   int i;

   // Clear the output label sets
   unique_outlabsets.clear() ;

   // Changes Octavian 20060325
   arc_outlabset_map = new const LabelSet *[nTransitions] ;
   for ( i = 0 ; i < nTransitions ; i++ )
      arc_outlabset_map[i] = NULL ;

   int nInitTrans = states[initState].nTrans ;
   int *initTrans = states[initState].trans ;
   bool *hasVisited = new bool[nTransitions] ;

   // First, determine labelset of each trans.
   // Collect all those transitions which do not have a labelset
   for ( i = 0 ; i < nTransitions ; i++ )  {
      hasVisited[i] = false ;
   }

   set<int> undecidedTrans ;

   // Transverse to all transitions. Collect all the deadlock transitions
   for ( i = 0 ; i < nInitTrans ; i++ )  {
      findUndecidedTrans( initTrans[i], hasVisited, undecidedTrans ) ;
   }

   // Second, find out a set of transitions which are the start of a loop.
   // Iterate all undecided trans
   bool *hasReturned = new bool[nTransitions] ;
   for ( i = 0 ; i < nTransitions ; i++ )  {
#ifdef DEBUG
      if ( hasVisited[i] == false )
	 error("WFSTLabelPushingNetwork::assignOutlabsToTrans - 2nd step - hasVisited == false") ;
#endif
      hasVisited[i] = false ;
      hasReturned[i] = false ;
   }

   const LabelSet *returnLabelSet ;
   set<int> newUndecidedTrans ;
   set<int> loopStartTrans ;

   // For each undecided transition, find the start of the loop
   for ( set<int>::iterator iter = undecidedTrans.begin() ;
	 iter != undecidedTrans.end() ;
	 iter++ )  {
      findLoopStartTrans( *iter, hasVisited, hasReturned, &returnLabelSet, newUndecidedTrans, loopStartTrans ) ;
   }

   undecidedTrans.clear() ;

   // Third, find all the loops
   // A list which stores sets of loop states
   list< set<int> > loopStatesList ;

   for ( set<int>::iterator currLoopStartTrans = loopStartTrans.begin() ;
	 currLoopStartTrans != loopStartTrans.end() ;
	 currLoopStartTrans++ )  {
      loopStatesList.push_back( set<int>() ) ;
      set<int> &loopStates = loopStatesList.back() ;

      for ( i = 0 ; i < nTransitions ; i++ )  {
	 hasVisited[i] = false ;
      }

      // Changes Octavian 20060508
#ifdef DEBUG
      bool isLoop = findLoop( *currLoopStartTrans, hasVisited, *currLoopStartTrans, loopStartTrans, loopStates, newUndecidedTrans ) ;
#else
      findLoop( *currLoopStartTrans, hasVisited, *currLoopStartTrans, loopStartTrans, loopStates, newUndecidedTrans ) ;
#endif

#ifdef DEBUG
      if ( isLoop == false )
	 error("WFSTLabelPushingNetwork::assignOutlabsToTrans - Cannot find loop. loopStart = %d, toState = %d", *currLoopStartTrans, transitions[*currLoopStartTrans].toState ) ;
      if ( loopStates.empty() )
	 error("WFSTLabelPushingNetwork::assignOutlabsToTrans - loop is empty. loopStart = %d, toState = %d", *currLoopStartTrans, transitions[*currLoopStartTrans].toState ) ;
#endif

   }

   // Fourth, combine loops
   combineLoop( loopStatesList ) ;

   // Fifth, assign output labels to loops
   assignOutlabsToLoops( loopStatesList, newUndecidedTrans ) ;

   // Sixth, there may be some undecided trans to be resolved
   for ( i = 0 ; i < nTransitions ; i++ )  {
      hasVisited[i] = false ;
   }

   set<int> finalUndecidedTrans ;

   for ( set<int>::iterator iter = newUndecidedTrans.begin() ;
	 iter != newUndecidedTrans.end() ;
	 iter++ )  {
      findUndecidedTrans( *iter, hasVisited, finalUndecidedTrans ) ;
   }

#ifdef DEBUG
   if ( !finalUndecidedTrans.empty() )
      error("WFSTLabelPushingNetwork::assignOutlabsToTrans - finalUndecidedTrans != empty") ;

   for ( i = 0 ; i < nTransitions ; i++ )  {
      const LabelSet *tmpLabelSet = arc_outlabset_map[i] ;
      if ( tmpLabelSet == NULL )
	 error("WFSTLabelPushingNetwork::_assignOutLabelToTrans - Trans %d has NULL labelset", i ) ;
      else if ( tmpLabelSet->empty() )
	 error("WFSTLabelPushingNetwork::_assignOutLabelToTrans - Trans %d has empty labelset", i ) ;

   }
#endif

   delete [] hasVisited ;
   delete [] hasReturned ;

}

// This function finds a set of undecided trans
const WFSTLabelPushingNetwork::LabelSet* WFSTLabelPushingNetwork::findUndecidedTrans( int transIndex , bool *hasVisited , set<int>& undecidedTrans )
{
   // Check if the transitions has been visited before
   if ( hasVisited[transIndex] == true )  {
      // Check if there is an output label set available
      // Changes 20060325
      return arc_outlabset_map[transIndex] ;
   }

   hasVisited[transIndex] = true ;

   int to = transitions[transIndex].toState ;
   int *nextTrans = states[to].trans ;
   int nextNTrans = states[to].nTrans ;

   // Use for collecting labels of successors
   const LabelSet *succLabelSet ;

   // Changes Octavian 20060523
   // LabelSet thisTransLabelSet ;
   // A pointer to a set which stores the output labels for this transition
   LabelSet *thisTransLabelSet = NULL ;

   // If at least one of the successors return NULL set, set it to true
   bool isSuccNullSet = false ;

   // True if this transition is going to the final state
   bool toFinalState = transGoesToFinalState( transitions + transIndex ) ;

   // True if the returned label set of a successor has only the
   // NONPUSHING_OUTLABEL label
   bool isSuccToFinalState = false ;

   // Union all the label sets from successors
   for ( int i = 0 ; i < nextNTrans ; i++ )  {
      // Find the the label set of the next transition
      succLabelSet = findUndecidedTrans( nextTrans[i] , hasVisited , undecidedTrans ) ;

      // Only need to union all labels if this transition has epsilon output
      // label and not to a final state
      if (( transitions[transIndex].outLabel == WFST_EPSILON ) && ( !toFinalState ))  {
	 if ( succLabelSet != NULL )  {
	    if ( succLabelSet->find( NONPUSHING_OUTLABEL ) == succLabelSet->end() )  {
	       // Only union the set if all the successors are not going to a
	       // final state and no dead trans has been encountered yet
	       if (( isSuccToFinalState == false ) && ( isSuccNullSet == false ))    {
		  // Changes Octavian 20060523
		  //********
		  if ( thisTransLabelSet == NULL )
		     thisTransLabelSet = new LabelSet ;

		  set_union(
			thisTransLabelSet->begin(), thisTransLabelSet->end(),
			succLabelSet->begin(), succLabelSet->end(),
			inserter( *thisTransLabelSet, thisTransLabelSet->begin()) ) ;
		  /* set_union(	thisTransLabelSet.begin(), thisTransLabelSet.end(),
				succLabelSet->begin(), succLabelSet->end(),
				inserter(thisTransLabelSet, thisTransLabelSet.begin()) ) ;
				*/
		  //*********
	       }
	    }
	    else  {
#ifdef DEBUG
	       if ( succLabelSet->size() != 1 )
		  error ("WFSTLabelPushingNetwork::findUndecidedTrans - Label -1 is found but more than 1 label in the set") ;
#endif
	       isSuccToFinalState = true ;
	    }
	 }
	 else  {
	    // If the outLabel is epsilon and this transition depends on one
	    // of the next transitions
	    isSuccNullSet = true ;
	 }
      }
   }

   // First case: this trans has an output label
   if ( transitions[transIndex].outLabel != WFST_EPSILON )  {
      // Has an output label on this transition
#ifdef DEBUG
      // Changes Octavian 20060523
      // *****************
      if ( thisTransLabelSet != NULL )
	 error("WFSTLabelPushingNetwork::findUndecidedTrans - thisTransLabelSet should be empty - valid output label");
      /*
      if ( !thisTransLabelSet.empty() )
	 error("WFSTLabelPushingNetwork::_findUndecidedTrans - thisTransLabelSet should be empty - valid output label");
	 */
      // ******************
#endif
      // Changes Octavian 20060523
      // ********************
      thisTransLabelSet = new LabelSet ;
      thisTransLabelSet->insert( transitions[transIndex].outLabel ) ;
      // thisTransLabelSet.insert( transitions[transIndex].outLabel ) ;
      // ********************
   }
   else if ( toFinalState )  {
      // Second case: Epsilon output label + go to a final state
#ifdef DEBUG
      // Changes Octavian 20060523
      if ( thisTransLabelSet != NULL )
	 error("WFSTLabelPushingNetwork::findUndecidedTrans - thisTransLabelSet should be empty - toFinalState") ;
      /*
      if ( !thisTransLabelSet.empty() )
	 error("WFSTLabelPushingNetwork::findUndecidedTrans - thisTransLabelSet should be empty - toFinalState") ;
	 */
      // ****************************
#endif
      // Changes Octavian 20060523
      thisTransLabelSet = new LabelSet ;
      thisTransLabelSet->insert( NONPUSHING_OUTLABEL ) ;
      // thisTransLabelSet.insert( NONPUSHING_OUTLABEL ) ;
      // **************************
   }
   else if ( isSuccToFinalState )  {
      // Third case: Epsilon output label + not go to a final state
      // But at least one of the successors are going to a final state
      // Changes Octavian 20060523
      if ( thisTransLabelSet == NULL )
	 thisTransLabelSet = new LabelSet ;
      else
	 thisTransLabelSet->clear() ;
      thisTransLabelSet->insert( NONPUSHING_OUTLABEL ) ;
      // thisTransLabelSet.clear() ;
      // thisTransLabelSet.insert( NONPUSHING_OUTLABEL ) ;
      // *********************
   }
   else if ( isSuccNullSet )  {
      // Fourth case: One of the successors is a dead trans
      // Changes Octavian 20060523
      if ( thisTransLabelSet != NULL )  {
	 thisTransLabelSet->clear() ;
	 delete thisTransLabelSet ;
      }
      // thisTransLabelSet.clear() ;
      // **********************
      undecidedTrans.insert( transIndex ) ;
      return NULL ;
   }
   // Fifth case: Otherwise all successors have an output label set
   // No need to change thisTransLabelSet
#ifdef DEBUG
   else  {
      // Changes Octavian 20060523
      if ( thisTransLabelSet == NULL )
	 error("WFSTLabelPushingNetwork::findUndecidedTrans - thisTransLabelSet should be non-empty - no succ is NULL") ;
      /*
      if ( thisTransLabelSet.empty() )
	 error("WFSTLabelPushingNetwork::findUndecidedTrans - thisTransLabelSet should be non-empty - no succ is NULL") ;
	 */
      // *******************
   }
#endif

   // Changes Octavian 20060523
   // pair<set<LabelSet>::iterator,bool> status = unique_outlabsets.insert( thisTransLabelSet ) ;
   pair<set<LabelSet>::iterator,bool> status = unique_outlabsets.insert( *thisTransLabelSet ) ;
   thisTransLabelSet->clear() ;
   delete thisTransLabelSet ;
   // ***************************
   arc_outlabset_map[transIndex] = &(*(status.first)) ;
   return (&(*(status.first))) ;
}


// This function iterates all undecided trans and find the start trans of a
// loop and a new set of undecided trans.
// newUndecidedTrans will store a set of undecided transitions.
// loopStartTrans will store a set of transitions which is the start of
// a loop.
// If this transition has been visited but not returned from the
// depth search yet, then it is LOOPEND. Otherwise, it is NOTLOOPEND.
// If one of the next transition is the LOOPEND, then this transition is
// the start of a loop.
WFSTLabelPushingNetwork::LoopStatus WFSTLabelPushingNetwork::findLoopStartTrans( int transIndex, bool *hasVisited, bool *hasReturned, const WFSTLabelPushingNetwork::LabelSet **returnLabelSet, set<int> &newUndecidedTrans, set<int> &loopStartTrans )
{
   if ( arc_outlabset_map[transIndex] != NULL )  {
      *returnLabelSet = arc_outlabset_map[transIndex] ;
      return NOTLOOPEND ;
   }
   else if ( hasVisited[transIndex] == true )  {
      // Has been visited before. If it has been returned, that transition
      // is not the end of a loop.
      // Otherwise, it is the end of a loop
      if ( hasReturned[transIndex] == true )  {
	 *returnLabelSet = NULL ;
	 return NOTLOOPEND ;
      }
      else  {
	 *returnLabelSet = NULL ;
	 return LOOPEND ;
      }
   }

   hasVisited[transIndex] = true ;

   int to = transitions[transIndex].toState ;
   int *nextTrans = states[to].trans ;
   int nextNTrans = states[to].nTrans ;

#ifdef DEBUG
   // Check. This transition should not have an output label and should not
   // go to a final state. Otherwise, it should have a label list already
   // registered.
   if (( transitions[transIndex].outLabel != WFST_EPSILON ) ||
	 ( transGoesToFinalState( transitions + transIndex ) ))  {
      error("WFSTLabelPushingNetwork::findLoopStartTrans - Trans having an outLabel or going towards a final state should have a label set") ;
   }
#endif

   // Use for collecting labels of successors
   const LabelSet *succLabelSet ;
   LabelSet thisTransLabelSet ;

   // If at least one of the successors return NULL set, set it to true
   bool isSuccNullSet = false ;

   // True if the returned label set of a successor has only the
   // NONPUSHING_OUTLABEL label
   bool isSuccToFinalState = false ;

   // True if this transition is the start a loop
   bool isThisLoopStartTrans = false ;

   // For each successor transition
   for ( int i = 0 ; i < nextNTrans ; i++ )  {
      LoopStatus loopStatus = findLoopStartTrans( nextTrans[i], hasVisited, hasReturned, &succLabelSet,
	    newUndecidedTrans, loopStartTrans ) ;

      // If the next transition is the end of a loop
      if ( loopStatus == LOOPEND )  {
	 isThisLoopStartTrans = true ;
	 break ;
      }
      else  {
	 if ( succLabelSet != NULL )  {
	    if ( succLabelSet->find( NONPUSHING_OUTLABEL ) == succLabelSet->end() )  {
	       if (( isSuccToFinalState == false ) && ( isSuccNullSet == false ))  {
		  set_union(	thisTransLabelSet.begin(), thisTransLabelSet.end(),
				succLabelSet->begin(), succLabelSet->end(),
				inserter(thisTransLabelSet, thisTransLabelSet.begin()) ) ;
	       }
	    }
	    else  {
#ifdef DEBUG
	       if ( succLabelSet->size() != 1 )
		  error ("WFSTLabelPushingNetwork::findLoopStartTrans - Label -1 is found but more than 1 label in the set") ;
#endif
	       isSuccToFinalState = true ;
	    }
	    // End of isSuccFinalState if-else
	 }
	 else  {
	    isSuccNullSet = true ;
	 }
	 // End of isSuccNullSet if-else
      }
      // End of LOOPEND if-else
   }

   hasReturned[transIndex] = true ;

   if (( isThisLoopStartTrans == true )||( isSuccNullSet == true ) || ( isSuccToFinalState == true ))
      thisTransLabelSet.clear() ;

   // First case: isSuccToFinalState == true
   if ( isSuccToFinalState == true )  {
      thisTransLabelSet.insert( NONPUSHING_OUTLABEL ) ;
   }
   else if ( isThisLoopStartTrans == true )  {
      // Second case: This transition is the start of a loop
      newUndecidedTrans.insert( transIndex ) ;
      loopStartTrans.insert( transIndex ) ;
      *returnLabelSet = NULL ;
      return NOTLOOPEND ;
   }
   else if ( isSuccNullSet == true )  {
      // Third case: One of the successor return no label set
      newUndecidedTrans.insert( transIndex ) ;
      *returnLabelSet = NULL ;
      return NOTLOOPEND ;
   }
#ifdef DEBUG
   else  {
      // Fourth case: Has all successors
      if ( thisTransLabelSet.empty() )
	 error("WFSTLabelPushingNetwork::findLoopStartTrans - thisTransLabelSet should not be empty") ;
   }
#endif

   pair<set<LabelSet>::iterator,bool> status = unique_outlabsets.insert( thisTransLabelSet ) ;
   arc_outlabset_map[transIndex] = &(*(status.first)) ;
   *returnLabelSet = &(*(status.first)) ;
   return NOTLOOPEND ;
}


// This function finds all the loops and put it in a set
// Return true if this trans is on the path of a loop starting at
// currLoopStartTrans. Otherwise return false.
// loopStartTransSet - a set of transitions which are the start of a loop
// loopStatesSet - a set of states where they are all on the same loop
// Start transversing from the start of a loop.
// If the next transition has been visited, check whether it is the start
// of the loop or has already been assigned to a loop. If yes, this
// transition is also inside the loop and put the "to" state of this
// transition in loopStatesSet.
// When checking one loop, it won't transverse into another loopStartTrans.
// So it is like cutting all other loopStartTrans when checking a
// particular loopStartTrans.
bool WFSTLabelPushingNetwork::findLoop( int transIndex, bool *hasVisited, const int currLoopStart, const set<int> &loopStartTransSet, set<int> &loopStatesSet, const set<int> &undecidedTransSet )
{
#ifdef DEBUG
   const LabelSet *tmpLabelSet = arc_outlabset_map[transIndex] ;
#endif

   // Terminating case
   // First case: This trans has a label set.
   if ( undecidedTransSet.find( transIndex ) == undecidedTransSet.end() )  {
      // Has a label set for this trans. Not on the path of a loop
#ifdef DEBUG
      if ( tmpLabelSet == NULL )
	 error("WFSTLabelPushingNetwork::findLoop - should have a label set for decided trans") ;
#endif
      return false ;
   }
#ifdef DEBUG
   else  {
      if ( tmpLabelSet != NULL )
	 error("WFSTLabelPushingNetwork::findLoop - should not have a label set for undecided trans") ;
   }
#endif

   int to = transitions[transIndex].toState ;

   // Second case: Has been visited before
   if ( hasVisited[transIndex] ) {
      if (( transIndex == currLoopStart ) || ( loopStatesSet.find( to ) != loopStatesSet.end() ))
	 return true ;
      else
	 return false ;
   }

   // Third case: Has not been visited before + it's one of the other loop
   // start.
   // Only allow currLoopStart. Other loopStart are deactivated temporarily
   // The first "if" is neccessary to allow currLoopStart to proceed since
   // hasVisited is false for the first time
   if ( transIndex != currLoopStart )  {
      if ( loopStartTransSet.find( transIndex ) != loopStartTransSet.end() )
	 return false ;
   }

   hasVisited[transIndex] = true ;

   // True if this trans is on the loop
   bool isThisTransOnTheLoop = false ;

   int *nextTrans = states[to].trans ;
   int nextNTrans = states[to].nTrans ;

   for ( int i = 0 ; i < nextNTrans ; i++ )  {
      bool isSuccOnTheLoop = findLoop( nextTrans[i], hasVisited, currLoopStart,
	    loopStartTransSet, loopStatesSet, undecidedTransSet ) ;

      if ( isSuccOnTheLoop )
	 isThisTransOnTheLoop = true ;
   }

   // Finish transversing all transitions
   if ( isThisTransOnTheLoop )  {
      // Put the to state of this trans to the loopStatesSet
      loopStatesSet.insert( to ) ;
      return true ;
   }

   return false ;

}


// This function combines loops that share some same states into one loop
void WFSTLabelPushingNetwork::combineLoop( list< set<int> > &loopList )
{
   list< set<int> >::iterator currSet = loopList.begin() ;
   list< set<int> >::iterator nextSet ;
   LabelSet intersectionSet ;

   while ( currSet != loopList.end() )  {
      nextSet = currSet ;
      nextSet++ ;
      while ( nextSet != loopList.end() )  {
#ifdef DEBUG
	 if (( (*currSet).empty() ) || ( (*nextSet).empty() ))
	    error("WFSTLabelPushingNetwork::combineLoop - either currSet or nextSet is empty") ;
#endif
	 // Find intersection
	 intersectionSet.clear() ;
	 set_intersection( (*currSet).begin(), (*currSet).end(), (*nextSet).begin(), (*nextSet).end(),
	       inserter( intersectionSet, intersectionSet.begin() ) ) ;

	 if ( intersectionSet.empty() )  {
	    nextSet++ ;
	 }
	 else  {
	    set_union( (*currSet).begin(), (*currSet).end(), (*nextSet).begin(), (*nextSet).end(),
		  inserter( *currSet, (*currSet).begin() ) ) ;
	    loopList.erase( nextSet ) ;
	    nextSet = currSet ;
	    nextSet++ ;
	 }
      }
      currSet++ ;
   }
}

// This function assign a common label set for each transition in a loop.
// Get a loop from the loop list. For each state of the loop, check if
// it is dependent to other loop. If all the states are not dependent
// on other loops, collect all the labels from all the set. Assign
// the union to all the loop transitions.
void WFSTLabelPushingNetwork::assignOutlabsToLoops( list< set<int> > &loopList , set<int> &undecidedTransSet )
{
   list< set<int> >::iterator currLoop ;

   while ( loopList.size() > 0 )  {
      currLoop = loopList.begin() ;

#ifdef DEBUG
      int numLoopsBefore = loopList.size() ;
#endif

      // Iterating all the loops
      while ( currLoop != loopList.end() )  {
#ifdef DEBUG
	 if ( (*currLoop).size() <= 0 )
	    error("WFSTLabelPushingNetwork::_assignOutLabesToLoops - loop size == 0") ;
#endif

	 // Get one loop
	 set<int>::iterator stateIter ;
	 bool isDependent = false ;

	 LabelSet thisLoopLabelSet ;
	 const LabelSet *succLabelSet ;

	 // For each state in the loop
	 for ( stateIter = (*currLoop).begin() ; stateIter != (*currLoop).end() ; stateIter++ )  {
	    int *nextTrans = states[*stateIter].trans ;
	    int nextNTrans = states[*stateIter].nTrans ;

	    // For each transition of a state
	    for ( int i = 0 ; i < nextNTrans ; i++ )  {
	       isDependent = findOutlabsOfOneTransFromLoopState( nextTrans[i], currLoop, loopList, &succLabelSet, undecidedTransSet ) ;

	       // One of the next transition depends on other loops
	       if ( isDependent )
		  break ;

	       // succLabelSet can be NULL if nextTrans is pointing towards a
	       // state of the currLoop
	       if ( succLabelSet != NULL ) {
		  set_union( thisLoopLabelSet.begin(), thisLoopLabelSet.end(),
			succLabelSet->begin(), succLabelSet->end(),
			inserter( thisLoopLabelSet, thisLoopLabelSet.begin() ) ) ;
	       }
	    }

	    // If one of the next transition depends on another loop, don't
	    // need to iterate other states of the currLoop anymore
	    if ( isDependent == true )
	       break ;
	 }
	 // End of state iteration

	 // If this loop is dependent on the other loop
	 if ( isDependent )  {
	    currLoop++ ;
	 }
	 else  {
	    // If not dependent on the other loop
	    // Assign all the union of output labels to all the states of the
	    // loop
#ifdef DEBUG
	    if ( thisLoopLabelSet.empty() )
	       error("WFSTLabelPushingNetwork::assignOutlabsToLoops - thisLoopLabelSet is empty") ;
#endif
	    if ( thisLoopLabelSet.find( NONPUSHING_OUTLABEL ) != thisLoopLabelSet.end() )  {
	       thisLoopLabelSet.clear() ;
	       thisLoopLabelSet.insert( NONPUSHING_OUTLABEL ) ;
	    }

	    pair<set<LabelSet>::iterator,bool> status = unique_outlabsets.insert( thisLoopLabelSet ) ;

	    // For each state in the loop
	    for ( stateIter = (*currLoop).begin() ; stateIter != (*currLoop).end() ; stateIter++ )  {

	       // For each transition pointing to this loop
	       int *loopTrans = states[*stateIter].trans ;
	       int loopNTrans = states[*stateIter].nTrans ;
	       for ( int i = 0 ; i < loopNTrans ; i++ )  {
		  int to = transitions[loopTrans[i]].toState ;

		  // If the transition is pointing into the loop
		  if ( (*currLoop).find( to ) != (*currLoop).end() )  {
		     arc_outlabset_map[ loopTrans[i] ] = &(*(status.first)) ;
		     undecidedTransSet.erase( loopTrans[i] ) ;
		  }
#ifdef DEBUG
		  else  {
		     if ( arc_outlabset_map[ loopTrans[i] ] == NULL )
			error("WFSTLabelPushingNetwork::_assignOutLabelToLoop - Not in loop but have no label set") ;
		  }
#endif
	       }
	    }

	    // Remove the loop from the list
	    currLoop = loopList.erase( currLoop ) ;
	 }
      }
      // End of iterating all the loops
#ifdef DEBUG
      int numLoopsAfter = loopList.size() ;
      if ( numLoopsAfter >= numLoopsBefore )
	 error("WFSTLabelPushingNetwork::assignOutlabsToLoops - No loops resolved") ;
#endif
   }
   // Exit when all loops are resolved
}


// Return true if this transition is dependent on the other loops.
// Otherwise return false.
// returnLabelSet is the label set of this transition
bool WFSTLabelPushingNetwork::findOutlabsOfOneTransFromLoopState( int transIndex, const list< set<int> >::iterator currLoop, list< set<int> > &loopList , const LabelSet **returnLabelSet , set<int> &undecidedTransSet )
{
   // Terminating case
   // If this transition has a label set
   // Changes 20050325
   if ( arc_outlabset_map[transIndex] != NULL )  {
      *returnLabelSet = arc_outlabset_map[transIndex] ;
      return false ;
   }

   int to = transitions[transIndex].toState ;
   // No label set
   if ( (*currLoop).find( to ) != (*currLoop).end() )  {
      // The toState is one of the states in the curr loop. Hence this
      // transition is not dependent
      *returnLabelSet = NULL ;
      return false ;
   }
   else  {
      for ( list< set<int> >::iterator tmpLoop = loopList.begin() ;
	    tmpLoop != loopList.end() ;
	    tmpLoop++ )  {
	 if ( tmpLoop != currLoop )  {
	    if ( (*tmpLoop).find( to ) != (*tmpLoop).end() )  {
	       // Dependent on other loop
	       *returnLabelSet = NULL ;
	       return true ;
	    }
	 }
      }
   }

   // Search through next transitions of this transition
   int *nextTrans = states[to].trans ;
   int nextNTrans = states[to].nTrans ;
   bool isDependent = false ;
   const LabelSet *succLabelSet ;
   LabelSet thisLabelSet ;

   for ( int i = 0 ; i < nextNTrans ; i++ )  {
      isDependent = findOutlabsOfOneTransFromLoopState( nextTrans[i], currLoop, loopList, &succLabelSet, undecidedTransSet ) ;
      if ( isDependent )
	 break ;

#ifdef DEBUG
      if ( succLabelSet == NULL )  {
	 // It means the next trans is pointing to currLoop, so this trans
	 // should also be in the loop
	 error("WFSTLabelPushingNetwork::findOutlabsOfOneTransFromLoopState - succLabelSet == NULL ") ;
      }
#endif

      set_union( thisLabelSet.begin(), thisLabelSet.end(),
	    succLabelSet->begin(), succLabelSet->end(),
	    inserter( thisLabelSet, thisLabelSet.begin() ) ) ;
   }

   if ( isDependent )  {
      *returnLabelSet = NULL ;
      return true ;
   }

   // This transition is not independent on other loops and it is not
   // pointing towards the loop i.e. it can be registered
   if ( thisLabelSet.find( NONPUSHING_OUTLABEL ) != thisLabelSet.end() )  {
      thisLabelSet.clear() ;
      thisLabelSet.insert( NONPUSHING_OUTLABEL ) ;
   }

   pair<set<LabelSet>::iterator,bool> status = unique_outlabsets.insert( thisLabelSet ) ;

   arc_outlabset_map[transIndex] = &(*(status.first)) ;
   undecidedTransSet.erase( transIndex ) ;
   *returnLabelSet = &(*(status.first)) ;
   return false ;

}


//****** WFSTSortedInLabelNetwork Implementation ******
// Changes Octavian
WFSTSortedInLabelNetwork::WFSTSortedInLabelNetwork() : WFSTNetwork() {}

WFSTSortedInLabelNetwork::WFSTSortedInLabelNetwork( real transWeightScalingFactor_ ) : WFSTNetwork( transWeightScalingFactor_ , 0.0 ) {}

// Changes Octavian 20060523
WFSTSortedInLabelNetwork::WFSTSortedInLabelNetwork( const char *wfstFilename , const char *inSymsFilename , const char *outSymsFilename , real transWeightScalingFactor_ , RemoveAuxOption removeAuxOption ) : WFSTNetwork( transWeightScalingFactor_ , 0.0 )
{
   FILE *fd ;

   if ( (fd = fopen( wfstFilename , "rb" )) == NULL )
      error("WFSTSortedInLabelNetwork::WFSTSortedInLabelNetwork - error opening wfstFilename") ;

   int cnt , i ;
   int from , to , in , out , final , maxOutLab=-1 , maxInLab=-1;
   float weight ;
   char *line ;

   line = new char[10000] ;
   while ( fgets( line , 10000 , fd ) != NULL )
   {
      if ( (cnt = sscanf( line , "%d %d %d %d %f" , &from , &to , &in , &out , &weight )) != 5 )
      {
         if ( (cnt = sscanf( line , "%d %d %d %d" , &from , &to , &in , &out )) != 4 )
         {
            if ( (cnt = sscanf( line , "%d %f" , &final , &weight )) != 2 )
            {
               if ( (cnt = sscanf( line , "%d" , &final )) != 1 )
               {
                  // Blank line or line with invalid format - just go to next line.
                  continue ;
               }
               else
                  weight = 0.0 ;
            }

            // Final state line
            if ( nFinalStates == nFinalStatesAlloc )
            {
               finalStates = (WFSTFinalState *)realloc( finalStates ,
                                        (nFinalStatesAlloc+1000)*sizeof(WFSTFinalState) ) ;
               nFinalStatesAlloc += 1000 ;
            }

            finalStates[nFinalStates].id = final ;
            finalStates[nFinalStates].weight = (real)(-weight * transWeightScalingFactor) ;
            nFinalStates++ ;
            continue ;
         }
         else
            weight = 0.0 ;
      }

      if ( (from < 0) || (to < 0) || (in < 0) || (out < 0) )
         error("WFSTSortedInLabelNetwork::WFSTSortedInLabelNetwork - something < 0. %d %d %d %d",from,to,in,out) ;

      if ( initState < 0 )
         initState = from ;   // init state is source state in first line of file

      if ( from > maxState )
         maxState = from ;
      if ( to > maxState )
         maxState = to ;

      // Add the new transition to the array of transitions
      if ( nTransitions == nTransitionsAlloc )
      {
         nTransitionsAlloc += 1000000 ;
         transitions = (WFSTTransition *)realloc( transitions ,
                                                  nTransitionsAlloc * sizeof(WFSTTransition) ) ;
         if ( transitions == NULL )
            error("WFSTSortedInLabelNetwork::WFSTSortedInLabelNetwork - transitions realloc failed") ;
         for ( i=nTransitions ; i<nTransitionsAlloc ; i++ )
            initWFSTTransition( transitions + i ) ;
      }

      transitions[nTransitions].id = nTransitions ;
      //transitions[nTransitions].fromState = from ;
      transitions[nTransitions].toState = to ;
      transitions[nTransitions].inLabel = in ;
      transitions[nTransitions].outLabel = out ;

      // FSM weights are -ve log
      transitions[nTransitions].weight = (real)(-weight * transWeightScalingFactor ) ;
      ++nTransitions ;

      if ( in > maxInLab ) {
         maxInLab = in;
      }
      if ( out > maxOutLab ) {
         maxOutLab = out;
      }

      // Make sure we have state entries for both from and to states in the
      // new transition.
      if ( maxState >= nStatesAlloc )
      {
         states = (WFSTState *)realloc( states , (maxState+1000)*sizeof(WFSTState) ) ;
         for ( i=nStatesAlloc ; i<(maxState+1000) ; i++ )
            initWFSTState( states + i ) ;
         nStatesAlloc = maxState + 1000 ;
      }

      // Initialise the from and to state labels if not already done.
      if ( states[from].label < 0 )
         states[from].label = from ;
      else if ( states[from].label != from )
         error("WFSTSortedInLabelNetwork::WFSTSortedInLabelNetwork - from state %d label mismatch" , from ) ;

      if ( states[to].label < 0 )
         states[to].label = to ;
      else if ( states[to].label != to )
         error("WFSTSortedInLabelNetwork::WFSTSortedInLabelNetwork - to state %d label mismatch" , to ) ;

      // Add the new transition index to the list in the from state
      // int ind = (states[from].nTrans)++ ;
      (states[from].nTrans)++ ;

      states[from].trans = (int *)realloc( states[from].trans , states[from].nTrans*sizeof(int) ) ;
      // Changes by Octavian
      int *insPtr = binarySearchInLabel( states[from].trans, states[from].nTrans-1, transitions[nTransitions-1].inLabel ) ;
      for (int *i = states[from].trans + states[from].nTrans - 1 ; i > insPtr ; i-- )  {
	 *i = *(i-1) ;
      }
      *insPtr = nTransitions - 1 ;
      //states[from].trans[ind] = nTransitions - 1 ;
      //**************************
      if ( states[from].nTrans > maxOutTransitions )
         maxOutTransitions = states[from].nTrans ;
   }

   // Changes by Octavian
   // Shrink the transitions array
   if ( nTransitionsAlloc > nTransitions )  {
      transitions = (WFSTTransition *)realloc( transitions ,
	       nTransitions * sizeof(WFSTTransition) ) ;

      if ( transitions == NULL )
	 error("WFSTSortedInLableNetwork::WFSTSortedInLabelNetwork - transitions realloc failed") ;

      nTransitionsAlloc = nTransitions ;
   }
   //**************

   for ( i=0 ; i<nFinalStates ; i++ )
   {
      if ( (finalStates[i].id < 0) || (finalStates[i].id > maxState) )
         error("WFSTSortedInLabelNetwork::WFSTSortedInLabelNetwork - finalState[%d].id out of range" , i ) ;
      if ( states[finalStates[i].id].label < 0 )
         error("WFSTSortedInLabelNetwork::WFSTSortedInLabelNetwork - finalState[%d] state label < 0" , i ) ;

      states[finalStates[i].id].finalInd = i ;
   }

   // Now create the input and/or output alphabets
   if ( inSymsFilename != NULL )
   {
      inputAlphabet = new WFSTAlphabet( inSymsFilename ) ;
      if ( maxInLab > inputAlphabet->getMaxLabel() )
         error("WFSTSortedInLabelNetwork::WFSTSortedInLabelNetwork - maxInLab > inputAlphabet->getMaxLabel()");
      maxInLab = inputAlphabet->getMaxLabel();
   }
   if ( outSymsFilename != NULL )
   {
      outputAlphabet = new WFSTAlphabet( outSymsFilename ) ;
      if ( maxOutLab > outputAlphabet->getMaxLabel() ) {
         error("WFSTSortedInLabelNetwork::WFSTSortedInLabelNetwork - maxOutLab=%d > outputAlphabet->getMaxLabel()=%d",
               maxOutLab, outputAlphabet->getMaxLabel() );
      }
      maxOutLab = outputAlphabet->getMaxLabel();
   }

   wordEndMarker = maxInLab + 1;
   if ( wordEndMarker <= maxOutLab ) {
      wordEndMarker = maxOutLab + 1;
   }

   // **********************************
   // Changes Octavian 20060523
   switch ( removeAuxOption )  {
      case REMOVEBOTH:
	 if ( (inputAlphabet != NULL) && (outputAlphabet != NULL) )
	 {
	    // Remove auxiliary symbols from the WFST (replace with epsilon)
	    removeAuxiliarySymbols( true ) ;
	 }
	 break ;
      case REMOVEINPUT:
	 if ( inputAlphabet != NULL )
	 {
	    removeAuxiliaryInputSymbols( true ) ;
	 }
	 break ;
      case NOTREMOVE:
	 break ;
      default:
	 error ("WFSTSortedInLabelNetwork::WFSTSortedInLabelNetwork - Invalid removeAuxOption") ;
	 break ;
   }
   /*
   if ( (inputAlphabet != NULL) && (outputAlphabet != NULL) )
   {
      // Remove auxiliary symbols from the WFST (replace with epsilon)
      removeAuxiliarySymbols( true ) ;
   }
   */
   // ***********************************

   delete [] line ;
   fclose( fd ) ;

   //fromBinFile = false ;
   nStates = (maxState+1) ;

}

// Get the weight, output label and state number on the epsilon path
// gStateArray will store the state numbers
// epsWeightArray will store the weights
// outLabelArray will store the output labels
void WFSTSortedInLabelNetwork::getStatesOnEpsPath( const int currGState, const real currEpsWeight, const int currOutLabel, int *gStateArray, real *epsWeightArray, int *outLabelArray, int *nGState, const int maxNGState )
{
#ifdef DEBUG
   if ( gStateArray == NULL )
      error("WFSTSortedInLabelNetwork::getStatesOnEpsPath - gStateArray == NULL") ;
   if ( epsWeightArray == NULL )
      error("WFSTSortedInLabelNetwork::getStatesOnEpsPath - epsWeightArray == NULL") ;
   if ( outLabelArray == NULL )
      error("WFSTSortedInLabelNetwork::getStatesOnEpsPath - outLabelArray == NULL") ;
#endif

   if ( *nGState < 0 )
      error("WFSTSortedInLabelNetwork::getStatesOnEpsPath - nGState < 0") ;
   if ( *nGState >= maxNGState )
      error("WFSTSortedInLabelNetwork::getStatesOnEpsPath - nGState >= maxNGState") ;

   // Check if currGState has already existed in the array
   for ( int i = 0 ; i < *nGState ; i++ )  {
      if ( gStateArray[i] == currGState )
	 error("WFSTSortedInLabelNetwork::getStatesOnEpsPath - currGState has already existed") ;
   }

   // Insert the currGState to the array
   gStateArray[*nGState] = currGState ;
   epsWeightArray[*nGState] = currEpsWeight ;
   outLabelArray[*nGState] = currOutLabel ;

   (*nGState)++ ;

   // Changes
   if ( states[currGState].nTrans > 0 )  {
      int nextGState ;
      real nextWeight ;
      int nextOutLabel ;
      int inLabel = getInfoOfOneTransition( currGState, 0, &nextWeight, &nextGState, &nextOutLabel) ;

      if ( inLabel == WFST_EPSILON )
	 getStatesOnEpsPath( nextGState, nextWeight, nextOutLabel, gStateArray, epsWeightArray, outLabelArray, nGState, maxNGState ) ;
   }

   return ;
}

// Changes Octavian 20060508
int WFSTSortedInLabelNetwork::getNextStateOnEpsPath( int gState, real *backoffWeight )
{
   if ( states[gState].nTrans > 0 )  {
      int nextGState ;
      real nextWeight ;
      int nextOutLabel ;
      int inLabel = getInfoOfOneTransition( gState, 0, &nextWeight, &nextGState, &nextOutLabel) ;

      if ( inLabel == WFST_EPSILON )  {
	 if ( nextOutLabel != WFST_EPSILON )
	    error("WFSTSortedInLabelNetwork::getNextStateOnEpsPath - outLabel != eps") ;

	 *backoffWeight = nextWeight ;
	 return nextGState ;
      }
   }

   return -1 ;
}


// A function which does binary search for inLabel on the array which
// contains transitions coming from the same state.
// The return pointer is the insertion point.
int *WFSTSortedInLabelNetwork::binarySearchInLabel( int *start, const int length, const int inLabel )
{
   // Terminating case
   // length = The length of the integer array before insertion
   if ( length == 0 )  {
      // Don't have anything - just insert to the start
      return start ;
   }
#ifdef DEBUG
   else if ( length < 0 )  {
      error("WFSTSortedInLabelNetwork::binarySearchInLabel - length < 0") ;
      return NULL ;
   }
#endif
   else if ( length == 1 )  {
      // Have one element in the array before insertion
      int inLabelInTransArray = transitions[*start].inLabel ;
      if ( inLabel < inLabelInTransArray )  {
	 return start ;
      }
      else if ( inLabel == inLabelInTransArray )  {
	 error("WFSTSortedInLabelNetwork::binarySearchInLabel - inLabel == inLabelInTransArray") ;
	 return NULL ;
      }
      else  {
	 return (start+1) ;
      }
   }
   else  {
      int middle = length / 2 ;
      int middleInLabel = transitions[*(start+middle)].inLabel ;

      if ( inLabel < middleInLabel )  {
	 return binarySearchInLabel( start, middle, inLabel ) ;
      }
      else if ( inLabel == middleInLabel )  {
	 error("WFSTSortedInLabelNetwork::binarySearchInLabel - inLabel == middleInLabel") ;
	 return NULL ;
      }
      else  {
	 if ( length == 2 )
	    return ( start + middle + 1 ) ;
	 else
	    return binarySearchInLabel( start+middle+1, length-middle-1, inLabel ) ;
      }
   }
}


}

