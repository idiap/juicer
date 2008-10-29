/*
 * Copyright 2004,2008 by IDIAP Research Institute
 *                        http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#ifndef WFST_MODEL_INC
#define WFST_MODEL_INC

#include "general.h"
#include "Models.h"
#include "DecHypHistPool.h"
#include "WFSTNetwork.h"

/*
  Author:		Darren Moore (moore@idiap.ch)
  Date:			14 October 2004
*/

/*
  Author: Octavian Cheng (ocheng@idiap.ch)
  Date: 		6 June 2006
*/

namespace Juicer
{

    /**
     * Each model is associated with a transition in the WFST
     */
    struct WFSTModel
    {
        WFSTTransition    *trans ;
        int hmmIndex;
        DecHyp			  *hyps1 ;
        DecHyp			  *hyps2 ;
        DecHyp			  *prevHyps ; // Current and previous hypotheses for each
        DecHyp			  *currHyps ; //   state in the phone model.
        int				  nActiveHyps ;

        // We also have a straight linked list of all active WFSTModel
        // elements.
        WFSTModel					*next ;
    };


    /**
     * Fixed Number of States pool
     */
    struct WFSTModelFNSPool		// FNS = Fixed Number of States
    {
        // All elements in this pool are for models with this many states.
        int						nStates ;
        int						nTotal ;
        int						nUsed ;
        int						nFree ;
        int						reallocAmount ;
        int						nAllocs ;
        WFSTModel				**allocs ;
        WFSTModel				**freeElems ;
    };


    /**
     * Pool of models
     */
    class WFSTModelPool
    {
    public:
        // Public member variables
        Models *models ;
        DecHypHistPool *decHypHistPool ;

        // Constructors / destructor
        WFSTModelPool() ;
        WFSTModelPool( Models *models_ , DecHypHistPool *decHypHistPool_ ) ;
        virtual ~WFSTModelPool() ;

        // Public methods
        WFSTModel *getElem( WFSTTransition *trans ) ;
        void returnElem( WFSTModel *elem ) ;
        int numUsed() { return nUsed ; } ;

    protected:
        // Private member variables
        int maxNStates ;		// max number of states in any phone model.
        int nUsed ;

        // Changes by Octavian - making two of the following virtual
        virtual void initFNSPool( int poolInd , int nStates , int initReallocAmount ) ;
        virtual void allocPool( int poolInd ) ;

        // Changes by Octavian
    private:
        WFSTModelFNSPool *pools ;	// different pools for models with diff num states.
        void initElem( WFSTModel *elem , int nStates ) ;
        void resetElem( WFSTModel *elem , int nStates ) ;
        void checkActive( WFSTModel *elem ) ;
    };

}

#endif

