/*
 * Copyright 2004 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#include <assert.h>

#include "WFSTLexGen.h"
#include "WFSTGeneral.h"

/*
	Author:	Darren Moore (moore@idiap.ch)
	Date:		20 October 2004
	$Id: WFSTLexGen.cpp,v 1.15 2005/09/27 04:12:45 moore Exp $
*/

using namespace Torch;

namespace Juicer {

/**
 * Builds a lexicon representing word pronunciations as a transducer
 */
WFSTLexGen::WFSTLexGen(
    DecLexInfo *lexInfo_ ,
    bool addPronunWithEndSil_ , 
    bool addPronunWithEndPause_ ,
    bool addPronunWithStartSil_ ,
    bool addPronunWithStartPause_ ,
    real pauseTeeTransLogProb_,
    bool outputAuxPhones_
)
{
    int i ;
   
    nNodes = 0 ;
    nNodesAlloc = 0 ;
    nodes = NULL ;
   
    root = getNode( -1 ) ;

    nAuxPhones = 0 ;
    auxPhones = NULL ;
    outputAuxPhones = outputAuxPhones_;
    //printf("Aux phones %s\n", outputAuxPhones ? "true" : "false");

    phiWordLabel = -1 ;

    lexInfo = lexInfo_ ;
    vocab = lexInfo->vocabulary ;
    monoLookup = lexInfo->getMonoLookup() ;

    addPronunWithEndSil = addPronunWithEndSil_ ;
    addPronunWithStartSil = addPronunWithStartSil_ ;
    if ( addPronunWithEndSil && (monoLookup->getSilMonophone() < 0) )
    {
        error("WFSTLexGen::WFSTLexGen"
              " - addPronunWithEndSil but no sil monophone specified"
              " in monoLookup.") ;
    }
    if ( addPronunWithStartSil && (monoLookup->getSilMonophone() < 0) )
    {
        error("WFSTLexGen::WFSTLexGen"
              " - addPronunWithStartSil but no sil monophone specified"
              " in monoLookup.") ;
    }
    addPronunWithEndPause = addPronunWithEndPause_ ;
    addPronunWithStartPause = addPronunWithStartPause_ ;
    if ( addPronunWithEndPause && (monoLookup->getPauseMonophone() < 0) )
    {
        error("WFSTLexGen::WFSTLexGen"
              " - addPronunWithEndPause but no pause monophone specified"
              " in monoLookup.") ;
    }
    if ( addPronunWithStartPause && (monoLookup->getPauseMonophone() < 0) )
    {
        error("WFSTLexGen::WFSTLexGen"
              " - addPronunWithStartPause but no pause monophone specified"
              " in monoLookup.") ;
    }
    pauseTeeTransLogProb = pauseTeeTransLogProb_ ;

    fsmFD = NULL ;
    fsmInitState = 0 ;
    fsmNStates = 1 ;   // Reserve 1 for the initial state
    fsmNFinalStates = 0 ;
    fsmNFinalStatesAlloc = 0 ;
    fsmFinalStates = NULL ;

    // Convert the lexInfo into a tree structure
    for ( i=0 ; i<lexInfo->nEntries ; i++ )
    {
        addDecLexInfoEntry( (lexInfo->entries) + i ) ;
    }

    // Setup the aux phones array
    if ( nAuxPhones > 0 )
        auxPhones = new int[nAuxPhones] ;
    for ( i=0 ; i<nAuxPhones ; i++ )
    {
        auxPhones[i] = monoLookup->getNumMonophones() + i ;
    }

    commonFinalState = false;
}


WFSTLexGen::~WFSTLexGen()
{
   int i ;

   if ( nodes != NULL )
   {
      for ( i=0 ; i<nNodes ; i++ )
      {
         if ( nodes[i].words != NULL )
            free( nodes[i].words ) ;
         if ( nodes[i].wordProbs != NULL )
            free( nodes[i].wordProbs ) ;
      }

      free( nodes ) ;
   }
   
   delete [] auxPhones ;
}


void WFSTLexGen::outputText()
{
   int phns[10000] ;
   int nPhns = 0 ;

   printf("****** BEGIN WFSTLexGen ******\n") ;
   printf("nNodes=%d nNodesAlloc=%d root=%d nAuxPhones=%d phiWordLabel=%d\n" ,
           nNodes , nNodesAlloc , root , nAuxPhones , phiWordLabel ) ;
           
   outputNode( root , &nPhns , phns ) ;
   printf("****** END WFSTLexGen ******\n") ;
}


void WFSTLexGen::outputNode( int nodeInd , int *nPhns , int *phns )
{
   int i , j ;

   if ( (nodeInd < 0) || (nodeInd >= nNodes) )
      error("WFSTLexGen::outputNode - nodeInd out of range") ;

   WFSTLexNode *node = nodes + nodeInd ;
   
   if ( nodeInd == root )
   {
      outputNode( node->firstSuc , nPhns , phns ) ;
      return ;
   }

   phns[*nPhns] = node->phone ;
   (*nPhns)++ ;
   
   if ( node->firstSuc >= 0 )
      outputNode( node->firstSuc , nPhns , phns ) ;

   if ( node->nWords > 0 )
   {
      for ( i=0 ; i<node->nWords ; i++ )
      {
         if ( (node->words[i] < 0) || (node->words[i] >= vocab->nWords) )
            error("WFSTLexGen::outputNode - node->words entry is out of range") ;
         
         printf("%-20s(%-6.3f)" , vocab->words[node->words[i]] , node->wordProbs[i] ) ;
                 
         for ( j=0 ; j<(*nPhns) ; j++ )
            printf(" %s" , monoLookup->getString( phns[j] ) ) ;

         if ( node->nWords > 1 )
            printf(" #%d" , i ) ;

         printf("\n") ;
         fflush( stdout ) ;
      }
   }
   
   (*nPhns)-- ;
   
   if ( node->next >= 0 )
      outputNode( node->next , nPhns , phns ) ;
}


int WFSTLexGen::addPhone( int prev , int phn , int wrd , real logProb )
{
   if ( (prev < 0) || (prev >= nNodes) )
      error("WFSTLexGen::addPhone - prev out of range") ;
      
   // Search for a 'phn' node in the sucs field of prev.
   int node = nodes[prev].firstSuc ;

   if ( node < 0 )
   {
      // 'prev' node has no children - add first one
      node = getNode( phn ) ;
      nodes[prev].firstSuc = node ;
      nodes[prev].nSucs = 1 ;   
   }
   else
   {
      // Node has children - search for existing child with same ID
      while ( node >= 0 )
      {
         if ( nodes[node].phone == phn )
            break ;
         node = nodes[node].next ;
      }

      if ( node < 0 )
      {
         // Add new node to head of list
         node = getNode( phn ) ;
         nodes[node].next = nodes[prev].firstSuc ;
         nodes[prev].firstSuc = node ;
         (nodes[prev].nSucs)++ ;
      }
   }
  
   // Add word information if required
   if ( wrd >= 0 )
       addWordToNode( node , wrd , logProb ) ;

   return node ;
}


int WFSTLexGen::getNode( int phn )
{
   if ( nNodes == nNodesAlloc )
   {
      nNodesAlloc += 10000 ;
      nodes = (WFSTLexNode *)realloc( nodes , nNodesAlloc*sizeof(WFSTLexNode) ) ;
   }

   nodes[nNodes].phone = phn ;
   nodes[nNodes].nWords = 0 ;
   nodes[nNodes].nWordsAlloc = 0 ;
   nodes[nNodes].words = NULL ;
   nodes[nNodes].wordProbs = NULL ;
   nodes[nNodes].nSucs = 0 ;
   nodes[nNodes].firstSuc = -1 ;
   nodes[nNodes].next = -1 ;

   nNodes++ ;
   return ( nNodes - 1 );
}


void WFSTLexGen::addWordToNode( int nodeInd , int wrd , real logProb )
{
   if ( (nodeInd < 0) || (nodeInd >= nNodes) )
      error("WFSTLexGen::addWordToNode - node out of range") ;
   
   WFSTLexNode *node = nodes + nodeInd ;
   
   if ( node->nWords == node->nWordsAlloc )
   {
      node->nWordsAlloc += 10 ;
      node->words = (int *)realloc( node->words , (node->nWordsAlloc)*sizeof(int) ) ;
      node->wordProbs = (real *)realloc( node->wordProbs , (node->nWordsAlloc)*sizeof(real) ) ;
   }

   node->words[node->nWords] = wrd ;
   node->wordProbs[node->nWords] = logProb ;
   (node->nWords)++ ;

   if ( node->nWords > nAuxPhones )
      nAuxPhones = node->nWords ;
}


void WFSTLexGen::addDecLexInfoEntry( DecLexInfoEntry *entry )
{
    int i ;
    int node = root ;

    // Build a list of each phone in the pronunciation
    for ( i=0 ; i<(entry->nPhones-1) ; i++ )
    {
        node = addPhone( node , entry->monophones[i] , -1 ) ;
    }

    // Special words do not require any other treatment
    bool noSil = ( !addPronunWithEndPause && !addPronunWithEndSil &&
                   !addPronunWithStartPause && !addPronunWithStartSil );
    if ( vocab->isSpecial( entry->vocabIndex ) || noSil )
    {
        node = addPhone(
            node , entry->monophones[(entry->nPhones)-1] , entry->vocabIndex , 
            entry->logPrior
        ) ;
        return;
    }

    // If we drop through to here we have to take care about extra
    // pronunciations.  There are lots of possibilities depending on
    // the combination of pause and silence models and whether or not
    // the pause skip is being added in the transducer.
    real basePronProb = entry->logPrior ;
    real pausePronProb = entry->logPrior ;
    real silPronProb = entry->logPrior ;

    if ( addPronunWithEndPause && (pauseTeeTransLogProb > LOG_ZERO) )
    {
        basePronProb += pauseTeeTransLogProb ;
        pausePronProb += log( 1.0 - exp( pauseTeeTransLogProb ) ) ;
    }

    // Normally the skip is in the pause - it's a tee model - but we
    // handle it here if a non-zero probability is supplied, or if
    // there there is silence but no pause.
    if (
        (!addPronunWithEndPause &&
         !addPronunWithStartPause &&
         (addPronunWithStartSil || addPronunWithEndSil))
        ||
        (addPronunWithEndPause &&
         (pauseTeeTransLogProb > LOG_ZERO))
    )
    {
        node = addPhone(
            node, entry->monophones[(entry->nPhones)-1], entry->vocabIndex,
            basePronProb
        );
    }
    else
    {
        node = addPhone(
            node, entry->monophones[(entry->nPhones)-1], -1,
            basePronProb
        );
    }

    // Add pronuns with sil/sp at end
    if ( addPronunWithEndSil && 
         ((entry->nPhones != 1) ||
          (entry->monophones[0] != monoLookup->getSilMonophone())) )
    {
        // Add another pronun that is the same as first one added
        // BUT with additional silence phone at end.
        if ( entry->monophones[(entry->nPhones)-1] ==
             monoLookup->getSilMonophone() )
        {
            error("WFSTLexGen::addDecLexInfoEntry"
                  " - addPronunWithEndSil true but lex entry has "
                  "silence monophone at end already.") ;
        }
        addPhone( node , monoLookup->getSilMonophone() ,
                  entry->vocabIndex , silPronProb ) ;
    }

    if ( addPronunWithEndPause && 
         ((entry->nPhones != 1) ||
          (entry->monophones[0] != monoLookup->getPauseMonophone())) )
    {
        // Add another pronun that is the same as first one added
        // BUT with additional pause phone at end.
        if ( entry->monophones[(entry->nPhones)-1] ==
             monoLookup->getPauseMonophone() )
        {
            error("WFSTLexGen::addDecLexInfoEntry"
                  " - addPronunWithEndPause true but lex entry has "
                  "pause monophone at end already.") ;
        }
        addPhone( node , monoLookup->getPauseMonophone() ,
                  entry->vocabIndex , pausePronProb ) ;
    }

    if ( addPronunWithStartSil && 
         ((entry->nPhones != 1) ||
          (entry->monophones[0] != monoLookup->getSilMonophone())) )
    {
        // Add another pronun that is the same as first one added BUT
        // with additional silence phone at START.
        if ( entry->monophones[0] == monoLookup->getSilMonophone() )
        {
            error("WFSTLexGen::addDecLexInfoEntry"
                  " - addPronunWithStartSil true but lex entry has "
                  "silence monophone at start already.") ;
        }

        node = root ;
        node = addPhone( node , monoLookup->getSilMonophone() , -1 ) ;
        for ( i=0 ; i<(entry->nPhones-1) ; i++ )
        {
            node = addPhone( node , entry->monophones[i] , -1 ) ;
        }
        node = addPhone(
            node, entry->monophones[(entry->nPhones)-1],
            entry->vocabIndex, entry->logPrior
        ) ;
    }

    if ( addPronunWithStartPause && 
         ((entry->nPhones != 1) ||
          (entry->monophones[0] != monoLookup->getPauseMonophone())) )
    {
        // Add another pronun that is the same as first one added BUT
        // with additional pause phone at START.
        if ( entry->monophones[0] == monoLookup->getPauseMonophone() )
        {
            error("WFSTLexGen::addDecLexInfoEntry"
                  " - addPronunWithStartPause true but lex entry has "
                  "pause monophone at start already.") ;
        }

        node = root ;
        node = addPhone( node , monoLookup->getPauseMonophone() , -1 ) ;
        for ( i=0 ; i<(entry->nPhones-1) ; i++ )
        {
            node = addPhone( node , entry->monophones[i] , -1 ) ;
        }
        node = addPhone(
            node, entry->monophones[(entry->nPhones)-1],
            entry->vocabIndex, entry->logPrior
        ) ;
    }
}


/**
 * Creates the lexicon transducer that is the union of the transducers
 * for all pronunciations Input labels are CI phonemes, output labels
 * are words.
 *
 * NOTE: We add 1 to all phone and word indices, because the 0th index is
 *       always reserved for the epsilon symbol.
 */
void WFSTLexGen::writeFSM(
    const char *fsmFName , const char *inSymbolsFName ,
    const char *outSymbolsFName , bool outputAuxPhones_ , 
    bool addPhiLoop
)
{
   int   i ;
   FILE  *inSymFD=NULL , *outSymFD=NULL ;
  
   fsmFD = NULL ;
   fsmInitState = 0 ;
   fsmNStates = 1 ;   // Reserve 1 for the initial state
   fsmNFinalStates = 0 ;
   fsmNFinalStatesAlloc = 0 ;
   fsmFinalStates = NULL ;
   
   commonFinalState = true;
   if ( commonFinalState )
   {
      fsmFinalStates = (int *)malloc( sizeof(int) );
      fsmNFinalStatesAlloc = 1;
      fsmNFinalStates = 1;
      fsmFinalStates[0] = fsmNStates++ ;
   }

   outputAuxPhones = outputAuxPhones_ ;
   if (addPhiLoop)
	   phiWordLabel = vocab->nWords + 1 ;

   // Kind of redundant given the above
   if ( (phiWordLabel >= 0) && (phiWordLabel <= vocab->nWords) )
      error("WFSTLexGen::writeFSM - phiWordLabel invalid") ;

   // Open the FSM file.
   if ( (fsmFD = fopen( fsmFName , "wb" )) == NULL )
      error("WFSTLexGen::writeFSM"
            " - error opening FSM output file: %s",fsmFName) ;
   
   // Recursively go through our tree and write word entries to FSM file.
   int nPhns = 0 ;
   int *phns = new int[10000] ;
   int inputPhiLabel = -1 ;
   writeFSMNode( root , &nPhns , phns ) ;
   delete [] phns ;
  
   if ( phiWordLabel >= 0 )
   {
      // Write self loop transitions on the initial state with input
      // label epsilon and output label phiWordLabel.
      inputPhiLabel = monoLookup->getNumMonophones() + nAuxPhones + 1 ;
      writeFSMTransition(
          fsmFD , fsmInitState , fsmInitState , inputPhiLabel , phiWordLabel
      ) ;
   }
   
#if 0
   // Write a silence loop on the initial state
   int sil = monoLookup->getSilMonophone();
   writeFSMTransition(
       fsmFD , fsmInitState , fsmInitState , sil+1 , WFST_EPSILON
   ) ;
#endif

   // Write the final states
   for ( i=0 ; i<fsmNFinalStates ; i++ )
   {
      writeFSMFinalState( fsmFD , fsmFinalStates[i] ) ;
   }
   
   // Close the FSM file.
   fclose( fsmFD ) ;

   // Deallocate the finalStates memory
   if ( fsmFinalStates != NULL )
      free( fsmFinalStates ) ;
   fsmFinalStates = NULL ;

   // Now write the input (phonemes) symbols file
   if ( (inSymFD = fopen( inSymbolsFName , "wb" )) == NULL )
      error("DecLexInfo::outputWFST"
            " - error opening input symbols file: %s",inSymbolsFName) ;
   
   // epsilon symbol is always 0th symbol
   writeFSMSymbol( inSymFD , WFST_EPSILON_STR , WFST_EPSILON ) ;
   for ( i=0 ; i<monoLookup->getNumMonophones() ; i++ )
   {
      writeFSMSymbol( inSymFD , monoLookup->getString(i) , i+1 ) ;
   }

   // write the auxiliary phonemes
   if ( outputAuxPhones )
   {
      char str[20] ;
      for ( i=0 ; i<nAuxPhones ; i++ )
      {
         sprintf( str , "#%d" , i ) ;
         writeFSMSymbol( inSymFD , str , auxPhones[i] + 1 ) ;
      }
   }

   if ( inputPhiLabel >= 0 )
   {
      writeFSMSymbol( inSymFD , WFST_PHI_STR , inputPhiLabel ) ;
   }
   
   fclose( inSymFD ) ;
   
   // Now write the output (words) symbols files
   if ( (outSymFD = fopen( outSymbolsFName , "wb" )) == NULL )
      error("DecLexInfo::outputWFST"
            " - error opening output symbols file: %s",outSymbolsFName) ;
   
   // epsilon symbol is always 0th symbol
   writeFSMSymbol( outSymFD , WFST_EPSILON_STR , WFST_EPSILON ) ;
   for ( i=0 ; i<vocab->nWords ; i++ )
   {
      if ( vocab->getNumPronuns(i) > 0 )
         writeFSMSymbol( outSymFD , vocab->words[i] , i+1 ) ;
   }
   if ( phiWordLabel >= 0 )
   {
      writeFSMSymbol( outSymFD , WFST_PHI_STR , phiWordLabel ) ;
   }
#if 1
      writeFSMSymbol( outSymFD , "#sil" , phiWordLabel + 1) ;
      writeFSMSymbol( outSymFD , "#sp" , phiWordLabel + 2) ;   
#endif
   
   fclose( outSymFD ) ;
}


void WFSTLexGen::writeFSMNode( int nodeInd , int *nPhns , int *phns )
{
   int i ;

   if ( (nodeInd < 0) || (nodeInd >= nNodes) )
      error("WFSTLexGen::outputNode - nodeInd out of range") ;

   WFSTLexNode *node = nodes + nodeInd ;
   
   if ( nodeInd == root )
   {
      writeFSMNode( node->firstSuc , nPhns , phns ) ;
      return ;
   }

   phns[*nPhns] = node->phone ;
   (*nPhns)++ ;
   
   if ( node->firstSuc >= 0 )
      writeFSMNode( node->firstSuc , nPhns , phns ) ;

   if ( node->nWords > 0 )
   {
      for ( i=0 ; i<node->nWords ; i++ )
      {
         if ( (node->words[i] < 0) || (node->words[i] >= vocab->nWords) )
            error("WFSTLexGen::writeFSMNode"
                  " - node->words entry is out of range") ;
         
         // We need to add an auxiliary phone before outputting FSM entry
         //   for this pronun.
         if ( outputAuxPhones )
         {
            phns[*nPhns] = auxPhones[i] ;
            outputFSMWord( node->words[i] , node->wordProbs[i] , (*nPhns) + 1 , phns ) ;
         }
         else
         {
            outputFSMWord( node->words[i] , node->wordProbs[i] , *nPhns , phns ) ;
         }
      }
   }
   
   (*nPhns)-- ;
   
   if ( node->next >= 0 )
      writeFSMNode( node->next , nPhns , phns ) ;
}


/**
 * PNG - hack
 * Translate output labels for sil and sp if necessary
 */
int WFSTLexGen::getOutputLabel(int iPhn)
{
#if 0
    assert(phiWordLabel >= 0);
    int label = WFST_EPSILON;
    if ( addPronunWithEndSil &&
         (iPhn == monoLookup->getSilMonophone()) )
        label = phiWordLabel + 1;
    if ( addPronunWithEndPause &&
         (iPhn == monoLookup->getPauseMonophone()) )
        label = phiWordLabel + 2;

    return label;
#else
    return WFST_EPSILON;
#endif
}

void WFSTLexGen::outputFSMWord(
    int word , real logProb , int nPhns , int *phns
)
{
    if ( nPhns <= 0 )
        error("WFSTLexGen::outputFSMWord - nPhns <= 0");
      
    int j ;

    // default case - all pronuns equally weighted to logProb
    real weight = -logProb;

    // change this to enable/disable weighting of pronuns
    bool doPronunWeighting=true ;
    if ( doPronunWeighting )
    {
        int scale=1 ;
        if ( addPronunWithEndSil &&
             (word != vocab->sentStartIndex) && 
             (word != vocab->sentEndIndex) )
        {
            scale++ ;
        }
        if ( addPronunWithStartSil &&
             (word != vocab->sentStartIndex) && 
             (word != vocab->sentEndIndex) )
        {
            scale++ ;
        }
        //if ( addPronunWithEndPause &&
        //     (word != vocab->sentStartIndex) && 
        //     (word != vocab->sentEndIndex) )
        //{
        //    scale++ ;
        //}

        //real weight ;

        // change this if you want to change the weighting behaviour
        bool weightByNWords=false ;
        if ( weightByNWords )
        {
            int nWords = vocab->nWords ;
            if ( (vocab->sentStartIndex >= 0) &&
                 (vocab->getNumPronuns(vocab->sentStartIndex) == 0) )
                nWords-- ;
            if ( (vocab->sentEndIndex >= 0) &&
                 (vocab->getNumPronuns(vocab->sentEndIndex) == 0) )
                nWords-- ;
            weight = log( nWords * vocab->getNumPronuns(word) * scale ) ;
        }
        else
        {
            //weight = log( vocab->getNumPronuns(word) * scale ) ;
            weight += log(scale);
        }
    }

    if ( ! commonFinalState )
    {
        // Write the first arc
        writeFSMTransition(
            fsmFD , fsmInitState , fsmNStates , phns[0] + 1 , word + 1 , weight
        ) ;
        fsmNStates++ ;
      
        // Write the remaining arcs - output labels are all epsilon
        for ( j=1 ; j<nPhns ; j++ )
        {
            writeFSMTransition(
                fsmFD , fsmNStates-1 , fsmNStates , phns[j] + 1 ,
                getOutputLabel(phns[j])
            ) ;
            fsmNStates++ ;
        }

        // Make a note of the final state for this pronunciation
        if ( fsmNFinalStates == fsmNFinalStatesAlloc )
        {
            fsmFinalStates = (int *)realloc(
                fsmFinalStates , (fsmNFinalStatesAlloc+1000)*sizeof(int)
            ) ;
            fsmNFinalStatesAlloc += 1000 ;
        }
        fsmFinalStates[fsmNFinalStates++] = fsmNStates - 1 ;
      
    }
    else
    {
        if ( nPhns > 1 )
        {
#if 0
            // Write initial epsilon arc
            writeFSMTransition(
                fsmFD , fsmInitState , fsmNStates ,
                WFST_EPSILON , word + 1 ,
                weight
            ) ;
            fsmNStates++ ;
            
            // Write the first arc
            writeFSMTransition(
                fsmFD , fsmNStates-1 , fsmNStates , phns[0] + 1 , WFST_EPSILON
            ) ;
            fsmNStates++ ;
#else
            // Write the first arc
            writeFSMTransition(
                fsmFD , fsmInitState , fsmNStates , phns[0] + 1 , word + 1 ,
                weight
            ) ;
            fsmNStates++ ;
#endif

            // Write arcs for all except the last phoneme in this pronun
            for ( j=1 ; j<(nPhns-1) ; j++ ) {
                writeFSMTransition(
                    fsmFD , fsmNStates-1 , fsmNStates , phns[j] + 1 ,
                    getOutputLabel(phns[j])
                );
                fsmNStates++;
            }
            // Write the last phoneme arc - goes to common final state
            if ( fsmNFinalStates != 1 )
                error("WFSTLexGen::outputFSMWord"
                      " - commonFinalState true but fsmNFinalStates != 1");
            writeFSMTransition(
                fsmFD , fsmNStates-1 , fsmFinalStates[0] , phns[j] + 1 ,
                getOutputLabel(phns[j])
            );
        }
        else
        {
            // Write the first arc, which goes to the common final state
            if ( fsmNFinalStates != 1 )
                error("WFSTLexGen::outputFSMWord"
                      " - commonFinalState true but fsmNFinalStates != 1 (2)");
            writeFSMTransition(
                fsmFD, fsmInitState, fsmFinalStates[0], phns[0] + 1, word + 1,
                weight
            );
        }
    }
}

}

