/*
 * Models.h
 *
 *  Created on: 12-Aug-2008
 *      Author: vinny
 */

#ifndef MODELS_H_INC
#define MODELS_H_INC

#include "general.h"
#include "htkparse.h"

/**
 * Models interface.
 * Models such as HTK MMFs should implement this interface.
 *
 * TODO: Put this class in a separate header file and use
 *       an include directive
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
        virtual real getSuccessorLogProb(int hmmInd, int stateInd, int sucInd) = 0;
#ifdef OPTIMISE_TEEMODEL
        virtual real getTeeWeight(int hmmInd) = 0;
#endif
    };

 #endif /* MODELS_H_INC */
