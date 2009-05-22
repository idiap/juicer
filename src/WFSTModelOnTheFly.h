/*
 * Copyright 2006 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#ifndef WFST_MODELONTHEFLY_INC
#define WFST_MODELONTHEFLY_INC

#include "general.h"
#include "WFSTModel.h"

/*
 	Author: Octavian Cheng (ocheng@idiap.ch)
	Date: 		6 June 2006
*/

namespace Juicer {


/*
 * The following are the data structures added for doing dynamic
 * composition
 */
    // Changes Octavian 20060417 20060531 20060630
/**
 * This structure contains
 * a gState Index, a matchedOutLabel and a pointer of a
 * hypothesis. The pair (gState, matchedOutLabel) will be the key
 * (index) for finding hypotheses. It is a linked list node, hence it
 * also has a pointer to the next node. It is used for organizing a
 * list of hypotheses in each emitting state and the last state of the
 * HMM. Because now each state will have more than 1 hypotheses. Each
 * hypothesis has a different gState number.
 *
 */
union NodeToDecHypOnTheFly_ptr {
   struct NodeToDecHypOnTheFly *next ;
   struct NodeToDecHypOnTheFly *left ;
} ;

struct NodeToDecHypOnTheFly
{
   int gState ;
   int matchedOutLabel ;
   // Changes Octavian 20060627
   // DecHypOnTheFly *hyp ;
   DecHypOnTheFly hyp ;
   // Changes Octavian 20060630
   // If it is used as a linked list, use ptr.next
   // It it is used as a binary search tree, use ptr.left
   union NodeToDecHypOnTheFly_ptr ptr ;
   // Changes Octavian 20060630
   struct NodeToDecHypOnTheFly *right ;
} ;

// Changes Octavian 20060417
/**
 * This structure contains a linked list of nodes.
 * This list serves as a map of gStateIndex to a hyp
 * It is the structure to maintain the linked list of NodeToDecHypOntheFly
 * nodes for each emitting state and the last state.
 */
struct NodeToDecHypOnTheFlyList
{
   struct NodeToDecHypOnTheFly *head ;
   struct NodeToDecHypOnTheFly *tail ;
} ;

// Changes Octavian 20060417
/**
 * A block memory pool for storing NodeToDecHypOnTheFly nodes
 */
class NodeToDecHypOnTheFlyPool
{
   public:
      NodeToDecHypOnTheFlyPool() ;
      NodeToDecHypOnTheFlyPool( int reallocAmount_ ) ;
      virtual ~NodeToDecHypOnTheFlyPool() ;
      NodeToDecHypOnTheFly *getSingleNode()  {
	 // Changes Octavian 20060627
	 NodeToDecHypOnTheFly *newNode = 
	    (NodeToDecHypOnTheFly *) ( pool->getElem() ) ;
	 DecHypHistPool::initDecHypOnTheFly( &(newNode->hyp), -1 , -1 ) ;
	 
	 // Changes Octavian 20060627
	 return newNode ; 
      } ;
      void returnSingleNode( NodeToDecHypOnTheFly *node )  {
	 // Changes Octavian 20060627
#ifdef DEBUG	 
	 if ( DecHypHistPool::isActiveHyp( &(node->hyp) ) == true )
	    error("NodeToDecHypOnTheFlyPool - the hyp is active") ;
#endif	 
	 pool->returnElem( (void *) node ) ;
	 return ;
      } ;
   protected:
      int reallocAmount ;
      BlockMemPool *pool ;
} ;

// Changes Octavian 20060424 20060531 20060630
// This is the data structure speically for the initial state
// of the HMM. It is implemented as a binary search tree because
// hypotheses of different state numbers are coming out of order.
// The pair (gState, matchedOutLabel) is a key to this binary search tree.
/*
struct NodeToDecHypOnTheFlyMap
{
   int gState ;
   int matchedOutLabel ;
   // Changes Octavian 20060627
   // DecHypOnTheFly *hyp ;
   DecHypOnTheFly hyp ;
   NodeToDecHypOnTheFlyMap *left ;
   NodeToDecHypOnTheFlyMap *right ;
} ;
*/

// Changes Octavian 20060424 20060630
// A block memory pool for storing NodeToDecHypOnTheFlyMap
/*
class NodeToDecHypOnTheFlyMapPool
{
   public:
      NodeToDecHypOnTheFlyMapPool() ;
      NodeToDecHypOnTheFlyMapPool( int reallocAmount_ ) ;
      virtual ~NodeToDecHypOnTheFlyMapPool() ;
      NodeToDecHypOnTheFlyMap *getSingleNode() {
	 // Changes Octavian 20060627
	 NodeToDecHypOnTheFlyMap *newNode = 
	    (NodeToDecHypOnTheFlyMap *)( pool->getElem() ) ;
	 DecHypHistPool::initDecHypOnTheFly( &(newNode->hyp), -1, -1 ) ;

	 // Changes Octavian 20060627	 
	 return newNode ;
      } ;
      void returnSingleNode( NodeToDecHypOnTheFlyMap *node )  {
	 // Changes Octavian 20060627
#ifdef DEBUG
	 if ( DecHypHistPool::isActiveHyp( &(node->hyp) ) == true )
	    error("NodeToDecHypOnTheFlyMapPool - the hyp is active") ;
#endif
	 pool->returnElem( (void *) node ) ;
	 return ;
      } ;

   protected:
      int reallocAmount ;
      BlockMemPool *pool ;
} ;
*/

 // Changes Octavian 20060417 20060423 20060630
/**
 * A data structure which represents an active model (ie a model which has
 * hypotheses)
 * Basic attribes are similar to WFSTModel
 * The main difference is that the initial state is implemented as a binary
 * search tree.
 * The emitting states and the last state is implemented as an ordered linked
 * list (NodeToDecHypOnTheFlyList).
 * In simple terms, if an HMM has 5 states (3 emitting states + init state +
 * last state), the init state is a binary search tree; the rest are ordered
 * linked list.
 * The two pools are basically for allocating nodes when necessary.
 * 
 * NOTE: WFSTOnTheFlyDecoder::extendInitMap also changes some attributes
 * of this class.
 */
class WFSTModelOnTheFly
{
public:
   // Methods
   // Other extenal functions will allocate/release memory of this class.
   WFSTModelOnTheFly() {} ;
   virtual ~WFSTModelOnTheFly() {} ;
  
   // Changes Octavian 20060531
   // This method returns the NodeToDecHypOnTheFly in the last state of this 
   // HMM. 
   // If isPushed == true, it will find the hyp from currHypsPushed.
   // Otherwise, it will return the hyp from currHypsNotPushed.
   // Return NULL if no more hyp is found.
   // Will automatically dequeue the record from the last state.
   // Remember to call returnSingleNode() after finishing this hyp.
   // gPushedState stores the pushed G state of the hyp.
   // matchedOutLabel stores the matched output label of the hyp -
   // 	matchedOutLabel = UNDECIDEDOUTLABEL if unpushed or
   // 	matchedOutLabel = a matching CoL outLabel if pushed
   /*
   DecHypOnTheFly *getLastStateHyp( 
	 bool isPushed, int *gPushedState, int *matchedOutLabel ) ;
	 */
   NodeToDecHypOnTheFly *getLastStateHyp( 
	 bool isPushed, int *gPushedState, int *matchedOutLabel ) ;


   // Get all the hypotheses in all the NodeToDecHypOnTheFlyLists
   // No particular order.
   // Return NULL if the model has no more hyps in the lists
   // Will auotmatically dequeue the record from the list
   // Remember to call returnSingleHyp() for the returned hyp
   // DecHypOnTheFly *getNonInitStateHyp() ;

   // Changes Octavian 20060627
   // Clear all hypotheses in the non-initial state of the HMM. Always call
   // this function after all frames have been processed.
   void clearNonInitStateHyp() ;

   // Changes Octavian 20060424 20060531 20060627 20060630
   // Find the hyp from the init state map. If found, isNew is false and 
   // return where it could be found. If not found, create a new entry
   // and return where the hyp could be found
   // Start searching from currNode.
   // The key of the map is (gState, matchedOutLabel)
   /*
   DecHypOnTheFly *findInitStateHyp( 
	 NodeToDecHypOnTheFlyMap *currNode, bool isPushed, int gState,
	 int matchedOutLabel, bool *isNew ) ;
	 */
   DecHypOnTheFly *findInitStateHyp( 
	 NodeToDecHypOnTheFly *currNode, bool isPushed, int gState,
	 int matchedOutLabel, bool *isNew ) ;

   // Changes Octavian 20060531 20060627
   // This function return the next hyp from all emit states 
   // according to the accending (gState, matchedOutLabel) order.
   // If isPushed == true, it will find the next hyp from all emit states of
   // prevHypsPushed. Otherwise, it will search from prevHypsNotPushed.
   // Return NULL if no more hyp is found.
   // Will automatically dequeue the hyp record from the corresponding 
   // linked list.
   // Remember to call returnSingleNode() after using NodeToDecHypOnTheFly.
   // The reason for the accending order to is to ensure the hyp are 
   // still in an accending-ordered linked list after Viterbi, 
   // so that it doesn't need to search to do the Viterbi.
   NodeToDecHypOnTheFly *getNextEmitStateHyps( 
	 bool isPushed, int *gPushedState, int *matchedOutLabel, 
	 int *fromHMMState ) ;

   // Changes Octavian 20060531 20060627
   // This function will search for a hyp in the hmmState of an HMM with 
   // (gPushedState, matchedOutLabel) as an index. If isPushed == 
   // true, it will search the currHypsPushed[hmmState-1]. Otherwise, it will
   // search the currHypsNotPushed[hmmState-1]. Return hyp pointer.
   // If found, hyp pointer is pointing towards an already-existed hyp and 
   // isNew is false. Otherwise, hyp pointer is pointing towards a newly-
   // created hyp (to which you can extend the old hyp to this new one) and 
   // isNew is true.
   // helper is an array of pointers which point to a node in the linked list.
   // For example, if the there 3 emitting states and a last state, then
   // the array will have 4 elements. Each is pointing towards a node of the
   // linked list of the corresponding state. When this function returns, it 
   // will point to a node which has a <= gState number than gPushedState and
   // a <= matchedOutLabel number than the argument matchedOutLabel.
   // The only exception is when there is only one node in the list.
   DecHypOnTheFly *findCurrEmitStateHypsFromInitSt( 
	 int hmmState, bool isPushed, int gPushedState, int matchedOutLabel, 
	 NodeToDecHypOnTheFly **helper, bool *isNew ) ;
  
   // Changes Octavian 20060531 20060627
   // This function will find the hyp on the hmmState of the HMM model with
   // the G state index of gPushedState and the matchedOutLabel. 
   // If isPushed == true, it will search in the currHypsPushed[hmmState-1] 
   // linked list. Otherwise, it will search in the 
   // currHypsNotPushed[hmmState-1] linked list. 
   // Return hyp pointer. See findCurrEmitStateHypsFromInitSt(). 
   // Because all hyps are organized in an accending order of gPushedState, 
   // what this function does is just checking the last elem of the linked 
   // list.
   DecHypOnTheFly *findCurrEmitStateHypsFromEmitSt( 
	 int hmmState, bool isPushed, int gPushedState, int matchedOutLabel, 
	 bool *isNew ) ;

   // Changes Octavian 20060531 20060627
   // This function puts newHyp to the hmmState of the HMM model.
   // (gPushedState, matchedOutLabel) is a key to differentiate other hyps 
   // in the same HMM state. If isPushed == true, put the newHyp in the 
   // currHypsPushed linked list. Otherwise, put it in the currHypsNotPushed 
   // linked list. It will maintain the ascending order of the original list,
   // so it is not just attaching at the tail - the main difference from 
   // putCurrEmitStateHypsFromEmitSt().
   // helper should point to the insertion point.
   /*
   void putCurrEmitStateHypsFromInitSt( 
	 DecHypOnTheFly *newHyp, int hmmState, bool isPushed, 
	 int gPushedState, int matchedOutLabel, 
	 NodeToDecHypOnTheFly **helper ) ;
	 */

   // Changes Octavian 20060531 20060627
   // This function puts newHyp to the hmmState of the HMM model.
   // (gPushedState, matchedOutLabel) is a key to differentiate other hyps 
   // in the same HMM state. If isPushed == true, put the newHyp in the 
   // currHypsPushed linked list. Otherwise, put it in the currHypsNotPushed 
   // linked list. Since hypotheses are coming in the accending order of 
   // gPushedState, this function just attaches the newHyp at the tail of 
   // the linked list.
   /*
   void putCurrEmitStateHypsFromEmitSt( 
	 DecHypOnTheFly *newHyp, int hmmState, bool isPushed, 
	 int gPushedState, int matchedOutLabel ) ;
	 */

   // This function creates the helper array
   NodeToDecHypOnTheFly **createHelper() ;

   // This function assign the helper array pointing to the head of 
   // currHypsNotPushed or currHypsPushed of each state
   void assignHelper( NodeToDecHypOnTheFly **helper, bool isPushed ) ;

   // Attributes
   WFSTTransition	*trans ;    	// A model is associated with a transition in the WFST
   DecodingHMM		*hmm ;		// Pointer to the HMM definition.

   // Changes Octavian 20060423 20060424 20060630
   /*
   NodeToDecHypOnTheFlyMap	*initMapNotPushed ;
   NodeToDecHypOnTheFlyMap	*initMapPushed ;
   */
   NodeToDecHypOnTheFly		*initMapNotPushed ;
   NodeToDecHypOnTheFly		*initMapPushed ;
  
   // Linked list to map gState to hyp pointer.
   // An array of linked list starting from the second state to the last
   NodeToDecHypOnTheFlyList	*hyp1NotPushed ;
   NodeToDecHypOnTheFlyList	*hyp1Pushed ;
   NodeToDecHypOnTheFlyList	*hyp2NotPushed ;
   NodeToDecHypOnTheFlyList 	*hyp2Pushed ;

   NodeToDecHypOnTheFlyList	*prevHypsNotPushed ;
   NodeToDecHypOnTheFlyList	*prevHypsPushed ;
   NodeToDecHypOnTheFlyList	*currHypsNotPushed ;
   NodeToDecHypOnTheFlyList	*currHypsPushed ;

   // A pool which manages the allocation of NodeToDecHypOnTheFly
   NodeToDecHypOnTheFlyPool	*pool ;
   
   // Changes Octavian 20060424 20060630
   // A pool which manages the allocation of NodeToDecHypOnTheFlyMap
   // NodeToDecHypOnTheFlyMapPool	*mapPool ;

   // Changes Octavian 20060627
   // A pool which manages hyp history
   // Created by decoder
   DecHypHistPool	*decHypHistPool ;
   
   int			nActiveHyps ;
   WFSTModelOnTheFly	*next ;	// We also have a straight linked list of all active WFSTModel elements.
   
};

// Changes Octavian 20060417
/**
 * A structure similar to WFSTModelFNSPool
 */
struct WFSTModelOnTheFlyFNSPool		// FNS = Fixed Number of States
{
   int	nStates ;	// All elements in this pool are for models with this many states.
   int	nTotal ;
   int	nUsed ;
   int	nFree ;
   int	reallocAmount ;
   int	nAllocs ;
   WFSTModelOnTheFly	**allocs ;
   WFSTModelOnTheFly	**freeElems ;
} ;


// Changes Octavian 20060417
/**
 */
class WFSTModelOnTheFlyPool : public WFSTModelPool
{
public:
   // Changes Octavian 20060627
   /*
   WFSTModelOnTheFlyPool( 
	 PhoneModels *phoneModels_ , DecHypHistPool *decHypHistPool_ , 
	 DecHypPool *decHypPool_ ) ;
   WFSTModelOnTheFlyPool( 
	 Models *models_ , DecHypHistPool *decHypHistPool_ ,
	 DecHypPool *decHypPool_ ) ;
	 */
//PNG   WFSTModelOnTheFlyPool( 
//PNG	 PhoneModels *phoneModels_ , DecHypHistPool *decHypHistPool_ ) ;
   WFSTModelOnTheFlyPool( 
	 Models *models_ , DecHypHistPool *decHypHistPool_ ) ;
   virtual ~WFSTModelOnTheFlyPool() ;

   WFSTModelOnTheFly *getElem( WFSTTransition *trans ) ;
   void returnElem( WFSTModelOnTheFly *elem ) ;

protected:
   // Changes Octavian 20060627
   // DecHypPool *decHypPool ;
   // Changes Octavian 20060417
   NodeToDecHypOnTheFlyPool *nodeToDecHypOnTheFlyPool ;
   // Changes Octavian 20060424 20060630
   // NodeToDecHypOnTheFlyMapPool *nodeToDecHypOnTheFlyMapPool ;

   virtual void initFNSPool( int poolInd , int nStates , int initReallocAmount ) ;
   virtual void allocPool( int poolInd ) ;

private:
   WFSTModelOnTheFlyFNSPool *poolsOnTheFly ;
   void initElem( WFSTModelOnTheFly *elem , int nStates ) ;
   void resetElem( WFSTModelOnTheFly *elem , int nStates ) ;
   void checkActive( WFSTModelOnTheFly *elem ) ;

   // Changes Octavian 20060424 20060531 20060630
   // Similar to WFSTOnTheFlyDecoder::extendInitMap()
   // Reset and return all hypotheses in the initial state of an HMM
   // void iterInitMap( NodeToDecHypOnTheFlyMap *currNode ) ;
   void iterInitMap( NodeToDecHypOnTheFly *currNode ) ;
 
} ;
//*****************************************

}

#endif
