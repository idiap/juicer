/*
 * Copyright 2004 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * Copyright 2008 by The University of Edinburgh
 *
 * See the file COPYING for the licence associated with this software.
 */

#ifndef WFST_CD_GEN_INC
#define WFST_CD_GEN_INC

#include "general.h"
#include "MonophoneLookup.h"
#include "HTKModels.h"


/*
	Author:	Darren Moore (moore@idiap.ch)
	Date:	   18 November 2004
	$Id: WFSTCDGen.h,v 1.7 2005/10/20 02:11:19 moore Exp $
*/


namespace Juicer
{


    typedef enum
    {
        WFST_CD_TYPE_MONOPHONE = 0 ,
        WFST_CD_TYPE_MONOPHONE_ANN ,
        WFST_CD_TYPE_XWORD_TRIPHONE ,
        WFST_CD_TYPE_XWORD_TRIPHONE_NDI ,   // NDI = NON DETERMINISTIC INVERSE
        WFST_CD_TYPE_INVALID
    } WFSTCDType ;



    struct WFSTCDSMNode
    {
        int   monophone ;
        int   state ;  // the state ID we have allocated to
        int   nextSib ;
        int   firstChild ;
    };


    class WFSTCDStateManager
    {
    public:
        WFSTCDStateManager( MonophoneLookup *monoLookup_ , int nAuxSyms_ ) ;
        virtual ~WFSTCDStateManager() ;

        int getWFSTState( int nMonophones , int *monophones ,
                          bool addNew=true , bool *isNew=NULL ) ;
        int getNumStates() { return nStates ; } ;
        int getEpsState() { return epsilonState ; } ;
        void outputText() ;

    private:
        MonophoneLookup   *monoLookup ;
        int               nAuxSyms;
        int               nMonoPlusAux;

        int               *firstLevelNodes ;

        int               nNodes ;
        int               nNodesAlloc ;
        WFSTCDSMNode      *nodes ;

        int               nStates ;
        int               epsilonState ;

        int getNode( int parentNode , int monophone , bool addNew ) ;
        int allocNode( int word ) ;
        void initNode( int ind ) ;
        void outputNode( int node , int *nMonophones , int *monophones ) ;
    };


/**
 * Generates a context dependency transducer.
 *
 * There is a \#define AUXLOOP in the code that selects whether to
 * handle auxiliary symbols as loops or not.  Not implies that they
 * will be handled using delay transitions like the short pause.
 */
    class WFSTCDGen
    {
    public:
        WFSTCDGen( WFSTCDType type_ , const char *htkModelsFName ,
                   const char *monophoneListFName , const char *silMonoStr ,
                   const char *pauseMonoStr ,
                   const char *tiedListFName , const char *sepChars ,
                   const char *priorsFName=NULL , int statesPerModel=0 ) ;
        virtual ~WFSTCDGen() ;

        void outputText() {}
        void writeFSM( const char *fsmFName , const char *inSymbolsFName ,
                       const char *outSymbolsFName ,
                       const char *lexInSymbolsFName ) ;

    private:
        WFSTCDType           type ;

        MonophoneLookup      *monoLookup ;
        PhoneLookup          *phoneLookup ;
        IModels               *models ;

        int                  nStates ;
        int                  nAuxSyms ;
        int                  inAuxSymsBase ;
        int                  outAuxSymsBase ;

        void writeFSMMonophone( FILE *fsmFD ) ;
        void writeFSMXWordTriphoneOld( FILE *fsmFD, bool ciSil, bool ciPause );
        void writeFSMXWordTriphoneDetInv( FILE *fsmFD, bool ciPause );
        void writeFSMXWordTriphoneNonDetInv(FILE *fsmFD, bool ciSil,
                                            bool ciPause);
        void writeFSMAuxTrans( FILE *fsmFD );

    };


}

#endif
