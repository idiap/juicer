/*
 * Copyright 2004 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#include <assert.h>

#include "WFSTGramGen.h"
#include "log_add.h"
#include "ARPALM.h"
#include "WordPairLM.h"

/*
	Author:	Darren Moore (moore@idiap.ch)
	Date:		20 October 2004
	$Id: WFSTGramGen.cpp,v 1.12 2005/09/27 04:11:18 moore Exp $
*/

using namespace Torch;

namespace Juicer {


//---------------- WFSTNGramStateManager implementation ----------------


WFSTNGramStateManager::WFSTNGramStateManager( DecVocabulary *vocab_ )
{
   vocab = vocab_ ;

   firstLevelNodes = new int[vocab->nWords + 1] ;  // 1 extra for <unk>
   for ( int i=0 ; i<(vocab->nWords+1) ; i++ )
   {
      firstLevelNodes[i] = -1 ;
   }
   
   nNodes = 0 ;
   nNodesAlloc = 0 ;
   nodes = NULL ;

   // "pre-allocate" a state for the epsilon state.
   nStates = 1 ;
   epsilonState = 0 ;

   // Figure out the initial state 
   initState = -1 ;
   if ( vocab->sentStartIndex >= 0 )
   {
      if ( vocab->getNumPronuns(vocab->sentStartIndex) > 0 )
      {
         // We have a sent start pronun - we need a separate initial state.
         initState = nStates++ ;
      }
      else
      {
         // We do not have a sent start pronun, so our initial state is the
         //   state identified by the context <s>
         //int wrd = vocab->sentStartIndex ;
         //initState = getWFSTState( 1 , &wrd ) ;
         int node = getNode( -1 , vocab->sentStartIndex ) ;
         initState = nodes[node].state = nStates++ ;
      }
   }
   else
   {
      // No sent start word - the initial state is the epsilon state
      initState = epsilonState ;
   }
}


WFSTNGramStateManager::~WFSTNGramStateManager()
{
   delete [] firstLevelNodes ;

   if ( nodes != NULL )
   {
      free( nodes ) ;
   }
}


int WFSTNGramStateManager::getWFSTState(
    int nWords , int *words , bool isFromState , 
    bool *isNew , bool isFinalLookup
)
{
   if ( isNew != NULL )
      *isNew = false ;
      
   if ( nWords == 0 )
   {
      // we want the epsilon state
      return epsilonState ;
   }

   int node = -1 ;
   for ( int i=0 ; i<nWords ; i++ )
   {
      if ( (i < (nWords-1)) && (words[i] == vocab->sentEndIndex) )
         error("WFSTNGramStateManager::getWFSTState - sent end word not last word in entry") ;
      node = getNode( node , words[i] ) ;
   }

   if ( nodes[node].state < 0 )
   {
      nodes[node].state = nStates++ ;
      if ( isNew != NULL )
         *isNew = true ;
   }

   if ( ! isFinalLookup ) {
      if ( isFromState ) {
         (nodes[node].nOut)++;
      } else {
         (nodes[node].nIn)++;
      }
   }

   return nodes[node].state ;
}


int WFSTNGramStateManager::lookupWFSTState( int nWords , int *words )
{
   // Does not add new states if one does not already exist - returns -1 in this case
   if ( nWords == 0 )
   {
      // we want the epsilon state
      return epsilonState ;
   }

   int node = -1 ;
   for ( int i=0 ; i<nWords ; i++ )
   {
      if ( (i < (nWords-1)) && (words[i] == vocab->sentEndIndex) )
         error("WFSTNGramStateManager::getWFSTState - sent end word not last word in entry") ;

      node = getNode( node , words[i] , false ) ;
      if ( node < 0 )
         return -1 ;
   }

   return nodes[node].state ;
}


int WFSTNGramStateManager::getInitState()
{
   if ( initState < 0 )
      error("WFSTNGramStateManager::getInitState - initState is NULL") ;
   
   return initState ;
}


int WFSTNGramStateManager::getNode( int parentNode , int word , bool addNew )
{
   int node=-1 ;
   
   if ( parentNode < 0 )
   {
      // look in firstLevelNodes
      if ( firstLevelNodes[word] < 0 )
      {
         if ( addNew )
            firstLevelNodes[word] = allocNode( word ) ;
         else
            return -1 ;
      }  
      
      node = firstLevelNodes[word] ; 
   }
   else
   {
      if ( word == vocab->sentStartIndex )
         error("WFSTNGramStateManager::getNode - sent start word is not first word in entry") ;
         
      // search through the children of 'parentNode' looking for a node
      //   with word field equal to 'word'.
      node = nodes[parentNode].firstChild ;
      while ( node >= 0 )
      {
         if ( nodes[node].word == word )
            break ;         
         node = nodes[node].nextSib ;
      }

      if ( node < 0 )
      {
         if ( addNew )
         {
            node = allocNode( word ) ;
            nodes[node].nextSib = nodes[parentNode].firstChild ;
            nodes[parentNode].firstChild = node ;
         }
         else
            return -1 ;
      }
   }

   return node ;
}
   

int WFSTNGramStateManager::allocNode( int word )
{
   if ( nNodes == nNodesAlloc )
   {
      nNodesAlloc += 10000 ;
      nodes = (WFSTNGramSMNode *)realloc( nodes , nNodesAlloc*sizeof(WFSTNGramSMNode) ) ;
      for ( int i=nNodes ; i<nNodesAlloc ; i++ )
         initNode( i ) ;
   }

   nodes[nNodes++].word = word ;
   return ( nNodes - 1 ) ;
}


void WFSTNGramStateManager::initNode( int ind )
{
   if ( (ind < 0) || (ind >= nNodesAlloc) )
      error("WFSTNGramStateManager::initNode - ind out of range") ;
   
   nodes[ind].word = -1 ;
   nodes[ind].state = -1 ;
   nodes[ind].nextSib = -1 ;
   nodes[ind].firstChild = -1 ;
   nodes[ind].nOut = 0 ;
   nodes[ind].nIn = 0 ;
}

/*
P(w1,w2,w3)
P(w2,w3)bo(w1,w2)
P(w2,w3)
P(w1,w2,w3,w4)
P(w2,w3,w4)bo(w1,w2,w3)
P(w2,w3,w4)
P(w3,w4)bo(w2,w3)
*/

void WFSTNGramStateManager::outputText()
{
   int *words = new int[30] ;
   int nWords=0 ;
   
   for ( int i=0 ; i<(vocab->nWords+1) ; i++ )
   {
      nWords = 0 ;
      outputNode( firstLevelNodes[i] , &nWords , words ) ;
   }

   delete [] words ;
}


void WFSTNGramStateManager::outputNode( int node , int *nWords , int *words )
{
   if ( node < 0 )
      return ;

   words[(*nWords)++] = nodes[node].word ;
 
   if ( nodes[node].state >= 0 )
   {
      // we want to output this node.
      printf("[") ;
      for ( int i=0 ; i<*nWords ; i++ )
      {
         if ( words[i] == vocab->nWords )
            printf(" <unk>") ;
         else if ( (words[i] >= 0) && (words[i] < vocab->nWords) )
            printf(" %s" , vocab->words[words[i]] ) ;
         else
            error("WFSTNGramStateManager::outputNode - words[i]=%d out of range",words[i]);
      }
      printf(" ] = %d\n" , nodes[node].state ) ;            
   }

   int next = nodes[node].firstChild ;
   while ( next >= 0 )
   {
      outputNode( next , nWords , words ) ;
      next = nodes[next].nextSib ;
   }

   (*nWords)-- ;
}


void WFSTNGramStateManager::outputNonAccessible()
{
   int *words = new int[30] ;
   int nWords=0 ;
   
   for ( int i=0 ; i<(vocab->nWords+1) ; i++ )
   {
      nWords = 0 ;
      outputNonAccessibleNode( firstLevelNodes[i] , &nWords , words ) ;
   }

   delete [] words ;
}


void WFSTNGramStateManager::outputNonAccessibleNode( int node , int *nWords , int *words )
{
   if ( node < 0 )
      return ;

   words[(*nWords)++] = nodes[node].word ;
 
   if ( nodes[node].state >= 0 ) {
      if ( nodes[node].nOut == 0 ) {
         // Non-coaccessible state
         printf("NonCoAcc:  ");
      } 
      if ( nodes[node].nIn == 0 ) {
         printf("NonAcc:  ");
      }

      if ((nodes[node].nOut == 0) || (nodes[node].nIn == 0)) {
         // we want to output this node.
         printf("[") ;
         for ( int i=0 ; i<*nWords ; i++ )
         {
            if ( words[i] == vocab->nWords )
               printf(" <unk>") ;
            else if ( (words[i] >= 0) && (words[i] < vocab->nWords) )
               printf(" %s" , vocab->words[words[i]] ) ;
            else
               error("WFSTNGramStateManager::outputNonCoaccessibleNode - words[i]=%d out of range",words[i]);
         }
         printf(" ] = %d\n" , nodes[node].state ) ;
      }
   }

   int next = nodes[node].firstChild ;
   while ( next >= 0 )
   {
      outputNonAccessibleNode( next , nWords , words ) ;
      next = nodes[next].nextSib ;
   }

   (*nWords)-- ;
}


//-------------------- WFSTNGramGen implementation ----------------------


WFSTGramGen::WFSTGramGen(
    DecVocabulary *vocab_ , WFSTGramType type_ , real lmScale_ , 
    real wordInsPen_ , const char *fName , const char *unkWord_
)
{
   if ( (vocab = vocab_) == NULL )
      error("WFSTGramGen::WFSTGramGen - vocab is NULL") ;
   if ( (type_ < 0) || (type_ >= WFST_GRAM_TYPE_INVALID) )
      error("WFSTGramGen::WFSTGramGen - type_ is invalid") ;
   if ( (type == WFST_GRAM_TYPE_NGRAM) && (arpaLMFName == NULL) )
      error("WFSTGramGen::WFSTGramGen - type is NGRAM but no arpa file specified") ;
      
   type = type_ ;
   wordInsPen = wordInsPen_ ;
   lmScale = lmScale_ ;
   
   unkWord = NULL ;
   if ( (unkWord_ != NULL) && (unkWord_[0] != '\0') )
   {
      unkWord = new char[strlen(unkWord_)+1] ;
      strcpy( unkWord , unkWord_ ) ;
   }
   
   wordPairFName = NULL ;
   arpaLMFName = NULL ;
   if ( type == WFST_GRAM_TYPE_NGRAM )
   {
      if ( (fName == NULL) || (fName[0] == '\0') )
         error("WFSTGramGen::WFSTGramGen - NGRAM type but no filename defined") ;
      arpaLMFName = new char[strlen(fName)+1] ;
      strcpy( arpaLMFName , fName ) ;
   }
   else if ( type == WFST_GRAM_TYPE_WORDPAIR )
   {
      if ( (fName == NULL) || (fName[0] == '\0') )
         error("WFSTGramGen::WFSTGramGen - WORDPAIR type but no filename defined") ;
      wordPairFName = new char[strlen(fName)+1] ;
      strcpy( wordPairFName , fName ) ;
   }

   phiLabel = -1 ;
}


WFSTGramGen::~WFSTGramGen()
{
   delete [] unkWord ;
   delete [] arpaLMFName ;
}


void WFSTGramGen::writeFSM(
    const char *fsmFName , const char *inSymbolsFName , 
    const char *outSymbolsFName , bool addSil , bool phiBOTrans
)
{
   FILE *fd ;
   int i ;

   // Open the FSM output file.
   if ( (fd = fopen( fsmFName , "wb" )) == NULL )
      error("WFSTGramGen::writeFSM - error opening FSM output file: %s",fsmFName) ;

   // Write the FSM output file.
   if ( type == WFST_GRAM_TYPE_WORDLOOP )
   {
      writeFSMWordLoop( fd ) ;
   }
   else if ( type == WFST_GRAM_TYPE_SIL_WORDLOOP_SIL )
   {
      writeFSMSilWordLoopSil( fd ) ;
   }
   else if ( type == WFST_GRAM_TYPE_NGRAM )
   {
      writeFSMARPA( fd , addSil , phiBOTrans ) ;
   }
   else if ( type == WFST_GRAM_TYPE_WORDPAIR )
   {
      writeFSMWordPair( fd ) ;
   }
   else
   {
      error("WFSTGramGen::writeFSM - unsupported type") ;
   }

   // Close the FSM output file.
   fclose( fd ) ;
   
   // Write the output symbols file.
   if ( (fd = fopen( outSymbolsFName , "wb" )) == NULL )
      error("WFSTGramGen::writeFSM - error opening output symbols file: %s",outSymbolsFName) ;
   
   // epsilon symbol is always 0th symbol
   writeFSMSymbol( fd , WFST_EPSILON_STR , WFST_EPSILON ) ;
   for ( i=0 ; i<vocab->nWords ; i++ )
   {
      if ( vocab->getNumPronuns(i) > 0 )
         writeFSMSymbol( fd , vocab->words[i] , i+1 ) ;
   }
   if ( phiLabel >= 0 )
   {
      writeFSMSymbol( fd , WFST_PHI_STR , phiLabel ) ;
   }
   fclose( fd ) ;

   // Write the input symbols file.
   if ( (fd = fopen( inSymbolsFName , "wb" )) == NULL )
      error("WFSTGramGen::writeFSM - error opening input symbols file: %s",inSymbolsFName) ;
   
   // epsilon symbol is always 0th symbol
   writeFSMSymbol( fd , WFST_EPSILON_STR , WFST_EPSILON ) ;
   for ( i=0 ; i<vocab->nWords ; i++ )
   {
      if ( vocab->getNumPronuns(i) > 0 )
         writeFSMSymbol( fd , vocab->words[i] , i+1 ) ;
   }
   if ( phiLabel >= 0 )
   {
      writeFSMSymbol( fd , WFST_PHI_STR , phiLabel ) ;
   }
   if (addSil)
   {
       assert(phiLabel > 0);
       writeFSMSymbol( fd , "#sil" , phiLabel + 1 ) ;
       writeFSMSymbol( fd , "#sp" , phiLabel + 2 ) ;       
   }
   fclose( fd ) ;
}


void WFSTGramGen::writeFSMWordLoop( FILE *fsmFD )
{
   warning("WFSTGramGen::writeFSMWordLoop - needs updating(?)") ;
   
   // There are 2 states
   int i , initSt=0 , finalSt=1 ;

   // Calculate the (-ve log) weight to use for each transition
   real weight = log( vocab->nWords ) ;

   // Transition from initSt to finalSt for each word in vocab.
   for ( i=0 ; i<vocab->nWords ; i++ )
   {
      if ( i == vocab->silIndex )
         continue ;
      if ( vocab->getNumPronuns(i) <= 0 )
         continue ;

      writeFSMTransition( fsmFD , initSt , finalSt , i+1 , i+1 , weight ) ;
   }

   if ( vocab->silIndex >= 0 )
   {
      // Silence transition from finalSt to finalSt with 0.0 weight
      writeFSMTransition( fsmFD , finalSt , finalSt , vocab->silIndex + 1 , 
                          vocab->silIndex + 1 , 0.0 ) ;
   }
   
   // Epsilon transition from finalSt to initSt.
   writeFSMTransition( fsmFD , finalSt , initSt , WFST_EPSILON , WFST_EPSILON , -wordInsPen ) ;

   // Write the final state entry
   writeFSMFinalState( fsmFD , finalSt ) ;
}


void WFSTGramGen::writeFSMSilWordLoopSil( FILE *fsmFD )
{
   if ( (vocab->sentStartIndex < 0) || (vocab->getNumPronuns(vocab->sentStartIndex) <= 0) )
      error("WFSTGramGen::writeFSMSilWordLoopSil - vocab->sentStartIndex < 0") ;
   if ( (vocab->sentEndIndex < 0) || (vocab->getNumPronuns(vocab->sentEndIndex) <= 0) )
      error("WFSTGramGen::writeFSMSilWordLoopSil - vocab->sentEndIndex < 0") ;
   if ( vocab->silIndex >= 0 )
      error("WFSTGramGen::writeFSMSilWordLoopSil - vocab->silIndex >= 0") ;

   // There are 4 states
   int i , initSt=0 , ws1=1 , ws2=2 , finalSt=3 ;

   // sentStart transition from initSt to ws1 with weight 0.0
   writeFSMTransition( fsmFD , initSt , ws1 , vocab->sentStartIndex+1 , 
                       vocab->sentStartIndex+1 , 0.0 ) ;
   
   // Transition from ws1 to ws2 for each (other) word in vocab.
   for ( i=0 ; i<vocab->nWords ; i++ )
   {
      if ( i == vocab->sentStartIndex )
         continue ;
      if ( i == vocab->sentEndIndex )
         continue ;
      if ( vocab->getNumPronuns(i) <= 0 )
         continue ;

      writeFSMTransition( fsmFD , ws1 , ws2 , i+1 , i+1 , -wordInsPen ) ;
   }

   // sentEnd transition from ws2 to finalSt with weight wordInsPen
   writeFSMTransition( fsmFD , ws2 , finalSt , vocab->sentEndIndex+1 , 
                       vocab->sentEndIndex+1 , -wordInsPen ) ;

   // Epsilon transition from ws2 to ws1 with weight 0.0
   writeFSMTransition( fsmFD , ws2 , ws1 , WFST_EPSILON , WFST_EPSILON , 0.0 ) ;

   // Write the final state entry
   writeFSMFinalState( fsmFD , finalSt ) ;
}


typedef enum
{
	ARPALM_BEFORE_DATA=0 ,
	ARPALM_IN_DATA ,
	ARPALM_IN_NGRAMS ,
	ARPALM_EXPECT_NGRAM_HDR ,
	ARPALM_EXPECT_END
} ARPALMReadState ;


void WFSTGramGen::writeFSMARPA( FILE *fsmFD , bool addSil , bool phiBOTrans )
{
   if ( phiBOTrans )
      phiLabel = vocab->nWords + 1 ;   // epsilon is label 0
   if ( addSil && (vocab->silIndex < 0) )
      warning("WFSTGramGen::writeFSMARPA"
              " - addSil true but vocab->silIndex < 0") ;

   // Read the ARPA file into memory.
   ARPALM *arpaLM = new ARPALM( arpaLMFName , vocab , unkWord ) ;
   arpaLM->Normalise();

   // Create the state manager we use to keep track the states we create
   WFSTNGramStateManager *stateMan = new WFSTNGramStateManager( vocab ) ;

   int n , i , fromSt , toSt , label ;
   int firstFromState=-1 ;
   bool haveFinalState=false ;
   real prob ;
   
   // If a sentence start word is defined AND we have a pronunciation for it
   //   then add a transition from the initState (which should already be correctly
   //   configured in the state manager) to the <s> state.
   if ( (vocab->sentStartIndex >= 0) && (vocab->getNumPronuns(vocab->sentStartIndex) > 0) )
   {
      firstFromState = fromSt = stateMan->getInitState() ;
      int wrd = vocab->sentStartIndex ;
      toSt = stateMan->getWFSTState( 1 , &wrd , false ) ;
      label = vocab->sentStartIndex + 1 ;
      writeFSMTransition( fsmFD , fromSt , toSt , label , label , 0.0 ) ;
   }
   
   // Add all 1...(N-1) entries (ie. backoff stuff)
   for ( n=0 ; n<(arpaLM->order-1) ; n++ )
   {
      // Add 1-grams, then 2-grams, then 3-grams, ....
      for ( i=0 ; i<arpaLM->n_ngrams[n] ; i++ )
      {
         // ** N-gram prob arc **
         // Retrieve to state from our state manager
         fromSt = -1 ;
         toSt = -1 ;

         if ( arpaLM->entries[n][i].log_prob > LOG_ZERO )
         {
            if ( arpaLM->entries[n][i].words[n] == vocab->sentEndIndex )
            {
               if ( vocab->getNumPronuns(vocab->sentEndIndex) > 0 )
               {
                  // Write a sent end transition to (unique) final state
                  fromSt = stateMan->getWFSTState( n , arpaLM->entries[n][i].words , true ) ;
                  int wrd = vocab->sentEndIndex ;
                  toSt = stateMan->getWFSTState( 1 , &wrd , false ) ;
                  label = vocab->sentEndIndex + 1 ;
                  prob = arpaLM->entries[n][i].log_prob * lmScale + wordInsPen ;
                  writeFSMTransition( fsmFD , fromSt , toSt , label , label , -prob ) ;
               }
               else
               {
                  // Write a final state entry
                  toSt = stateMan->getWFSTState( n , arpaLM->entries[n][i].words , false , 
                                                 NULL , true ) ;
                  prob = arpaLM->entries[n][i].log_prob * lmScale ;
                  writeFSMFinalState( fsmFD , toSt , -prob ) ;
               }
               haveFinalState = true ;
            }
            else
            {
               // Retrieve to state from our state manager
               fromSt = stateMan->getWFSTState( n , arpaLM->entries[n][i].words , true ) ;
               toSt = stateMan->getWFSTState( n+1 , arpaLM->entries[n][i].words , false ) ;

               // Adjust LM prob using lmScale and wordInsPen
               prob = arpaLM->entries[n][i].log_prob * lmScale + wordInsPen ;

               if ( arpaLM->entries[n][i].words[n] == arpaLM->unk_id )
               {
                  // We want to write an arc for each word in our vocab that is unknown to the LM
                  for ( int u=0 ; u<arpaLM->n_unk_words ; u++ )
                  {
                     label = arpaLM->unk_words[u] ;
                     writeFSMTransition( fsmFD , fromSt , toSt , label , label , -prob ) ;
                  }
               }
               else
               {
                  // Write arc to WFST file
                  label = arpaLM->entries[n][i].words[n] + 1 ; // epsilon is label 0
                  writeFSMTransition( fsmFD , fromSt , toSt , label , label , -prob ) ;
               }
            }
            if ( firstFromState < 0 )
               firstFromState = fromSt ;
         }

         // ** Back-off arc **
         // Do not back-off if last word in entry is sentEnd word.
         // Adjust BO weight using lmScale
         if ( (arpaLM->entries[n][i].log_bo > LOG_ZERO) && 
              (arpaLM->entries[n][i].words[n] != vocab->sentEndIndex) )
         {
            fromSt = stateMan->getWFSTState( n+1 , arpaLM->entries[n][i].words , true ) ;
            toSt = stateMan->getWFSTState( n , (arpaLM->entries[n][i].words)+1 , false ) ;
            prob = arpaLM->entries[n][i].log_bo * lmScale ;

			// phi backoff transitions apply only to the input labels
            if ( phiBOTrans )
               label = phiLabel ;
            else
               label = WFST_EPSILON ;
               
            // Write arc to WFST file
            writeFSMTransition( fsmFD , fromSt , toSt , label , WFST_EPSILON , -prob ) ;

            if ( firstFromState < 0 )
               firstFromState = fromSt ;
         }
      }
   }

   // Add the N-gram entries
   bool isNew ;
   n = arpaLM->order - 1 ;
   for ( i=0 ; i<arpaLM->n_ngrams[n] ; i++ )
   {
      fromSt = -1 ;
      toSt = -1 ;
      if ( arpaLM->entries[n][i].log_prob > LOG_ZERO )
      {
         // ** N-gram prob arc **
         if ( arpaLM->entries[n][i].words[n] == vocab->sentEndIndex )
         {
            if ( vocab->getNumPronuns(vocab->sentEndIndex) > 0 )
            {
               // Write a sent end transition to (unique) final state
               fromSt = stateMan->getWFSTState( n , arpaLM->entries[n][i].words , true ) ;
               int wrd = vocab->sentEndIndex ;
               toSt = stateMan->getWFSTState( 1 , &wrd , false ) ;
               label = vocab->sentEndIndex + 1 ;
               prob = arpaLM->entries[n][i].log_prob * lmScale + wordInsPen ;
               writeFSMTransition( fsmFD , fromSt , toSt , label , label , -prob ) ;
            }
            else
            {
               // Write a final state entry
               toSt = stateMan->getWFSTState( n , arpaLM->entries[n][i].words , false ,
                                              NULL , true ) ;
               prob = arpaLM->entries[n][i].log_prob * lmScale ;
               writeFSMFinalState( fsmFD , toSt , -prob ) ;
            }
            haveFinalState = true ;
         }
         else
         {
            // Retrieve from and to states from our state manager
            fromSt = stateMan->getWFSTState( n , arpaLM->entries[n][i].words , true , &isNew ) ;
            if ( isNew )
            {
               // The 'from' state is new - therefore there is no pre-existing back-off path.
               // We need to add a default back-off path.
               addDefaultBackoffPath( fsmFD, stateMan, fromSt , n-1 , 
                                      (arpaLM->entries[n][i].words)+1 ) ;
            }
            
            toSt = stateMan->getWFSTState( n , (arpaLM->entries[n][i].words)+1 , false , &isNew ) ;
            if ( isNew )
            {
               // The 'to' state is new - therefore there is no pre-existing back-off path.
               // We need to add a default back-off path.
               addDefaultBackoffPath( fsmFD , stateMan , toSt , 
                                      n-1 , (arpaLM->entries[n][i].words)+2 ) ;
            }

            // Adjust LM prob using lmScale and wordInsPen
            prob = arpaLM->entries[n][i].log_prob * lmScale + wordInsPen ;

            if ( arpaLM->entries[n][i].words[n] == arpaLM->unk_id )
            {
               // We want to write an arc for each word in our vocab that is unknown to the LM
               for ( int u=0 ; u<arpaLM->n_unk_words ; u++ )
               {
                  label = arpaLM->unk_words[u] + 1 ;  // epsilon is label 0
                  writeFSMTransition( fsmFD , fromSt , toSt , label , label , -prob ) ;
               }
            }
            else
            {
               // Write arc to WFST file
               label = arpaLM->entries[n][i].words[n] + 1 ; // epsilon is label 0
               writeFSMTransition( fsmFD , fromSt , toSt , label , label , -prob ) ;
            }
         }

         if ( firstFromState < 0 )
            firstFromState = fromSt ;
      }
   }

   if ( addSil )
   {
      warning("WFSTGramGen::writeFSMARPA"
              " - are you sure you want to addSil arcs to all states?");
      // Write silence transitions (ie. self-loop on each state)
      // Include epsilon state.
#if 0
      // Nice way to add just silence
      label = vocab->silIndex + 1 ; // epsilon is label 0
      for ( i=0 ; i<stateMan->getNumStates() ; i++ )
      {
         writeFSMTransition( fsmFD , i , i , label , label , 0.0 ) ;
      }
#else
      // Hack to add both SIL and SP
      assert(phiLabel > 0);
      for ( i=0 ; i<stateMan->getNumStates() ; i++ )
      {
          writeFSMTransition(fsmFD, i , i , phiLabel+1 , WFST_EPSILON , 0.0 );
          writeFSMTransition(fsmFD, i , i , phiLabel+2 , WFST_EPSILON , 0.0 );
      }
#endif
   }

   // Get init state from state manager and check that it corresponds with the
   //   from state of the first arc written to the FSM file.
   int init=-1 , eps=-1 ;
   init = stateMan->getInitState() ;
   if ( init != firstFromState )
   {
      warning("Initial state is not the 'from' state on 1st line of output fsm.\n"
              "Please move a line with 'from' state field = %d to beginning of file",init) ;
              //"An easy way to prevent this is to make start word (%s) unigram the "
              //"first unigram entry in the ARPA LM file" , 
              //init , vocab->words[vocab->sentStartIndex] ) ;
   }
   eps = stateMan->getEpsState() ;

   if ( haveFinalState == false )
   {   
      // No final states have been written to file yet.
      // This could happen in 1 of two ways:
      //   a) not having a sentEndWord defined in vocab
      //   b) having sentEndWord in vocab but not finding any
      //      P( </s> | ... ) entries in the N-gram LM.
      //    
      // In this case, all states (except initial and epsilon states are final states)
      for ( int i=0 ; i<stateMan->getNumStates() ; i++ )
      {
         if ( (i == eps) || (i == init) )
            continue ;

         writeFSMFinalState( fsmFD , i ) ;
      }
   }
   else
   {
      if ( (vocab->sentEndIndex >= 0) && (vocab->getNumPronuns(vocab->sentEndIndex) > 0) )
      {
         // We have a sentEndWord with a pronun, in which case there is a unique
         //    final state which has not yet been output.
         int wrd = vocab->sentEndIndex ;
         toSt = stateMan->getWFSTState( 1 , &wrd , false , NULL , true ) ;
         writeFSMFinalState( fsmFD , toSt , 0.0 ) ;
      }
   }
//stateMan->outputText();
//stateMan->outputNonAccessible();

   delete arpaLM ;
   delete stateMan ;
}


void WFSTGramGen::addDefaultBackoffPath(
	FILE *fsmFD , WFSTNGramStateManager *stateMan , 
	int fromSt , int toNWords , int *toWords
	)
{  
   // Helper method called from writeFSMARPA.

   // Say 'fromSt' is the state corresponding to w1,w2,w3
   // 'toNWords' should be 2 in this case and 'toWords' should contain w2,w3
   // We want to add a transition from 'fromSt' to state w2,w3 with weight 0.0
   // We also need to check to see if the w2,w3 state is new - if it is
   //   then we need to recursively add a default backoff path for it.

   bool isNew ;
   int toSt = stateMan->getWFSTState( toNWords , toWords , false , &isNew ) ;
   
   writeFSMTransition( fsmFD , fromSt , toSt , WFST_EPSILON , WFST_EPSILON , 0.0 ) ;
   
   if ( isNew && (toNWords > 1) )
   {
      addDefaultBackoffPath( fsmFD , stateMan , toSt , toNWords-1 , toWords+1 ) ;
   }
}


void WFSTGramGen::writeFSMWordPair( FILE *fsmFD )
{
   WordPairLM *wplm = new WordPairLM( wordPairFName , vocab ) ;
   //wplm->outputText() ;
   
   // Create the state manager we use to keep track the states we create
   WFSTNGramStateManager *stateMan = new WFSTNGramStateManager( vocab ) ;

   int i , j , fromSt , toSt , label ;
   int firstFromState=-1 ;
   bool haveFinalState=false ;
   real prob ;
   
   // If a sentence start word is defined AND we have a pronunciation for it
   //   then add a transition from the initState (which should already be correctly
   //   configured in the state manager) to the <s> state.
   if ( (vocab->sentStartIndex >= 0) && (vocab->getNumPronuns(vocab->sentStartIndex) > 0) )
   {
      firstFromState = fromSt = stateMan->getInitState() ;
      int wrd = vocab->sentStartIndex ;
      toSt = stateMan->getWFSTState( 1 , &wrd , false ) ;
      label = vocab->sentStartIndex + 1 ;
      writeFSMTransition( fsmFD , fromSt , toSt , label , label , 0.0 ) ;
   }

   // Write the word pair entries for each word in the vocab
   int nSucs ;
   int *sucs ;
   real logProb ;
   for ( i=0 ; i<vocab->nWords ; i++ )
   {
      nSucs = wplm->getNumSucWords( i ) ;
      if ( nSucs <= 0 )
         continue ;
      sucs = wplm->getSucWords( i ) ;
      logProb = log( 1.0 / (real)nSucs ) ;
     
      for ( j=0 ; j<nSucs ; j++ )
      {
         fromSt = -1 ;
         toSt = -1 ;

         if ( sucs[j] == vocab->sentEndIndex )
         {
            if ( vocab->getNumPronuns(vocab->sentEndIndex) > 0 )
            {
               // Write a sent end transition to (unique) final state
               fromSt = stateMan->getWFSTState( 1 , &i , true ) ;
               int wrd = vocab->sentEndIndex ;
               toSt = stateMan->getWFSTState( 1 , &wrd , false ) ;
               label = vocab->sentEndIndex + 1 ;
               prob = logProb * lmScale + wordInsPen ;
               writeFSMTransition( fsmFD , fromSt , toSt , label , label , -prob ) ;
            }
            else
            {
               // Write a final state entry
               toSt = stateMan->getWFSTState( 1 , &i , false , NULL , true ) ;
               prob = logProb * lmScale ;
               writeFSMFinalState( fsmFD , toSt , -prob ) ;
            }
            haveFinalState = true ;
         }
         else
         {
            // Retrieve from and to states from our state manager
            fromSt = stateMan->getWFSTState( 1 , &i , true ) ;
            toSt = stateMan->getWFSTState( 1 , sucs+j , false ) ;

            // Adjust LM prob using lmScale and wordInsPen
            prob = logProb * lmScale + wordInsPen ;

            // Write arc to WFST file
            label = sucs[j] + 1 ; // epsilon is label 0
            writeFSMTransition( fsmFD , fromSt , toSt , label , label , -prob ) ;
         }

         if ( firstFromState < 0 )
            firstFromState = fromSt ;
      }
   }

   // Get init state from state manager and check that it corresponds with the
   //   from state of the first arc written to the FSM file.
   int init=-1 , eps=-1 ;
   init = stateMan->getInitState() ;
   if ( init != firstFromState )
   {
      warning("Initial state is not the 'from' state on 1st line of output fsm.\n"
              "Please move a line with 'from' state field = %d to beginning of file",init) ;
   }
   eps = stateMan->getEpsState() ;

   if ( haveFinalState == false )
   {
      error("WFSTGramGen::writeFSMWordPair - no entries with </s> as suc encountered in file") ;
   }
   else
   {
      if ( (vocab->sentEndIndex >= 0) && (vocab->getNumPronuns(vocab->sentEndIndex) > 0) )
      {
         // We have a sentEndWord with a pronun, in which case there is a unique
         //    final state which has not yet been output.
         int wrd = vocab->sentEndIndex ;
         toSt = stateMan->getWFSTState( 1 , &wrd , false , NULL , true ) ;
         writeFSMFinalState( fsmFD , toSt , 0.0 ) ;
      }
   }
   //stateMan->outputText();
   
   delete wplm ;
   delete stateMan ;
}

}
