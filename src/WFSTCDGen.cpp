/*
 * Copyright 2004 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#include "WFSTCDGen.h"
#include "WFSTGeneral.h"
#include "WFSTNetwork.h"

/*
	Author:	Darren Moore (moore@idiap.ch)
	Date:		18 November 2004
	$Id: WFSTCDGen.cpp,v 1.7 2005/10/20 02:09:39 moore Exp $
*/

// Define to handle auxiliary symbols with self loops
#define AUXLOOP

using namespace Torch;

namespace Juicer {


//---------------- WFSTCDStateManager implementation ----------------


WFSTCDStateManager::WFSTCDStateManager(
    MonophoneLookup *monoLookup_ , int nAuxSyms_
)
{
   monoLookup = monoLookup_ ;
   nAuxSyms = nAuxSyms_;
   nMonoPlusAux = monoLookup->getNumMonophones() + nAuxSyms;

   firstLevelNodes = new int[nMonoPlusAux + 1] ; // 1 extra for <eps>
   for ( int i=0 ; i<(nMonoPlusAux+1) ; i++ )
   {
      firstLevelNodes[i] = -1 ;
   }
   
   nNodes = 0 ;
   nNodesAlloc = 0 ;
   nodes = NULL ;

   nStates = 1 ;
   epsilonState = 0 ;
}


WFSTCDStateManager::~WFSTCDStateManager()
{
   delete [] firstLevelNodes ;

   if ( nodes != NULL )
   {
      free( nodes ) ;
   }
}


int WFSTCDStateManager::getWFSTState( int nMonophones , int *monophones , 
                                      bool addNew , bool *isNew )
{
   if ( isNew != NULL )
      *isNew = false ;

   if ( nMonophones == 0 )
   {
      // we want the epsilon state
      return epsilonState ;
   }

   int node = -1 , mono=-1 ;
   for ( int i=0 ; i<nMonophones ; i++ )
   {
      if ( monophones[i] < 0 )
         mono = 0 ;  // ie. <eps>
      else if ( monophones[i] < nMonoPlusAux )
         mono = monophones[i] + 1 ;
      else
         error("WFSTCDStateManager::getWFSTState - invalid monophone index %d", monophones[i]) ;
         
      node = getNode( node , mono , addNew ) ;
      if ( node < 0 )
         return -1 ;
   }

   if ( nodes[node].state < 0 )
   {
      if ( addNew )
      {
         nodes[node].state = nStates++ ;
         if ( isNew != NULL )
            *isNew = true ;
      }
      else
         error("WFSTCDStateManager::getWFSTState - addNew = false but nodes[node].state invalid");
   }

   return nodes[node].state ;
}


int WFSTCDStateManager::getNode( int parentNode , int monophone , bool addNew )
{
   int node=-1 ;
   
   if ( parentNode < 0 )
   {
      // look in firstLevelNodes
      if ( firstLevelNodes[monophone] < 0 )
      {
         if ( addNew )
            firstLevelNodes[monophone] = allocNode( monophone ) ;
         else
            return -1 ;
      }  
      
      node = firstLevelNodes[monophone] ; 
   }
   else
   {
      // search through the children of 'parentNode' looking for a node
      //   with monophone field equal to 'monophone'.
      node = nodes[parentNode].firstChild ;
      while ( node >= 0 )
      {
         if ( nodes[node].monophone == monophone )
            break ;         
         node = nodes[node].nextSib ;
      }

      if ( node < 0 )
      {
         if ( addNew )
         {
            node = allocNode( monophone ) ;
            nodes[node].nextSib = nodes[parentNode].firstChild ;
            nodes[parentNode].firstChild = node ;
         }
         else
            return -1 ;
      }
   }

   return node ;
}
   

int WFSTCDStateManager::allocNode( int monophone )
{
   if ( nNodes == nNodesAlloc )
   {
      nNodesAlloc += 10000 ;
      nodes = (WFSTCDSMNode *)realloc( nodes , nNodesAlloc*sizeof(WFSTCDSMNode) ) ;
      for ( int i=nNodes ; i<nNodesAlloc ; i++ )
         initNode( i ) ;
   }

   nodes[nNodes++].monophone = monophone ;
   return ( nNodes - 1 ) ;
}


void WFSTCDStateManager::initNode( int ind )
{
   if ( (ind < 0) || (ind >= nNodesAlloc) )
      error("WFSTCDStateManager::initNode - ind out of range") ;
   
   nodes[ind].monophone = -1 ;
   nodes[ind].state = -1 ;
   nodes[ind].nextSib = -1 ;
   nodes[ind].firstChild = -1 ;
}


void WFSTCDStateManager::outputText()
{
   int *monophones = new int[30] ;
   int nMonophones=0 ;
   
   for ( int i=0 ; i<(nMonoPlusAux+1) ; i++ )
   {
      nMonophones = 0 ;
      outputNode( firstLevelNodes[i] , &nMonophones , monophones ) ;
   }

   delete [] monophones ;
}


void WFSTCDStateManager::outputNode(
    int node , int *nMonophones , int *monophones
)
{
   if ( node < 0 )
      return ;

   monophones[(*nMonophones)++] = nodes[node].monophone ;
 
   if ( nodes[node].state >= 0 )
   {
      // we want to output this node.
      printf("[") ;
      for ( int i=0 ; i<*nMonophones ; i++ )
      {
         if ( monophones[i] == 0 )
            printf(" <eps>") ;
         else if ( (monophones[i] > 0) && (monophones[i] <= monoLookup->getNumMonophones()) )
            printf(" %s" , monoLookup->getString( monophones[i] - 1 ) ) ;
         else if ( (monophones[i] > monoLookup->getNumMonophones()) && 
                   (monophones[i] <= nMonoPlusAux) )
             // Phil - This is wrong really - aux is not necessarily numeric
             // but it doesn't matter so much here.
            printf(" #%d" , monophones[i] - monoLookup->getNumMonophones() - 1 );
         else
            error("WFSTCDStateManager::outputNode - monophones[i]=%d out of range",monophones[i]);
      }
      printf(" ] = %d\n" , nodes[node].state ) ;            
   }

   int next = nodes[node].firstChild ;
   while ( next >= 0 )
   {
      outputNode( next , nMonophones , monophones ) ;
      next = nodes[next].nextSib ;
   }

   (*nMonophones)-- ;
}


//-------------------- WFSTCDGen implementation ----------------------


WFSTCDGen::WFSTCDGen( WFSTCDType type_ , const char *htkModelsFName , 
                      const char *monophoneListFName , const char *silMonoStr , 
                      const char *pauseMonoStr , const char *tiedListFName , const char *sepChars ,
                      const char *priorsFName , int statesPerModel )
{
   type = type_ ;
   if ( (type < 0) || (type >= WFST_CD_TYPE_INVALID) )
      error("WFSTCDGen::WFSTCDGen - invalid type") ;
   if ( (monophoneListFName == NULL) || (monophoneListFName[0] == '\0') )
      error("WFSTCDGen::WFSTCDGen - monophoneListFName undefined") ;

   monoLookup = NULL ;
   phoneLookup = NULL ;
   models = NULL ;

   nStates = 0 ;
   nAuxSyms = 0 ;
   inAuxSymsBase = 0 ;
   outAuxSymsBase = 0 ;

   // Create the phone lookup structures.
   monoLookup = new MonophoneLookup( monophoneListFName , silMonoStr , pauseMonoStr ) ;

   if ( type == WFST_CD_TYPE_MONOPHONE_ANN )
   {
      if ( (priorsFName == NULL) || (priorsFName[0] == '\0') )
         error("WFSTCDGen::WFSTCDGen - type = WFST_CD_TYPE_MONOPHONE_ANN but no priorsFName") ;
      if ( statesPerModel <= 0 )
         error("WFSTCDGen::WFSTCDGen - type = WFST_CD_TYPE_MONOPHONE_ANN but statesPerModel <= 0") ;

      phoneLookup = new PhoneLookup( monoLookup , monophoneListFName , "" ) ;

      // Load the models.
      models = new HTKModels();
      models->Load( monophoneListFName , priorsFName , statesPerModel ) ;

      // Add the model indices to the PhoneLookup
      HMM *hmm ;
      int i ;   
      for ( i=0 ; i<models->getNumHMMs() ; i++ )
      {
         hmm = models->getHMM( i ) ;
         phoneLookup->addModelInd( hmm->name , i ) ;
      }
      phoneLookup->verifyAllModels() ;
   }
   else
   {
      if ( (htkModelsFName == NULL) || (htkModelsFName[0] == '\0') )
         error("WFSTCDGen::WFSTCDGen - htkModelsFName undefined") ;
      if ( (tiedListFName == NULL) || (tiedListFName[0] == '\0') )
         error("WFSTCDGen::WFSTCDGen - tiedListFName undefined") ;
      if ( (type_ != WFST_CD_TYPE_MONOPHONE) &&
           ((sepChars == NULL) || (sepChars[0] == '\0')) )
      {
         error("WFSTCDGen::WFSTCDGen - sepChars not defined") ;
      }

      // Create the phone lookup structures.
      phoneLookup = new PhoneLookup( monoLookup , tiedListFName , sepChars ) ;

      // Load the models.
      models = new HTKModels();
      models->Load( htkModelsFName ) ;

      //models->outputText("modelout.txt") ;

      // Add the model indices to the PhoneLookup
      HMM *hmm ;
      int i ;
      for ( i=0 ; i<models->getNumHMMs() ; i++ )
      {
         hmm = models->getHMM( i ) ;
         phoneLookup->addModelInd( hmm->name , i ) ;
      }
      phoneLookup->verifyAllModels() ;
   }
}


WFSTCDGen::~WFSTCDGen()
{
   delete monoLookup ;
   delete phoneLookup ;
   delete models ;
}


void WFSTCDGen::writeFSM(
    const char *fsmFName , const char *inSymbolsFName ,
    const char *outSymbolsFName , const char *lexInSymbolsFName
)
{
   FILE *fd ;
   WFSTAlphabet *lexInSyms=NULL;

   // Load the lexicon input symbols file & determine the auxiliary symbol info
   if ( (lexInSymbolsFName == NULL) || (lexInSymbolsFName[0] == '\0') ) {
      // Assume no auxiliary symbols
      nAuxSyms = 0;
      inAuxSymsBase = -1;
      outAuxSymsBase = -1;
   } else {   
      lexInSyms = new WFSTAlphabet( lexInSymbolsFName );
      nAuxSyms = lexInSyms->getNumAuxSyms();
      inAuxSymsBase = models->getNumHMMs();
      outAuxSymsBase = monoLookup->getNumMonophones();
   }

   // Open the FSM file.
   if ( (fd = fopen( fsmFName , "wb" )) == NULL )
      error("WFSTCDGen::writeFSM - error opening FSM output file: %s",fsmFName) ;

   if ( (type == WFST_CD_TYPE_MONOPHONE) || (type == WFST_CD_TYPE_MONOPHONE_ANN) ) {

      writeFSMMonophone( fd ) ;
      
   } else if ( type == WFST_CD_TYPE_XWORD_TRIPHONE ) {
#if 1
       // New
       writeFSMXWordTriphoneDetInv( fd, phoneLookup->haveCIPause() ) ;
#else
       // Old
       writeFSMXWordTriphoneOld(
           fd, phoneLookup->haveCISilence(), phoneLookup->haveCIPause()
       ) ;
#endif
   } else if ( type == WFST_CD_TYPE_XWORD_TRIPHONE_NDI ) {
   
      writeFSMXWordTriphoneNonDetInv( fd, phoneLookup->haveCISilence(), 
                                      phoneLookup->haveCIPause() ) ;
   }
   else
      error("WFSTCDGen::writeFSM - invalid type") ;

#ifdef AUXLOOP
   writeFSMAuxTrans(fd);
#endif

   // Close the FSM file.
   fclose( fd ) ;

   // Open the input symbols file.
   if ( (fd = fopen( inSymbolsFName , "wb" )) == NULL )
      error("WFSTCDGen::writeFSM - error opening in syms output file: %s",inSymbolsFName) ;

   // Write all input symbols (ie. models) + auxiliary symbols
   int i ;
   HMM *hmm ;
   writeFSMSymbol( fd , WFST_EPSILON_STR , 0 ) ;
   for ( i=0 ; i<models->getNumHMMs() ; i++ )
   {
      hmm = models->getHMM( i ) ;
      writeFSMSymbol( fd , hmm->name , i+1 ) ;
   }

#if 0
   // PNG - This is a bug - aux symbols are not necessarily numeric
   char str[10] ;
   for ( i=0 ; i<nAuxSyms ; i++ )
   {
      sprintf( str , "#%d" , i ) ;
      writeFSMSymbol( fd , str , inAuxSymsBase+i+1 ) ;
   }
#else
   if (nAuxSyms)
   {
       int inAux = inAuxSymsBase;
       for (i=0; i<=lexInSyms->getMaxLabel(); i++)
       {
           if (lexInSyms->isAuxiliary(i))
               writeFSMSymbol( fd , lexInSyms->getLabel(i) , ++inAux ) ;
       }
   }
#endif

   // Close the input symbols file.
   fclose( fd ) ;

   // Open the output symbols file.
   if ( (fd = fopen( outSymbolsFName , "wb" )) == NULL )
      error("WFSTCDGen::writeFSM - error opening out syms output file: %s",outSymbolsFName) ;

   // Write all output symbols (ie. monophones) + auxiliary symbols
   writeFSMSymbol( fd , WFST_EPSILON_STR , 0 ) ;
   for ( i=0 ; i<monoLookup->getNumMonophones() ; i++ )
   {
      writeFSMSymbol( fd , monoLookup->getString(i) , i+1 ) ;
   }
#if 0
   // PNG - as above - aux symbols are not necessarily numeric
   for ( i=0 ; i<nAuxSyms ; i++ )
   {
      sprintf( str , "#%d" , i ) ;
      writeFSMSymbol( fd , str , outAuxSymsBase+i+1 ) ;
   }
#else
   if (nAuxSyms)
   {
       int outAux = outAuxSymsBase;
       for (i=0; i<=lexInSyms->getMaxLabel(); i++)
       {
           if (lexInSyms->isAuxiliary(i))
               writeFSMSymbol( fd , lexInSyms->getLabel(i) , ++outAux ) ;
       }
   }
#endif

   // Close the output symbols file.
   fclose( fd );
   delete lexInSyms;
}


void WFSTCDGen::writeFSMMonophone( FILE *fsmFD )
{
   // A single state with a self loop for each (output) monophone
   // index (from monoLookup) with its corresponding (input) monophone
   // model index (from phoneLookup)

   int lab ;
   for ( int i=0 ; i<monoLookup->getNumMonophones() ; i++ )
   {
      // Current output label is 'i'.
      // Determine input label.
      if ( (lab = phoneLookup->getModelInd( monoLookup->getString(i) )) < 0 )
         error("WFSTCDGen::writeFSMMonophone - invalid modelInd %d", i) ;

      // Write transition
      writeFSMTransition( fsmFD , 0 , 0 , lab+1 , i+1 ) ;   // 0 is for epsilon
   }

   // Write final state
   writeFSMFinalState( fsmFD , 0 ) ;
   nStates = 1 ;
  
   // Write self-loop arcs on each state for each auxiliary symbol
   int i, j;
   for ( i=0 ; i<nAuxSyms ; i++ ) {
      for ( j=0 ; j<nStates ; j++ ) {
         writeFSMTransition( fsmFD , j , j , inAuxSymsBase+i+1 , outAuxSymsBase+i+1 ) ;
      }
   }
}

/**
 * This is the old way of creating the xwrd triphone CD transducer.
 * It has a deterministic inverse EXCEPT the auxiliary symbols were
 * just added as self-loop arcs to each state which is WRONG.
 *
 * NOTE: The transducer output from this function has states that do
 * not lie on a path from initial state to a final state.
 */
void WFSTCDGen::writeFSMXWordTriphoneOld(
    FILE *fsmFD , bool ciSil , bool ciPause
)
{
   // Create the state manager
   WFSTCDStateManager *stateMan =
       new WFSTCDStateManager( monoLookup , nAuxSyms );

   int silMono = monoLookup->getSilMonophone() ;
   int silModel = phoneLookup->getCISilenceModelInd() ;
   if ( ciSil && ((silMono < 0) || (silModel < 0)) )
      error("WFSTCDGen::writeFSMXWordTriphone - ciSil && ((silMono < 0)||(silModel < 0))") ;
   int pauseMono = monoLookup->getPauseMonophone() ;
   int pauseModel = phoneLookup->getCIPauseModelInd() ;
   if ( ciPause && ((pauseMono < 0) || (pauseModel < 0)) )
      error("WFSTCDGen::writeFSMXWordTriphone - ciPause && ((pauseMono < 0)||(pauseModel < 0))") ;

   int nPhones = phoneLookup->getNumPhones() ;
   int maxCD = phoneLookup->getMaxCD() ;
   if ( maxCD != 3 )
      error("WFSTCDGen::writeFSMXWordTriphone - maxCD != 3") ;
   
   // Allocate memory
   int **monoInds = new int*[nPhones] , i , j ;
   monoInds[0] = new int[nPhones*maxCD] ;
   for ( i=1 ; i<nPhones ; i++ )
      monoInds[i] = monoInds[0] + (i * maxCD) ;
   int *modelInds = new int[nPhones] ;
   
   // Retrieve a properly formatted list of all CD phones and their
   // corresponding model index.
   phoneLookup->getAllModelInfo( monoInds , modelInds ) ;

   // 1. Write <eps>/ph transitions from (<eps>,<eps>) state to (<eps>,ph) state
   //      for all ph.
   //    Also handle case where pause is first monophone.
   int stPhns[3] , fromSt=-1 , toSt=-1 , inLab=-1 , outLab=-1 ;
   bool isNewState=false ;
   for ( i=0 ; i<monoLookup->getNumMonophones() ; i++ )
   {
      if ( ciPause && (i == pauseMono) )
         continue ;

      fromSt = stateMan->getEpsState() ;
      stPhns[0] = -1 ; stPhns[1] = i ;
      toSt = stateMan->getWFSTState( 2 , stPhns ) ;
      inLab = WFST_EPSILON ;
      outLab = i + 1 ;  // epsilon is index 0
      writeFSMTransition( fsmFD , fromSt , toSt , inLab , outLab ) ;

      if ( ciPause && (i != silMono) )
      {
         // sp/i arc from (<eps>,sp) to (<eps>,i)
         stPhns[0] = -1 ; stPhns[1] = pauseMono ;
         fromSt = stateMan->getWFSTState( 2 , stPhns , true , &isNewState ) ;
         // toSt is same as above
         inLab = pauseModel + 1 ;
         outLab = i + 1 ;
         writeFSMTransition( fsmFD , fromSt , toSt , inLab , outLab ) ;
         
         if ( isNewState )
         {
            // <eps>/sp arc from (<eps>,<eps>) to (<eps>,sp)
            toSt = fromSt ;
            fromSt = stateMan->getEpsState() ;
            inLab = WFST_EPSILON ;
            outLab = pauseMono + 1 ;
            writeFSMTransition( fsmFD , fromSt , toSt , inLab , outLab ) ;
         }
      }
   }

   // Process each of the phones that we have a (possibly shared) model for.
   // NORMAL CASE: For each ph1-ph2+ph3 write a transition from state (ph1,ph2) to state 
   //              (ph2,ph3) with input label the model index of ph1-ph2+ph3 and output 
   //              label ph3. 
   //              Note: This should work OK for the ph2+ph3 case (ph1=<eps>).
   //              Note: This should work OK for the ph1-ph2 case (ph3=<eps>).
   //              Note: This should work OK for the ph2 case (ph1=<eps>,ph3=<eps>)
   // 
   // EXCEPTION 1: For each ph1-ph2+sil write a transition from state (ph1,ph2) to state
   //              (<eps>,sil) with input label the model index of ph1-ph2+sil and output 
   //              label sil monophone index.
   //              Only if ciSil is true of course - otherwise treat as normal.
   // EXCEPTION 2: For (CI) sil write a transition from state (<eps>,sil) to state (sil,ph)
   //              with input label the model index of sil and output label monophone index ph
   //              FOR ALL POSSIBLE VALUES OF ph.
   //              2 additional arcs:
   //              1: (<eps>,sil) to (<eps>,sil) inlab=sil model ind, outlabel=sil mono ind
   //              2: (<eps>,sil) to (sil,<eps>) in label sil model ind, output label <eps>.
   //              THE (sil,<eps>) STATE IS A FINAL STATE.
   //
   // FINAL STATES: (ph,<eps>) for all values of ph incl. sil are final states.

   // ** PAUSE HANDLING ** (i.e. if ciPause is true)
   // NORMAL CASE: For each ph1-ph2+ph3, 3 arcs required (N.B. no duplicates of 1&3)
   //              1: (ph1,ph2) to (ph1,ph2,sp), inlab=<eps> outlab=sp mono
   //              2: (ph1,ph2,sp) to (ph2,sp,ph3), inlab=ph1-ph2+ph3 modelind outlab=ph3 mono
   //              3: (ph2,sp,ph3) to (ph2,ph3), inlab=sp modelind outlab=<eps>
   //
   // EXCEPTION 1: No pause stuff for this case (as we disallow sil following sp)
   // EXCEPTION 2: No pause stuff for this case (as we disallow sp following sil)
   
  
   for ( i=0 ; i<nPhones ; i++ )
   {
      if ( monoInds[i][1] < 0 )
         error("WFSTCDGen::writeFSMXWordTriphone - monoInds[i][1] is < 0") ;
      if ( ciPause && ((monoInds[i][0] == pauseMono) || (monoInds[i][2] == pauseMono)) )
         error("WFSTCDGen::writeFSMXWordTriphone - monoInds[i][0] or [2] is pauseMono") ;
      
      if ( ciSil && (monoInds[i][2] == silMono) )
      {
         // Exception 1 case.
         if ( monoInds[i][1] == silMono )
            error("WFSTCDGen::writeFSMXWordTriphone - invalid ph-sil+sil detected") ;

         fromSt = stateMan->getWFSTState( 2 , monoInds[i] ) ;
         stPhns[0] = -1 ; stPhns[1] = silMono ;
         toSt = stateMan->getWFSTState( 2 , stPhns ) ;
         inLab = modelInds[i] + 1 ;
         outLab = silMono + 1 ;
         writeFSMTransition( fsmFD , fromSt , toSt , inLab , outLab ) ;
      }
      else if ( ciSil && (monoInds[i][1] == silMono) )
      {
         // Exception 2 case.
         if ( (monoInds[i][0] >= 0) || (monoInds[i][2] >= 0) )
            error("WFSTCDGen::writeFSMXWordTriphone - invalid ph1-sil+ph3 detected") ;
         if ( modelInds[i] != silModel )
            error("WFSTCDGen::writeFSMXWordTriphone - unexpected CI sil model index") ;
            
         stPhns[0] = -1 ; stPhns[1] = silMono ;
         fromSt = stateMan->getWFSTState( 2 , stPhns ) ;
         inLab = modelInds[i] + 1 ;
         for ( j=0 ; j<monoLookup->getNumMonophones() ; j++ )
         {
            if ( j == silMono )
            {
               // self-loop
               toSt = fromSt ;
            }
            else if ( ciPause && (j == pauseMono) )
            {
               // do not allow silence followed by pause
               continue ;
            }
            else
            {
               stPhns[0] = silMono ; stPhns[1] = j ;
               toSt = stateMan->getWFSTState( 2 , stPhns ) ;
            }
            outLab = j + 1 ;
            writeFSMTransition( fsmFD , fromSt , toSt , inLab , outLab ) ;
         }

         // arc to silence final state
         stPhns[0] = silMono ; stPhns[1] = -1 ;
         toSt = stateMan->getWFSTState( 2 , stPhns ) ;
         outLab = WFST_EPSILON ;
         writeFSMTransition( fsmFD , fromSt , toSt , inLab , outLab ) ;
      }
      else
      {
         // Normal case.
         fromSt = stateMan->getWFSTState( 2 , monoInds[i] ) ;
         toSt = stateMan->getWFSTState( 2 , monoInds[i]+1 ) ;
         inLab = modelInds[i] + 1 ;
         outLab = monoInds[i][2] + 1 ;
         writeFSMTransition( fsmFD , fromSt , toSt , inLab , outLab ) ;

         if ( ciPause )
         {
            stPhns[0] = monoInds[i][0] ; stPhns[1] = monoInds[i][1] ; stPhns[2] = pauseMono ;
            toSt = stateMan->getWFSTState( 3 , stPhns , true , &isNewState ) ;
            if ( isNewState )
            {
               // 1: (ph1,ph2) to (ph1,ph2,sp), inlab=<eps> outlab=sp
               inLab = WFST_EPSILON ;
               outLab = pauseMono + 1 ;
               writeFSMTransition( fsmFD , fromSt , toSt , inLab , outLab ) ;
            }
            // 2: (ph1,ph2,sp) to (ph2,sp,ph3), inlab=ph1-ph2+ph3 modelind outlab=ph3 mono
            fromSt = toSt ;
            stPhns[0] = monoInds[i][1] ; stPhns[1] = pauseMono ; stPhns[2] = monoInds[i][2] ;
            toSt = stateMan->getWFSTState( 3 , stPhns , true , &isNewState ) ;
            inLab = modelInds[i] + 1 ;
            outLab = monoInds[i][2] + 1 ;
            writeFSMTransition( fsmFD , fromSt , toSt , inLab , outLab ) ;
            if ( isNewState )
            {
               // 3: (ph2,sp,ph3) to (ph2,ph3), inlab=sp modelind outlab=<eps>
               fromSt = toSt ;
               toSt = stateMan->getWFSTState( 2 , monoInds[i]+1 ) ;
               inLab = pauseModel + 1 ;
               outLab = WFST_EPSILON ;
               writeFSMTransition( fsmFD , fromSt , toSt , inLab , outLab ) ;
            }
         }
      }
   }

   nStates = stateMan->getNumStates() ;
   
   // Write final states.
   // EACH (ph,<eps>) STATE IS A FINAL STATE (incl. (sil,<eps>))
   stPhns[1] = -1 ;
   for ( i=0 ; i<monoLookup->getNumMonophones() ; i++ )
   {
      if ( ciPause && (i == pauseMono) )
         continue ;

      stPhns[0] = i ;
      fromSt = stateMan->getWFSTState( 2 , stPhns , false ) ;
      if ( fromSt >= 0 )
         writeFSMFinalState( fsmFD , fromSt ) ;
   }

   delete [] monoInds[0] ;
   delete [] monoInds ;
   delete [] modelInds ;
   delete stateMan;
}

/**
 * This method produces a CD transducer that has a deterministic
 * inverse.  This assumes that there is a CI silence phone.  If
 * ciPause is true, then assumes there is a CI pause phone.  No other
 * CI phones permitted.  All CD phones assumed to be
 * triphones. (i.e. no biphones allowed)
 */
void WFSTCDGen::writeFSMXWordTriphoneDetInv( FILE *fsmFD, bool ciPause )
{
   // Create the state manager
   WFSTCDStateManager *stateMan =
       new WFSTCDStateManager( monoLookup , nAuxSyms ) ;

   int silMono = monoLookup->getSilMonophone() ;
   int silModel = phoneLookup->getCISilenceModelInd() ;
   if ( (silMono < 0) || (silModel < 0) )
      error("WFSTCDGen::writeFSMXWordTriphone - (silMono < 0)||(silModel < 0)") ;
   int pauseMono = monoLookup->getPauseMonophone() ;
   int pauseModel = phoneLookup->getCIPauseModelInd() ;
   if ( ciPause && ((pauseMono < 0) || (pauseModel < 0)) )
      error("WFSTCDGen::writeFSMXWordTriphone - ciPause && ((pauseMono < 0)||(pauseModel < 0))") ;

   int nPhones = phoneLookup->getNumPhones();
   int maxCD = phoneLookup->getMaxCD();
   if ( maxCD != 3 )
      error("WFSTCDGen::writeFSMXWordTriphone - maxCD != 3");
   
   // Allocate memory
   int **monoInds = new int*[nPhones] , i, j; // PNG: k deleted
   monoInds[0] = new int[nPhones*maxCD];
   for ( i=1 ; i<nPhones ; i++ )
      monoInds[i] = monoInds[0] + (i * maxCD);
   int *modelInds = new int[nPhones];
   
   int stPhns[5] , fromSt=-1 , toSt=-1 , inLab=-1 , outLab=-1 ;
   bool isNewState=false ;

   // Retrieve a properly formatted list of all CD phones and their corresponding model index.
   phoneLookup->getAllModelInfo( monoInds , modelInds ) ;

   // (5a) <eps>: from (<eps>,<eps>) to (<eps>,sil) with <eps>/sil
   fromSt = stateMan->getEpsState();
   stPhns[0] = -1; stPhns[1] = silMono;
   toSt = stateMan->getWFSTState( 2, stPhns, true, &isNewState );
   inLab = WFST_EPSILON;
   outLab = silMono + 1;
   writeFSMTransition( fsmFD, fromSt, toSt, inLab, outLab );

   // Save (<eps>,sil) state index for future use.
   int epsSilSt = toSt;
   
   for ( i=0 ; i<nPhones ; i++ )
   {
      if ( monoInds[i][1] < 0 )
         error("WFSTCDGen::writeFSMXWordTriphone - monoInds[i][1] is < 0") ;
         
      if ( monoInds[i][0] < 0 ) {
      
         if ( monoInds[i][2] < 0 ) {
         
            // monophone - only valid ones are sil & sp.
            if ( monoInds[i][1] == silMono )
               continue;   // we deal with this elsewhere
            else if ( ciPause && (monoInds[i][1] == pauseMono ) )
               continue;   // we deal with this elsewhere
            else
               error("WFSTCDGen::writeFSMXWordTriphone - invalid monophone encountered");
               
         } else {

            // ph2+ph3 biphone - invalid
            error("WFSTCDGen::writeFSMXWordTriphone - invalid ph2+ph3 biphone encountered");

         }
         
      } else if ( monoInds[i][2] < 0 ) {

         // ph1-ph2 biphone - invalid
         error("WFSTCDGen::writeFSMXWordTriphone - invalid ph1-ph2 biphone encountered");

      } else {
         
         if ( monoInds[i][1] == silMono )
            error("WFSTCDGen::writeFSMXWordTriphone - invalid ph1-sil+ph3 detected when ciSil is true");
            
         if ( monoInds[i][2] == silMono ) {
            
            // (2a) ph1-ph2+sil: from (ph1,ph2) to (<eps>,sil) with ph1-ph2+sil/sil
            stPhns[0] = monoInds[i][0]; stPhns[1] = monoInds[i][1];
            fromSt = stateMan->getWFSTState( 2, stPhns, true, &isNewState );
            inLab = modelInds[i] + 1;
            outLab = silMono + 1;
            writeFSMTransition( fsmFD, fromSt, epsSilSt, inLab, outLab );

            // (2c) ph1-ph2+sil: from (ph1,ph2,#x) to (sil,#x,sil) with ph1-ph2+sil/sil
#ifndef AUXLOOP
            for ( j=0 ; j<nAuxSyms ; j++ ) {
               stPhns[0] = monoInds[i][0]; stPhns[1] = monoInds[i][1]; 
               stPhns[2] = outAuxSymsBase + j;
               fromSt = stateMan->getWFSTState( 3, stPhns, true, &isNewState );
               stPhns[0] = silMono; stPhns[1] = outAuxSymsBase + j; stPhns[2] = silMono;
               toSt = stateMan->getWFSTState( 3, stPhns, true, &isNewState );
               // inLab & outLab OK from above
               writeFSMTransition( fsmFD, fromSt, toSt, inLab, outLab );
            }               
#endif

            if ( ciPause ) {

               // (2b) ph1-ph2+sil: from (ph1,ph2,sp) to (sil,sp,sil) with ph1-ph2+sil/sil
               stPhns[0] = monoInds[i][0]; stPhns[1] = monoInds[i][1]; stPhns[2] = pauseMono;
               fromSt = stateMan->getWFSTState( 3, stPhns, true, &isNewState );
               stPhns[0] = silMono; stPhns[1] = pauseMono; stPhns[2] = silMono;
               toSt = stateMan->getWFSTState( 3, stPhns, true, &isNewState );
               // inLab & outLab OK from above
               writeFSMTransition( fsmFD, fromSt, toSt, inLab, outLab );

               // (2d) ph1-ph2+sil: from (ph1,ph2,sp,#x) to (sil,sp,#x,sil) with ph1-ph2+sil/sil
#ifndef AUXLOOP
               for ( j=0 ; j<nAuxSyms ; j++ ) {
                  stPhns[0] = monoInds[i][0]; stPhns[1] = monoInds[i][1]; 
                  stPhns[2] = pauseMono; stPhns[3] = outAuxSymsBase + j;
                  fromSt = stateMan->getWFSTState( 4, stPhns, true, &isNewState );
                  stPhns[0] = silMono; stPhns[1] = pauseMono;
                  stPhns[2] = outAuxSymsBase + j; stPhns[3] = silMono;
                  toSt = stateMan->getWFSTState( 4, stPhns, true, &isNewState );
                  // inLab & outLab OK from above
                  writeFSMTransition( fsmFD, fromSt, toSt, inLab, outLab );
               }
#endif
            }

         } else {

            // (1a) ph1+ph2+ph3 (ph1=sil is OK): from (ph1,ph2) to (ph2,ph3) with ph1-ph2+ph3/ph3
            stPhns[0] = monoInds[i][0]; stPhns[1] = monoInds[i][1];
            fromSt = stateMan->getWFSTState( 2, stPhns, true, &isNewState );
            stPhns[0] = monoInds[i][1]; stPhns[1] = monoInds[i][2];
            toSt = stateMan->getWFSTState( 2, stPhns, true, &isNewState );
            inLab = modelInds[i] + 1;
            outLab = monoInds[i][2] + 1;
            writeFSMTransition( fsmFD, fromSt, toSt, inLab, outLab );

            // (1c) ph1-ph2+ph3: from (ph1,ph2,#x) to (ph2,#x,ph3) with ph1-ph2+ph3/ph3
            stPhns[0] = monoInds[i][0]; stPhns[1] = monoInds[i][1]; stPhns[3] = monoInds[i][2];
#ifndef AUXLOOP
            for ( j=0 ; j<nAuxSyms ; j++ ) {
               stPhns[2] = outAuxSymsBase + j;
               fromSt = stateMan->getWFSTState( 3, stPhns, true, &isNewState );
               toSt = stateMan->getWFSTState( 3, stPhns+1, true, &isNewState );
               // inLab & outLab OK from above
               writeFSMTransition( fsmFD, fromSt, toSt, inLab, outLab );
            }               
#endif

            if ( ciPause ) {

               // (1b) ph1-ph2+ph3: from (ph1,ph2,sp) to (ph2,sp,ph3) with ph1-ph2+ph3/ph3
               stPhns[0] = monoInds[i][0]; stPhns[1] = monoInds[i][1]; stPhns[2] = pauseMono;
               fromSt = stateMan->getWFSTState( 3, stPhns, true, &isNewState );
               stPhns[0] = monoInds[i][1]; stPhns[1] = pauseMono; stPhns[2] = monoInds[i][2];
               toSt = stateMan->getWFSTState( 3, stPhns, true, &isNewState );
               // inLab & outLab OK from above
               writeFSMTransition( fsmFD, fromSt, toSt, inLab, outLab );

               // (1d) ph1-ph2+ph3: from (ph1,ph2,sp,#x) to (ph2,sp,#x,ph3) with ph1-ph2+ph3/ph3
               stPhns[0] = monoInds[i][0]; stPhns[1] = monoInds[i][1]; stPhns[2] = pauseMono;
               stPhns[4] = monoInds[i][2]; 
#ifndef AUXLOOP
               for ( j=0 ; j<nAuxSyms ; j++ ) {
                  stPhns[3] = outAuxSymsBase + j;
                  fromSt = stateMan->getWFSTState( 4, stPhns, true, &isNewState );
                  toSt = stateMan->getWFSTState( 4, stPhns+1, true, &isNewState );
                  // inLab & outLab OK from above
                  writeFSMTransition( fsmFD, fromSt, toSt, inLab, outLab );
               }
#endif
            }
         }
      }
   }
   
   // (3a) sil: from (<eps>,sil) to (<eps,sil>) with sil/sil
   inLab = silModel + 1;
   outLab = silMono + 1;
   writeFSMTransition( fsmFD, epsSilSt, epsSilSt, inLab, outLab );

   // (3h) sil: from (<eps>,sil,#x) to (sil,#x,sil) with sil/sil
   stPhns[0] = -1; stPhns[1] = silMono; stPhns[3] = silMono;
#ifndef AUXLOOP
   for ( j=0 ; j<nAuxSyms ; j++ ) {
      stPhns[2] = outAuxSymsBase + j;
      fromSt = stateMan->getWFSTState( 3, stPhns, true, &isNewState );
      toSt = stateMan->getWFSTState( 3, stPhns+1, true, &isNewState );
      // inLab & outLab OK from above
      writeFSMTransition( fsmFD, fromSt, toSt, inLab, outLab );
   }
#endif     

   // (3d) sil: from (<eps>,sil) to (sil,<eps>) with sil/<eps>
   //       (sil,<eps>) is a final state
   stPhns[0] = silMono; stPhns[1] = -1;
   toSt = stateMan->getWFSTState( 2, stPhns, true, &isNewState );
   inLab = silModel + 1;
   outLab = WFST_EPSILON;
   writeFSMTransition( fsmFD, epsSilSt, toSt, inLab, outLab );
   writeFSMFinalState( fsmFD , toSt );

   // (3j) sil: from (<eps>,sil,#x) to (sil,#x,<eps>) with sil/<eps>
   inLab = silModel + 1;
   outLab = WFST_EPSILON;
   stPhns[0] = -1; stPhns[1] = silMono; stPhns[3] = -1;
#ifndef AUXLOOP
   for ( j=0 ; j<nAuxSyms ; j++ ) {
      stPhns[2] = outAuxSymsBase + j;
      fromSt = stateMan->getWFSTState( 3, stPhns, true, &isNewState );
      toSt = stateMan->getWFSTState( 3, stPhns+1, true, &isNewState );
      writeFSMTransition( fsmFD, fromSt, toSt, inLab, outLab );
   }
#endif

   // (6c) #x: from (sil,#x,<eps>) to (sil,<eps>) with #x/<eps>
   outLab = WFST_EPSILON;
   stPhns[0] = silMono; stPhns[1] = -1;
   toSt = stateMan->getWFSTState( 2, stPhns, true, &isNewState );
#ifndef AUXLOOP
   for ( j=0 ; j<nAuxSyms ; j++ ) {
      stPhns[1] = outAuxSymsBase + j; stPhns[2] = -1;
      fromSt = stateMan->getWFSTState( 3, stPhns, true, &isNewState );
      inLab = inAuxSymsBase + j + 1;
      writeFSMTransition( fsmFD, fromSt, toSt, inLab, outLab );
   }
#endif

   inLab = silModel + 1;
   for ( i=0 ; i<monoLookup->getNumMonophones() ; i++ ) {

      if ( (i == silMono) || (ciPause && (i == pauseMono)) )
         continue;
         
      outLab = i + 1;
      stPhns[0] = silMono; stPhns[1] = i;
      toSt = stateMan->getWFSTState( 2 , stPhns , false );
      if ( toSt >= 0 ) {

         // (3b) sil: from (<eps>,sil) to (sil,x) with sil/x for all existing states (sil,x)
         writeFSMTransition( fsmFD, epsSilSt, toSt, inLab, outLab );

         // (3f) sil: from (<eps>,sil,#x) to (sil,#x,y) with sil/y for all existing states (sil,y)
         stPhns[0] = -1; stPhns[1] = silMono; stPhns[3] = i;
#ifndef AUXLOOP
         for ( j=0 ; j<nAuxSyms ; j++ ) {
            stPhns[2] = outAuxSymsBase + j;
            fromSt = stateMan->getWFSTState( 3, stPhns, true, &isNewState );
            toSt = stateMan->getWFSTState( 3, stPhns+1, true, &isNewState );
            // inLab & outLab OK from above
            writeFSMTransition( fsmFD, fromSt, toSt, inLab, outLab );
         }               
#endif
         if ( ciPause ) {
         
            // (3c) sil: (<eps>,sil,sp) to (sil,sp,x) with sil/x for all existing states (sil,x)
            stPhns[0] = -1; stPhns[1] = silMono; stPhns[2] = pauseMono; stPhns[3] = i;
            fromSt = stateMan->getWFSTState( 3 , stPhns , true, &isNewState );
            toSt = stateMan->getWFSTState( 3 , stPhns+1 , true, &isNewState );
            // inLab & outLab OK from above
            writeFSMTransition( fsmFD, fromSt, toSt, inLab, outLab );

            // (3g) sil: from (<eps>,sil,sp,#x) to (sil,sp,#x,y) with sil/y 
            //           for all existing states (sil,y)
            stPhns[0] = -1; stPhns[1] = silMono; stPhns[2] = pauseMono; stPhns[4] = i;
#ifndef AUXLOOP
            for ( j=0 ; j<nAuxSyms ; j++ ) {
               stPhns[3] = outAuxSymsBase + j;
               fromSt = stateMan->getWFSTState( 4, stPhns, true, &isNewState );
               toSt = stateMan->getWFSTState( 4, stPhns+1, true, &isNewState );
               // inLab & outLab OK from above
               writeFSMTransition( fsmFD, fromSt, toSt, inLab, outLab );
            }
#endif
         }            
      }
   }

   for ( i=0 ; i<monoLookup->getNumMonophones() ; i++ ) {

      if ( i == pauseMono )
         continue;

      stPhns[0] = i;
      for ( j=0 ; j<monoLookup->getNumMonophones() ; j++ ) {

         if ( j == pauseMono )
            continue;
         if ( (i == silMono) && (j == silMono) )
            continue;

         stPhns[1] = j;
         fromSt = stateMan->getWFSTState( 2 , stPhns , false );
         if ( fromSt >= 0 ) {
         
            // (5d) <eps>: from (y,z) to (y,z,#x) with <eps>/#x
            //             for all existing states (y,z)
#ifndef AUXLOOP
            for ( k=0 ; k<nAuxSyms ; k++ ) {
               stPhns[2] = outAuxSymsBase + k;
               toSt = stateMan->getWFSTState( 3 , stPhns , false );
               if ( toSt < 0 )
                  error("WFSTCDGen::wrtFSMXWrdTri - (ph2,ph3) exists, (ph2,ph3,#x) does not (5d)");
               inLab = WFST_EPSILON;
               outLab = outAuxSymsBase + k + 1;
               writeFSMTransition( fsmFD, fromSt, toSt, inLab, outLab );
            }
#endif
            // (6a) #x: from (y,#x,z) to (y,z) with #x/<eps>
            // Use 'from' state from (5d) as the 'to' state.
            toSt = fromSt;
            stPhns[2] = j;
#ifndef AUXLOOP
            for ( k=0 ; k<nAuxSyms ; k++ ) {
               stPhns[1] = outAuxSymsBase + k;
               fromSt = stateMan->getWFSTState( 3 , stPhns , false );
               if ( fromSt < 0 )
                  error("WFSTCDGen::wrtFSMXWrdTri - (ph2,ph3) exists, (ph2,#x,ph3) does not (6a)");
               inLab = inAuxSymsBase + k + 1;
               outLab = WFST_EPSILON;
               writeFSMTransition( fsmFD, fromSt, toSt, inLab, outLab );
            }
#endif
            if ( ciPause ) {
         
               // (5e) <eps>: from (y,z,sp) to (y,z,sp,#x) with <eps>/#x
               //             for all existing states (y,z)
               stPhns[1] = j; stPhns[2] = pauseMono;
               fromSt = stateMan->getWFSTState( 3 , stPhns , false );
               if ( fromSt < 0 )
                  error("WFSTCDGen::writeFSMXWordTriphone - (ph2,ph3) exists, (ph2,ph3,sp) not");
#ifndef AUXLOOP
               for ( k=0 ; k<nAuxSyms ; k++ ) {
                  stPhns[3] = outAuxSymsBase + k;
                  toSt = stateMan->getWFSTState( 4 , stPhns , false );
                  if ( toSt < 0 )
                     error("WFSTCDGn::wrtFSMXWrdTri - (ph2,ph3) exists, (ph2,ph3,sp,#x) does not");
                  inLab = WFST_EPSILON;
                  outLab = outAuxSymsBase + k + 1;
                  writeFSMTransition( fsmFD, fromSt, toSt, inLab, outLab );
               }
#endif
            }
         }
      }
   }

   // (5f) <eps>: from (<eps>,sil) to (<eps>,sil,#x) with <eps>/#x
   inLab = WFST_EPSILON;
   stPhns[0] = -1; stPhns[1] = silMono;
#ifndef AUXLOOP
   for ( j=0 ; j<nAuxSyms ; j++ ) {
      stPhns[2] = outAuxSymsBase + j;
      toSt = stateMan->getWFSTState( 3, stPhns, true, &isNewState );
      outLab = outAuxSymsBase + j + 1;
      writeFSMTransition( fsmFD, epsSilSt, toSt, inLab, outLab );
   }               
#endif

   // (6b) #x: from (sil,#x,sil) to (<eps>,sil) with #x/<eps>
   stPhns[0] = silMono; stPhns[2] = silMono;
#ifndef AUXLOOP
   for ( k=0 ; k<nAuxSyms ; k++ ) {
      stPhns[1] = outAuxSymsBase + k;
      fromSt = stateMan->getWFSTState( 3, stPhns, true, &isNewState );
      inLab = inAuxSymsBase + k + 1;
      outLab = WFST_EPSILON;
      writeFSMTransition( fsmFD, fromSt, epsSilSt, inLab, outLab );
   }
#endif

   if ( ciPause ) {

      // (3e) sil: from (<eps>,sil,sp) to (sil,sp,sil) with sil/sil
      stPhns[0] = -1; stPhns[1] = silMono; stPhns[2] = pauseMono;
      fromSt = stateMan->getWFSTState( 3, stPhns, true, &isNewState );
      stPhns[0] = silMono; stPhns[1] = pauseMono; stPhns[2] = silMono;
      toSt = stateMan->getWFSTState( 3, stPhns, true, &isNewState );
      inLab = silModel + 1;
      outLab = silMono + 1;
      writeFSMTransition( fsmFD, fromSt, toSt, inLab, outLab );
      
      // (3i) sil: from (<eps>,sil,sp,#x) to (sil,sp,#x,sil) with sil/sil
      stPhns[0] = -1; stPhns[1] = silMono; stPhns[2] = pauseMono; stPhns[4] = silMono;
#ifndef AUXLOOP
      for ( j=0 ; j<nAuxSyms ; j++ ) {
         stPhns[3] = outAuxSymsBase + j;
         fromSt = stateMan->getWFSTState( 4, stPhns, true, &isNewState );
         toSt = stateMan->getWFSTState( 4, stPhns+1, true, &isNewState );
         // inLab & outLab OK from above
         writeFSMTransition( fsmFD, fromSt, toSt, inLab, outLab );
      }               
#endif

      // (5c) <eps>: from (<eps>,sil) to (<eps>,sil,sp) with <eps>/sp
      stPhns[0] = -1; stPhns[1] = silMono; stPhns[2] = pauseMono;
      toSt = stateMan->getWFSTState( 3, stPhns, true, &isNewState );
      inLab = WFST_EPSILON;
      outLab = pauseMono + 1;
      writeFSMTransition( fsmFD, epsSilSt, toSt, inLab, outLab );

      // (5g) <eps>: from (<eps>,sil,sp) to (<eps>,sil,sp,#x) with <eps>/#x
      stPhns[0] = -1; stPhns[1] = silMono; stPhns[2] = pauseMono;
      fromSt = stateMan->getWFSTState( 3, stPhns, true, &isNewState );
#ifndef AUXLOOP
      for ( j=0 ; j<nAuxSyms ; j++ ) {
         stPhns[3] = outAuxSymsBase + j;
         toSt = stateMan->getWFSTState( 4, stPhns, true, &isNewState );
         // inLab OK from above
         outLab = outAuxSymsBase + j + 1;
         writeFSMTransition( fsmFD, fromSt, toSt, inLab, outLab );
      }               
#endif      
      // (4b) sp: from (sil,sp,sil) to (<eps>,sil) with sp/<eps>
      stPhns[0] = silMono; stPhns[1] = pauseMono; stPhns[2] = silMono;
      fromSt = stateMan->getWFSTState( 3, stPhns, true, &isNewState );
      inLab = pauseModel + 1;
      outLab = WFST_EPSILON;
      writeFSMTransition( fsmFD, fromSt, epsSilSt, inLab, outLab );
     
      // (4d) sp: from (sil,sp,#x,sil) to (sil,#x,sil) with sp/<eps>
      stPhns[0] = silMono; stPhns[3] = silMono;
#ifndef AUXLOOP
      for ( j=0 ; j<nAuxSyms ; j++ ) {
         stPhns[2] = outAuxSymsBase + j;
         stPhns[1] = pauseMono;
         fromSt = stateMan->getWFSTState( 4, stPhns, true, &isNewState );
         stPhns[1] = silMono;
         toSt = stateMan->getWFSTState( 3, stPhns+1, true, &isNewState );
         // inLab & outLab OK from above
         writeFSMTransition( fsmFD, fromSt, toSt, inLab, outLab );
      }               
#endif
      // inLab & outLab OK from above
      for ( i=0 ; i<monoLookup->getNumMonophones() ; i++ ) {

         if ( i == pauseMono )
            continue;

         stPhns[0] = i;
         for ( j=0 ; j<monoLookup->getNumMonophones() ; j++ ) {

            if ( j == pauseMono )
               continue;
            if ( (i == silMono) && (j == silMono) )
               continue;

            // (4a) sp: from (x,sp,y) to (x,y) with sp/<eps> for all existing states (x,sp,y)
            stPhns[1] = j;
            toSt = stateMan->getWFSTState( 2 , stPhns , false );
            if ( toSt < 0 )
               continue;
            stPhns[1] = pauseMono; stPhns[2] = j;
            fromSt = stateMan->getWFSTState( 3 , stPhns , false );
            if ( fromSt < 0 )
               error("WFSTCDGen::writeFSMXWordTriphone - (ph2,ph3) exists, (ph2,sp,ph3) does not");
            writeFSMTransition( fsmFD, fromSt, toSt, inLab, outLab );
            
            // (5b) <eps>: from (x,y) to (x,y,sp) with <eps>/sp for all existing states (x,y,sp)
            fromSt = toSt;
            stPhns[1] = j; stPhns[2] = pauseMono;
            toSt = stateMan->getWFSTState( 3 , stPhns , false );
            if ( toSt < 0 )
               error("WFSTCDGen::writeFSMXWordTriphone - (ph2,ph3) exists, (ph2,ph3,sp) does not");
            writeFSMTransition( fsmFD, fromSt, toSt, WFST_EPSILON, pauseMono+1 );

            // (4c) sp: from (y,sp,#x,z) to (y,#x,z) with sp/<eps>
            //          for all existing states (y,sp,#x,z)
            stPhns[3] = j; 
#ifndef AUXLOOP
            for ( k=0 ; k<nAuxSyms ; k++ ) {
               stPhns[1] = i; stPhns[2] = outAuxSymsBase + k;
               toSt = stateMan->getWFSTState( 3, stPhns+1, false );
               if ( toSt < 0 )
                  continue;
               stPhns[1] = pauseMono;
               fromSt = stateMan->getWFSTState( 4, stPhns, false );
               if ( fromSt < 0 )
                  error("WFSTCDGen::writeFSMXWordTriphone - (y,#x,z) exists, (y,sp,#x,z) does not");
               // inLab & outLab OK from above
               writeFSMTransition( fsmFD, fromSt, toSt, inLab, outLab );
            }               
#endif
         }
      }
   }
   
   nStates = stateMan->getNumStates() ;
   
   delete [] monoInds[0] ;
   delete [] monoInds ;
   delete [] modelInds ;
   delete stateMan;
}


void WFSTCDGen::writeFSMXWordTriphoneNonDetInv( FILE *fsmFD, bool ciSil, bool ciPause )
{
   // This method produces a CD transducer that does NOT have a
   // deterministic inverse.

   // Create the state manager
   WFSTCDStateManager *stateMan = new WFSTCDStateManager( monoLookup , nAuxSyms ) ;

   int silMono = monoLookup->getSilMonophone() ;
   int silModel = phoneLookup->getCISilenceModelInd() ;
   if ( ciSil && ((silMono < 0) || (silModel < 0)) )
      error("WFSTCDGen::writeFSMXWordTriphone - ciSil && ((silMono < 0)||(silModel < 0))") ;
   int pauseMono = monoLookup->getPauseMonophone() ;
   int pauseModel = phoneLookup->getCIPauseModelInd() ;
   if ( ciPause && ((pauseMono < 0) || (pauseModel < 0)) )
      error("WFSTCDGen::writeFSMXWordTriphone - ciPause && ((pauseMono < 0)||(pauseModel < 0))") ;

   int nPhones = phoneLookup->getNumPhones() ;
   int maxCD = phoneLookup->getMaxCD() ;
   if ( maxCD != 3 )
      error("WFSTCDGen::writeFSMXWordTriphone - maxCD != 3") ;
   
   // Allocate memory
   int **monoInds = new int*[nPhones], i;
   monoInds[0] = new int[nPhones*maxCD] ;
   for ( i=1 ; i<nPhones ; i++ )
      monoInds[i] = monoInds[0] + (i * maxCD) ;
   int *modelInds = new int[nPhones] ;
   
   int stPhns[3] , fromSt=-1 , toSt=-1 , inLab=-1 , outLab=-1 ;
   bool isNewState=false ;

   // Retrieve a properly formatted list of all CD phones and their
   // corresponding model index.
   phoneLookup->getAllModelInfo( monoInds , modelInds ) ;

   if ( ciSil ) {
      
      // (8a): sil/sil arc from (<eps>,<eps>) to (sil,<eps>)
      // (8b): sil/sil arc from (sil,<eps>) to (sil,<eps>)
      fromSt = stateMan->getEpsState();
      stPhns[0] = silMono; stPhns[1] = -1;
      toSt = stateMan->getWFSTState( 2, stPhns, true, &isNewState );
      inLab = silModel + 1;
      outLab = silMono + 1;
      writeFSMTransition( fsmFD, fromSt, toSt, inLab, outLab );
      writeFSMTransition( fsmFD, toSt, toSt, inLab, outLab );

   }

   for ( i=0 ; i<nPhones ; i++ )
   {
      if ( monoInds[i][1] < 0 )
         error("WFSTCDGen::writeFSMXWordTriphone - monoInds[i][1] is < 0") ;
         
      if ( monoInds[i][0] < 0 ) {
      
         if ( monoInds[i][2] < 0 ) {
         
            // monophone - only valid ones are sil & sp.
            if ( ciSil && (monoInds[i][1] == silMono) )
               continue;   // we deal with this elsewhere
            else if ( ciPause && (monoInds[i][1] == pauseMono ) )
               continue;   // we deal with this elsewhere
            else
               error("WFSTCDGen::writeFSMXWordTriphone - invalid monophone encountered");
               
         } else {

            // (1)+(9) ph2+ph3 (ph3=sil is OK): from (<eps>,<eps>) to (ph2,ph3) with ph2+ph3/ph2
            fromSt = stateMan->getEpsState();
            stPhns[0] = monoInds[i][1]; stPhns[1] = monoInds[i][2];
            toSt = stateMan->getWFSTState( 2, stPhns, true, &isNewState );
            inLab = modelInds[i] + 1 ;
            outLab = monoInds[i][1] + 1 ;
            writeFSMTransition( fsmFD, fromSt, toSt, inLab, outLab );

         }
         
      } else if ( monoInds[i][2] < 0 ) {

         // ph1-ph2 case
         if ( ciSil && (monoInds[i][0] == silMono) ) {

            // (10) sil-ph2: from (sil,<eps>) to (ph2,<eps>) with sil-ph2/ph2
            stPhns[0] = silMono; stPhns[1] = -1;
            fromSt = stateMan->getWFSTState( 2, stPhns, true, &isNewState );
            stPhns[0] = monoInds[i][1]; stPhns[1] = -1;
            toSt = stateMan->getWFSTState( 2, stPhns, true, &isNewState );
            inLab = modelInds[i] + 1;
            outLab = monoInds[i][1] + 1;
            writeFSMTransition( fsmFD, fromSt, toSt, inLab, outLab );
         
         } else {

            // (3) ph1-ph2: from (ph1,ph2) to (ph2,<eps>) with ph1-ph2/ph2
            stPhns[0] = monoInds[i][0]; stPhns[1] = monoInds[i][1];
            fromSt = stateMan->getWFSTState( 2, stPhns, true, &isNewState );
            stPhns[0] = monoInds[i][1]; stPhns[1] = -1;
            toSt = stateMan->getWFSTState( 2, stPhns, true, &isNewState );
            inLab = modelInds[i] + 1;
            outLab = monoInds[i][1] + 1;
            writeFSMTransition( fsmFD, fromSt, toSt, inLab, outLab );
         
         }
      
      } else {

         // ph1-ph2+ph3 case
         if ( ciSil && (monoInds[i][0] == silMono) ) {
            
            // (7) sil-ph2+ph3: from (sil,<eps>) to (ph2,ph3) with sil-ph2+ph3/ph2
            stPhns[0] = silMono; stPhns[1] = -1;
            fromSt = stateMan->getWFSTState( 2, stPhns, true, &isNewState );
            stPhns[0] = monoInds[i][1]; stPhns[1] = monoInds[i][2];
            toSt = stateMan->getWFSTState( 2, stPhns, true, &isNewState );
            inLab = modelInds[i] + 1;
            outLab = monoInds[i][1] + 1;
            writeFSMTransition( fsmFD, fromSt, toSt, inLab, outLab );

         } else {

            // (4)+(6) ph1+ph2+ph3 (ph3=sil is OK): from (ph1,ph2) to (ph2,ph3) with ph1-ph2+ph3/ph2
            stPhns[0] = monoInds[i][0]; stPhns[1] = monoInds[i][1];
            fromSt = stateMan->getWFSTState( 2, stPhns, true, &isNewState );
            stPhns[0] = monoInds[i][1]; stPhns[1] = monoInds[i][2];
            toSt = stateMan->getWFSTState( 2, stPhns, true, &isNewState );
            inLab = modelInds[i] + 1;
            outLab = monoInds[i][1] + 1;
            writeFSMTransition( fsmFD, fromSt, toSt, inLab, outLab );

         }
      }
   }
      
   if ( ciSil ) {

      // (8c): from (x,sil) to (sil,<eps>) with sil/sil, for all existing states (x,sil)
      stPhns[0] = silMono ; stPhns[1] = -1 ;
      toSt = stateMan->getWFSTState( 2 , stPhns , false ) ;
      if ( toSt < 0 )
         error("WFSTCDGen::writeFSMXWordTriphone - (sil,<eps>) not found");
      inLab = silModel + 1;
      outLab = silMono + 1;
         
      for ( i=0 ; i<monoLookup->getNumMonophones() ; i++ ) {

         if ( (i == silMono) || (ciPause && (i == pauseMono)) )
            continue;

         stPhns[0] = i ; stPhns[1] = silMono ;
         fromSt = stateMan->getWFSTState( 2 , stPhns , false ) ;
         if ( fromSt >= 0 )
            writeFSMTransition( fsmFD, fromSt, toSt, inLab, outLab );

      }

   }

   if ( ciPause ) {

      // (5): from (x,y) to (x,y) with sp/sp, for all existing states (x,y)
      inLab = pauseModel + 1;
      outLab = pauseMono + 1;
      for ( i=0 ; i<stateMan->getNumStates() ; i++ )
         writeFSMTransition( fsmFD, i, i, inLab, outLab );

   }
   
   nStates = stateMan->getNumStates() ;
   
   // Write final states.
   // (x,<eps>) for all existing states (x,<eps>) are final states (incl. (sil,<eps>))
   stPhns[1] = -1 ;
   for ( i=0 ; i<monoLookup->getNumMonophones() ; i++ )
   {
      if ( ciPause && (i == pauseMono) )
         continue ;

      stPhns[0] = i ;
      fromSt = stateMan->getWFSTState( 2 , stPhns , false ) ;
      if ( fromSt >= 0 )
         writeFSMFinalState( fsmFD , fromSt ) ;
   }

   // Write self-loop arcs on each state for each auxiliary symbol
   for ( i=0 ; i<nAuxSyms ; i++ ) {
      for ( int j=0 ; j<nStates ; j++ ) {
         writeFSMTransition( fsmFD , j , j , inAuxSymsBase+i+1 , outAuxSymsBase+i+1 ) ;
      }
   }

   delete [] monoInds[0] ;
   delete [] monoInds ;
   delete [] modelInds ;
   delete stateMan;
}

/**
 * Add auxiliary self loops to all states.
 * Taken from the previous version of this file.
 */
void WFSTCDGen::writeFSMAuxTrans( FILE *fsmFD )
{
    for (int i=0 ; i<nAuxSyms ; i++)
    {
        for (int j=0 ; j<nStates ; j++)
        {
            writeFSMTransition(
                fsmFD , j , j , inAuxSymsBase+i+1 , outAuxSymsBase+i+1
            ) ;
        }
    }
}

}

