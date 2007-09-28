/*
 * Copyright 2006 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#include <assert.h>

#include "WFSTModelOnTheFly.h"
#include "log_add.h"

/*
	Author:		Octavian Cheng (ocheng@idiap.ch)
	Date:		7 June 2006
*/

using namespace Torch;

namespace Juicer {


// **************************
// Changes Octavian 20060417
// NodeToDecHypOnTheFlyPool implementation
NodeToDecHypOnTheFlyPool::NodeToDecHypOnTheFlyPool()
{
   reallocAmount = 0 ;
   pool = NULL ;
}

NodeToDecHypOnTheFlyPool::NodeToDecHypOnTheFlyPool( int reallocAmount_ )
{
   reallocAmount = reallocAmount_ ;
   pool = new BlockMemPool( sizeof(NodeToDecHypOnTheFly), reallocAmount ) ;
}

NodeToDecHypOnTheFlyPool::~NodeToDecHypOnTheFlyPool()
{
   delete pool ;
}

// **************************
// Changes Octavian 20060424 20060630
// NodeToDecHypOnTheFlyMapPool implementation
/*
NodeToDecHypOnTheFlyMapPool::NodeToDecHypOnTheFlyMapPool()
{
   reallocAmount = 0 ;
   pool = NULL ;
}

NodeToDecHypOnTheFlyMapPool::NodeToDecHypOnTheFlyMapPool( int reallocAmount_ )
{
   reallocAmount = reallocAmount_ ;
   pool = new BlockMemPool( sizeof(NodeToDecHypOnTheFlyMap), reallocAmount ) ;
}

NodeToDecHypOnTheFlyMapPool::~NodeToDecHypOnTheFlyMapPool()
{
   delete pool ;
}
*/

// ********************************
// Changes Octavian 20060417 20060531 20060627 20060630
// WFSTModelOnTheFly implementation
/*
DecHypOnTheFly *WFSTModelOnTheFly::getLastStateHyp( bool isPushed, int *gPushedState, int *matchedOutLabel )
*/
NodeToDecHypOnTheFly *WFSTModelOnTheFly::getLastStateHyp( bool isPushed, int *gPushedState, int *matchedOutLabel )
{
   int hmmLastStateIndex = hmm->n_states - 1 ;
#ifdef DEBUG
   if ( hmmLastStateIndex <= 0 )
      error("WFSTModelOnTheFly::getLastStateHyp - No last state") ;
#endif

   NodeToDecHypOnTheFlyList *list ;
   
   // We minus one here is because currHypsPushed[0] is hmm->states[1]
   // hmm->states[0] is this->initMapPushed
   if ( isPushed )
      list = currHypsPushed + hmmLastStateIndex - 1 ;
   else
      list = currHypsNotPushed + hmmLastStateIndex - 1 ;
  
   // Changes Octavian 20060627
   // DecHypOnTheFly *tmp = NULL ;
   NodeToDecHypOnTheFly *firstNode = NULL ;
   if ( list->head != NULL )  {
      firstNode = list->head ;
      // Changes Octavian 20060627
      // tmp = firstNode->hyp ;
      
      *gPushedState = firstNode->gState ;
      // Changes Octavian 20060531
      if ( matchedOutLabel != NULL )
	 *matchedOutLabel = firstNode->matchedOutLabel ;
      // Changes Octavian 20060630
      list->head = firstNode->ptr.next ;

      // Changes Octavian 20060627
      // pool->returnSingleNode( firstNode ) ;

      if ( list->head == NULL )
	 list->tail = NULL ;
   }
#ifdef DEBUG
   else  {
      if ( list->tail != NULL )
	 error("WFSTModelOnTheFly::getLastStateHyp - tail is not NULL") ;
   }
#endif
   // Changes Octavian 20060627
   // return tmp ; 
   return firstNode ;
}


// Changes Octavian 20060627 20060630
//DecHypOnTheFly *WFSTModelOnTheFly::getNonInitStateHyp()
void WFSTModelOnTheFly::clearNonInitStateHyp()
{
   int hmmLastStateIndex = hmm->n_states - 1 ;
#ifdef DEBUG
   if ( hmmLastStateIndex <= 0 )
      error("WFSTModelOnTheFly::clearNonInitStateHyp - No non-init states") ;
#endif

   // Changes Octavian 20060627
   // DecHypOnTheFly *tmp = NULL ;
   
   // The reason for minus one is the same as above
   for ( int i = 0 ; i < hmmLastStateIndex ; i++ )  {
      // Changes Octavian 20060627
      /*
      if ( currHypsNotPushed[i].head != NULL )  {
	 NodeToDecHypOnTheFly *firstNode = currHypsNotPushed[i].head ;
	 // Changes Octavian 20060627
	 tmp = &(firstNode->hyp) ;
	 currHypsNotPushed[i].head = firstNode->next ;
	 pool->returnSingleNode( firstNode ) ;
	 if ( currHypsNotPushed[i].head == NULL )
	    currHypsNotPushed[i].tail = NULL ;
	 break ;
      }
      */
      NodeToDecHypOnTheFly *firstNode = currHypsNotPushed[i].head ;
      while ( firstNode != NULL )  {
	 decHypHistPool->resetDecHypOnTheFly( &(firstNode->hyp) ) ;
	 pool->returnSingleNode( firstNode ) ;
	 // Changes Octavian 20060630
	 firstNode = firstNode->ptr.next ;
      }
      currHypsNotPushed[i].head = NULL ;
      currHypsNotPushed[i].tail = NULL ;

      // Changes Octavian 20060627
      /*
      if ( currHypsPushed[i].head != NULL )  {
	 NodeToDecHypOnTheFly *firstNode = currHypsPushed[i].head ;
	 // Changes Octavian 20060627
	 tmp = &(firstNode->hyp) ;
	 currHypsPushed[i].head = firstNode->next ;
	 pool->returnSingleNode( firstNode ) ;
	 if ( currHypsPushed[i].head == NULL )
	    currHypsPushed[i].tail = NULL ;
	 break ;
      }
      */
      
      firstNode = currHypsPushed[i].head ;
      while ( firstNode != NULL )  {
	 decHypHistPool->resetDecHypOnTheFly( &(firstNode->hyp) ) ;
	 pool->returnSingleNode( firstNode ) ;
	 // Changes Octavian 20060630
	 firstNode = firstNode->ptr.next ;
      }
      currHypsPushed[i].head = NULL ;
      currHypsPushed[i].tail = NULL ;

   }

#ifdef DEBUG
   // No more hyps in all the non-init states
   // Changes Octavian 20060627
   // if ( tmp == NULL )  {

   for ( int i = 0 ; i < hmmLastStateIndex ; i++ )  {
      if (  ( currHypsNotPushed[i].head != NULL ) ||
	    ( currHypsNotPushed[i].tail != NULL )  )
	 error("WFSTModelOnTheFly::clearNonInitStateHyp - prevHypsNotPushed not empty") ;
      
      if (  ( currHypsPushed[i].head != NULL ) || 
	    ( currHypsPushed[i].tail != NULL )  )
	 error("WFSTModelOnTheFly::clearNonInitStateHyp - prevHypsPushed not empty") ;
   }
   //}
#endif

   // Changes Octavian 20060627
   // return tmp ;
   return ;
}


// Changes Octavian 20060423 20060424 20060531 20060627 20060630
/*
DecHypOnTheFly **WFSTModelOnTheFly::findInitStateHyp( NodeToDecHypOnTheFlyMap *currNode, bool isPushed, int gState, int matchedOutLabel, bool *isNew )
*/
/*
DecHypOnTheFly *WFSTModelOnTheFly::findInitStateHyp( NodeToDecHypOnTheFlyMap *currNode, bool isPushed, int gState, int matchedOutLabel, bool *isNew )
*/
DecHypOnTheFly *WFSTModelOnTheFly::findInitStateHyp( NodeToDecHypOnTheFly *currNode, bool isPushed, int gState, int matchedOutLabel, bool *isNew )
{
   // Terminating case
   // Case 1: currNode == NULL ( ie currNode = root )
   if ( currNode == NULL )  {
      // Changes Octavian 20060630
      // NodeToDecHypOnTheFlyMap *newNode = mapPool->getSingleNode() ;
      NodeToDecHypOnTheFly *newNode = pool->getSingleNode() ;
      newNode->gState = gState ;
      // Changes Octavian 20060531
      newNode->matchedOutLabel = matchedOutLabel ;
      // Changes Octavian 20060630
      newNode->ptr.left = NULL ;
      newNode->right = NULL ;
      if ( isPushed ) 
	 initMapPushed = newNode ;
      else
	 initMapNotPushed = newNode ;
      *isNew = true ;
      return &(newNode->hyp) ;
   }

   // Case 2: Find the node
   // Changes Octavian 20060531
   if ( ( currNode->gState == gState ) && ( currNode->matchedOutLabel == matchedOutLabel ) )  {
      *isNew = false ;
      return &(currNode->hyp) ;
   }
   
   // Other cases: Cannot find the node in the currNode.
   if ( currNode->gState > gState )  {
      // Go to left child
      // Change Octavian 20060630
      if ( currNode->ptr.left == NULL )  {
	 // Changes Octavian 20060630
	 // NodeToDecHypOnTheFlyMap *newNode = mapPool->getSingleNode() ;
	 NodeToDecHypOnTheFly *newNode = pool->getSingleNode() ;
	 newNode->gState = gState ;
	 // Changes Octavian 20060531
	 newNode->matchedOutLabel = matchedOutLabel ;
	 // Changes Octavian 20060630
	 newNode->ptr.left = NULL ;
	 newNode->right = NULL ;
	 // Changes Octavian 20060630
	 currNode->ptr.left = newNode ;
	 *isNew = true ;
	 return &(newNode->hyp) ;
      }
      else  {
	 // Changes Octavian 20060630
	 return findInitStateHyp( 
	       currNode->ptr.left, isPushed, gState, matchedOutLabel, isNew ) ;
      }
   }
   else if ( currNode->gState < gState ) {
      // Changes Octavian 20060531
      // Go to right child
      if ( currNode->right == NULL )  {
	 // Changes Octavian 20060630
	 // NodeToDecHypOnTheFlyMap *newNode = mapPool->getSingleNode() ;
	 NodeToDecHypOnTheFly *newNode = pool->getSingleNode() ;
	 newNode->gState = gState ;
	 // Changes Octavian 20060531
	 newNode->matchedOutLabel = matchedOutLabel ;
	 // Changes Octavian 20060630
	 newNode->ptr.left = NULL ;
	 newNode->right = NULL ;
	 currNode->right = newNode ;
	 *isNew = true ;
	 return &(newNode->hyp) ;
      }
      else  {
	 return findInitStateHyp( 
	       currNode->right, isPushed, gState, matchedOutLabel, isNew ) ;
      }
   }
   else  {
      // Changes Octavian 20060531
      // Here the currNode has the same G state, but the matchedOutLabel is 
      // different
      if ( currNode->matchedOutLabel > matchedOutLabel )  {
	 // Left child
	 // Changes Octavian 20060630
	 if ( currNode->ptr.left == NULL )  {
	    // Changes Octavian 20060630
	    // NodeToDecHypOnTheFlyMap *newNode = mapPool->getSingleNode() ;
	    NodeToDecHypOnTheFly *newNode = pool->getSingleNode() ;
	    newNode->gState = gState ;
	    // Changes Octavian 20060531
	    newNode->matchedOutLabel = matchedOutLabel ;
	    // Changes Octavian 20060630
	    newNode->ptr.left = NULL ;
	    newNode->right = NULL ;
	    // Changes Octavian 20060630
	    currNode->ptr.left = newNode ;
	    *isNew = true ;
	    return &(newNode->hyp) ;
	 }
	 else  {
	    // Changes Octavian 20060630
	    return findInitStateHyp(
		  currNode->ptr.left, isPushed, gState, matchedOutLabel, 
		  isNew ) ;
	 }
      }
      else  {
	 // Right child
	 if ( currNode->right == NULL )  {
	    // Changes Octavian 20060630
	    // NodeToDecHypOnTheFlyMap *newNode = mapPool->getSingleNode() ;
	    NodeToDecHypOnTheFly *newNode = pool->getSingleNode() ;
	    newNode->gState = gState ;
	    // Changes Octavian 20060531
	    newNode->matchedOutLabel = matchedOutLabel ;
	    // Changes Octavian 20060630
	    newNode->ptr.left = NULL ;
	    newNode->right = NULL ;
	    currNode->right = newNode ;
	    *isNew = true ;
	    return &(newNode->hyp) ;
	 }
	 else  {
	    return findInitStateHyp(
		  currNode->right, isPushed, gState, matchedOutLabel, isNew ) ;
	 }
      }
   }
}


// Changes Octavian 20060531 20060627
/*
DecHypOnTheFly *WFSTModelOnTheFly::getNextEmitStateHyps( bool isPushed, int *gPushedState, int *matchedOutLabel, int *fromHMMState )
*/
NodeToDecHypOnTheFly *WFSTModelOnTheFly::getNextEmitStateHyps( bool isPushed, int *gPushedState, int *matchedOutLabel, int *fromHMMState )
{
   int hmmLastStateIndex = hmm->n_states - 1 ;
#ifdef DEBUG
   if ( hmmLastStateIndex <= 0 )
      error("WFSTModelOnTheFly::getNextEmitStateHyps - No non-init states") ;
#endif

   NodeToDecHypOnTheFlyList *list ;
   if ( isPushed )
      list = prevHypsPushed ;
   else
      list = prevHypsNotPushed ;

   // Changes Octavian 20060627
   // DecHypOnTheFly *minHyp = NULL ;
   
   int minGState = -1;
   // Changes Octavian 20060531
   int minMatchedOutLabel = -1;
   int minLocation = -1;

   // Changes Octavian 20060627
   NodeToDecHypOnTheFly *minNode = NULL ;
   NodeToDecHypOnTheFly *currHead ;
   
   // We minus 1 here because prevHypsNotPushed[hmmLastStateIndex-1] = 
   // hmm->states[hmmLastStateIndex]. Hence, this function just iterates 
   // all emit states.
   for ( int i = 0 ; i < hmmLastStateIndex - 1 ; i++ )  {
      // Changes Octavian 2006027
      currHead = list[i].head ;
      if ( currHead != NULL )  {
	 // Check if we have a min gState hyp before
	 // Changes Octavian 20060627
	 if ( minNode == NULL )  {
	    // Changes Octavian 20060627
	    // minHyp = list[i].head->hyp ;
	    minNode = currHead ;
	    minGState = currHead->gState ;
	    // Changes Octavian 20060531
	    minMatchedOutLabel = currHead->matchedOutLabel ;
	    minLocation = i ;
	 }
	 else  {
	    if ( currHead->gState < minGState )  {
	       // Changes Octavian 20060627
	       // minHyp = list[i].head->hyp ;
	       minNode = currHead ;
	       minGState = currHead->gState ;
	       // Changes Octavian 20060531
	       minMatchedOutLabel = currHead->matchedOutLabel ;
	       minLocation = i ;
	    }
	    else if ( currHead->gState == minGState )  {
	       // Changes Octavian 20060531
	       if ( currHead->matchedOutLabel < minMatchedOutLabel )  {
		  // Changes Octavian 20060627
		  // minHyp = list[i].head->hyp ;
		  minNode = currHead ;
		  // minGState = list[i].head->gState ;
		  // Changes Octavian 20060531
		  minMatchedOutLabel = currHead->matchedOutLabel ;
		  minLocation = i ;
	       }
	    }
	 }
      }
   }
  
   // Changes Octavian 20060627
   if ( minNode != NULL )  {
      // Changes Octavian 20060627
      /*
      NodeToDecHypOnTheFly *toDel = list[minLocation].head ;
      list[minLocation].head = toDel->next ;
      if ( toDel->next == NULL )
	 list[minLocation].tail = NULL ;
      pool->returnSingleNode( toDel ) ;
      */
      // Changes Octavian 20060630
      list[minLocation].head = minNode->ptr.next ;
      // Changes Octavian 20060630
      if ( minNode->ptr.next == NULL )
	 list[minLocation].tail = NULL ;
      
      *gPushedState = minGState ;
      // Changes Octavian 20060531
      *matchedOutLabel = minMatchedOutLabel ;
      *fromHMMState = minLocation + 1 ;
   }

   // Changes Octavian 20060627
   // return minHyp ;
   return minNode ;
}


// Changes Octavian 20060531
// Changes Octavian 20060627
DecHypOnTheFly *WFSTModelOnTheFly::findCurrEmitStateHypsFromInitSt( int hmmState, bool isPushed, int gPushedState, int matchedOutLabel, NodeToDecHypOnTheFly **helper, bool *isNew )
{
#ifdef DEBUG
   if ( ( hmmState <= 0 ) || ( hmmState >= hmm->n_states - 1 ) )
      error("WFSTModelOnTheFly::findCurrEmitStateHypsFromInitSt - hmmState out of range") ;
#endif

   int index = hmmState - 1 ;
   
   NodeToDecHypOnTheFly *listHead ;
   // Changes Octavian 20060627
   NodeToDecHypOnTheFlyList *list ;

   if ( isPushed )  {
      listHead = currHypsPushed[index].head ;
      // Changes Octavian 20060627
      list = currHypsPushed + index ;
   }
   else  {
      listHead = currHypsNotPushed[index].head ;
      // Changes Octavian 20060627
      list = currHypsNotPushed + index ;
   }
   
   // First case: if the helper is pointing to NULL, it means no node in
   // the linked list
   if ( helper[index] == NULL )  {
#ifdef DEBUG
      if ( listHead != NULL )
	 error("WFSTModelOnTheFly::findCurrEmitStateHypsFromInitSt - listHead not NULL") ;
#endif
      // Changes Octavian 20060627
      NodeToDecHypOnTheFly *newNode = pool->getSingleNode() ;
      newNode->gState = gPushedState ;
      newNode->matchedOutLabel = matchedOutLabel ;
      // Changes Octavian 20060630
      newNode->ptr.next = NULL ;
      list->head = list->tail = newNode ;
      helper[index] = newNode ;
      *isNew = true ;

      // Changes Octavian 20060627
      // return NULL ;
      return &(newNode->hyp) ;
   }
   
   // helper should point to the node just smaller than or equal to the 
   // gPushedState. If gPushedState are equal, the node's matchedOutLabel 
   // should be smaller than or equal to the argument matchedOutLabel.
   
   // Search for the cloest gState that is just smaller than the gPushedState
   // Changes Octavian 20060630
   while ( helper[index]->ptr.next != NULL )  {
      // Changes Octavian 20060531 20060630
      if ( helper[index]->ptr.next->gState < gPushedState )  {
	 // Changes Octavian 20060630
	 helper[index] = helper[index]->ptr.next ;
      }
      else if ( helper[index]->ptr.next->gState == gPushedState )  {
	 // Changes Octavian 20060531
	 if ( helper[index]->ptr.next->matchedOutLabel <= matchedOutLabel )
	    helper[index] = helper[index]->ptr.next ;
	 else
	    break ;
      }
      else  {
	 break ;
      }
   }
  
   int helperGState = helper[index]->gState ;
   
   // Changes Octavian 20060627
   int helperMatchedOutLabel = helper[index]->matchedOutLabel ;
   bool isNewHead = false ;
   
   if ( helperGState == gPushedState )  {
      // Second case: We've found the node with the same gPushedState.
      // Return hypothesis if matchedOutLabel is the same also. Otherwise,
      // return the newly created hypothesis.
      // Changes Octavian 20060531 20060627
      if ( helperMatchedOutLabel == matchedOutLabel )  {
	 // Changes Octavian 20060627
	 *isNew = false ;
	 return &(helper[index]->hyp) ;
      }
      else if ( helperMatchedOutLabel < matchedOutLabel )  {
	 // Changes Octavian 20060627
	 // Insert a new Node just behind the helper
	 isNewHead = false ;
	       }
      else  {
#ifdef DEBUG
	 if ( helper[index] != listHead )
	    error("WFSTModelOnTheFly::findCurrEmitStateHypFromInitSt - listHead != helper[index] (1)") ;
#endif
	 // Changes Octavian 20060627
	 isNewHead = true ;
      }
   }
   else if ( helperGState < gPushedState )  {
      // Third case: We can't find the node. But we've found the insertion
      // point, which is stored in the helper
      
      // Changes Octavian 20060627
      isNewHead = false ;
      // return NULL ;
   }
   else  {
      // Fourth case: It is only valid when the helper is pointing towards
      // the listHead
#ifdef DEBUG
      if ( helper[index] != listHead )
	 error("WFSTModelOnTheFly::findCurrEmitStateHypFromInitSt - listHead != helper[index] (2)") ;
#endif
      
      // Changes Octavian 20060627
      isNewHead = true ;
      //return NULL ;
   }

   // Changes Octavian 20060627
   if ( isNewHead )  {
      NodeToDecHypOnTheFly *newNode = pool->getSingleNode() ;
      newNode->gState = gPushedState ;
      newNode->matchedOutLabel = matchedOutLabel ;
      // Changes Octavian 20060630
      newNode->ptr.next = list->head ;
      list->head = newNode ;
      helper[index] = newNode ;
      *isNew = true ;
      return &(newNode->hyp) ;
   }
   else  {
      NodeToDecHypOnTheFly *newNode = pool->getSingleNode() ;
      newNode->gState = gPushedState ;
      newNode->matchedOutLabel = matchedOutLabel ;
      // Changes Octavian 20060630
      newNode->ptr.next = helper[index]->ptr.next ;
      // Changes Octavian 20060630
      helper[index]->ptr.next = newNode ;
      helper[index] = newNode ;
      // Changes Octavian 20060630
      if ( newNode->ptr.next == NULL )
	 list->tail = newNode ;
      *isNew = true ;
      return &(newNode->hyp) ;
   }   
}


// Changes Octavian 20060531 20060627
DecHypOnTheFly *WFSTModelOnTheFly::findCurrEmitStateHypsFromEmitSt( int hmmState, bool isPushed, int gPushedState, int matchedOutLabel, bool *isNew )
{
#ifdef DEBUG
   // The range can go to the last HMM state
   if ( ( hmmState <= 0 ) || ( hmmState > hmm->n_states - 1 ) )
      error("WFSTModelOnTheFly::findCurrEmitStateHypsFromEmitSt - hmmState out of range") ;
#endif

   // Changes Octavian 20060627
   // NodeToDecHypOnTheFly *node ;
   NodeToDecHypOnTheFlyList *list ;
   
   if ( isPushed )  {
      // Changes Octavian 20060627
      list = currHypsPushed + hmmState - 1 ;
      //node = currHypsPushed[hmmState-1].tail ;
   }
   else  {
      // Changes Octavian 20060627
      list = currHypsNotPushed + hmmState - 1 ;
      //node = currHypsNotPushed[hmmState-1].tail ;
   }

   // Changes Octavian 20060627
   // DecHypOnTheFly *hyp = NULL ;
   
   // Changes Octavian 20060627
   if ( list->tail != NULL )  {
      if ( ( list->tail->gState == gPushedState ) 
	    && ( list->tail->matchedOutLabel == matchedOutLabel ) )  {
	 *isNew = false ;
	 return &(list->tail->hyp) ;
	 // hyp = node->hyp ;
	 
      }
      else  {
#ifdef DEBUG
	 // Changes Octavian 20060531
	 if ( list->tail->gState > gPushedState )
	    error("WFSTModelOnTheFly::findCurrEmitStateHypFromEmitSt - Not in ascending order") ;
	 // Changes Octavian 20060531
	 if ( ( list->tail->gState == gPushedState ) &&
	       ( list->tail->matchedOutLabel >= matchedOutLabel ) )
	    error("WFSTModelOnTheFly::findCurrEmitStateHypFromEmitSt - same G state and matchedOutLabel not in ascending order") ;
#endif
	 NodeToDecHypOnTheFly *newNode = pool->getSingleNode() ;
	 newNode->gState = gPushedState ;
	 newNode->matchedOutLabel = matchedOutLabel ;
	 // Changes Octavian 20060630
	 newNode->ptr.next = NULL ;
	 list->tail->ptr.next = newNode ;
	 list->tail = newNode ;
	 *isNew = true ;
	 return &(newNode->hyp) ;
      }
   }
   else  {
      NodeToDecHypOnTheFly *newNode = pool->getSingleNode() ;
      newNode->gState = gPushedState ;
      newNode->matchedOutLabel = matchedOutLabel ;
      // Changes Octavian 20060630
      newNode->ptr.next = NULL ;
      list->head = list->tail = newNode ;
      *isNew = true ;
      return &(newNode->hyp) ;
   }
   
   // Chnages Octavian 20060627
   // return hyp ;
}


// Changes Octavian 20060531 20060627
/*
void WFSTModelOnTheFly::putCurrEmitStateHypsFromInitSt( DecHypOnTheFly *newHyp, int hmmState, bool isPushed, int gPushedState, int matchedOutLabel, NodeToDecHypOnTheFly **helper )
{
#ifdef DEBUG
   // The range cannot go to the last HMM state
   if ( ( hmmState <= 0 ) || ( hmmState >= hmm->n_states - 1 ) )
      error("WFSTModelOnTheFly::putCurrEmitStateHypsFromInitSt - hmmState out of range") ;
#endif

   NodeToDecHypOnTheFlyList *list ;
   int index = hmmState - 1 ;
   
   if ( isPushed )
      list = currHypsPushed + index ;
   else
      list = currHypsNotPushed + index ;

   NodeToDecHypOnTheFly *newNode = pool->getSingleNode() ;
   newNode->gState = gPushedState ;
   // Changes Octavian 20060531
   newNode->matchedOutLabel = matchedOutLabel ;
   newNode->hyp = newHyp ;
   
   // First case: Empty linked list
   if ( helper[index] == NULL )  {
#ifdef DEBUG
      if ( ( list->head != NULL ) || ( list->tail != NULL ) )
	 error("WFSTModelOnTheFly::putCurrEmitStateHypsFromInitSt - list->head and list->tail are not NULL") ;
#endif
      newNode->next = NULL ;
      list->head = list->tail = newNode ;
      helper[index] = newNode ;
   }
   else  {
      // Changes Octavian 20060531
      bool isNewHead = false ;
      if ( helper[index]->gState > gPushedState )  {
	 // Second case: helper is pointing to a node with the greater gState.
	 // Only valid for list head AND emit state
	 isNewHead = true ;      
      }
      else if ( helper[index]->gState == gPushedState )  {
	 // Changes Octavian 20060531
	 if ( helper[index]->matchedOutLabel > matchedOutLabel )  {
	    // Third case (a): helper is pointing to a node with the same 
	    // gState but matchedOutLabel is greater.
	    isNewHead = true ;
	 }
#ifdef DEBUG
	 else if ( helper[index]->matchedOutLabel == matchedOutLabel )  {
	    // Third case (b): same matchedOutLabel. Error.
	    error ("WFSTModelOnTheFly::putCurrEmitStateFromInitSt - same gState and same matchedOutLabel") ;
	 }
#endif
	 else  {
	    // Third case (c): helper is pointing to a node with the same
	    // gState but smaller matchedOutLabel
	    isNewHead = false ;
	 }	 
      }
      else  {
	 // Fourth case: helper is pointing to a node with smaller gState. 
	 // Put the newNode behind the helper.
	 isNewHead = false ;
      }

      // Changes Octavian 20060531
      if ( isNewHead )  {
#ifdef DEBUG
	 if ( helper[index] != list->head )
	    error ("WFSTModelOnTheFly::putCurrEmitStateFromInitSt - list->head != helper[index]") ;
#endif
	 newNode->next = list->head ;
	 list->head = newNode ;
	 helper[index] = newNode ;
      }
      else  {
	 newNode->next = helper[index]->next ;
	 helper[index]->next = newNode ;
	 if ( newNode->next == NULL )  {
#ifdef DEBUG
	    if ( helper[index] != list->tail )
	       error("WFSTModelOnTheFly::putCurrEmitStateFromInitSt - list->tail != helper[index]") ;
#endif
	    list->tail = newNode ;
	 }
	 helper[index] = newNode ;
      }
   }
   return ;

}
*/


// Changes Octavian 20060531 20060627
/*
void WFSTModelOnTheFly::putCurrEmitStateHypsFromEmitSt( DecHypOnTheFly *newHyp, int hmmState, bool isPushed, int gPushedState, int matchedOutLabel )
{
#ifdef DEBUG
   // The range can go to the last HMM state
   if ( ( hmmState <= 0 ) || ( hmmState > hmm->n_states - 1 ) )
      error("WFSTModelOnTheFly::putCurrEmitStateHypsFromEmitSt - hmmState out of range") ;
#endif

   NodeToDecHypOnTheFlyList *list ;
   if ( isPushed )
      list = currHypsPushed + hmmState - 1 ;
   else
      list = currHypsNotPushed + hmmState - 1 ;

   NodeToDecHypOnTheFly *node ;

   if ( list->tail == NULL )  {
      node = pool->getSingleNode() ;
      node->gState = gPushedState ;
      // Changes Octavian 20060531
      node->matchedOutLabel = matchedOutLabel ;
      node->hyp = newHyp ;
      node->next = NULL ;
      list->head = list->tail = node ;
   }
   else  {
#ifdef DEBUG
      // Changes Octavian 20060531
      if ( list->tail->gState > gPushedState )
	 error("WFSTModelOnTheFly::putCurrEmitStateHypFromEmitSt - Not in ascending order") ;
      // Changes Octavian 20060531
      if ( ( list->tail->gState == gPushedState ) &&
	    ( list->tail->matchedOutLabel >= matchedOutLabel ) )
	 error("WFSTModelOnTheFly::putCurrEmitStateHypFromEmitSt - same G state and matchedOutLabel not in ascending order") ;
#endif
      node = pool->getSingleNode() ;
      node->gState = gPushedState ;
      // Changes Octavian 20060531
      node->matchedOutLabel = matchedOutLabel ;
      node->hyp = newHyp ;
      node->next = NULL ;
      list->tail->next = node ;
      list->tail = node ;
   }
}
*/

NodeToDecHypOnTheFly **WFSTModelOnTheFly::createHelper()
{
#ifdef DEBUG
   if ( hmm->n_states <= 1 )
      error("WFSTModelOnTheFly::createHelper - no helper to create") ;
#endif
   return ( new NodeToDecHypOnTheFly*[hmm->n_states-1] ) ;
}

void WFSTModelOnTheFly::assignHelper( NodeToDecHypOnTheFly **helper, bool isPushed )
{
   for ( int i = 0 ; i < hmm->n_states - 1 ; i++ )  {
      if ( isPushed )  {
	 helper[i] = currHypsPushed[i].head ;
      }
      else  {
	 helper[i] = currHypsNotPushed[i].head ;
      }
   }
}

//PNG
#if 0
//*****************************
// Changes by Octavian
// WFSTModelOnTheFlyPool Implementation
// Changes Octavian 20060627
WFSTModelOnTheFlyPool::WFSTModelOnTheFlyPool( 
      PhoneModels *phoneModels_ , DecHypHistPool *decHypHistPool_ )
{
   // Changes by Octavian 20060627
   /*
   if ( (decHypPool = decHypPool_) == NULL )
      error("WFSTModelOnTheFlyPool::WFSTModelOnTheFlyPool - decHypPool_ is NULL") ;
      */
   //************************
   
   int i ;
   if ( (phoneModels = phoneModels_) == NULL )
      error("WFSTModelOnTheFlyPool::WFSTModelOnTheFlyPool - phoneModels_ is NULL") ;
   
   models = NULL ;
   if ( (decHypHistPool = decHypHistPool_) == NULL )
      error("WFSTModelOnTheFlyPool::WFSTModelOnTheFlyPool - decHypHistPool_ is NULL") ;

   // Determine the maximum number of states over all models in phoneModels.
   maxNStates = 0 ;
   nUsed = 0 ;
   
   for ( i=0 ; i<phoneModels->nModels ; i++ )
   {
      if ( phoneModels->models[i]->n_states > maxNStates )
	 maxNStates = phoneModels->models[i]->n_states ;
   }
  
   // Changes by Octavian
   poolsOnTheFly = new WFSTModelOnTheFlyFNSPool[maxNStates] ;
   // ***********************
   
   // Changes Octavian 20060417
   nodeToDecHypOnTheFlyPool = new NodeToDecHypOnTheFlyPool( DEFAULT_DECHYPPOOL_SIZE ) ;

   // Changes Octavian 20060424 20060630
   // nodeToDecHypOnTheFlyMapPool = new NodeToDecHypOnTheFlyMapPool( DEFAULT_DECHYPPOOL_SIZE ) ;
   
   // Should call WFSTModelOnTheFlyPool::initFNSPool()
   for ( i=0 ; i<maxNStates ; i++ )
      initFNSPool( i , i+1 , 100 ) ;

}
#endif

// Changes Octavian 20060627
WFSTModelOnTheFlyPool::WFSTModelOnTheFlyPool( 
	 Models *models_ , DecHypHistPool *decHypHistPool_ )
{
   // Changes by Octavian 20060627
   /*
   if ( (decHypPool = decHypPool_) == NULL )
      error("WFSTModelOnTheFlyPool::WFSTModelOnTheFlyPool(2) - decHypPool_ is NULL") ;
      */
   //************************

   if ( (models = models_) == NULL )
      error("WFSTModelOnTheFlyPool::WFSTModelOnTheFlyPool(2) - models_ is NULL") ;
//PNG   phoneModels = NULL ;
   
   if ( (decHypHistPool = decHypHistPool_) == NULL )
      error("WFSTModelOnTheFlyPool::WFSTModelOnTheFlyPool(2) - decHypHistPool_ is NULL") ;

   // Determine the maximum number of states over all models in phoneModels.
   maxNStates = 0 ;
   nUsed = 0 ;
   DecodingHMM *dh ;
   int i ;
   for ( i=0 ; i<models->getNumHMMs() ; i++ )
   {
      dh = models->getDecHMM( i ) ;
      if ( dh->n_states > maxNStates )
	 maxNStates = dh->n_states ;
   }

   // Changes by Octavian
   poolsOnTheFly = new WFSTModelOnTheFlyFNSPool[maxNStates] ;
   // pools = new WFSTModelFNSPool[maxNStates] ;
   // **************************************************

   // Changes Octavian 20060417
   nodeToDecHypOnTheFlyPool = new NodeToDecHypOnTheFlyPool( DEFAULT_DECHYPPOOL_SIZE ) ;

   // Changes Octavian 20060424 20060630
   // nodeToDecHypOnTheFlyMapPool = new NodeToDecHypOnTheFlyMapPool( DEFAULT_DECHYPPOOL_SIZE ) ;
 
   // Should call WFSTModelOnTheFlyPool::initFNSPool() 
   for ( i=0 ; i<maxNStates ; i++ )
      initFNSPool( i , i+1 , 100 ) ;
}


WFSTModelOnTheFlyPool::~WFSTModelOnTheFlyPool()
{
   int i , j ;

   for ( i=0 ; i<maxNStates ; i++ )
   {
      // Changes
      if ( poolsOnTheFly != NULL )  {
	 if ( poolsOnTheFly[i].allocs != NULL )
	 {
	    for ( j=0 ; j<poolsOnTheFly[i].nAllocs ; j++ )
	    {
	       delete [] poolsOnTheFly[i].allocs[j][0].hyp1NotPushed ;
	       delete [] poolsOnTheFly[i].allocs[j][0].hyp1Pushed ;
	       delete [] poolsOnTheFly[i].allocs[j] ;
	    }
	    free( poolsOnTheFly[i].allocs ) ;
	 }
	 
	 if ( poolsOnTheFly[i].freeElems != NULL )
	    free( poolsOnTheFly[i].freeElems ) ;
      }
   }
   delete [] poolsOnTheFly ;
   
   // Changes by Octavian 20060417
   delete nodeToDecHypOnTheFlyPool ;

   // Changes by Octavian 20060424 20060630
   // delete nodeToDecHypOnTheFlyMapPool ;
}


WFSTModelOnTheFly *WFSTModelOnTheFlyPool::getElem( WFSTTransition *trans )
{
   DecodingHMM *hmm ;
     
   // Changes by Octavian
   WFSTModelOnTheFlyFNSPool *pool ;
   WFSTModelOnTheFly *tmp ;
   /*
   WFSTModelFNSPool *pool ;
   WFSTModel *tmp ;
   */

   if ( trans->inLabel == 0 )
      error("WFSTModelOnTheFlyPool::getElem - trans->inLabel = 0 (i.e. epsilon)") ;
   
   int index = trans->inLabel - 1 ;
   hmm = getDecHMM( index ) ;

   // Changes by Octavian
   pool = poolsOnTheFly + hmm->n_states - 1 ;
   /*
   pool = pools + hmm->n_states - 1 ;
   */
   
   if ( pool->nFree == 0 )
      allocPool( hmm->n_states - 1 ) ;
   
   pool->nFree-- ;
   pool->nUsed++ ;

#ifdef DEBUG
   if ( (pool->nFree < 0) || (pool->nUsed > pool->nTotal) )
      error("WFSTModelOnTheFlyPool::getElem - nFree or nUsed invalid") ;
#endif

   tmp = pool->freeElems[pool->nFree] ;
   pool->freeElems[pool->nFree] = NULL ;

   // Configure the new element.
   tmp->trans = trans ;
   tmp->hmm = hmm ;
   tmp->nActiveHyps = 0 ;
   tmp->next = NULL ;

   // Update the global nUsed counter.
   ++nUsed ;

   return tmp ;
}


void WFSTModelOnTheFlyPool::returnElem( WFSTModelOnTheFly *elem )
{
   // Changes by Octavian
   WFSTModelOnTheFlyFNSPool *pool ;
   /*
   WFSTModelFNSPool *pool ;
   */

   int nStates ;

   nStates = elem->hmm->n_states ;
   resetElem( elem , nStates ) ;
   
   // Changes by Octavian 
   pool = poolsOnTheFly + nStates - 1 ;
   /*
   pool = pools + nStates - 1 ;
   */
   
   pool->freeElems[pool->nFree] = elem ;
   pool->nFree++ ;
   pool->nUsed-- ;
   
#ifdef DEBUG
   if ( (pool->nFree > pool->nTotal) || (pool->nUsed < 0) )
      error("WFSTModelOnTheFlyPool::returnElem - nFree or nUsed invalid") ;
#endif

   // Update the global nUsed counter.
   --nUsed ;

}


void WFSTModelOnTheFlyPool::initFNSPool( int poolInd , int nStates , int initReallocAmount )
{
   if ( (poolInd < 0) || (poolInd >= maxNStates) )
      error("WFSTModelOnTheFlyPool::initFNSPool - poolInd out of range") ;
   if ( initReallocAmount <= 0 )
      error("WFSTModelOnTheFlyPool::initFNSPool - initReallocAmount <= 0") ;

   // Changes by Octavian
   poolsOnTheFly[poolInd].nStates = nStates ;
   poolsOnTheFly[poolInd].nTotal = 0 ;
   poolsOnTheFly[poolInd].nUsed = 0 ;
   poolsOnTheFly[poolInd].nFree = 0 ;
   poolsOnTheFly[poolInd].reallocAmount = initReallocAmount ;
   poolsOnTheFly[poolInd].nAllocs = 0 ;
   poolsOnTheFly[poolInd].allocs = NULL ;
   poolsOnTheFly[poolInd].freeElems = NULL ;
   
   /*
   pools[poolInd].nStates = nStates ;
   pools[poolInd].nTotal = 0 ;
   pools[poolInd].nUsed = 0 ;
   pools[poolInd].nFree = 0 ;
   pools[poolInd].reallocAmount = initReallocAmount ;
   pools[poolInd].nAllocs = 0 ;
   pools[poolInd].allocs = NULL ;
   pools[poolInd].freeElems = NULL ;
   */

   // Should call WFSTModelOnTheFlyPool::allocPool()
   allocPool( poolInd ) ;
}


void WFSTModelOnTheFlyPool::allocPool( int poolInd )
{
   // Changes by Octavian
   WFSTModelOnTheFlyFNSPool *pool ;
   WFSTModelOnTheFly *tmp ;
   /*
   WFSTModelFNSPool *pool ;
   WFSTModel *tmp ;
   */
   
   int i ;
   // Changes by Octavian
   pool = poolsOnTheFly + poolInd ;
   /*
   pool = pools + poolInd ;
   */

#ifdef DEBUG
   // Check to see if we really need to allocate more entries
   if ( pool->nFree > 0 )
      return ;
   if ( pool->nFree < 0 )
      error("WFSTModelPool::allocPool - nFree < 0") ;
#endif
   
   // Changes by Octavian
   pool->freeElems = (WFSTModelOnTheFly **)realloc( pool->freeElems , 
	 (pool->nTotal+pool->reallocAmount)*sizeof(WFSTModelOnTheFly *) ) ;
   pool->allocs = (WFSTModelOnTheFly **)realloc( pool->allocs , 
	 (pool->nAllocs+1)*sizeof(WFSTModelOnTheFly *));
   pool->allocs[pool->nAllocs] = new WFSTModelOnTheFly[pool->reallocAmount] ;

   /*
   pool->freeElems = (WFSTModel **)realloc( pool->freeElems , 
	 (pool->nTotal+pool->reallocAmount)*sizeof(WFSTModel *) ) ;
   pool->allocs = (WFSTModel **)realloc( pool->allocs , (pool->nAllocs+1)*sizeof(WFSTModel *));
   pool->allocs[pool->nAllocs] = new WFSTModel[pool->reallocAmount] ;
   */
  
   
   // Changes by Octavian
   tmp = pool->allocs[pool->nAllocs] ;
   
   // Changes Octavian 20060417
   if ( pool->nStates > 1 )  {
      // Allocate NodeToDecHypOnTheFlyList
      tmp[0].hyp1NotPushed = new NodeToDecHypOnTheFlyList[ pool->reallocAmount * (pool->nStates-1) * 2 ] ;
      tmp[0].hyp2NotPushed = tmp[0].hyp1NotPushed + ( pool->nStates - 1 ) ;
      tmp[0].hyp1Pushed = new NodeToDecHypOnTheFlyList[ pool->reallocAmount * (pool->nStates-1) * 2 ] ;
      tmp[0].hyp2Pushed = tmp[0].hyp1Pushed + ( pool->nStates - 1 ) ;
   }
   else  {
      // No need to allocate NodeToDecHypOnTheFlyList
      tmp[0].hyp1NotPushed = NULL ;
      tmp[0].hyp1Pushed = NULL ;
      tmp[0].hyp2NotPushed = NULL ;
      tmp[0].hyp2Pushed = NULL ;
   }
   // Changes Octavian 20060417
   /*
   tmp[0].hyp1NotPushed = new NodeToDecHypOnTheFlyMap[ pool->reallocAmount * pool->nStates * 2 ] ;
   tmp[0].hyp2NotPushed = tmp[0].hyp1NotPushed + pool->nStates ;
   tmp[0].hyp1Pushed = new NodeToDecHypOnTheFlyMap[ pool->reallocAmount * pool->nStates * 2 ] ;
   tmp[0].hyp2Pushed = tmp[0].hyp1Pushed + pool->nStates ;
   */
 
   initElem( tmp , pool->nStates ) ;
   pool->freeElems[0] = tmp ;
  
   // Changes Octavian 20060417
   for ( i=1 ; i<pool->reallocAmount ; i++ )
   {
      if ( pool->nStates > 1 )  {
	 tmp[i].hyp1NotPushed = tmp[i-1].hyp2NotPushed + pool->nStates - 1 ;
	 tmp[i].hyp2NotPushed = tmp[i].hyp1NotPushed + pool->nStates - 1 ;
	 tmp[i].hyp1Pushed = tmp[i-1].hyp2Pushed + pool->nStates - 1 ;
	 tmp[i].hyp2Pushed = tmp[i].hyp1Pushed + pool->nStates - 1;
      }
      else  {
	 tmp[i].hyp1NotPushed = NULL ;
	 tmp[i].hyp1Pushed = NULL ;
	 tmp[i].hyp2NotPushed = NULL ;
	 tmp[i].hyp2Pushed = NULL ;
      }
      
      initElem( tmp + i , pool->nStates ) ;
      pool->freeElems[i] = tmp + i ;
   }
   /*
   for ( i=1 ; i<pool->reallocAmount ; i++ )
   {
      tmp[i].hyp1NotPushed = tmp[i-1].hyp2NotPushed + pool->nStates ;
      tmp[i].hyp2NotPushed = tmp[i].hyp1NotPushed + pool->nStates ;
      tmp[i].hyp1Pushed = tmp[i-1].hyp2Pushed + pool->nStates ;
      tmp[i].hyp2Pushed = tmp[i].hyp1Pushed + pool->nStates ;
      initElem( tmp + i , pool->nStates ) ;
      pool->freeElems[i] = tmp + i ;
   }
   */
 
   // Allocate the currHyps and prevHyps fields of the new WFSTModel elements
   // in one big block.
   /*
   tmp = pool->allocs[pool->nAllocs] ;
   tmp[0].hyps1 = new DecHyp[ pool->reallocAmount * pool->nStates * 2 ] ;
   tmp[0].hyps2 = tmp[0].hyps1 + pool->nStates ;
   initElem( tmp , pool->nStates ) ;
   pool->freeElems[0] = tmp ;

   for ( i=1 ; i<pool->reallocAmount ; i++ )
   {
      tmp[i].hyps1 = tmp[i-1].hyps2 + pool->nStates ;
      tmp[i].hyps2 = tmp[i].hyps1 + pool->nStates ;
      initElem( tmp + i , pool->nStates ) ;
      pool->freeElems[i] = tmp + i ;
   }
   */

   pool->nTotal += pool->reallocAmount ;
   pool->nFree += pool->reallocAmount ;
   pool->nAllocs++ ;
   
   // change the realloc amount to be equal to the new total size.
   // ie. exponentially increasing allocation.
   pool->reallocAmount = pool->nTotal ;

}

void WFSTModelOnTheFlyPool::initElem( WFSTModelOnTheFly *elem , int nStates )
{
   //int i ;
   
   elem->trans = NULL ;
   elem->hmm = NULL ;
  
   // Changes Octavian 20060423
   elem->initMapNotPushed = NULL ;
   elem->initMapPushed = NULL ;
   
   // Changes by Octavian
   elem->currHypsNotPushed = elem->hyp1NotPushed ;
   elem->currHypsPushed = elem->hyp1Pushed ;
   elem->prevHypsNotPushed = elem->hyp2NotPushed ;
   elem->prevHypsPushed = elem->hyp2Pushed ;

   // Changes Octavian 20060417
   // nStates-1 because the initial state is implemented as a map, not list
   for ( int i = 0 ; i < (nStates-1) ; i++ )  {
      elem->hyp1NotPushed[i].head = NULL ;
      elem->hyp1NotPushed[i].tail = NULL ;
      elem->hyp1Pushed[i].head = NULL ;
      elem->hyp1Pushed[i].tail = NULL ;
      elem->hyp2NotPushed[i].head = NULL ;
      elem->hyp2NotPushed[i].tail = NULL ;
      elem->hyp2Pushed[i].head = NULL ;
      elem->hyp2Pushed[i].tail = NULL ;
   }

   // Changes Octavian 20060417
   elem->pool = nodeToDecHypOnTheFlyPool ;

   // Changes Octavian 20060424 20060630
   // elem->mapPool = nodeToDecHypOnTheFlyMapPool ;

   // Changes Octavian 20060627
   elem->decHypHistPool = decHypHistPool ;
   
   /*
   elem->currHyps = elem->hyps1 ;
   elem->prevHyps = elem->hyps2 ;
   */
   
   // Changes by Octavian
   /*
   for ( i=0 ; i<nStates ; i++ )
   {
      DecHypHistPool::initDecHyp( elem->currHyps + i , i ) ;
      DecHypHistPool::initDecHyp( elem->prevHyps + i , i ) ;
   }
   */
   
   elem->nActiveHyps = 0 ;
   elem->next = NULL ;

}

// Similar to WFSTOnTheFlyDecoder::extendInitMap()
// Changes Octavian 20060630
//void WFSTModelOnTheFlyPool::iterInitMap( NodeToDecHypOnTheFlyMap *currNode )
void WFSTModelOnTheFlyPool::iterInitMap( NodeToDecHypOnTheFly *currNode )
{
   if ( currNode == NULL )
      return ;

   // Left first
   if ( currNode->ptr.left != NULL )  {
#ifdef DEBUG
      // Changes Octavian 20060531
      if ( currNode->ptr.left->gState > currNode->gState )
	 error("WFSTModelOnTheFlyPool::iterInitMap - left > curr") ;
      // Changes Octavian 20060531
      if ( ( currNode->ptr.left->gState == currNode->gState ) && 
	    ( currNode->ptr.left->matchedOutLabel >= currNode->matchedOutLabel ) )
	 error("WFSTModelOnTheFlyPool::iterInitMap - left.matchedOutLabel") ;
#endif
      iterInitMap( currNode->ptr.left ) ;
   }

   // currNode
   // Changes Octavian 20060627
   DecHypOnTheFly *currHyp = &(currNode->hyp) ;

   decHypHistPool->resetDecHypOnTheFly( currHyp ) ;
   // Changes Octavian 20060627
   //decHypPool->returnSingleHyp( currHyp ) ;

   // Right last
   if ( currNode->right != NULL )  {
#ifdef DEBUG
      // Changes Octavian 20060531
      if ( currNode->right->gState < currNode->gState )
	 error("WFSTModelOnTheFlyPool::iterInitMap - right < curr") ;
      // Changes Octavian 20060531
      if ( ( currNode->right->gState == currNode->gState ) && 
	    ( currNode->right->matchedOutLabel <= currNode->matchedOutLabel ) )
	 error("WFSTModelOnTheFlyPool::iterInitMap - right.matchedOutLabel") ;
#endif
      iterInitMap( currNode->right ) ;
   }

   // Changes Octavian 20060630
   // nodeToDecHypOnTheFlyMapPool->returnSingleNode( currNode ) ;
   nodeToDecHypOnTheFlyPool->returnSingleNode( currNode ) ;
   return ;
}

void WFSTModelOnTheFlyPool::resetElem( WFSTModelOnTheFly *elem , int nStates )
{
#ifdef DEBUG
   int i ;
#endif
 
   // Changes Octavian 20060627
   // DecHypOnTheFly *tmpDecHyp ;
   
   iterInitMap( elem->initMapNotPushed ) ;
   elem->initMapNotPushed = NULL ;
   iterInitMap( elem->initMapPushed ) ;
   elem->initMapPushed = NULL ;

   // Changes Octavian 20060627
   /*      
   while (( tmpDecHyp = elem->getNonInitStateHyp() ) != NULL )  {
      decHypHistPool->resetDecHypOnTheFly( tmpDecHyp ) ;
      // Changes Octavian 20060627
      // decHypPool->returnSingleHyp( tmpDecHyp ) ;
   }
   */
   elem->clearNonInitStateHyp() ;

   elem->trans = NULL ;
   elem->hmm = NULL ;

   
   // Changes by Octavian
#ifdef DEBUG
   if ( elem->initMapNotPushed != NULL ) 
      error("WFSTModelOnTheFlyPool::resetElem - initMapNotPushed not empty") ;
   if ( elem->initMapPushed != NULL )
      error("WFSTModelOnTheFlyPool::resetElem - initMapPushed not empty") ;
      
   for ( i = 0 ; i < (nStates-1) ; i++ )  {
      if (  ( elem->prevHypsNotPushed[i].head != NULL ) || 
	    ( elem->prevHypsNotPushed[i].tail != NULL ) )
	 error("WFSTModelOnTheFlyPool::resetElem - prevHypsNotPushed non-NULL") ;
      if (  ( elem->prevHypsPushed[i].head != NULL ) ||
	    ( elem->prevHypsPushed[i].tail != NULL ) )
	 error("WFSTModelOnTheFlyPool::resetElem - prevHypsPushed non-NULL") ;

      if (  ( elem->currHypsNotPushed[i].head != NULL ) || 
	    ( elem->currHypsNotPushed[i].tail != NULL ) )
	 error("WFSTModelOnTheFlyPool::resetElem - currHypsNotPushed non-NULL") ;
      if (  ( elem->currHypsPushed[i].head != NULL ) ||
	    ( elem->currHypsPushed[i].tail != NULL ) )
	 error("WFSTModelOnTheFlyPool::resetElem - currHypsPushed non-NULL") ;
   }
#endif
   
   elem->nActiveHyps = 0 ;
   elem->next = NULL ;

}

void WFSTModelOnTheFlyPool::checkActive( WFSTModelOnTheFly *elem )
{
   error("WFSTModelOnTheFlyPool::checkActive - Not implemented") ;
}

//*************************************

}
