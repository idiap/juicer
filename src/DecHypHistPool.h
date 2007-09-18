/*
 * Copyright 2005 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#ifndef DECHYPHISTPOOL_INC
#define DECHYPHISTPOOL_INC

#include "general.h"
#include "log_add.h"
#include "BlockMemPool.h"
#include "WFSTLattice.h"

/*
	Author:	Darren Moore (moore@idiap.ch)
	$Id: DecHypHistPool.h,v 1.7.4.2 2006/09/06 18:47:12 juicer Exp $
*/

/*
   	Author: Octavian Cheng (ocheng@idiap.ch)
	Date: 6 June 2006
*/

namespace Juicer {

/*
 * Phil
 *
 * This sucks!  These structures implement a kind of dangerous
 * polymorphism based on the first three elements of each structure
 * being the same.
 */

struct DecHypHist
{
    unsigned char    type ;
    int              nConnect ;
    DecHypHist       *prev ;

    int              state ;
    int              time ;
    real             score ;			// complete score, acoustic + lmProb*lmScaleFactor
    real             acousticScore ;	// just acoustic score
    real             lmScore ;			// just the LM score (no scaling or wordInsPen)
};

struct LabDecHypHist
{
    unsigned char    type ;
    int              nConnect ;
    DecHypHist       *prev ;
    int              label ;
};

#if 0

struct LabTimeDecHypHist
{
   unsigned char     type ;
   int               nConnect ;
   DecHypHist        *prev ;
   
   int               label ;
   int               time ;
};

struct LabTimeScoreDecHypHist
{
   unsigned char     type ;
   int               nConnect ;
   DecHypHist        *prev ;
   
   int               label ;
   int               time ;
   real              score ;
};

struct LabTime2ScoreDecHypHist
{
   unsigned char     type ;
   int               nConnect ;
   DecHypHist        *prev ;
   
   int               label ;
   int               time ;
   real              score1 ;
   real              score2 ;
};

#endif

struct LatticeDecHypHist
{
   unsigned char     type ;
   int               nConnect ;
   DecHypHist        *prev ;
   
   int               latState ;
   real              accScore ;
};

#define DHHTYPE               1
#define LABDHHTYPE            2
#define LABTIMEDHHTYPE        3
#define LABTIMESCOREDHHTYPE   4
#define LABTIME2SCOREDHHTYPE  5
#define LATTICEDHHTYPE        6

#if 0
struct StateDecHypHist
{
    int 				phone ;
	int					state ;
	int					time ;
	real				gamma ;
	int					bestGaussian ;
	StateDecHypHist	*prev ;
	int					nConnect ;
};

#endif

//
// Phil
//
// This was 5, but 2 makes the class below smaller and the program
// faster.  You'd imagine that it would be 32 bytes long, but sizeof
// gives 36.  Changing nLabelsNR from char to int has no effect on the
// class size, so there's some alignment going on in the compiler.
//
// nLabelsNR is (I think!) a tally of the number of output labels that
// have been read with no corresponding word end label.  Max 1?  In
// fact, the word end labels can be removed in the WFST composition
// phase, so there's something not quite optimal here.
//
#define MAXNUMDECHYPNRLABELS   2

// Changes Octavian
/**
 * Change to a class. Originally is just a C-structure
 */
class DecHyp
{
public:
    DecHypHist *hist ;
    int state;
	char nLabelsNR;
    real	score ;
    real	acousticScore ;	// just acoustic score
    real	lmScore ;		// just lm score
    int labelsNR[MAXNUMDECHYPNRLABELS];
    DecHyp() {
        state = -1 ;
        nLabelsNR = 0 ;
        score = LOG_ZERO ;
        acousticScore = LOG_ZERO ;
        lmScore = LOG_ZERO ;
        hist = NULL ;
    } ;
    virtual ~DecHyp() {} ;
};



#define MAXNUMDECHYPOUTLABELS   5

// Changes by Octavian
/**
 * Data structure for a on-the-fly hypothesis
 */
class DecHypOnTheFly : public DecHyp
{
public:
   // Current G state - used for finding set intersection
   int currGState ;		   
   // Look-ahead weight that has been pushed to this hyp
   real pushedWeight ;			
   // Anticipated output label. When the hyp reaches the end state
   // of an HMM, the decoder uses this variable for lattice generation,
   // model-level output and word-level output
   int nextOutLabel[MAXNUMDECHYPOUTLABELS] ;
   int nNextOutLabel ;
   
   DecHypOnTheFly() : DecHyp()  {
      currGState = -1 ;
      pushedWeight = 0.0 ;
      nNextOutLabel = 0 ;
   } ;

   virtual ~DecHypOnTheFly() {} ;   
};

// Changes by Octavian 
/*
struct DecHyp
{
   int			state;
   char        nLabelsNR;
   int         labelsNR[MAXNUMDECHYPNRLABELS];
   real			score ;
   real			acousticScore ;	// just acoustic score
   real			lmScore ;			// just lm score
   DecHypHist	*hist ;
};
*/

// Changes by Octavian
// A block memory pool for storing hypothesis - can choose to store DecHyp 
// or DecHypOnTheFly
// Changes by Octavian 20060417
#define DEFAULT_DECHYPPOOL_SIZE			5000

// Changes Octavian 20060627
#if 0
class DecHypPool
{
public:
   DecHypPool() ;
   DecHypPool( int reallocAmount_, bool isOnTheFly_ ) ;
   virtual ~DecHypPool() ;
   void *getSingleHyp() ;
   void returnSingleHyp( void *hyp ) ;
   bool isAllFreed() { return ( decHypPool->isAllFreed() ) ; } ;

private:
   int 		reallocAmount ;
   bool 	isOnTheFly ;
   BlockMemPool *decHypPool ;
};
#endif

/**
 * Pool for decoder hypotheses
 */
class DecHypHistPool
{
public:
   
   // Public member variables
   /*
	int				n_total ;
	int				n_free ;
	int				n_used ;
	int				realloc_amount ;
	int				n_allocs ;
	DecHypHist		**allocs ;
	DecHypHist		**free_elems ;
	*/

   // NOT USED - ONLY SO THAT SOME OLD CLASSES COMPILE - REMOVE ASAP
    //PNG bool 				stateHistMode ;


   // Constructors / destructor
   DecHypHistPool( int reallocAmount_ ) ;
   virtual ~DecHypHistPool() ;

   // OUT OF DATE - REMOVE ASAP
	// Creates the pool. 
	// 'max_size' is the initial size of the pool.
	// 'realloc_amount_' is the number of additional DecHypHist elements 
	//   that are to be allocated each time the pool empties.
	// If 'realloc_amount_' is undefined, then 'max_size' elements are
	//   allocated each time the pool empties.
	//PNG DecHypHistPool( int max_size , bool stateHistMode_ , int reallocAmount_=-1 ) ;

	// Public methods
	// Gets the next free DecHypHist element from the pool. Allocates
	//   new elements if the pool is empty.
	// Initialises the new DecHypHist element.
   void addLabelHistToDecHyp( DecHyp *hyp , int label_ ) ;
   
   // Changes Octavian 20060523
   // Used for on-the-fly hypotheses. Similar to addLabelHistToDecHyp()
   // Can add multiple labels to the history. Multiple labels can occur if
   // the epsilon arcs of the G transducers also have output labels.
   // The second argument of this function -- label_ is not used.
   void addLabelHistToDecHypOnTheFly( DecHypOnTheFly *hypOnTheFly , int label_ ) ;
   
   void addLatticeHistToDecHyp( DecHyp *hyp , int latState_ , real accScore_ ) ;
   void addHistToDecHyp( DecHyp *hyp , int state_ , real score_ , int time_ , 
								 real acousticScore_ = LOG_ZERO , real lmScore_ = LOG_ZERO ) ;

   void poolStats() ;
   static void initDecHyp( DecHyp *hyp, int state );

   // Changes Octavian
   // Initialize a DecHypOnTheFly object
   static void initDecHypOnTheFly( DecHypOnTheFly *hyp, int state_, int gState_ ) ;

   static bool isActiveHyp( DecHyp *hyp );
   static void registerLabel( DecHyp *hyp, int label );
   static void registerLabelOnTheFly( DecHypOnTheFly *hypOnTheFly ) ;


   void registerEnd( DecHyp *hyp, real score_, int time_, real acousticScore_=LOG_ZERO, 
                     real lmScore_=LOG_ZERO );

   void resetDecHyp( DecHyp *hyp ) ;

   // Changes Octavian
   // Reset a DecHypOnTheFly object
   void resetDecHypOnTheFly( DecHypOnTheFly *hyp ) ;
   void extendDecHyp( DecHyp *hyp , DecHyp *new_hyp , real score_ , 
							 real acousticScore_ = LOG_ZERO , real lmScore_ = LOG_ZERO ) ;

   // Changes Octavian
   // Extend a hypothesis to a new hypothesis
   void extendDecHypOnTheFly(
	 DecHypOnTheFly *hyp , DecHypOnTheFly *new_hyp , 
	 real score_ , real acousticScore_ = LOG_ZERO , 
	 real lmScore_ = LOG_ZERO , int currGState_ = -1 ,
	 real pushedWeight_ = 0.0 , int *nextOutLabel_ = NULL , 
	 int nNextOutLabel_ = 0 ) ; 
	
   void returnSingleElem( DecHypHist *elem ) ;
   void setLattice( WFSTLattice *lattice_ ) { lattice = lattice_ ; latticeMode = true ; } ;

private:
   
   // Private member variables
   int            reallocAmount ;
   BlockMemPool   *dhhPool ;
   BlockMemPool   *labPool ;
   BlockMemPool   *labTimePool ;
   BlockMemPool   *labTimeScorePool ;
   BlockMemPool   *labTime2ScorePool ;
   BlockMemPool   *latticePool ;

   bool           latticeMode ;
   WFSTLattice    *lattice ;
   
   // Private Methods
   void returnElem( DecHypHist *elem ) ;

};


}

#endif
