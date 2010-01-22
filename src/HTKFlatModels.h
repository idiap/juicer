/*
 * Copyright 2008 by The University of Edinburgh
 *
 * See the file COPYING for the licence associated with this software.
 */

/*
 * vi:ts=4:tw=78:shiftwidth=4:expandtab
 * vim600:fdm=marker
 *
 * HTKFlatModels.h  -  store GMM parameters in flat arrays for fast
 * computation, derived from HTKModels.
 * 
 */

#ifndef _HTKFLATMODELS_H
#define _HTKFLATMODELS_H

#include "HTKModels.h"

namespace Juicer
{
    // to avoid name confusion with those in HTKModels, all
    // declarations here have a prefix F or f
    typedef struct {
        int  compNum;   // Gaussian component numbers for this GMM
        int  compInd;    // compInd of the first component
    } FMixture;

    class HTKFlatModels : public HTKModels {
    public:
        HTKFlatModels();
        virtual ~HTKFlatModels();
        virtual void init(); // initialise private data from loaded HTKModels
        void readBinary( const char *fName );
        void Load( const char *htkModelsFName ,
                   bool removeInitialToFinalTransitions_=false ) ;
        real calcOutput( int gmmInd ) ; // new version of GMM obversion calculation
        real calcOutput( int hmmInd , int stateInd ); // new version of GMM obversion calculation
        void newFrame( int frame , real **input, int nFrame);
        void setBlockSize(int bs);

    protected:
        int fvecSize4;
        int fnMixtures4;
        int fnGaussians;
        int fnGaussians4;
        FMixture *fMixtures;         // mixture structures with the same order as in HTKModels::mixtures
        // real *fWeights;          // Gaussian weights, no longer needed, // added to dets
        real *fDets;             // Gaussian determinants (gconst + log weight)
        real *fMeans;            // Gaussian means
        real *fVars;             // Gaussian variances
        void* fBuffer;            // data buffer

        int fnBlock;
        real* fCache;            // block cache
        int* fCacheT;            // time of last calculation
        real** currInputData;
        int currInputLen;

        real *fMean(int gmmId)   {return fMeans+fvecSize4*fMixtures[gmmId].compInd;}
        real *fVar(int gmmId)    {return fVars+fvecSize4*fMixtures[gmmId].compInd;}
        real *fDet(int gmmId)    {return fDets+fMixtures[gmmId].compInd;}
        // real *fWeight(int gmmId) {return fWeights+fMixtures[gmmId].compInd;}
        real calcGMMOutput( int gmmInd );
        real logAdd(real x, real y);
    };

}; // namespace juicer
#endif /* ifndef _HTKFLATMODELS_H */

