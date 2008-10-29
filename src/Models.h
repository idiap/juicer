/*
 * Copyright 2008 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * Copyright 2008 by The University of Edinburgh
 *
 * See the file COPYING for the licence associated with this software.
 */

#ifndef MODELS_H
#define MODELS_H

#include "general.h"
#include "htkparse.h"

namespace Juicer
{
    /**
     * Models interface.
     * Models such as HTK MMFs should implement this interface.
     */
    class Models
    {
    public:
        virtual ~Models() {}
        virtual void Load(
            const char *phonesListFName,
            const char *priorsFName,
            int statesPerModel
        ) = 0;
        virtual void Load(
            const char *htkModelsFName,
            bool removeInitialToFinalTransitions_ = false
        ) = 0;

        virtual void readBinary( const char *fName ) = 0 ;
        virtual void output( const char *fName, bool outputBinary ) = 0 ;
        virtual void newFrame( int frame , const real *input ) = 0 ;

        virtual real calcOutput( int hmmInd , int stateInd ) = 0 ;
        virtual real calcOutput( int gmmInd ) = 0 ;

        virtual int getNumHMMs() = 0 ;
        virtual int getCurrFrame() = 0 ;
        virtual const char* getHMMName( int hmmInd ) = 0 ;
        virtual int getInputVecSize() = 0 ;

        // To replace DecodingHMM
        virtual int getNumStates(int hmmInd) = 0;
        virtual int getNumSuccessors(int hmmInd, int stateInd) = 0;
        virtual int getSuccessor(int hmmInd, int stateInd, int sucInd) = 0;
        virtual real getSuccessorLogProb(
            int hmmInd, int stateInd, int sucInd
        ) = 0;
        virtual real getTeeLogProb(int hmmInd) = 0;
    };
}

#endif /* MODELS_H */
