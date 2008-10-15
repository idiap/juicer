/*
 * Copyright 2004,2008 by IDIAP Research Institute
 *                        http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#include <assert.h>
#include "HTKModels.h"
#include "log_add.h"

/*
   Author:  Darren Moore (moore@idiap.ch)
   Date:    15 November 2004
*/

using namespace Torch;

namespace Juicer {


#define  MEANVARVEC_REALLOC     1000
#define  WEIGHTVEC_REALLOC      1000
#define  TRANSMAT_REALLOC       1000
#define  MIXTURE_REALLOC        1000
#define  GMM_REALLOC            1000
#define  HMM_REALLOC            1000


HTKModels::HTKModels()
{
   currFrame = -1 ;
   currInput = NULL ;
   vecSize = 0 ;
   
   nMeanVecs = 0 ;
   nMeanVecsAlloc = 0 ;
   meanVecs = NULL ;

   nVarVecs = 0 ;
   nVarVecsAlloc = 0 ;
   varVecs = NULL ;

   removeInitialToFinalTransitions = false ;
   nTransMats = 0 ;
   nTransMatsAlloc = 0 ;
   transMats = NULL ;

   nMixtures = 0 ;
   nMixturesAlloc = 0 ;
   mixtures = NULL ;
   
   nGMMs = 0 ;
   nGMMsAlloc = 0 ;
   gMMs = NULL ;
   currGMMOutputs = NULL ;

   nHMMs = 0 ;
   nHMMsAlloc = 0 ;
   hMMs = NULL ;

   hybridMode = false ;
   logPriors = NULL ;

   outFD = stdout ;
   inFD = NULL ;
   fromBinFile = false ;
}


void HTKModels::Load(
    const char *phonesListFName , const char *priorsFName , int statesPerModel
)
{
   if ( (phonesListFName == NULL) || (phonesListFName[0] == '\0') )
      error("HTKModels::Models(3) - phonesListFName not defined") ;
   if ( (priorsFName == NULL) || (priorsFName[0] == '\0') )
      error("HTKModels::Models(3) - priorsFName not defined") ;
   if ( statesPerModel <= 2 )
      error("HTKModels::Models(3) - statesPerModel <= 2 (ie. no emitting states)") ;
  
   // Initialise member variables
   currFrame = -1 ;
   currInput = NULL ;
   vecSize = 0 ;
   
   nMeanVecs = 0 ;
   nMeanVecsAlloc = 0 ;
   meanVecs = NULL ;

   nVarVecs = 0 ;
   nVarVecsAlloc = 0 ;
   varVecs = NULL ;

   removeInitialToFinalTransitions = false ;
   nTransMats = 0 ;
   nTransMatsAlloc = 0 ;
   transMats = NULL ;

   nMixtures = 0 ;
   nMixturesAlloc = 0 ;
   mixtures = NULL ;
   
   nGMMs = 0 ;
   nGMMsAlloc = 0 ;
   gMMs = NULL ;
   currGMMOutputs = NULL ;

   nHMMs = 0 ;
   nHMMsAlloc = 0 ;
   hMMs = NULL ;

   hybridMode = false ;
   logPriors = NULL ;

   outFD = stdout ;
   inFD = NULL ;
   fromBinFile = false ;
   
   // open files
   FILE *phonesFD , *priorsFD ;
   if ( (phonesFD = fopen( phonesListFName , "rb" )) == NULL )
      error("HTKModels::Models(3) - error opening phonesListFName") ;
   if ( (priorsFD = fopen( priorsFName , "rb" )) == NULL )
      error("HTKModels::Models(3) - error opening priorsFName") ;

   char *phone = new char[10000] ;
   char *priorStr = new char[10000] ;
   char *ptr , *ptr2 ;
   float prior ;
   int i ;
   while ( fgets( phone , 10000 , phonesFD ) != NULL )
   {
      // Strip whitespace, ignore blank lines.
      if ( (ptr = strtok( phone , "\r\n\t " )) == NULL )
         continue ;

      // Read next priors value
      ptr2 = NULL ;
      while ( fgets( priorStr , 10000 , priorsFD ) != NULL )
      {
         if ( (ptr2 = strtok( priorStr , "\r\n\t " )) == NULL )
            continue ;
         if ( sscanf( ptr2 , "%f" , &prior ) != 1 )
            error("HTKModels::Models(3) - non-real %s detected in priors file" , ptr2 ) ;

         // We got a prior - break
         break ;
      }

      if ( ptr2 == NULL )
         error("HTKModels::Models(3) - EOF while reading prior for phone %s" , ptr ) ;

      // Create a HMM for this phone.
      if ( nHMMs == nHMMsAlloc )
      {
         nHMMsAlloc += HMM_REALLOC ;
         hMMs = (HMM *)realloc( hMMs , nHMMsAlloc*sizeof(HMM) ) ;
         logPriors = (real *)realloc( logPriors , nHMMsAlloc*sizeof(real) ) ;
      }

      HMM *hmm = hMMs + nHMMs ;
      hmm->name = new char[strlen(ptr)+1] ;
      strcpy( hmm->name , ptr ) ;
      hmm->nStates = statesPerModel ;
      hmm->gmmInds = new int[statesPerModel] ;
      for ( i=1 ; i<(statesPerModel-1) ; i++ )
      {
         hmm->gmmInds[i] = nHMMs ;
      }
      hmm->gmmInds[0] = -1 ;
      hmm->gmmInds[statesPerModel-1] = -1 ;
      hmm->transMatrixInd = 0 ;
      logPriors[nHMMs] = log( prior ) ;
      nHMMs++ ;
   }

   delete [] phone ;
   delete [] priorStr ;
   
   fclose( phonesFD ) ;
   fclose( priorsFD ) ;

   // Add a single, shared transition matrix (all HMMs already point to it).
   real **trans = new real*[statesPerModel] ;
   trans[0] = new real[statesPerModel*statesPerModel] ;
   for ( i=1 ; i<statesPerModel ; i++ )
      trans[i] = trans[0] + (i * statesPerModel) ;
   for ( i=0 ; i<(statesPerModel*statesPerModel) ; i++ )
      trans[0][i] = 0.0 ;

   for ( i=0 ; i<(statesPerModel-1) ; i++ )
   {
      if ( i == 0 )
      {
         trans[0][1] = 1.0 ;
      }
      else
      { 
         trans[i][i] = 0.5 ;
         trans[i][i+1] = 0.5 ;
      }
   }

   addTransMatrix( NULL , statesPerModel , trans ) ;
   delete [] trans[0] ;
   delete [] trans ;
   
   vecSize = nHMMs ;
   hybridMode = true ;
}


void HTKModels::Load(
    const char *htkModelsFName , bool removeInitialToFinalTransitions_
)
{
   if ( (htkModelsFName == NULL) || (htkModelsFName[0] == '\0') )
      error("HTKModels::Models(2) - htkModelsFName undefined") ;
   
   currFrame = -1 ;
   currInput = NULL ;
   vecSize = 0 ;
   
   nMeanVecs = 0 ;
   nMeanVecsAlloc = 0 ;
   meanVecs = NULL ;

   nVarVecs = 0 ;
   nVarVecsAlloc = 0 ;
   varVecs = NULL ;

   removeInitialToFinalTransitions = removeInitialToFinalTransitions_ ;
   nTransMats = 0 ;
   nTransMatsAlloc = 0 ;
   transMats = NULL ;

   nMixtures = 0 ;
   nMixturesAlloc = 0 ;
   mixtures = NULL ;
   
   nGMMs = 0 ;
   nGMMsAlloc = 0 ;
   gMMs = NULL ;
   currGMMOutputs = NULL ;

   nHMMs = 0 ;
   nHMMsAlloc = 0 ;
   hMMs = NULL ;

   hybridMode = false ;
   logPriors = NULL ;

   outFD = stdout ;
   inFD = NULL ;
   fromBinFile = false ;

   // Open HTK models file
   FILE *fd ;
   if ( (fd = fopen( htkModelsFName , "rb" )) == NULL )
      error("HTKModels::Models(2) - error opening htkModelsFName") ;

	// Use our HTK (bison) parser to read the file.
	if ( htkparse( (void *)fd ) != 0 )
		error("HTKModels::Models(2) - htkparse failed") ;

   // Initialise our data structures using result of parse
   initFromHTKParseResult() ;

   // Clean up parse result & close HTK models file
   cleanHTKDef() ;
   fclose( fd ) ;
}


HTKModels::~HTKModels()
{
   int i ;

   // Delete mean vecs
   if ( meanVecs != NULL )
   {
      for ( i=0 ; i<nMeanVecs ; i++ )
      {
         delete [] meanVecs[i].name ;
         delete [] meanVecs[i].means ;
      }
      if ( fromBinFile )
         delete [] meanVecs ;
      else
         free( meanVecs ) ;
   }
   
   // Delete var vecs
   if ( varVecs != NULL )
   {
      for ( i=0 ; i<nVarVecs ; i++ )
      {
         delete [] varVecs[i].name ;
         delete [] varVecs[i].vars ;
      }
      if ( fromBinFile )
         delete [] varVecs ;
      else
         free( varVecs ) ;
   }

   // Delete the transition matrices
   if ( transMats != NULL )
   {
      for ( i=0 ; i<nTransMats ; i++ )
      {
         delete [] transMats[i].name ;
         delete [] transMats[i].nSucs ;
         if ( transMats[i].sucs != NULL )
         {
            delete [] transMats[i].sucs[0] ;
            delete [] transMats[i].probs[0] ;
            delete [] transMats[i].sucs ;
            delete [] transMats[i].probs ;
         }
      }
      if ( fromBinFile )
         delete [] transMats ;
      else
         free( transMats ) ;
   }

   // Delete mixtures
   if ( mixtures != NULL )
   {
      for ( i=0 ; i<nMixtures ; i++ )
      {
         delete [] mixtures[i].name ;
         delete [] mixtures[i].meanVecInds ;
      }
      if ( fromBinFile )
         delete [] mixtures ;
      else
         free( mixtures ) ;      
   }
  
   // Delete GMM's
   if ( gMMs != NULL )
   {
      for ( i=0 ; i<nGMMs ; i++ )
      {
         delete [] gMMs[i].name ;
         delete [] gMMs[i].compWeights ;
      }
      if ( fromBinFile )
         delete [] gMMs ;
      else
         free( gMMs ) ;
   }

   // Delete HMM's
   if ( hMMs != NULL )
   {
      for ( i=0 ; i<nHMMs ; i++ )
      {
         delete [] hMMs[i].name ;
         delete [] hMMs[i].gmmInds ;
      }
      if ( fromBinFile )
         delete [] hMMs ;
      else
         free( hMMs ) ;
   }

   // Delete currGMMOutputs
   delete [] currGMMOutputs ;

   // Delete logPriors
   if ( logPriors != NULL )
   {
      if ( fromBinFile )
         delete [] logPriors ;
      else
         free( logPriors ) ;
   }
}


void HTKModels::initFromHTKParseResult()
{
	// Results of htkparse are in the global variable "htk_def"

   vecSize = htk_def.global_opts.vec_size ;

   // Add all of the shared mixtures
   int i ;
   for ( i=0 ; i<htk_def.n_mix_pools ; i++ )
   {
      if ( htk_def.mix_pools[i]->name == NULL )
         error("HTKModels::initFromHTKParseResult - htk_def.mix_pools[i]->name undefined") ;
      addMixture( htk_def.mix_pools[i] ) ;
   }
   
   // Add all of the shared transition matrices
   for ( i=0 ; i<htk_def.n_sh_transmats ; i++ )
   {
      if ( htk_def.sh_transmats[i]->sh_name == NULL )
         error("HTKModels::initFromHTKParseResult - htk_def.sh_transmats[i]->sh_name undefined") ;
      addTransMatrix( htk_def.sh_transmats[i]->sh_name , htk_def.sh_transmats[i]->n_states ,
                      htk_def.sh_transmats[i]->transp ) ;
   }
   
   // Add all of the shared states
   for ( i=0 ; i<htk_def.n_sh_states ; i++ )
   {
      if ( htk_def.sh_states[i]->sh_name == NULL )
         error("HTKModels::initFromHTKParseResult - htk_def.sh_states[i]->sh_name undefined") ;
      addGMM( htk_def.sh_states[i] ) ;
   }
   
   // Add all of the HMMs
   for ( i=0 ; i<htk_def.n_hmms ; i++ )
   {
      if ( htk_def.hmms[i]->name == NULL )
         error("HTKModels::initFromHTKParseResult - htk_def.hmms[i]->name undefined") ;
         
      addHMM( htk_def.hmms[i] ) ;      
   }

   // Configure the buffers that hold current outputs
   currGMMOutputs = new real[nGMMs] ;
   for ( i=0 ; i<nGMMs ; i++ )
      currGMMOutputs[i] = LOG_ZERO ;

   fromBinFile = false ;
}


void HTKModels::newFrame( int frame , const real *input )
{
   if ( (frame > 0) && (frame != (currFrame+1)) )
      error("HTKModels::newFrame - invalid frame") ;

   if ( (hybridMode == false) && (currFrame >= 0) )
   {
      // clear outputs from previous frame
      int i ;
      for ( i=0 ; i<nMixtures ; i++ )
         mixtures[i].currCompOutputsValid = false ;
      for ( i=0 ; i<nGMMs ; i++ )
         currGMMOutputs[i] = LOG_ZERO ;
   }

   currFrame = frame ;
   currInput = input ;
}


real HTKModels::calcOutput( int hmmInd , int stateInd )
{
#ifdef DEBUG
   if ( (hmmInd < 0) || (hmmInd >= nHMMs) )
      error("HTKModels::calcOutput - hmmInd out of range") ;
   if ( (stateInd < 0) || (stateInd >= hMMs[hmmInd].nStates) )
      error("HTKModels::calcOutput - stateInd out of range") ;
   if ( hMMs[hmmInd].gmmInds[stateInd] < 0 )
      error("HTKModels::calcOutput - no gmm associated with stateInd") ;
#endif

   if ( hybridMode )
   {
#ifdef DEBUG
      if ( hmmInd != hMMs[hmmInd].gmmInds[stateInd] )
         error("HTKModels::calcOutput - unexpected gmmInds value") ;
#endif
      return ( currInput[hmmInd] - logPriors[hmmInd] ) ;
   }
   else
      return calcGMMOutput( hMMs[hmmInd].gmmInds[stateInd] ) ;
}


real HTKModels::calcOutput( int gmmInd )
{
#ifdef DEBUG
   if ( hybridMode )
   {
      if ( (gmmInd < 0) || (gmmInd >= nHMMs) )
         error("HTKModels::calcOutput(2) - gmmInd out of range") ;
   }
   else
   {
      if ( (gmmInd < 0) || (gmmInd >= nGMMs) )
         error("HTKModels::calcOutput(2) - gmmInd out of range") ;
   }
#endif

   if ( hybridMode )
   {
      //printf("%.3f %.3f\n",currInput[gmmInd],logPriors[gmmInd]);fflush(stdout);
      return ( currInput[gmmInd] - logPriors[gmmInd] ) ;
   }
   else
      return calcGMMOutput( gmmInd ) ;   
}


int HTKModels::addHMM( HTKHMM *hmm )
{
   if ( nHMMs == nHMMsAlloc )
   {
      nHMMsAlloc += HMM_REALLOC ;
      hMMs = (HMM *)realloc( hMMs , nHMMsAlloc*sizeof(HMM) ) ;
   }

   HMM *curr = hMMs + nHMMs ;

   // Configure the HMM name.
   curr->name = NULL ;
   if ( hmm->name != NULL )
   {
      curr->name = new char[strlen(hmm->name)+1] ;
      strcpy( curr->name , hmm->name ) ;
   }
   else
      error("HTKModels::addHMM - hmm does not have a name") ;

   // Configure the number of states in the HMM
   curr->nStates = hmm->n_states ;
   curr->gmmInds = new int[curr->nStates] ;
   int i ;
   for ( i=0 ; i<curr->nStates ; i++ )
   {
      curr->gmmInds[i] = -1 ;
   }

   // Add the state definitions
   for ( i=1 ; i<(curr->nStates-1) ; i++ )
   {
      if ( hmm->emit_states[i-1]->sh_name != NULL )
      {
         // This state uses a shared state definition
         curr->gmmInds[i] = getGMM( hmm->emit_states[i-1]->sh_name ) ;
         if ( curr->gmmInds[i] < 0 )
            error("HTKModels::addHMM - shared state not found") ;
      }
      else
      {
         // This state is not shared.
         curr->gmmInds[i] = addGMM( hmm->emit_states[i-1] ) ;
      }
   }

   // Add the state transition probabilities
   if ( hmm->transmat->sh_name != NULL )
   {
      curr->transMatrixInd = getTransMatrix( hmm->transmat->sh_name ) ;
      if ( curr->transMatrixInd < 0 )
         error("HTKModels::addHMM - shared transition matrix not found %s",hmm->transmat->sh_name) ;
   }
   else
   {
      // The transition matrix is not shared.
      if ( curr->nStates != hmm->transmat->n_states )
         error("HTKModels::addHMM - curr->nStates != hmm->transmat->n_states") ;
      curr->transMatrixInd = addTransMatrix( NULL , curr->nStates , hmm->transmat->transp ) ;
   }

   nHMMs++ ;
   return ( nHMMs - 1 ) ;
}


int HTKModels::addGMM( HTKHMMState *st )
{
   if ( nGMMs == nGMMsAlloc )
   {
      nGMMsAlloc += GMM_REALLOC ;
      gMMs = (GMM *)realloc( gMMs , nGMMsAlloc*sizeof(GMM) ) ;
   }
   
   GMM *curr = gMMs + nGMMs ;
   
   curr->name = NULL ;
   if ( st->sh_name != NULL )
   {
      curr->name = new char[strlen(st->sh_name)+1] ;
      strcpy( curr->name , st->sh_name ) ;
   }
      
   if ( st->mixes == NULL )
   {
      // This GMM uses a shared mixture definition.
      if ( (st->pool_ind < 0) || (st->pool_ind >= htk_def.n_mix_pools) )
         error("HTKModels::addGMM - st had invalid pool_ind") ;

      // Find the shared mixture in our 'mixtures' array that has the same name
      //   as the one referenced by 'st->pool_ind'.
      // Configure mixtureInd of new GMM accordingly.
      curr->mixtureInd = getMixture( htk_def.mix_pools[st->pool_ind]->name ) ;
      if ( curr->mixtureInd < 0 )
         error("HTKModels::addGMM - shared mixture not found") ;

      int nComps = mixtures[curr->mixtureInd].nComps ;
      if ( st->n_mixes != nComps )
         error("HTKModels::addGMM - st->n_mixes != nComps") ;
      
      // Mixture component weights
      curr->compWeights = new real[nComps * 2] ;
      curr->logCompWeights = curr->compWeights + nComps ;
      
      // Configure the weight vector
      for ( int i=0 ; i<nComps ; i++ )
      {
         curr->compWeights[i] = st->weights[i] ;
         if ( st->weights[i] > 0.0 )
            curr->logCompWeights[i] = log( st->weights[i] ) ;
         else
            curr->logCompWeights[i] = LOG_ZERO ;
      }
      if ( (nComps == 1) && (curr->compWeights[0] != 1.0) )
         error("HTKModels::addGMM - (nComps == 1) && (curr->compWeights[0] != 1.0)") ;
   }
   else
   {
      // This shared state has it's own mixture definition.
      // Add a new mixture.
      curr->mixtureInd = addMixture( st->n_mixes , st->mixes ) ;

      // Grab an empty weight vector
      curr->compWeights = new real[(st->n_mixes)*2] ;
      curr->logCompWeights = curr->compWeights + st->n_mixes ;

      // Configure the weight vector
      for ( int i=0 ; i<st->n_mixes ; i++ )
      {
         curr->compWeights[i] = st->mixes[i]->weight ;
         if ( st->mixes[i]->weight > 0.0 )
            curr->logCompWeights[i] = log( st->mixes[i]->weight ) ;
         else
            curr->logCompWeights[i] = LOG_ZERO ;
      }
      
      if ( (st->n_mixes == 1) && (curr->compWeights[0] != 1.0) )
         error("HTKModels::addGMM - (st->n_mixes == 1) && (curr->compWeights[0] != 1.0)") ;
   }

   nGMMs++ ;
   return ( nGMMs - 1 ) ;
}


int HTKModels::getGMM( const char *name )
{
   for ( int i=0 ; i<nGMMs ; i++ )
   {
      if ( gMMs[i].name == NULL )
         continue ;

      if ( strcmp( name , gMMs[i].name ) == 0 )
         return i ;
   }

   return -1 ;
}


int HTKModels::addMixture( HTKMixturePool *mix )
{
   if ( nMixtures == nMixturesAlloc )
   {
      nMixturesAlloc += MIXTURE_REALLOC ;
      mixtures = (Mixture *)realloc( mixtures , nMixturesAlloc*sizeof(Mixture) ) ;
   }

   // 1. Mixture name
   Mixture *curr = mixtures + nMixtures ;
   curr->name = NULL ;
   if ( mix->name != NULL )
   {
      curr->name = new char[strlen(mix->name)+1] ;
      strcpy( curr->name , mix->name ) ;
   }

   // 2. Number of mixture components + alloc arrays to hold indices of 
   //    mean and var vectors for each component.
   curr->nComps = mix->n_mixes ;
   curr->meanVecInds = new int[curr->nComps * 2] ;
   curr->varVecInds = curr->meanVecInds + curr->nComps ;
   int i ;
   for ( i=0 ; i<curr->nComps ; i++ )
   {
      curr->meanVecInds[i] = -1 ;
      curr->varVecInds[i] = -1 ;
   }

   // 3. Mixture components
   for ( i=0 ; i<curr->nComps ; i++ )
   {
      // Means - N.B. assuming no shared mean vecs
      if ( mix->mixes[i]->n_means != vecSize )
         error("HTKModels::addMixture - n_means != vecSize") ;
      curr->meanVecInds[i] = addMeanVec( NULL , mix->mixes[i]->means ) ;

      // Vars - N.B. assuming no shared var vecs
      if ( mix->mixes[i]->n_vars != vecSize )
         error("HTKModels::addMixture - n_vars != vecSize") ;
      curr->varVecInds[i] = addVarVec( NULL , mix->mixes[i]->vars ) ;
   }

   // 4. Memory to store current outputs for each component
   curr->currCompOutputsValid = false ;
   curr->currCompOutputs = new real[curr->nComps] ;
   for ( i=0 ; i<curr->nComps ; i++ )
      curr->currCompOutputs[i] = LOG_ZERO ;
      
   nMixtures++ ;
   return ( nMixtures - 1 ) ;
}


int HTKModels::addMixture( int nComps , HTKMixture **comps )
{
   if ( nMixtures == nMixturesAlloc )
   {
      nMixturesAlloc += MIXTURE_REALLOC ;
      mixtures = (Mixture *)realloc( mixtures , nMixturesAlloc*sizeof(Mixture) ) ;
   }

   Mixture *curr = mixtures + nMixtures ;
   
   curr->name = NULL ;
   curr->nComps = nComps ;
   curr->meanVecInds = new int[nComps * 2] ;
   curr->varVecInds = curr->meanVecInds + nComps ;
   int i ;
   for ( i=0 ; i<nComps ; i++ )
   {
      curr->meanVecInds[i] = -1 ;
      curr->varVecInds[i] = -1 ;
   }

   for ( i=0 ; i<nComps ; i++ )
   {
      // Means - N.B. assuming no shared mean vecs
      if ( comps[i]->n_means != vecSize )
         error("HTKModels::addMixture(2) - n_means != vecSize") ;
      curr->meanVecInds[i] = addMeanVec( NULL , comps[i]->means ) ;

      // Vars - N.B. assuming no shared var vecs
      if ( comps[i]->n_vars != vecSize )
         error("HTKModels::addMixture(2) - n_vars != vecSize") ;
      curr->varVecInds[i] = addVarVec( NULL , comps[i]->vars ) ;
   }

   // 4. Memory to store current outputs for each component
   curr->currCompOutputsValid = false ;
   curr->currCompOutputs = new real[curr->nComps] ;
   for ( i=0 ; i<curr->nComps ; i++ )
      curr->currCompOutputs[i] = LOG_ZERO ;

   nMixtures++ ;
   return ( nMixtures - 1 ) ;
}


int HTKModels::getMixture( const char *name )
{
   for ( int i=0 ; i<nMixtures ; i++ )
   {
      if ( mixtures[i].name == NULL )
         continue ;

      if ( strcmp( name , mixtures[i].name ) == 0 )
         return i ;
   }

   return -1 ;
}


int HTKModels::addMeanVec( const char *name , real *means )
{
   if ( means == NULL )
      error("HTKModels::addMeanVec - means is NULL") ;

   if ( nMeanVecs == nMeanVecsAlloc )
   {
      nMeanVecsAlloc += MEANVARVEC_REALLOC ;
      meanVecs = (MeanVec *)realloc( meanVecs , nMeanVecsAlloc*sizeof(MeanVec) ) ;
   }

   meanVecs[nMeanVecs].name = NULL ;
   meanVecs[nMeanVecs].means = new real[vecSize] ;

   if ( name != NULL )
   {
      meanVecs[nMeanVecs].name = new char[strlen(name)+1] ;
      strcpy( meanVecs[nMeanVecs].name , name ) ;
   }
   
   memcpy( meanVecs[nMeanVecs].means , means , vecSize*sizeof(real) ) ;
  
   nMeanVecs++ ;
   return ( nMeanVecs - 1 ) ;
}


int HTKModels::addVarVec( const char *name , real *vars )
{
   if ( vars == NULL )
      error("HTKModels::addVarVec - vars is NULL") ;

   if ( nVarVecs == nVarVecsAlloc )
   {
      nVarVecsAlloc += MEANVARVEC_REALLOC ;
      varVecs = (VarVec *)realloc( varVecs , nVarVecsAlloc*sizeof(VarVec) ) ;
   }

   varVecs[nVarVecs].name = NULL ;
   if ( name != NULL )
   {
      varVecs[nVarVecs].name = new char[strlen(name)+1] ;
      strcpy( varVecs[nVarVecs].name , name ) ;
   }
   
   varVecs[nVarVecs].vars = new real[vecSize * 2] ;
   varVecs[nVarVecs].minusHalfOverVars = varVecs[nVarVecs].vars + vecSize ;
   memcpy( varVecs[nVarVecs].vars , vars , vecSize*sizeof(real) ) ;

   // Pre-compute some stuff we can use to evaluate likelihoods
   real v ;
   varVecs[nVarVecs].sumLogVarPlusNObsLog2Pi = (real)(vecSize * LOG_2_PI) ;
   for ( int i=0 ; i<vecSize ; i++ )
   {
      v = varVecs[nVarVecs].vars[i] ;
      varVecs[nVarVecs].minusHalfOverVars[i] = (real)-0.5 / v ;
      varVecs[nVarVecs].sumLogVarPlusNObsLog2Pi += log( v ) ;
   }
   varVecs[nVarVecs].sumLogVarPlusNObsLog2Pi *= -0.5 ;
  
   nVarVecs++ ;
   return ( nVarVecs - 1 ) ;
}


int HTKModels::addTransMatrix( const char *name , int nStates , real **trans )
{
   if ( nStates < 0 )
      error("HTKModels::addTransMatrix - nStates < 0") ;
   if ( trans == NULL )
      error("HTKModels::addTransMatrix - trans is NULL") ;
   
   if ( nTransMats == nTransMatsAlloc )
   {
      nTransMatsAlloc += TRANSMAT_REALLOC ;
      transMats = (TransMatrix *)realloc( transMats , nTransMatsAlloc*sizeof(TransMatrix) ) ;
   }

   TransMatrix *curr = transMats + nTransMats ;

   // Configure name
   curr->name = NULL ;
   if ( name != NULL )
   {
      curr->name = new char[strlen(name)+1] ;
      strcpy( curr->name , name ) ;
   }

   // Configure nStates
   curr->nStates = nStates ;
   curr->nSucs = new int[nStates] ;
   curr->sucs = new int*[nStates] ;
   curr->probs = new real*[nStates * 2] ;
   curr->logProbs = curr->probs + nStates ;
   int i ;
   for ( i=0 ; i<curr->nStates ; i++ )
   {
      curr->nSucs[i] = 0 ;
      curr->sucs[i] = NULL ;
      curr->probs[i] = NULL ;
   }

   // Configure transProbs
   int j , totalTrans=0 ;
   for ( i=0 ; i<nStates ; i++ )
   {
      for ( j=0 ; j<nStates ; j++ )
      {
         if ( trans[i][j] > 0.0 )
         {
             if ( (i == 0) && (j==(nStates-1)) &&
                  removeInitialToFinalTransitions )
                 continue ;
             totalTrans++ ;
         }
      }
   }

   curr->sucs[0] = new int[totalTrans] ;
   curr->probs[0] = new real[totalTrans*2] ;
   curr->logProbs[0] = curr->probs[0] + totalTrans ;
   int ind=0 ;
   real teeProb=0.0 ;
   for ( i=0 ; i<nStates ; i++ )
   {
      for ( j=0 ; j<nStates ; j++ )
      {
         if ( trans[i][j] > 0.0 )
         {
            if ( (i == 0) && (j==(nStates-1)) &&
                 removeInitialToFinalTransitions )
            {
                if ( (teeProb = trans[i][j]) >= 1.0 )               
                    error("HTKModels::addTransMatrix"
                          " - initial-final transition had prob. >= 1.0") ;
                continue ;
            }

            curr->sucs[0][ind] = j ;
            curr->probs[0][ind] = trans[i][j] ;
            curr->logProbs[0][ind] = log( trans[i][j] ) ;
            if ( curr->nSucs[i] == 0 )
            {
               curr->sucs[i] = curr->sucs[0] + ind ;
               curr->probs[i] = curr->probs[0] + ind ;
               curr->logProbs[i] = curr->logProbs[0] + ind ;
            }
            (curr->nSucs[i])++ ;
            ind++ ;
         }
      }
   }

   // If we removed the tee transition, re-normalise the transitions
   // from the initial state.
   if ( teeProb > 0.0 )
   {
      for ( i=0 ; i<curr->nSucs[0] ; i++ )
      {
         curr->probs[0][i] /= (1.0 - teeProb) ;
         curr->logProbs[0][i] -= log(1.0 - teeProb) ;
      }
   }

   nTransMats++ ;
   return ( nTransMats - 1 ) ;
}  
            

int HTKModels::getTransMatrix( const char *name )
{
   for ( int i=0 ; i<nTransMats ; i++ )
   {
      if ( transMats[i].name == NULL )
         continue ;

      if ( strcmp( name , transMats[i].name ) == 0 )
         return i ;
   }

   return -1 ;
   
}


void HTKModels::output( const char *fName , bool outputBinary )
{
   if ( (outFD = fopen( fName , "wb" )) == NULL )
      error("HTKModels::output - error opening file") ;

   int i ;

   if ( outputBinary == false )
   {
      // Output global options
      fprintf( outFD , "~o <VecSize> %d <StreamInfo> 1 %d\n" , vecSize , vecSize ) ;

      // Output shared mean vectors
      for ( i=0 ; i<nMeanVecs ; i++ )
      {
         if ( meanVecs[i].name != NULL )
            outputMeanVec( i , false , false ) ;
      }

      // Output shared variance vectors
      for ( i=0 ; i<nVarVecs ; i++ )
      {
         if ( varVecs[i].name != NULL )
            outputVarVec( i , false , false ) ;
      }

      // Output shared mixtures
      for ( i=0 ; i<nMixtures ; i++ )
      {
         if ( mixtures[i].name != NULL )
            outputMixture( i , NULL , false , false ) ;
      }

      // Output shared states
      for ( i=0 ; i<nGMMs ; i++ )
      {
         if ( gMMs[i].name != NULL )
            outputGMM( i , false , false ) ;
      }

      // Output shared transition matrices
      for ( i=0 ; i<nTransMats ; i++ )
      {
         if ( transMats[i].name != NULL )
            outputTransMat( i , false , false ) ;
      }

      // Output all HMM's
      for ( i=0 ; i<nHMMs ; i++ )
         outputHMM( i , false ) ;
   }
   else
   {
      char id[5] ;
      strcpy( id , "JMBI" ) ;

      // Write the ID
      fwrite( id , sizeof(int) , 1 , outFD ) ;

      // Write global options
      fwrite( &vecSize , sizeof(int) , 1 , outFD ) ;

      // Write the numbers of everything
      fwrite( &nMeanVecs , sizeof(int) , 1 , outFD ) ;
      fwrite( &nVarVecs , sizeof(int) , 1 , outFD ) ;
      fwrite( &nMixtures , sizeof(int) , 1 , outFD ) ;
      fwrite( &nGMMs , sizeof(int) , 1 , outFD ) ;
      fwrite( &nTransMats , sizeof(int) , 1 , outFD ) ;
      fwrite( &nHMMs , sizeof(int) , 1 , outFD ) ;

      // Output all mean vectors
      for ( i=0 ; i<nMeanVecs ; i++ )
      {
         outputMeanVec( i , false , true ) ;
      }

      // Output all variance vectors
      for ( i=0 ; i<nVarVecs ; i++ )
      {
         outputVarVec( i , false , true ) ;
      }

      // Output all mixtures
      for ( i=0 ; i<nMixtures ; i++ )
      {
         outputMixture( i , NULL , false , true ) ;
      }

      // Output all states
      for ( i=0 ; i<nGMMs ; i++ )
      {
         outputGMM( i , false , true ) ;
      }

      // Output all transition matrices
      for ( i=0 ; i<nTransMats ; i++ )
      {
         outputTransMat( i , false , true ) ;
      }

      // Output all HMM's
      for ( i=0 ; i<nHMMs ; i++ )
      {
         outputHMM( i , true ) ;
      }

      // Output Hybrid mode flag
      fwrite( &hybridMode , sizeof(bool) , 1 , outFD ) ;

      // Write logPriors if hybridMode is true
      if ( hybridMode )
         fwrite( logPriors , sizeof(real) , nHMMs , outFD ) ;
   }

   fclose( outFD ) ;
   outFD = NULL ;
}


void HTKModels::readBinary( const char *fName )
{
   if ( (inFD = fopen( fName , "rb" )) == NULL )
      error("HTKModels::readBinary - error opening file") ;

   char id[5] ;

   // Read and check the ID
   if ( fread( id , sizeof(int) , 1 , inFD ) != 1 )
      error("HTKModels::readBinary - error reading ID") ;
   id[4] = '\0' ;
   if ( strcmp( id , "JMBI" ) != 0 )
      error("HTKModels::readBinary - invalid ID = %s" , id ) ;

   // Read the global options
   if ( fread( &vecSize , sizeof(int) , 1 , inFD ) != 1 )
      error("HTKModels::readBinary - error reading vecSize") ;

   // Read the numbers of everything
   if ( fread( &nMeanVecsAlloc , sizeof(int) , 1 , inFD ) != 1 )
      error("HTKModels::readBinary - error reading nMeanVecs") ;
   if ( fread( &nVarVecsAlloc , sizeof(int) , 1 , inFD ) != 1 )
      error("HTKModels::readBinary - error reading nVarVecs") ;
   if ( fread( &nMixturesAlloc , sizeof(int) , 1 , inFD ) != 1 )
      error("HTKModels::readBinary - error reading nMixtures") ;
   if ( fread( &nGMMsAlloc , sizeof(int) , 1 , inFD ) != 1 )
      error("HTKModels::readBinary - error reading nGMMs") ;
   if ( fread( &nTransMatsAlloc , sizeof(int) , 1 , inFD ) != 1 )
      error("HTKModels::readBinary - error reading nTransMats") ;
   if ( fread( &nHMMsAlloc , sizeof(int) , 1 , inFD ) != 1 )
      error("HTKModels::readBinary - error reading nHMMs") ;

   // Allocate arrays
   if ( nMeanVecsAlloc > 0 )
      meanVecs = new MeanVec[nMeanVecsAlloc] ;
   else
      meanVecs = NULL ;
   if ( nVarVecsAlloc > 0 )
      varVecs = new VarVec[nVarVecsAlloc] ;
   else
      varVecs = NULL ;
   if ( nMixturesAlloc > 0 )
      mixtures = new Mixture[nMixturesAlloc] ;
   else
      mixtures = NULL ;
   if ( nGMMsAlloc > 0 )
      gMMs = new GMM[nGMMsAlloc] ;
   else
      gMMs = NULL ;
   if ( nTransMatsAlloc > 0 )
      transMats = new TransMatrix[nTransMatsAlloc] ;
   else
      transMats = NULL ;
   if ( nHMMsAlloc > 0 )
      hMMs = new HMM[nHMMsAlloc] ;
   else
      hMMs = NULL ;
 
   nMeanVecs = 0 ;
   nVarVecs = 0 ;
   nMixtures = 0 ;
   nGMMs = 0 ;
   nTransMats = 0 ;
   nHMMs = 0 ;

   int i ;
   // Read the mean vectors
   for ( i=0 ; i<nMeanVecsAlloc ; i++ )
   {
      readBinaryMeanVec() ;
   }
   
   // Read the variance vectors
   for ( i=0 ; i<nVarVecsAlloc ; i++ )
   {
      readBinaryVarVec() ;
   }

   // Read the mixtures
   for ( i=0 ; i<nMixturesAlloc ; i++ )
   {
      readBinaryMixture() ;
   }

   // Read the GMMs
   for ( i=0 ; i<nGMMsAlloc ; i++ )
   {
      readBinaryGMM() ;
   }

   // Read the transition matrices
   for ( i=0 ; i<nTransMatsAlloc ; i++ )
   {
      readBinaryTransMat() ;
   }
   
   // Read the HMMs
   for ( i=0 ; i<nHMMsAlloc ; i++ )
   {
      readBinaryHMM() ;
   }

   // Read the hybrid mode flag
   if ( fread( &hybridMode , sizeof(bool) , 1 , inFD ) != 1 )
      error("HTKModels::readBinary - error reading hybridMode") ;

   // If hybridMode is true, read logPriors
   if ( hybridMode && (nHMMs > 0) )
   {
      logPriors = new real[nHMMs] ;
      if ( (int)fread( logPriors , sizeof(real) , nHMMs , inFD ) != nHMMs )
         error("HTKModels::readBinary - error reading logPriors") ;
   }
   else
      logPriors = NULL ;

   // Finished reading - close file
   fclose( inFD ) ;
   inFD = NULL ;

   currGMMOutputs = NULL ;
   if ( hybridMode == false )
   {
      // Configure the buffers that hold current outputs
      currGMMOutputs = new real[nGMMs] ;
      for ( i=0 ; i<nGMMs ; i++ )
      {
         currGMMOutputs[i] = LOG_ZERO ;
      }
   }

   fromBinFile = true ;
}


void HTKModels::outputHMM( int ind , bool outputBinary )
{
   if ( (ind < 0) || (ind >= nHMMs) )
      error("HTKModels::outputHMM - ind out of range") ;

   HMM *curr = hMMs + ind ;

   if ( outputBinary == false )
   {
      // Output macro + name
      fprintf( outFD , "~h \"%s\"\n" , curr->name ) ;
      fprintf( outFD , "<BeginHMM>\n") ;

      // Output states
      fprintf( outFD , "  <NumStates> %d\n" , curr->nStates ) ;
      for ( int i=1 ; i<(curr->nStates-1) ; i++ )
      {
         fprintf( outFD , "  <State> %d\n" , i+1 ) ;
         outputGMM( curr->gmmInds[i] , true , false ) ;
      }

      // Output transition probs
      outputTransMat( curr->transMatrixInd , true , false ) ;
      fprintf( outFD , "<EndHMM>\n" ) ;
   }
   else
   {
      char id[5] ;
      strcpy( id , "JMHM" ) ;

      // Write the ID
      fwrite( id , sizeof(int) , 1 , outFD ) ;

      // Write the length of the name
      int len=0 ;
      if ( curr->name != NULL )
         len = strlen( curr->name ) + 1 ;
      fwrite( &len , sizeof(int) , 1 , outFD ) ;

      if ( len > 0 )
      {
         // Write the name
         fwrite( curr->name , sizeof(char) , len , outFD ) ;
      }
      
      // Write the number of states
      fwrite( &(curr->nStates) , sizeof(int) , 1 , outFD ) ;

      // Write the indices of the GMMs for the states
      fwrite( curr->gmmInds , sizeof(int) , curr->nStates , outFD ) ;

      // Write the index of the transition matrix
      fwrite( &(curr->transMatrixInd) , sizeof(int) , 1 , outFD ) ;
   }
}


void HTKModels::readBinaryHMM()
{
   char id[5] ;

   // Read and check the ID
   if ( fread( id , sizeof(int) , 1 , inFD ) != 1 )
      error("HTKModels::readBinaryHMM - error reading ID") ;
   id[4] = '\0' ;
   if ( strcmp( id , "JMHM" ) != 0 )
      error("HTKModels::readBinaryHMM - invalid ID = %s" , id ) ;

   // Make sure we are not about to exceed the pre-allocated memory
   if ( nHMMs == nHMMsAlloc )
      error("HTKModels::readBinaryHMM - nHMMs == nHMMsAlloc") ;

   HMM *curr = hMMs + nHMMs ;

   // Read the length of the name
   int len ;
   if ( fread( &len , sizeof(int) , 1 , inFD ) != 1 )
      error("HTKModels::readBinaryHMM - error reading name length") ;

   // Allocate space for the name if required and read name from file
   if ( len > 0 )
   {
      curr->name = new char[len] ;
      if ( (int)fread( curr->name , sizeof(char) , len , inFD ) != len )
         error("HTKModels::readBinaryHMM - error reading name") ;
   }
   else
   {
      curr->name = NULL ;
   }

   // Read the number of states
   if ( fread( &(curr->nStates) , sizeof(int) , 1 , inFD ) != 1 )
      error("HTKModels::readBinaryHMM - error reading nStates") ;

   // Allocate memory for the gmmInds array
   curr->gmmInds = new int[curr->nStates] ;

   // Read the gmmInds array
   if ( (int)fread( curr->gmmInds , sizeof(int) , curr->nStates , inFD ) != curr->nStates )
      error("HTKModels::readBinaryHMM - error reading gmmInds") ;

   // Read the transMatrixInd
   if ( fread( &(curr->transMatrixInd) , sizeof(int) , 1 , inFD ) != 1 )
      error("HTKModels::readBinaryHMM - error reading transMatrixInd") ;

   nHMMs++ ;
}


void HTKModels::outputGMM( int ind , bool isRef , bool outputBinary )
{
   if ( (ind < 0) || (ind >= nGMMs) )
      error("HTKModels::outputGMM - ind %d out of range" , ind ) ;

   GMM *curr = gMMs + ind ;

   if ( outputBinary == false )
   {
      if ( isRef )
      {
         // Something we are outputting references this GMM.
         if ( curr->name != NULL )
         {
            // reference is to a shared state/GMM
            fprintf( outFD , "    ~s \"%s\"\n" , curr->name ) ;
         }
         else
         {
            outputMixture( curr->mixtureInd , curr->compWeights , true , false ) ;
         }
      }
      else
      {
         if ( curr->name == NULL )
            error("HTKModels::outputGMM - curr->name is NULL") ;
         fprintf( outFD , "~s \"%s\"\n" , curr->name ) ;
         outputMixture( curr->mixtureInd , curr->compWeights , true , false ) ;
      }
   }
   else
   {
      char id[5] ;
      strcpy( id , "JMGM" ) ;

      // Write the ID
      fwrite( id , sizeof(int) , 1 , outFD ) ;

      // Write the length of the name
      int len=0 ;
      if ( curr->name != NULL )
      {
         len = strlen( curr->name ) + 1 ;
      }
      fwrite( &len , sizeof(int) , 1 , outFD ) ;

      if ( len > 0 )
      {
         // Write the name
         fwrite( curr->name , sizeof(char) , len , outFD ) ;
      }

      // Write the mixture index
      fwrite( &(curr->mixtureInd) , sizeof(int) , 1 , outFD ) ;

      // Write the number of component weights
      fwrite( &(mixtures[curr->mixtureInd].nComps) , sizeof(int) , 1 , outFD ) ;

      // Write the component weights and log weights
      fwrite( curr->compWeights , sizeof(real) , mixtures[curr->mixtureInd].nComps , outFD ) ;
      fwrite( curr->logCompWeights , sizeof(real) , mixtures[curr->mixtureInd].nComps , outFD ) ;
   }   
}


void HTKModels::readBinaryGMM()
{
   char id[5] ;

   // Read and check the ID
   if ( fread( id , sizeof(int) , 1 , inFD ) != 1 )
      error("HTKModels::readBinaryGMM - error reading ID") ;
   id[4] = '\0' ;
   if ( strcmp( id , "JMGM" ) != 0 )
      error("HTKModels::readBinaryGMM - invalid ID = %s" , id ) ;

   // Make sure we are not about to exceed the pre-allocated memory
   if ( nGMMs == nGMMsAlloc )
      error("HTKModels::readBinaryGMM - nGMMs == nGMMsAlloc") ;
   
   GMM *curr = gMMs + nGMMs ;

   // Read the length of the name
   int len ;
   if ( fread( &len , sizeof(int) , 1 , inFD ) != 1 )
      error("HTKModels::readBinaryGMM - error reading name length") ;

   // Allocate space for the name if required and read name from file
   if ( len > 0 )
   {
      curr->name = new char[len] ;
      if ( (int)fread( curr->name , sizeof(char) , len , inFD ) != len )
         error("HTKModels::readBinaryGMM - error reading name") ;
   }
   else
   {
      curr->name = NULL ;
   }

   // Read the mixture index
   if ( fread( &(curr->mixtureInd) , sizeof(int) , 1 , inFD ) != 1 )
      error("HTKModels::readBinaryGMM - error reading mixtureInd") ;

   // Read the number of components
   int tmpNComps ;
   if ( fread( &tmpNComps , sizeof(int) , 1 , inFD ) != 1 )
      error("HTKModels::readBinaryGMM - error reading num components") ;

   // Allocate memory for the component weights and log component weights
   curr->compWeights = new real[tmpNComps * 2] ;
   curr->logCompWeights = curr->compWeights + tmpNComps ;

   // Read the compWeights + logCompWeights
   if ( (int)fread( curr->compWeights , sizeof(real) , tmpNComps*2 , inFD ) != (tmpNComps*2) )
      error("HTKModels::readBinaryGMM - error reading compWeights+nCompWeights") ;

   nGMMs++ ;
}


void HTKModels::outputMixture( int mixInd , real *compWeights , bool isRef , bool outputBinary )
{
   if ( (mixInd < 0) || (mixInd >= nMixtures) )
      error("HTKModels::outputMixture - mixInd out of range") ;
   
   Mixture *mix = mixtures + mixInd ;

   if ( outputBinary == false )
   {
      if ( isRef )
      {
         if ( mix->nComps > 1 )
            fprintf( outFD , "    <NumMixes> %d\n" , mix->nComps ) ;

         if ( mix->name != NULL )
         {
            // Tied-mixture case
            fprintf( outFD , "    <TMix> %s" , mix->name ) ;
            for ( int i=0 ; i<mix->nComps ; i++ )
               fprintf( outFD , " %.4e" , compWeights[i] ) ;
            fprintf( outFD , "\n" ) ;
         }
         else
         {
            if ( mix->nComps == 1 )
            {
               outputMeanVec( mix->meanVecInds[0] , true , false ) ;
               outputVarVec( mix->varVecInds[0] , true , false ) ;
            }
            else
            {
               for ( int i=0 ; i<mix->nComps ; i++ )
               {
                  fprintf( outFD , "    <Mixture> %d %.4e\n" , i+1 , compWeights[i] ) ;
                  outputMeanVec( mix->meanVecInds[i] , true , false ) ;
                  outputVarVec( mix->varVecInds[i] , true , false ) ;
               }
            }
         }
      }
      else
      {
         if ( mix->name == NULL )
            error("HTKModels::outputMixture - mix->name == NULL") ;

         for ( int i=0 ; i<mix->nComps ; i++ )
         {
            fprintf( outFD , "~m \"%s%d\"\n" , mix->name , i+1 ) ;
            outputMeanVec( mix->meanVecInds[i] , true , false ) ;
            outputVarVec( mix->varVecInds[i] , true , false ) ;
         }
      }
   }
   else
   {
      char id[5] ;
      strcpy( id , "JMMX" ) ;

      // Write the ID
      fwrite( id , sizeof(int) , 1 , outFD ) ;

      // Write the length of the name
      int len=0 ;
      if ( mix->name != NULL )
      {
         len = strlen( mix->name ) + 1 ;
      }
      fwrite( &len , sizeof(int) , 1 , outFD ) ;

      if ( len > 0 )
      {
         // Write the name
         fwrite( mix->name , sizeof(char) , len , outFD ) ;
      }

      // Write the number of components
      fwrite( &(mix->nComps) , sizeof(int) , 1 , outFD ) ;

      // Write the mean and variance vector indices
      fwrite( mix->meanVecInds , sizeof(int) , mix->nComps , outFD ) ;
      fwrite( mix->varVecInds , sizeof(int) , mix->nComps , outFD ) ;
   }
}


void HTKModels::readBinaryMixture()
{
   char id[5] ;

   // Read and check the ID
   if ( fread( id , sizeof(int) , 1 , inFD ) != 1 )
      error("HTKModels::readBinaryMixture - error reading ID") ;
   id[4] = '\0' ;
   if ( strcmp( id , "JMMX" ) != 0 )
      error("HTKModels::readBinaryMixture - invalid ID = %s" , id ) ;

   // Make sure we are not about to exceed the pre-allocated memory
   if ( nMixtures == nMixturesAlloc )
      error("HTKModels::readBinaryMixture - nMixtures == nMixturesAlloc") ;

   Mixture *curr = mixtures + nMixtures ;

   // Read the length of the name
   int len ;
   if ( fread( &len , sizeof(int) , 1 , inFD ) != 1 )
      error("HTKModels::readBinaryMixture - error reading name length") ;

   // Allocate space for the name if required and read name from file
   if ( len > 0 )
   {
      curr->name = new char[len] ;
      if ( (int)fread( curr->name , sizeof(char) , len , inFD ) != len )
         error("HTKModels::readBinaryMixture - error reading name") ;
   }
   else
   {
      curr->name = NULL ;
   }

   // Read the number of components
   if ( fread( &(curr->nComps) , sizeof(int) , 1 , inFD ) != 1 )
      error("HTKModels::readBinaryMixture - error reading nComps") ;

   // Allocate memory for meanVecInds and varVecInds
   curr->meanVecInds = new int[curr->nComps * 2] ;
   curr->varVecInds = curr->meanVecInds + curr->nComps ;

   // Read meanVecInds and varVecInds
   if ( (int)fread( curr->meanVecInds , sizeof(int) , curr->nComps*2 , inFD ) != (curr->nComps*2) )
      error("HTKModels::readBinaryMixture - error reading meanVecInds+varVecInds") ;

   // Initialise the currCompOutputs stuff
   curr->currCompOutputsValid = false ;
   curr->currCompOutputs = new real[curr->nComps] ;
   for ( int i=0 ; i<curr->nComps ; i++ )
      curr->currCompOutputs[i] = LOG_ZERO ;

   nMixtures++ ;
}


void HTKModels::outputMeanVec( int ind , bool isRef , bool outputBinary )
{
   if ( (ind < 0) || (ind >= nMeanVecs) )
      error("HTKModels::outputMeanVec - ind out of range") ;

   MeanVec *mvec = meanVecs + ind ;

   if ( outputBinary == false )
   {
      if ( isRef )
      {
         if ( mvec->name != NULL )
            fprintf( outFD , "      ~u \"%s\"\n" , mvec->name ) ;
         else
         {
            fprintf( outFD , "      <Mean> %d\n       " , vecSize ) ;
            for ( int i=0 ; i<vecSize ; i++ )
               fprintf( outFD , " %.4e" , mvec->means[i] ) ;
            fprintf( outFD , "\n" ) ;
         }
      }
      else
      {
         if ( mvec->name == NULL )
            error("HTKModels::outputMeanVec - mvec->name is NULL") ;

         fprintf( outFD , "~u \"%s\"\n" , mvec->name ) ;
         fprintf( outFD , "  <Mean> %d\n   " , vecSize ) ;
         for ( int i=0 ; i<vecSize ; i++ )
            fprintf( outFD , " %.4e" , mvec->means[i] ) ;
         fprintf( outFD , "\n" ) ;
      }
   }
   else
   {
      char id[5] ;
      strcpy( id , "JMMN" ) ;

      // Write the ID
      fwrite( id , sizeof(int) , 1 , outFD ) ;

      // Write the length of the name
      int len=0 ;
      if ( mvec->name != NULL )
      {
         len = strlen( mvec->name ) + 1 ;
      }
      fwrite( &len , sizeof(int) , 1 , outFD ) ;

      if ( len > 0 )
      {
         // Write the name
         fwrite( mvec->name , sizeof(char) , len , outFD ) ;
      }

      // Write the mean values
      fwrite( mvec->means , sizeof(real) , vecSize , outFD ) ;
   }
}


void HTKModels::readBinaryMeanVec()
{
   char id[5] ;

   // Read and check the ID
   if ( fread( id , sizeof(int) , 1 , inFD ) != 1 )
      error("HTKModels::readBinaryMeanVec - error reading ID") ;
   id[4] = '\0' ;
   if ( strcmp( id , "JMMN" ) != 0 )
      error("HTKModels::readBinaryMeanVec - invalid ID = %s" , id ) ;

   // Make sure we are not about to exceed the pre-allocated memory
   if ( nMeanVecs == nMeanVecsAlloc )
      error("HTKModels::readBinaryMeanVec - nMeanVecs == nMeanVecsAlloc") ;
      
   MeanVec *curr = meanVecs + nMeanVecs ;

   // Read the length of the name
   int len ;
   if ( fread( &len , sizeof(int) , 1 , inFD ) != 1 )
      error("HTKModels::readBinaryMeanVec - error reading name length") ;

   // Allocate space for the name if required and read name from file
   if ( len > 0 )
   {
      curr->name = new char[len] ;
      if ( (int)fread( curr->name , sizeof(char) , len , inFD ) != len )
         error("HTKModels::readBinaryMeanVec - error reading name") ;
   }
   else
   {
      curr->name = NULL ;
   }

   // Allocate memory for the mean values
   curr->means = new real[vecSize] ;

   // Read the variance values and derivations
   if ( (int)fread( curr->means , sizeof(real) , vecSize , inFD ) != vecSize )
      error("HTKModels::readBinaryMeanVec - error reading means") ;

   nMeanVecs++ ;
}


void HTKModels::outputVarVec( int ind , bool isRef , bool outputBinary )
{
   if ( (ind < 0) || (ind >= nVarVecs) )
      error("HTKModels::outputVarVec - ind out of range") ;

   VarVec *vvec = varVecs + ind ;

   if ( outputBinary == false )
   {
      if ( isRef )
      {
         if ( vvec->name != NULL )
            fprintf( outFD , "      ~v \"%s\"\n" , vvec->name ) ;
         else
         {
            fprintf( outFD , "      <Variance> %d\n       " , vecSize ) ;
            for ( int i=0 ; i<vecSize ; i++ )
               fprintf( outFD , " %.4e" , vvec->vars[i] ) ;
            fprintf( outFD , "\n" ) ;
         }
      }
      else
      {
         if ( vvec->name == NULL )
            error("HTKModels::outputVarVec - mvec->name is NULL") ;

         fprintf( outFD , "~v \"%s\"\n" , vvec->name ) ;
         fprintf( outFD , "  <Variance> %d\n   " , vecSize ) ;
         for ( int i=0 ; i<vecSize ; i++ )
            fprintf( outFD , " %.4e" , vvec->vars[i] ) ;
         fprintf( outFD , "\n" ) ;
      }
   }
   else
   {
      char id[5] ;
      strcpy( id , "JMVR" ) ;

      // Write the ID
      fwrite( id , sizeof(int) , 1 , outFD ) ;

      // Write the length of the name
      int len=0 ;
      if ( vvec->name != NULL )
      {
         len = strlen( vvec->name ) + 1 ;
      }
      fwrite( &len , sizeof(int) , 1 , outFD ) ;

      if ( len > 0 )
      {
         // Write the name
         fwrite( vvec->name , sizeof(char) , len , outFD ) ;
      }

      // Write the variance values and derivations
      fwrite( vvec->vars , sizeof(real) , vecSize , outFD ) ;
      fwrite( vvec->minusHalfOverVars , sizeof(real) , vecSize , outFD ) ;
      fwrite( &(vvec->sumLogVarPlusNObsLog2Pi) , sizeof(real) , 1 , outFD ) ;
   }
}


void HTKModels::readBinaryVarVec()
{
   char id[5] ;

   // Read and check the ID
   if ( fread( id , sizeof(int) , 1 , inFD ) != 1 )
      error("HTKModels::readBinaryVarVec - error reading ID") ;
   id[4] = '\0' ;
   if ( strcmp( id , "JMVR" ) != 0 )
      error("HTKModels::readBinaryVarVec - invalid ID = %s" , id ) ;

   // Make sure we are not about to exceed the pre-allocated memory
   if ( nVarVecs == nVarVecsAlloc )
      error("HTKModels::readBinaryVarVec - nVarVecs == nVarVecsAlloc") ;

   VarVec *curr = varVecs + nVarVecs ;

   // Read the length of the name
   int len ;
   if ( fread( &len , sizeof(int) , 1 , inFD ) != 1 )
      error("HTKModels::readBinaryVarVec - error reading name length") ;

   // Allocate space for the name if required and read name from file
   if ( len > 0 )
   {
      curr->name = new char[len] ;
      if ( (int)fread( curr->name , sizeof(char) , len , inFD ) != len )
         error("HTKModels::readBinaryVarVec - error reading name") ;
   }
   else
   {
      curr->name = NULL ;
   }

   // Allocate memory for the variance values and derivations
   curr->vars = new real[vecSize * 2] ;
   curr->minusHalfOverVars = curr->vars + vecSize ;

   // Read the variance values and derivations
   if ( (int)fread( curr->vars , sizeof(real) , vecSize*2 , inFD ) != (vecSize*2))
       error("HTKModels::readBinaryVarVec - error reading vars+minusHalfOverVars") ;

   if ( fread( &(curr->sumLogVarPlusNObsLog2Pi) , sizeof(real) , 1 , inFD ) != 1 )
       error("HTKModels::readBinaryVarVec - error reading sumLogVarPlusNObsLog2Pi") ;
   
   nVarVecs++ ;
}


void HTKModels::outputTransMat( int ind , bool isRef , bool outputBinary )
{
   if ( (ind < 0) || (ind >= nTransMats) )
      error("HTKModels::outputTransMat - ind out of range") ;

   TransMatrix *mat = transMats + ind ;

   if ( outputBinary == false )
   {
      if ( isRef )
      {
         if ( mat->name != NULL )
         {
            fprintf( outFD , "  ~t \"%s\"\n" , mat->name ) ;
         }
         else
         {
            fprintf( outFD , "  <TransP> %d\n" , mat->nStates ) ;
            int ind=0 ;
            for ( int i=0 ; i<mat->nStates ; i++ )
            {
               fprintf( outFD , "   " ) ;
               ind = 0 ;
               for ( int j=0 ; j<mat->nStates ; j++ )
               {
                  if ( (ind < mat->nSucs[i]) && (j == mat->sucs[i][ind]) )
                  {
                     fprintf( outFD , " %.4e" , mat->probs[i][ind] ) ;
                     //fprintf( outFD, " %.4e(%.3f)", mat->probs[i][ind], mat->logProbs[i][ind] ) ;
                     ind++ ;
                  }
                  else
                  {
                     fprintf( outFD , " %.4e" , 0.0 ) ;
                     //fprintf( outFD , " %.4e(%.3f)" , 0.0 , 0.0 ) ;
                  }
               }
               fprintf( outFD , "\n" ) ;
            }
         }
      }
      else
      {
         if ( mat->name == NULL )
            error("HTKModels::outputTransMat - mat->name is NULL") ;

         fprintf( outFD , "~t \"%s\"\n" , mat->name ) ;
         fprintf( outFD , "<TransP> %d\n" , mat->nStates ) ;
         int ind=0 ;
         for ( int i=0 ; i<mat->nStates ; i++ )
         {
            fprintf( outFD , "   " ) ;
            ind = 0 ;
            for ( int j=0 ; j<mat->nStates ; j++ )
            {
               if ( (ind < mat->nSucs[i]) && (j == mat->sucs[i][ind]) )
               {
                  fprintf( outFD , " %.4e" , mat->probs[i][ind] ) ;
                  //fprintf( outFD , " %.4e(%.3f)" , mat->probs[i][ind] , mat->logProbs[i][ind] ) ;
                  ind++ ;
               }
               else
               {
                  fprintf( outFD , " %.4e" , 0.0 ) ;
                  //fprintf( outFD , " %.4e(%.3f)" , 0.0 , 0.0 ) ;
               }
            }
            fprintf( outFD , "\n" ) ;
         }
      }
   }
   else
   {
      char id[5] ;
      strcpy( id , "JMTM" ) ;

      // Write the ID
      fwrite( id , sizeof(int) , 1 , outFD ) ;

      // Write the length of the name
      int len=0 ;
      if ( mat->name != NULL )
      {
         len = strlen( mat->name ) + 1 ;
      }
      fwrite( &len , sizeof(int) , 1 , outFD ) ;

      if ( len > 0 )
      {
         // Write the name
         fwrite( mat->name , sizeof(char) , len , outFD ) ;
      }

      // Write the number of states
      fwrite( &(mat->nStates) , sizeof(int) , 1 , outFD ) ;

      // Write the number of sucs array
      fwrite( mat->nSucs , sizeof(int) , mat->nStates , outFD ) ;

      // Write the sucs arrays for all states
      int i ;
      for ( i=0 ; i<mat->nStates ; i++ )
      {
         if ( mat->nSucs[i] > 0 )
         {
            fwrite( mat->sucs[i] , sizeof(int) , mat->nSucs[i] , outFD ) ;
         }
      }

      // Write the probs arrays for all states
      for ( i=0 ; i<mat->nStates ; i++ )
      {
         if ( mat->nSucs[i] > 0 )
         {
            fwrite( mat->probs[i] , sizeof(real) , mat->nSucs[i] , outFD ) ;
         }
      }
      
      // Write the logProbs arrays for all states
      for ( i=0 ; i<mat->nStates ; i++ )
      {
         if ( mat->nSucs[i] > 0 )
         {
            fwrite( mat->logProbs[i] , sizeof(real) , mat->nSucs[i] , outFD ) ;
         }
      }
   }
}


void HTKModels::readBinaryTransMat()
{
   char id[5] ;

   // Read and check the ID
   if ( fread( id , sizeof(int) , 1 , inFD ) != 1 )
      error("HTKModels::readBinaryTransMat - error reading ID") ;
   id[4] = '\0' ;
   if ( strcmp( id , "JMTM" ) != 0 )
      error("HTKModels::readBinaryTransMat - invalid ID = %s" , id ) ;

   // Make sure we are not about to exceed the pre-allocated memory
   if ( nTransMats == nTransMatsAlloc )
      error("HTKModels::readBinaryTransMat - nTransMats == nTransMatsAlloc") ;

   TransMatrix *curr = transMats + nTransMats ;

   // Read the length of the name
   int len ;
   if ( fread( &len , sizeof(int) , 1 , inFD ) != 1 )
      error("HTKModels::readBinaryTransMat - error reading name length") ;

   // Allocate space for the name if required and read name from file
   if ( len > 0 )
   {
      curr->name = new char[len] ;
      if ( (int)fread( curr->name , sizeof(char) , len , inFD ) != len )
         error("HTKModels::readBinaryTransMat - error reading name") ;
   }
   else
   {
      curr->name = NULL ;
   }
   
   // Read the number of states
   if ( fread( &(curr->nStates) , sizeof(int) , 1 , inFD ) != 1 )
      error("HTKModels::readBinaryTransMat - error reading nStates") ;
   
   // Allocate memory for suc info for all states
   curr->nSucs = new int[curr->nStates] ;
   curr->sucs = new int*[curr->nStates] ;
   curr->probs = new real*[(curr->nStates)*2] ;
   curr->logProbs = curr->probs + curr->nStates ;

   // Read the nSucs array
   if ( (int)fread( curr->nSucs , sizeof(int) , curr->nStates , inFD ) != curr->nStates )
      error("HTKModels::readBinaryTransMat - error reading nSucs array") ;

   // Determine the total number of transitions
   int i , totalTrans=0 ;
   for ( i=0 ; i<curr->nStates ; i++ )
   {
      totalTrans += curr->nSucs[i] ;
   }

   // Allocate and configure memory for all suc info
   curr->sucs[0] = new int[totalTrans] ;
   curr->probs[0] = new real[totalTrans * 2] ;
   curr->logProbs[0] = curr->probs[0] + totalTrans ;

   int ind=curr->nSucs[0] ;
   for ( i=1 ; i<curr->nStates ; i++ )
   {
      if ( curr->nSucs[i] > 0 )
      {
         curr->sucs[i] = curr->sucs[0] + ind ;
         curr->probs[i] = curr->probs[0] + ind ;
         curr->logProbs[i] = curr->logProbs[0] + ind ;
         ind += curr->nSucs[i] ;
      }
      else
      {
         curr->sucs[i] = NULL ;
         curr->probs[i] = NULL ;
         curr->logProbs[i] = NULL ;
      }
   }
   
   // Read all sucs for all states at once
   if ( (int)fread( curr->sucs[0] , sizeof(int) , totalTrans , inFD ) != totalTrans )
      error("HTKModels::readBinaryTransMat - error reading sucs") ;
      
   // Read all probs and logProbs for all states at once
   if ( (int)fread( curr->probs[0] , sizeof(real) , totalTrans*2 , inFD ) != (totalTrans*2) )
      error("HTKModels::readBinaryTransMat - error reading probs+logProbs") ;

   nTransMats++ ;
}


//PNG inline
real HTKModels::calcGMMOutput( int gmmInd )
{
   if ( currGMMOutputs[gmmInd] <= LOG_ZERO )
   {
      currGMMOutputs[gmmInd] = calcMixtureOutput( gMMs[gmmInd].mixtureInd , 
                                                  gMMs[gmmInd].logCompWeights ) ;
   }
   return currGMMOutputs[gmmInd] ;
}


//PNG inline
real HTKModels::calcMixtureOutput( int mixInd , const real *logCompWeights )
{
#ifdef DEBUG
   if ( (mixInd < 0) || (mixInd >= nMixtures) )
      error("HTKModels::calcMixtureOutput - mixInd out of range") ;
   if ( logCompWeights == NULL )
      error("HTKModels::calcMixtureOutput - logCompWeights is NULL") ;
#endif

   Mixture *mix = mixtures + mixInd ;
   int i ;

   if ( mix->currCompOutputsValid == false )
   {
      real *means , *mhovs , xmu , sumxmu ;
      const real *x ;
      int j ;
      for ( i=0 ; i<mix->nComps ; i++ )
      {
         // calculate the mixture output
         means = meanVecs[mix->meanVecInds[i]].means ;
         mhovs = varVecs[mix->varVecInds[i]].minusHalfOverVars ;
         x = currInput ;

         sumxmu = 0.0 ;
         for ( j=0 ; j<vecSize ; j++ )
         {
            xmu = ( *(x++) - *(means++) ) ;
            sumxmu += xmu * xmu * *(mhovs++) ;
         }
         sumxmu += varVecs[mix->varVecInds[i]].sumLogVarPlusNObsLog2Pi ;
         mix->currCompOutputs[i] = sumxmu ;
      }

      mix->currCompOutputsValid = true ;
   }

   // weight the mixture output and add to the output value.
   real logProb = LOG_ZERO ;
   for ( i=0 ; i<mix->nComps ; i++ )
   {
      logProb = logAdd( logProb , mix->currCompOutputs[i] + logCompWeights[i] ) ;
   }

	return logProb ;
}


void HTKModels::outputStats( FILE *fd )
{
   fprintf( fd , "vecSize=%d " , vecSize ) ;
   fprintf( fd , "nMeanVecs=%d nVarVecs=%d nTransMats=%d nMixtures=%d nGMMs=%d nHMMs=%d\n" ,
                  nMeanVecs , nVarVecs , nTransMats , nMixtures , nGMMs , nHMMs ) ;

   real totalF=0.0 ;
   int i , j , k , totalI=0 ;

   // Calculate meanVecs checksum
   for ( i=0 ; i<nMeanVecs ; i++ )
   {
      for ( j=0 ; j<vecSize ; j++ )
         totalF += meanVecs[i].means[j] ;
   }
   fprintf( fd , "  means checksum=%f\n" , totalF ) ; fflush( fd ) ;
    
   // Calculate varVecs checksum
   totalF = 0.0 ;
   for ( i=0 ; i<nVarVecs ; i++ )
   {
      for ( j=0 ; j<vecSize ; j++ )
      {
         totalF += varVecs[i].vars[j] ;
         totalF += varVecs[i].minusHalfOverVars[j] ;
      }
      totalF += varVecs[i].sumLogVarPlusNObsLog2Pi ;
   }
   fprintf( fd , "  vars checksum=%f\n" , totalF ) ; fflush( fd ) ;

   // Calculate transMatrix checksum
   totalF = 0.0 ; totalI = 0 ;
   for ( i=0 ; i<nTransMats ; i++ )
   {
      for ( j=0 ; j<transMats[i].nStates ; j++ )
      {
         totalI += transMats[i].nSucs[j] ;
         for ( k=0 ; k<transMats[i].nSucs[j] ; k++ )
         {
            totalI += transMats[i].sucs[j][k] ;
            totalF += transMats[i].probs[j][k] ;
            totalF += transMats[i].logProbs[j][k] ;
         }
      }
   }
   fprintf( fd , "  transMats checksum=%f %d\n" , totalF , totalI ) ; fflush( fd ) ;

   // Calculate mixtures checksum
   totalI = 0 ;
   for ( i=0 ; i<nMixtures ; i++ )
   {
      for ( j=0 ; j<mixtures[i].nComps ; j++ )
      {
         totalI += mixtures[i].meanVecInds[j] ;
         totalI += mixtures[i].varVecInds[j] ;
      }
   }
   fprintf( fd , "  mixtures checksum=%d\n" , totalI ) ; fflush( fd ) ;

   // Calculate gMMs checksum
   totalF = 0.0 ; totalI = 0 ;
   for ( i=0 ; i<nGMMs ; i++ )
   {
      totalI += gMMs[i].mixtureInd ;
      for ( j=0 ; j<mixtures[gMMs[i].mixtureInd].nComps ; j++ )
      {
         totalF += gMMs[i].compWeights[j] ;
         totalF += gMMs[i].logCompWeights[j] ;
      }
   }
   fprintf( fd , "  gMMs checksum=%f %d\n" , totalF , totalI ) ; fflush( fd ) ;

   // Calculate HMMs checksum
   totalI = 0 ;
   for ( i=0 ; i<nHMMs ; i++ )
   {
      totalI += hMMs[i].nStates ;
      totalI += hMMs[i].transMatrixInd ;
      for ( j=0 ; j<(hMMs[i].nStates-2) ; j++ )
      {
         totalI += hMMs[i].gmmInds[j] ;
      }
   }
   fprintf( fd , "  hMMs checksum=%d\n" , totalI ) ; fflush( fd ) ;
}


void testModelsIO( const char *htkModelsFName , const char *phonesListFName , 
                   const char *priorsFName , int statesPerModel )
{
   bool hybrid=false ;
   if ( (htkModelsFName != NULL) && (htkModelsFName[0] != '\0') )
      hybrid = false ;
   else if ( (phonesListFName != NULL) && (phonesListFName[0] != '\0') )
      hybrid = true ;
   else
      error("testModelsIO - input undefined") ;
   
   if ( hybrid )
   {
      HTKModels *m1 = new HTKModels();
      m1->Load( phonesListFName , priorsFName , statesPerModel ) ;
      printf("m1: ") ; m1->outputStats() ;
      m1->output( "m1.bin" , true ) ;
      delete m1 ;
      
      HTKModels *m2 = new HTKModels() ;
      m2->readBinary( "m1.bin" ) ;
      printf("m2: ") ; m2->outputStats() ;
      m2->output( "m2.bin" , true ) ;
      delete m2 ;
      
      HTKModels *m3 = new HTKModels() ;
      m3->readBinary( "m2.bin" ) ;
      printf("m3: ") ; m3->outputStats() ;
      m3->output( "m3.bin" , true ) ;
      delete m3 ;
      
      HTKModels *m4 = new HTKModels() ;
      m4->readBinary( "m3.bin" ) ;
      printf("m4: ") ; m4->outputStats() ;
      m4->output( "m4.bin" , true ) ;
      delete m4 ;
   }
   else
   {
      HTKModels *m1 = new HTKModels();
      m1->Load( htkModelsFName , true ) ;
   
      printf("m1: ") ; m1->outputStats() ;
      m1->output( "m1.txt" , false ) ;
      m1->output( "m1.bin" , true ) ;
      delete m1 ;

      HTKModels *m2txt = new HTKModels();
      m2txt->Load( "m1.txt" , true ) ;
      printf("m2txt: ") ; m2txt->outputStats() ;
      m2txt->output( "m2txt.txt" , false ) ;
      m2txt->output( "m2txt.bin" , true ) ;
      delete m2txt ;

      HTKModels *m2bin = new HTKModels() ;
      m2bin->readBinary( "m1.bin" ) ;
      printf("m2bin: ") ; m2bin->outputStats() ;
      m2bin->output( "m2bin.txt" , false ) ;
      m2bin->output( "m2bin.bin" , true ) ;
      delete m2bin ;

      HTKModels *m3txttxt = new HTKModels();
      m3txttxt->Load( "m2txt.txt" , true ) ;
      printf("m3txttxt: ") ; m3txttxt->outputStats() ;
      m3txttxt->output( "m3txttxt.txt" , false ) ;
      m3txttxt->output( "m3txttxt.bin" , true ) ;
      delete m3txttxt ;
      HTKModels *m3bintxt = new HTKModels();
      m3bintxt->Load( "m2bin.txt" , true ) ;
      printf("m3bintxt: ") ; m3bintxt->outputStats() ;
      m3bintxt->output( "m3bintxt.txt" , false ) ;
      m3bintxt->output( "m3bintxt.bin" , true ) ;
      delete m3bintxt ;

      HTKModels *m3txtbin = new HTKModels() ;
      m3txtbin->readBinary( "m2txt.bin" ) ;
      printf("m3txtbin: ") ; m3txtbin->outputStats() ;
      m3txtbin->output( "m3txtbin.txt" , false ) ;
      m3txtbin->output( "m3txtbin.bin" , true ) ;
      delete m3txtbin ;

      HTKModels *m3binbin = new HTKModels() ;
      m3binbin->readBinary( "m2bin.bin" ) ;
      printf("m3binbin: ") ; m3binbin->outputStats() ;
      m3binbin->output( "m3binbin.txt" , false ) ;
      m3binbin->output( "m3binbin.bin" , true ) ;
      delete m3binbin ;
   }
}


}
