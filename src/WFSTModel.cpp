/*
 * Copyright 2004 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#include <assert.h>

#include "WFSTModel.h"
#include "log_add.h"


/*
	Author:		Darren Moore (moore@idiap.ch)
	Date:			14 October 2004
*/

/*
	Author:		Octavian Cheng (ocheng@idiap.ch)
	Date:		7 June 2006
*/

using namespace Torch;

namespace Juicer {

// Changes
WFSTModelPool::WFSTModelPool()
{
//PNG   phoneModels = NULL ;
   models = NULL ;
   decHypHistPool = NULL ;
   maxNStates = 0 ;
   nUsed = 0 ;
   pools = NULL ;
}

//PNG
#if 0
WFSTModelPool::WFSTModelPool( PhoneModels *phoneModels_ , DecHypHistPool *decHypHistPool_ )
{
	int i ;
	
	if ( (phoneModels = phoneModels_) == NULL )
		error("WFSTModelPool::WFSTModelPool - phoneModels_ is NULL") ;
   models = NULL ;
	if ( (decHypHistPool = decHypHistPool_) == NULL )
		error("WFSTModelPool::WFSTModelPool - decHypHistPool_ is NULL") ;

	// Determine the maximum number of states over all models in phoneModels.
	maxNStates = 0 ;
   nUsed = 0 ;
	for ( i=0 ; i<phoneModels->nModels ; i++ )
	{
		if ( phoneModels->models[i]->n_states > maxNStates )
			maxNStates = phoneModels->models[i]->n_states ;
	}
	
	pools = new WFSTModelFNSPool[maxNStates] ;
	for ( i=0 ; i<maxNStates ; i++ )
		initFNSPool( i , i+1 , 100 ) ;
}
#endif

WFSTModelPool::WFSTModelPool(
    Models *models_ , DecHypHistPool *decHypHistPool_
)
{
    if ( (models = models_) == NULL )
        error("WFSTModelPool::WFSTModelPool(2) - models_ is NULL") ;
    if ( (decHypHistPool = decHypHistPool_) == NULL )
        error("WFSTModelPool::WFSTModelPool(2) - decHypHistPool_ is NULL") ;

    // Determine the maximum number of states over all models in phoneModels.
    maxNStates = 0 ;
    nUsed = 0 ;
    //DecodingHMM *dh ;
    int i ;
    for ( i=0 ; i<models->getNumHMMs() ; i++ )
    {
        //dh = models->getDecHMM( i ) ;
        //if ( dh->n_states > maxNStates )
        //    maxNStates = dh->n_states ;
        int n = models->getNumStates(i);
        if (n > maxNStates)
            maxNStates = n;
    }

    pools = new WFSTModelFNSPool[maxNStates] ;
    for ( i=0 ; i<maxNStates ; i++ )
        initFNSPool( i , i+1 , 100 ) ;
}


WFSTModelPool::~WFSTModelPool()
{
	int i , j ;

	for ( i=0 ; i<maxNStates ; i++ )
	{
	   // Changes
	   if ( pools != NULL )  {
		if ( pools[i].allocs != NULL )
		{
			for ( j=0 ; j<pools[i].nAllocs ; j++ )
			{
				delete [] pools[i].allocs[j][0].hyps1 ;
				delete [] pools[i].allocs[j] ;
			}
			free( pools[i].allocs ) ;
		}

		if ( pools[i].freeElems != NULL )
			free( pools[i].freeElems ) ;
	   }
	}
	delete [] pools ;
}


void WFSTModelPool::initFNSPool( int poolInd , int nStates , int initReallocAmount )
{
	if ( (poolInd < 0) || (poolInd >= maxNStates) )
		error("WFSTModelPool::initFNSPool - poolInd out of range") ;
	if ( initReallocAmount <= 0 )
		error("WFSTModelPool::initFNSPool - initReallocAmount <= 0") ;
	
	pools[poolInd].nStates = nStates ;
	pools[poolInd].nTotal = 0 ;
	pools[poolInd].nUsed = 0 ;
	pools[poolInd].nFree = 0 ;
	pools[poolInd].reallocAmount = initReallocAmount ;
	pools[poolInd].nAllocs = 0 ;
	pools[poolInd].allocs = NULL ;
	pools[poolInd].freeElems = NULL ;

	allocPool( poolInd ) ;
}


void WFSTModelPool::allocPool( int poolInd )
{
	WFSTModelFNSPool *pool ;
	WFSTModel *tmp ;
	int i ;

	pool = pools + poolInd ;

#ifdef DEBUG
	// Check to see if we really need to allocate more entries
	if ( pool->nFree > 0 )
		return ;
	if ( pool->nFree < 0 )
		error("WFSTModelPool::allocPool - nFree < 0") ;
#endif
	
	pool->freeElems = (WFSTModel **)realloc(
        pool->freeElems ,
        (pool->nTotal+pool->reallocAmount)*sizeof(WFSTModel *)
    ) ;
	pool->allocs = (WFSTModel **)realloc(
        pool->allocs , (pool->nAllocs+1)*sizeof(WFSTModel *)
    );
	pool->allocs[pool->nAllocs] = new WFSTModel[pool->reallocAmount] ;
    //printf("new WFSTModel[%d]\n", pool->reallocAmount);

	// Allocate the currHyps and prevHyps fields of the new WFSTModel
	// elements in one big block.
	tmp = pool->allocs[pool->nAllocs] ;
#ifdef LOCAL_HYPS
	tmp[0].hyps1 = new DecHyp[ pool->reallocAmount * pool->nStates ] ;
#else
	tmp[0].hyps1 = new DecHyp[ pool->reallocAmount * pool->nStates * 2 ] ;
    //printf("new DecHyp[%d] %d\n", pool->reallocAmount * pool->nStates * 2, pool->nStates);
	tmp[0].hyps2 = tmp[0].hyps1 + pool->nStates ;
#endif
	initElem( tmp , pool->nStates ) ;
	pool->freeElems[0] = tmp ;
    //pool->freeElems[pool->reallocAmount-1] = tmp ; // Allocate from beginning
	for ( i=1 ; i<pool->reallocAmount ; i++ )
	{
#ifdef LOCAL_HYPS
		tmp[i].hyps1 = tmp[i-1].hyps1 + pool->nStates ;
#else
		tmp[i].hyps1 = tmp[i-1].hyps2 + pool->nStates ;
		tmp[i].hyps2 = tmp[i].hyps1 + pool->nStates ;
#endif
		initElem( tmp + i , pool->nStates ) ;
		pool->freeElems[i] = tmp + i ;
        //pool->freeElems[pool->reallocAmount-1-i] = tmp + i ; // Allocate from beginning
	}

	pool->nTotal += pool->reallocAmount ;
	pool->nFree += pool->reallocAmount ;
	pool->nAllocs++ ;

	// change the realloc amount to be equal to the new total size.
	// ie. exponentially increasing allocation.
	pool->reallocAmount = pool->nTotal ;
}

	
WFSTModel *WFSTModelPool::getElem( WFSTTransition *trans )
{
//	DecodingHMM *hmm ;
	WFSTModelFNSPool *pool ;
	WFSTModel *tmp ;

    if ( trans->inLabel == 0 )
        error("WFSTModelPool::getElem - trans->inLabel = 0 (i.e. epsilon)") ;
   
    int index = trans->inLabel - 1 ;
    int nStates = models->getNumStates(index);
//	hmm = getDecHMM( index ) ;
//	pool = pools + hmm->n_states - 1 ;
    pool = pools + nStates - 1 ;
	
	if ( pool->nFree == 0 )
        //allocPool( hmm->n_states - 1 ) ;
        allocPool( nStates - 1 ) ;
		
	pool->nFree-- ;
	pool->nUsed++ ;

#ifdef DEBUG
	if ( (pool->nFree < 0) || (pool->nUsed > pool->nTotal) )
		error("WFSTModelPool::getElem - nFree or nUsed invalid") ;
#endif
	
	tmp = pool->freeElems[pool->nFree] ;
	pool->freeElems[pool->nFree] = NULL ;

	// Configure the new element.
	tmp->trans = trans ;
    //tmp->hmm = hmm ;
    tmp->hmmIndex = index;
	tmp->nActiveHyps = 0 ;
	tmp->next = NULL ;

    // Update the global nUsed counter.
    ++nUsed ;

	return tmp ;
}


void WFSTModelPool::returnElem( WFSTModel *elem )
{
	WFSTModelFNSPool *pool ;
	int nStates ;

    //nStates = elem->hmm->n_states ;
    nStates = models->getNumStates(elem->hmmIndex);

    // Phil - This is a big cache miss during decoding as all but the
    // last hyp will be out of cache.  If DecHyps need to be reset, do
    // it before they are used first.
    if (elem->nActiveHyps)
        resetElem( elem , nStates ) ;
	pool = pools + nStates - 1 ;
	pool->freeElems[pool->nFree] = elem ;
	pool->nFree++ ;
	pool->nUsed-- ;

#ifdef DEBUG
	if ( (pool->nFree > pool->nTotal) || (pool->nUsed < 0) )
		error("WFSTModelPool::returnElem - nFree or nUsed invalid") ;
#endif

   // Update the global nUsed counter.
   --nUsed ;
}


void WFSTModelPool::initElem( WFSTModel *elem , int nStates )
{
	int i ;

	elem->trans = NULL ;
//	elem->hmm = NULL ;

	elem->currHyps = elem->hyps1 ;
#ifndef LOCAL_HYPS
	elem->prevHyps = elem->hyps2 ;
#endif
	for ( i=0 ; i<nStates ; i++ )
	{
		DecHypHistPool::initDecHyp( elem->currHyps + i , i ) ;
#ifndef LOCAL_HYPS
		DecHypHistPool::initDecHyp( elem->prevHyps + i , i ) ;
#endif
	}
	elem->nActiveHyps = 0 ;
	elem->next = NULL ;
#ifdef MIRROR_SCORE0
    elem->score0 = LOG_ZERO;
#endif
}


void WFSTModelPool::resetElem( WFSTModel *elem , int nStates )
{
	int i ;
	
    elem->trans = NULL ;
//	elem->hmm = NULL ;
	for ( i=0 ; i<nStates ; i++ )
	{
		decHypHistPool->resetDecHyp( elem->currHyps + i ) ;
#ifndef LOCAL_HYPS
		decHypHistPool->resetDecHyp( elem->prevHyps + i ) ;
#endif
	}
	elem->nActiveHyps = 0 ;
	elem->next = NULL ;
#ifdef MIRROR_SCORE0
    elem->score0 = LOG_ZERO;
#endif
}


void WFSTModelPool::checkActive( WFSTModel *elem )
{
	int i ;

    int nStates = models->getNumStates(elem->hmmIndex);
    //for ( i=0 ; i<elem->hmm->n_states ; i++ )
    for ( i=0 ; i<nStates ; i++ )
	{
		if ( (elem->currHyps[i].score > LOG_ZERO) ||
             (elem->currHyps[i].hist != NULL) )
			error("WFSTModel::checkActive - elem->currHyps[%d] is ACTIVE",i) ;
#ifndef LOCAL_HYPS
		if ( (elem->prevHyps[i].score > LOG_ZERO) ||
             (elem->prevHyps[i].hist != NULL) )
			error("WFSTModel::checkActive - elem->prevHyps[%d] is ACTIVE",i) ;
#endif
	}
}


#if 0
DecodingHMM *WFSTModelPool::getDecHMM( int ind )
{
   DecodingHMM *dh ;

   if ( ind < 0 )
      error("WFSTModelPool::getDecHMM - ind < 0") ;

//PNG   if ( phoneModels == NULL )
//PNG   {
      if ( ind >= models->getNumHMMs() )
         error("WFSTModelPool::getDecHMM - ind >= models->getNumHMMs()") ;
      dh = models->getDecHMM( ind ) ;
//PNG   }
 //PNG  else
 //PNG  {
//PNG      if ( ind >= phoneModels->nModels )
//PNG         error("WFSTModelPool::getDecHMM - ind >= phoneModels->nModels") ;
//PNG      dh = phoneModels->models[ind] ;
//PNG   }

   return dh ;
}
#endif

}
