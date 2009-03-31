/*
 * Copyright 2004,2008 by IDIAP Research Institute
 *                        http://www.idiap.ch
 *
 * Copyright 2008 by The University of Sheffield
 *
 * Copyright 2008 by The University of Edinburgh
 *
 * See the file COPYING for the licence associated with this software.
 */

#ifndef HTKMODELS_INC
#define HTKMODELS_INC

#include "Models.h"

/*
  Author:	Darren Moore (moore@idiap.ch)
  Date:		15 Nov 2004
*/

namespace Juicer
{

    struct MeanVec
    {
        char  *name ;
        real  *means ;
    };

    struct VarVec
    {
        char  *name ;
        real  *vars ;
        real  *minusHalfOverVars ;
        real  sumLogVarPlusNObsLog2Pi ;
    };

    struct TransMatrix
    {
        char  *name ;
        int   nStates ;
        int   *nSucs ;
        int   **sucs ;
        real  **probs ;
        real  **logProbs ;
        SEIndex *seIndexes; /* N-1 entries: seIndexes[1,..,N-1] */
        real **trP;
    };

    struct Mixture
    {
        char  *name ;
        int   nComps ;
        int   *meanVecInds ;
        int   *varVecInds ;
        real  *currCompOutputs ;
        bool  currCompOutputsValid ;
    };

    struct GMM
    {
        char  *name ;
        int   mixtureInd ;
        real  *compWeights ;
        real  *logCompWeights ;
    };

    /**
     * HMM representation
     */
    struct HMM
    {
        char  *name ;
        int   nStates ;
        int   *gmmInds ;
        int   transMatrixInd ;
        real teeWeight;
    };


    /**
     * HTK models
     */
    class HTKModels : public Models
    {
    public:
        HTKModels() ;
        virtual ~HTKModels() ;

        void Load( const char *phonesListFName , const char *priorsFName ,
                   int statesPerModel ) ;
        void Load( const char *htkModelsFName ,
                   bool removeInitialToFinalTransitions_=false ) ;

        void readBinary( const char *fName ) ;
        void output( const char *fName , bool outputBinary ) ;
        void outputStats( FILE *fd=stdout ) ;
        void newFrame( int frame , real **input, int nFrame);
        void setBlockSize(int bs);
        real calcOutput( int hmmInd , int stateInd ) ;
        real calcOutput( int gmmInd ) ;

        int getNumHMMs() { return nHMMs ; } ;
        int getCurrFrame() { return currFrame ; } ;
        const char* getHMMName( int hmmInd ) { return hMMs[hmmInd].name ; } ;
        int getInputVecSize() { return vecSize ; } ;

        int getNumStates(int hmmInd) { return hMMs[hmmInd].nStates; }
        int getNumSuccessors(int hmmInd, int stateInd)
        {
            int transMatrixInd = hMMs[hmmInd].transMatrixInd;
            return transMats[transMatrixInd].nSucs[stateInd];
        }
        int getSuccessor(int hmmInd, int stateInd, int sucInd)
        {
            int transMatrixInd = hMMs[hmmInd].transMatrixInd;
            return transMats[transMatrixInd].sucs[stateInd][sucInd];
        }
        real getSuccessorLogProb(int hmmInd, int stateInd, int sucInd)
        {
            int transMatrixInd = hMMs[hmmInd].transMatrixInd;
            return transMats[transMatrixInd].logProbs[stateInd][sucInd];
        }
        real getTeeLogProb(int hmmInd) { return hMMs[hmmInd].teeWeight; }
        real** getTransMat(int hmmInd) {
            int transMatrixInd = hMMs[hmmInd].transMatrixInd;
            return transMats[transMatrixInd].trP;
        }
        SEIndex* getSEIndex(int hmmInd) {
            int transMatrixInd = hMMs[hmmInd].transMatrixInd;
            return transMats[transMatrixInd].seIndexes;
        }

    protected:
        int            currFrame ;
        const real     *currInput ;
        int            vecSize ;

        int            nMeanVecs ;
        int            nMeanVecsAlloc ;
        MeanVec        *meanVecs ;

        int            nVarVecs ;
        int            nVarVecsAlloc ;
        VarVec         *varVecs ;

        bool           removeInitialToFinalTransitions ;
        int            nTransMats ;
        int            nTransMatsAlloc ;
        TransMatrix    *transMats ;

        int            nMixtures ;
        int            nMixturesAlloc ;
        Mixture        *mixtures ;

        int            nGMMs ;
        int            nGMMsAlloc ;
        GMM            *gMMs ;
        real           *currGMMOutputs ;

        int            nHMMs ;
        int            nHMMsAlloc ;
        HMM            *hMMs ;

        FILE           *inFD ;
        FILE           *outFD ;
        bool           fromBinFile ;

        bool           hybridMode ;
        real           *logPriors ;

        void initFromHTKParseResult() ;

        void createTrPandSEIndex();

        int addHMM( HTKHMM *hmm ) ;
        int addGMM( HTKHMMState *st ) ;
        int getGMM( const char *name ) ;
        int addMixture( HTKMixturePool *mix ) ;
        int addMixture( int nComps , HTKMixture **comps ) ;
        int getMixture( const char *name ) ;
        int addMeanVec( const char *name , real *means ) ;
        int addVarVec( const char *name , real *vars ) ;
        int addTransMatrix( const char *name , int nStates , real **trans ) ;
        int getTransMatrix( const char *name ) ;

        void outputHMM( int ind , bool outputBinary ) ;
        void outputGMM( int ind , bool isRef , bool outputBinary ) ;
        void outputMixture(
            int mixInd , real *compWeights , bool isRef , bool outputBinary
        ) ;
        void outputMeanVec( int ind , bool isRef , bool outputBinary ) ;
        void outputVarVec( int ind , bool isRef , bool outputBinary ) ;
        void outputTransMat( int ind , bool isRef , bool outputBinary ) ;

        void readBinaryHMM() ;
        void readBinaryGMM() ;
        void readBinaryMixture() ;
        void readBinaryMeanVec() ;
        void readBinaryVarVec() ;
        void readBinaryTransMat() ;

        inline real calcGMMOutput( int gmmInd ) ;
        inline real calcMixtureOutput(
            int mixInd , const real *logCompWeights
        ) ;

        char* transBuffer; // single buffer for trP and SEIndex
    };

    void testModelsIO(
        const char *htkModelsFName , const char *phonesListFName ,
        const char *priorsFName , int statesPerModel
    ) ;


}


#endif


