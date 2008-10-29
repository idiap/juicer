/*
 * Copyright 2005 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#include <cassert>
#include "DecHypHistPool.h"
#include "log_add.h"

/*
	Author:	Darren Moore (moore@idiap.ch)
	$Id: DecHypHistPool.cpp,v 1.8.4.2 2006/09/06 18:47:12 juicer Exp $
*/

/*
	Author:	Octavian Cheng (ocheng@idiap.ch)
	Date:	7 June 2006
*/

using namespace Torch;

namespace Juicer {

// Changes by Octavian
// DecHypPool class implementation
// Changes Octavian 20060627
#if 0
DecHypPool::DecHypPool()
{
   reallocAmount = 0 ;
   isOnTheFly = false ;
   decHypPool = NULL ;
}

DecHypPool::DecHypPool( int reallocAmount_, bool isOnTheFly_ )
{
   reallocAmount = reallocAmount_ ;
   isOnTheFly = isOnTheFly_ ;
   
   if ( isOnTheFly )  {
      decHypPool = new BlockMemPool( sizeof(DecHypOnTheFly), reallocAmount ) ;
   }
   else  {
      decHypPool = new BlockMemPool( sizeof(DecHyp), reallocAmount ) ;
   }
}


DecHypPool::~DecHypPool()
{
   delete decHypPool ;
}


void *DecHypPool::getSingleHyp()
{
   void *tmp = decHypPool->getElem() ;
   if ( isOnTheFly )
      DecHypHistPool::initDecHypOnTheFly( (DecHypOnTheFly *) tmp, -1, -1 ) ;
   else
      DecHypHistPool::initDecHyp( (DecHyp *) tmp, -1 ) ;

   return tmp ;
}


void DecHypPool::returnSingleHyp( void *hyp )
{
   if ( DecHypHistPool::isActiveHyp( (DecHyp *)hyp ) == true )
      error("DecHypPool::returnSignleHyp - the return hyp is active") ;
   decHypPool->returnElem( hyp ) ;
   return ;
}
#endif

//****************************************

DecHypHistPool::DecHypHistPool( int reallocAmount_ )
{
   reallocAmount = reallocAmount_ ;
   
   dhhPool = new BlockMemPool( sizeof(DecHypHist) , reallocAmount ) ;
   labPool = new BlockMemPool( sizeof(LabDecHypHist) , reallocAmount ) ;
#if 0
   labTimePool =
       new BlockMemPool( sizeof(LabTimeDecHypHist) , reallocAmount ) ;
   labTimeScorePool =
       new BlockMemPool( sizeof(LabTimeScoreDecHypHist) , reallocAmount ) ;
   labTime2ScorePool =
       new BlockMemPool( sizeof(LabTime2ScoreDecHypHist) , reallocAmount ) ;
#endif
   latticePool =
       new BlockMemPool( sizeof(LatticeDecHypHist) , reallocAmount ) ;

   latticeMode = false ;
   lattice = NULL ;
}


// PNG
#if 0
DecHypHistPool::DecHypHistPool( int max_size , bool stateHistMode_ , int realloc_amount_ )
{
   error("DecHypHistPool::DecHypHistPool(2) - no longer supported") ;
}
#endif

DecHypHistPool::~DecHypHistPool()
{
   delete dhhPool ;
   delete labPool ;
#if 0
   delete labTimePool ;
   delete labTimeScorePool ;
   delete labTime2ScorePool ;
#endif
   delete latticePool ;
}


void DecHypHistPool::returnElem( DecHypHist *elem )
{
    switch ( elem->type )
    {
    case DHHTYPE:
        if ( elem->prev != NULL )
        {
            if ( --(elem->prev->nConnect) <= 0 )
                returnElem( elem->prev ) ;
            elem->prev = NULL ;
        }
        dhhPool->returnElem( elem ) ;
        break ;
    case LABDHHTYPE:
    {
        LabDecHypHist *tmp = (LabDecHypHist *)elem ;
        if ( tmp->prev != NULL )
        {
            if ( --(tmp->prev->nConnect) <= 0 )
                returnElem( tmp->prev ) ;
            tmp->prev = NULL ;
        }
        labPool->returnElem( tmp ) ;
    }
    break ;
#if 0
    case LABTIMEDHHTYPE:
    {
        LabTimeDecHypHist *tmp = (LabTimeDecHypHist *)elem ;
        if ( tmp->prev != NULL )
        {
            if ( --(tmp->prev->nConnect) <= 0 )
                returnElem( tmp->prev ) ;
            tmp->prev = NULL ;
        }
        labTimePool->returnElem( tmp ) ;
    }
    break ;
    case LABTIMESCOREDHHTYPE:
    {
        LabTimeScoreDecHypHist *tmp = (LabTimeScoreDecHypHist *)elem ;
        if ( tmp->prev != NULL )
        {
            if ( --(tmp->prev->nConnect) <= 0 )
                returnElem( tmp->prev ) ;
            tmp->prev = NULL ;
        }
        labTimeScorePool->returnElem( tmp ) ;
    }
    break ;
    case LABTIME2SCOREDHHTYPE:
    {
        LabTime2ScoreDecHypHist *tmp = (LabTime2ScoreDecHypHist *)elem ;
        if ( tmp->prev != NULL )
        {
            if ( --(tmp->prev->nConnect) <= 0 )
                returnElem( tmp->prev ) ;
            tmp->prev = NULL ;
        }
        labTime2ScorePool->returnElem( tmp ) ;
    }
    break ;
#endif
    case LATTICEDHHTYPE:
    {
        LatticeDecHypHist *tmp = (LatticeDecHypHist *)elem ;
        if ( tmp->prev != NULL )
        {
            if ( --(tmp->prev->nConnect) <= 0 )
                returnElem( tmp->prev ) ;
            tmp->prev = NULL ;
        }
        latticePool->returnElem( tmp ) ;
    }
    break ;
    default:
        error("DecHypHistPool::returnElem"
              " - elem had unrecognised type field") ;
        break ;
    }
}


void DecHypHistPool::poolStats()
{
   /*
	printf("\n++++++++++++ %d %d ++++++++++++\n",getCnt,retCnt) ;
	fflush(stdout);

	getCnt = 0 ;
	retCnt = 0 ;
   */
}

void DecHypHistPool::initDecHyp( DecHyp *hyp , int state_ )
{
   hyp->state = state_;
   hyp->nLabelsNR = 0;
   hyp->score = LOG_ZERO ;
   hyp->acousticScore = LOG_ZERO ;
   hyp->lmScore = LOG_ZERO ;
   hyp->hist = NULL ;
}

// Changes
void DecHypHistPool::initDecHypOnTheFly( DecHypOnTheFly *hyp , int state_ , int gState_ )
{
   initDecHyp( (DecHyp*)hyp, state_ ) ;
   hyp->currGState = gState_ ; 
   hyp->pushedWeight = 0.0 ;
   hyp->nNextOutLabel = 0 ;
}


bool DecHypHistPool::isActiveHyp( DecHyp *hyp )
{
	if ( hyp == NULL )
		error("DecHypHistPool::isActiveHyp - hyp is NULL") ;

	if ( hyp->score > LOG_ZERO) 
		return true ;
	else if ( hyp->hist != NULL )
		error("DecHypHistPool::isActiveHyp - hist is not NULL but score is <= LOG_ZERO") ;

	return false ;
}

void DecHypHistPool::resetDecHypHist(DecHypHist* hist )
{
    assert(hist);
    if ( latticeMode && (hist->type == LATTICEDHHTYPE) )
    {
        lattice->registerInactiveTrans(
            ((LatticeDecHypHist *)hist)->latState
        ) ;
    }
  
    if ( --(hist->nConnect) <= 0 )
    {
        // Only this hypothesis is accessing this history information.
        // Return the element to it's pool.
#ifdef DEBUG
        if ( hist->nConnect < 0 )
            error("resetDecHyp - nConnect < 0") ;
#endif
        returnElem( hist ) ;
    }

    // hist = NULL ; // set by caller
}


void DecHypHistPool::resetDecHyp( DecHyp *hyp )
{
    // PNG: Minimise writing for deactivation as they're all cache
    // misses and deactivation happens a lot
    hyp->score = LOG_ZERO ;

    if ( hyp->hist != NULL )
    {
        if ( latticeMode && (hyp->hist->type == LATTICEDHHTYPE) )
        {
            lattice->registerInactiveTrans(
                ((LatticeDecHypHist *)(hyp->hist))->latState
            ) ;
        }
      
        if ( --(hyp->hist->nConnect) <= 0 )
        {
            // Only this hypothesis is accessing this history information.
            // Return the element to it's pool.
#ifdef DEBUG
            if ( hyp->hist->nConnect < 0 )
                error("resetDecHyp - nConnect < 0") ;
#endif
            returnElem( hyp->hist ) ;
        }

        hyp->hist = NULL ;
    }
}

// Changes
void DecHypHistPool::resetDecHypOnTheFly( DecHypOnTheFly *hyp )
{
   resetDecHyp( (DecHyp *)hyp ) ;
   hyp->currGState = -1 ;
   hyp->pushedWeight = 0.0 ;
   hyp->nNextOutLabel = 0 ;
}

// Changes
// If hyp is a DecHypOnTheFly hyp, then it will ignore label_
void DecHypHistPool::addLabelHistToDecHyp( DecHyp *hyp , int label_ )
{
   LabDecHypHist *tmp ;

   tmp = (LabDecHypHist *)(labPool->getElem()) ;
   tmp->type = LABDHHTYPE ;
   tmp->nConnect = 1 ;
   tmp->prev = NULL ;
   
   tmp->label = label_ ;
   
   if ( hyp->hist != NULL )
   {
      tmp->prev = hyp->hist ;
      // hyp->hist->n_connect does not change.
   }
   hyp->hist = (DecHypHist *)tmp ;
}

// Changes Octavian 20060523
void DecHypHistPool::addLabelHistToDecHypOnTheFly( DecHypOnTheFly *hypOnTheFly , int label_ )
{
   LabDecHypHist *tmp ;
   int nOutLabel = hypOnTheFly->nNextOutLabel ;
   
   for ( int i = 0 ; i < nOutLabel ; i++ )  {
      tmp = (LabDecHypHist *)(labPool->getElem()) ;
      tmp->type = LABDHHTYPE ;
      tmp->nConnect = 1 ;
      tmp->prev = NULL ;
      
      tmp->label = hypOnTheFly->nextOutLabel[i] ;
      
      if ( hypOnTheFly->hist != NULL )
      {
	 tmp->prev = hypOnTheFly->hist ;
	 // hyp->hist->n_connect does not change.
      }
      hypOnTheFly->hist = (DecHypHist *)tmp ;
   }
}



void DecHypHistPool::addLatticeHistToDecHyp( DecHyp *hyp , int latState_ , real accScore_ )
{
	LatticeDecHypHist *tmp ;

	tmp = (LatticeDecHypHist *)(latticePool->getElem()) ;
   tmp->type = LATTICEDHHTYPE ;
   tmp->nConnect = 1 ;
   tmp->prev = NULL ;
  
   tmp->latState = latState_ ;
   tmp->accScore = accScore_ ;

	if ( hyp->hist != NULL )
	{
		tmp->prev = hyp->hist ;
		// hyp->hist->n_connect does not change.
	}
	hyp->hist = (DecHypHist *)tmp ;
}


void DecHypHistPool::addHistToDecHyp(
    DecHyp *hyp , int state_ , real score_ , int time_ , 
    real acousticScore_ , real lmScore_
)
{
	DecHypHist *tmp ;

	tmp = (DecHypHist *)(dhhPool->getElem()) ;
   tmp->type = DHHTYPE ;
   tmp->nConnect = 1 ;
   tmp->prev = NULL ;
  
   tmp->state = state_ ;
   tmp->score = score_ ;
   tmp->time = time_ ;
   tmp->acousticScore = acousticScore_ ;
   tmp->lmScore = lmScore_ ;

	if ( hyp->hist != NULL )
	{
		tmp->prev = hyp->hist ;
		// hyp->hist->n_connect does not change.
	}
	hyp->hist = tmp ;
}

// Changes
void DecHypHistPool::registerLabel( DecHyp *hyp , int label )
{
   // Save this label for when there are hist elements needing it.
#ifdef DEBUG
   if ( hyp->nLabelsNR >= MAXNUMDECHYPNRLABELS )
      error("DecHypHistPool::registerLabel - hyp->nLabelsNR >= MAXNUMDECHYPNRLABELS");
   if ( (hyp->hist != NULL) && (hyp->hist->state < 0) )
      error("DecHypHistPool::registerLabel - (hyp->hist != NULL) && (hyp->hist->state < 0)");
#endif
 
   hyp->labelsNR[(int)(hyp->nLabelsNR)++] = label;

}

// Changes
// It will not reset nNextOutLabel
void DecHypHistPool::registerLabelOnTheFly( DecHypOnTheFly *hypOnTheFly )
{
   // Save this label for when there are hist elements needing it.
#ifdef DEBUG
   if ( hypOnTheFly->nLabelsNR >= MAXNUMDECHYPNRLABELS )
      error("DecHypHistPool::registerLabel - hyp->nLabelsNR >= MAXNUMDECHYPNRLABELS");
   if ( (hypOnTheFly->hist != NULL) && (hypOnTheFly->hist->state < 0) )
      error("DecHypHistPool::registerLabel - (hyp->hist != NULL) && (hyp->hist->state < 0)");
#endif

   // DecHypOnTheFly
   int nOutLabels = hypOnTheFly->nNextOutLabel ;
   for ( int i = 0 ; i < nOutLabels ; i++ )
      hypOnTheFly->labelsNR[(int)(hypOnTheFly->nLabelsNR)++] = hypOnTheFly->nextOutLabel[i] ;
}


void DecHypHistPool::registerEnd( DecHyp *hyp , real score_ , int time_ , 
								  real acousticScore_ , real lmScore_ )
{
    // This is called when an actual word-end is reached, so at that point we
    //   want to save hypothesis scores and end-time.

#ifdef DEBUG
    if ( hyp->nLabelsNR <= 0 )
        error("DecHypHistPool::registerLabel - hyp->nLabelsNR <= 0");
    if ( (hyp->hist != NULL) && (hyp->hist->state < 0) )
        error("DecHypHistPool::registerLabel - "
              "(hyp->hist != NULL) && (hyp->hist->state < 0)");
#endif

    addHistToDecHyp(
        hyp, hyp->labelsNR[0], score_, time_, acousticScore_, lmScore_
    );
    for ( int i=1 ; i<hyp->nLabelsNR ; i++ ) {
        hyp->labelsNR[i-1] = hyp->labelsNR[i];
    }
    (hyp->nLabelsNR)-- ;
}

void DecHypHistPool::extendDecHyp(
    DecHyp *hyp , DecHyp *new_hyp , real score_ , 
    real acousticScore_ , real lmScore_
)
{
#ifdef DEBUG
   if ( (hyp == NULL) || (new_hyp == NULL) )
      error("extendDecHyp - hyp or new_hyp is NULL\n") ;
#endif

   resetDecHyp( new_hyp );
   for ( int i=0 ; i<hyp->nLabelsNR ; i++ )
      new_hyp->labelsNR[i] = hyp->labelsNR[i];
   new_hyp->nLabelsNR = hyp->nLabelsNR;
   new_hyp->score = score_ ;
   new_hyp->acousticScore = acousticScore_ ;
   new_hyp->lmScore = lmScore_ ;
   new_hyp->hist = hyp->hist ;
   if ( new_hyp->hist != NULL )
   {
      new_hyp->hist->nConnect++ ;
      if ( latticeMode && (hyp->hist->type == LATTICEDHHTYPE) )
      {
         lattice->registerActiveTrans( ((LatticeDecHypHist *)(hyp->hist))->latState ) ;
      }
   }
}

// Changes
void DecHypHistPool::extendDecHypOnTheFly( 
      DecHypOnTheFly *hyp , DecHypOnTheFly *new_hyp , 
      real score_ , real acousticScore_ , 
      real lmScore_ , int currGState_, 
      real pushedWeight_ , int *nextOutLabel_ , int nNextOutLabel_ )
{
#ifdef DEBUG
   if ( nextOutLabel_ == NULL )
      error("DecHypHistPool::extendDecHypOnTheFly - nextOutLabel_ == NULL") ;
#endif

   extendDecHyp( (DecHyp *)hyp, (DecHyp *)new_hyp, score_, acousticScore_, lmScore_ ) ;
   new_hyp->currGState = currGState_ ;
   new_hyp->pushedWeight = pushedWeight_ ;
   new_hyp->nNextOutLabel = nNextOutLabel_ ;
   for ( int i = 0 ; i < nNextOutLabel_ ; i++ ) 
      new_hyp->nextOutLabel[i] = nextOutLabel_[i] ;
}


void DecHypHistPool::returnSingleElem( DecHypHist *elem )
{
   switch ( elem->type )
   {
   case DHHTYPE:
      dhhPool->returnElem( elem ) ;
      break ;
   case LABDHHTYPE:
      {
         LabDecHypHist *tmp = (LabDecHypHist *)elem ;
         labPool->returnElem( tmp ) ;
      }
      break ;
#if 0
   case LABTIMEDHHTYPE:
      {
         LabTimeDecHypHist *tmp = (LabTimeDecHypHist *)elem ;
         labTimePool->returnElem( tmp ) ;
      }
      break ;
   case LABTIMESCOREDHHTYPE:
      {
         LabTimeScoreDecHypHist *tmp = (LabTimeScoreDecHypHist *)elem ;
         labTimeScorePool->returnElem( tmp ) ;
      }
      break ;
   case LABTIME2SCOREDHHTYPE:
      {
         LabTime2ScoreDecHypHist *tmp = (LabTime2ScoreDecHypHist *)elem ;
         labTime2ScorePool->returnElem( tmp ) ;
      }
      break ;
#endif
   case LATTICEDHHTYPE:
      {
         LatticeDecHypHist *tmp = (LatticeDecHypHist *)elem ;
         latticePool->returnElem( tmp ) ;
      }
      break ;
   default:
      error("DecHypHistPool::returnSingleElem - elem had unrecognised type field") ;
      break ;
   }
}


}
