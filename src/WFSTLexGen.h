/*
 * Copyright 2004 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#ifndef WFST_LEX_GEN_INC
#define WFST_LEX_GEN_INC

#include "general.h"
#include "DecLexInfo.h"
#include "log_add.h"


/*
	Author:	Darren Moore (moore@idiap.ch)
	Date:		20 October 2004
	$Id: WFSTLexGen.h,v 1.9 2005/09/27 04:17:24 moore Exp $
*/


namespace Juicer {


struct WFSTLexNode
{
   int               phone ;

   int               nWords ;
   int               nWordsAlloc ;
   int               *words ;
   real              *wordProbs ;

   int               nSucs ;
   int               firstSuc ;  // children
   int               next ;      // siblings
};

/**
 * Class to convert a lexicon into a transducer representation
 */
class WFSTLexGen
{
public:
    WFSTLexGen(
        DecLexInfo *lexInfo_ ,
        bool addPronunWithEndSil_=false , 
        bool addPronunWithEndPause_=false ,
        bool addPronunWithStartSil_=false , 
        bool addPronunWithStartPause_=false , 
        real pauseTeeTransLogProb_=LOG_ZERO,
        bool outputAuxPhones_ = true
    ) ;

   virtual ~WFSTLexGen() ;

   void outputText() ;
   void writeFSM( const char *fsmFName , const char *inSymbolsFName , 
                  const char *outSymbolsFName , bool outputAuxPhones_=true ,
                  bool addPhiLoop=false ) ;

private:
   int                  nNodes ;
   int                  nNodesAlloc ;
   WFSTLexNode          *nodes ;
   
   int                  root ;

   int                  nAuxPhones ;
   int                  *auxPhones ;
   bool                 outputAuxPhones ;
   int                  phiWordLabel ;
   bool                 commonFinalState ;

   DecLexInfo           *lexInfo ;
   DecVocabulary        *vocab ;
   MonophoneLookup      *monoLookup ;
   bool                 addPronunWithEndSil ;
   bool                 addPronunWithStartSil ;
   bool                 addPronunWithEndPause ;
   bool                 addPronunWithStartPause ;
   real                 pauseTeeTransLogProb ;

   FILE                 *fsmFD ;
   int                  fsmInitState ;
   int                  fsmNStates ;
   int                  fsmNFinalStates ;
   int                  fsmNFinalStatesAlloc ;
   int                  *fsmFinalStates ;

   int addPhone( int prev , int phn , int wrd , real logProb=0.0 ) ;
   int getNode( int phn ) ;
   void addWordToNode( int nodeInd , int wrd , real logProb=0.0 ) ;
   void addDecLexInfoEntry( DecLexInfoEntry *entry ) ;
   void outputNode( int nodeInd , int *nPhns , int *phns ) ;
   void writeFSMNode( int nodeInd , int *nPhns , int *phns ) ;
   void outputFSMWord( int word , real logProb , int nPhns , int *phns ) ;
    int getOutputLabel(int iPhn);
};


}

#endif
