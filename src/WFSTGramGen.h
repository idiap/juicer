/*
 * Copyright 2004 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#ifndef WFST_GRAM_GEN_INC
#define WFST_GRAM_GEN_INC

#include "general.h"
#include "DecVocabulary.h"
#include "WFSTGeneral.h"


/*
	Author:	Darren Moore (moore@idiap.ch)
	Date:		20 October 2004
	$Id: WFSTGramGen.h,v 1.9 2005/09/27 04:16:07 moore Exp $
*/


namespace Juicer {


typedef enum
{
   WFST_GRAM_TYPE_WORDLOOP = 0 ,
   WFST_GRAM_TYPE_SIL_WORDLOOP_SIL ,
   WFST_GRAM_TYPE_NGRAM ,
   WFST_GRAM_TYPE_WORDPAIR ,
   WFST_GRAM_TYPE_INVALID
} WFSTGramType ;


struct WFSTNGramSMNode
{
   int   word ;   // index into vocab
   int   state ;  // the state ID we have allocated to 
   int   nextSib ;
   int   firstChild ;
   int   nOut ;
   int   nIn ;
};


class WFSTNGramStateManager
{
public:
   WFSTNGramStateManager( DecVocabulary *vocab_ ) ;
   virtual ~WFSTNGramStateManager() ;

   int getWFSTState( int nWords , int *words , bool isFromState , 
                     bool *isNew=NULL , bool isFinalLookup=false ) ;
   int lookupWFSTState( int nWords , int *words ) ;
   int getInitState() ;
   int getNumStates() { return nStates ; } ;
   int getEpsState() { return epsilonState ; } ;
   void outputText() ;
   void outputNonAccessible() ;
   
private:
   DecVocabulary     *vocab ;

   int               *firstLevelNodes ;

   int               nNodes ;
   int               nNodesAlloc ;
   WFSTNGramSMNode   *nodes ;

   int               nStates ;
   int               epsilonState ;
   int               initState ;

   int getNode( int parentNode , int word , bool addNew=true ) ;
   int allocNode( int word ) ;
   void initNode( int ind ) ;
   void outputNode( int node , int *nWords , int *words ) ;
   void outputNonAccessibleNode( int node , int *nWords , int *words ) ;
};


class WFSTGramGen
{
public:
   WFSTGramGen(
       DecVocabulary *vocab_ , WFSTGramType type_ , real lmScale_ , 
       real wordInsPen_ , const char *fName=NULL , const char *unkWord_=NULL
   ) ;
   virtual ~WFSTGramGen() ;

   void writeFSM(
       const char *fsmFName , const char *inSymbolsFName , 
       const char *outSymbolsFName ,
       bool addSil=false , bool phiBOTrans=false, bool normalise=false
   ) ;
   int getPhiLabel() { return phiLabel ; } ;

private:
   WFSTGramType   type ;
   DecVocabulary  *vocab ;
   real           wordInsPen ;
   real           lmScale ;
   char           *unkWord ;
   char           *arpaLMFName ;
   char           *wordPairFName ;
   int            phiLabel ;

   void writeFSMWordLoop( FILE *fsmFD ) ;
   void writeFSMSilWordLoopSil( FILE *fsmFD ) ;
   void writeFSMARPA(
       FILE *fsmFD , bool addSil , bool phiBOTrans, bool normalise
   ) ;
   void addDefaultBackoffPath( FILE *fsmFD , WFSTNGramStateManager *stateMan , 
                               int fromSt , int toNWords , int *toWords ) ; 
   void writeFSMWordPair( FILE *fsmFD ) ;
};


}

#endif

