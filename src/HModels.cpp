/*
 * Copyright 2008,2009 by The University of Sheffield
 * and The University of Edinburgh
 *
 * See the file COPYING for the licence associated with this software.
 */

/*
 * HModels.cpp:
 *
 * This is the adapter class that connects HTK to Juicer via the API
 *
 * Much of the code is copied from HTKTools/HHEd.c or HTKLVrec/HDecode.c
 *
 * Environment variables for this module are:
 *
 *  Created on: 08-Aug-2008
 *      Author: Vincent Wan, University of Sheffield, UK.
 */

#include <assert.h>

#include "stdlib.h"
#include "HModels.h"
#include "LogFile.h"

namespace Juicer{

    /* The singleton */
    HModels* sHModels = 0;

/**
 * Includes from HTK
 */

#include "HSigP.h"
#include "HVQ.h"
#include "HUtil.h"
#include "HTrain.h"
#include "HAdapt.h"

/**
 * Constructor: We initialise this class by running HTK's init functions
 * following the same procedure in HTKLVrec/HDecode.c
 */
HModels::HModels()
{
    /*
     * Initialise some variables specified in the header file.
     * Copied and modified from HHEd.c
     */
	HTKMList = new char[1024];
	HTKMList[0] = '\0';
	hmmDir = NULL;
	hmmExt = NULL;
        inputXformDir = NULL;
	noAlias = FALSE;
	isHModelsinitialised = false;
	useHAdapt = false;
}

/**
 * Constructor: We initialise this class by running HTK's init functions
 * following the same procedure in HTKLVrec/HDecode.c
 */
HModels::HModels( const char * mlist )
{
    /*
     * Initialise some variables specified in the header file.
     * Copied and modified from HHEd.c
     */
	HTKMList = new char[1024];
	HTKMList[0] = '\0';
	hmmDir = NULL;
	hmmExt = NULL;
        inputXformDir = NULL;
	noAlias = FALSE;
	isHModelsinitialised = false;
	useHAdapt = false;
	SetHTKModelsList(mlist);
}

/**
 * Free up memory
 */
HModels::~HModels()
{
    delete[] HTKMList;
    if (isHModelsinitialised) {
        delete[] stateProbCache;
        for( int i=0;i<hSet.numTransP;i++ )
        {
             SEIndex* se = transMats[i].seIndexes;
             ++se; /* remember we index from 1 */
             delete[] se;
             int n = transMats[i].nStates;
             for (int j = 0; j < n; ++j)
                 delete[] transMats[i].trP[j];
             delete[] transMats[i].trP;
            for( int j=0;j<maxStates;j++ )
            {
                delete[] transMats[i].logProbs[j];
                delete[] transMats[i].sucs[j];
            }
			delete[] transMats[i].logProbs;
			delete[] transMats[i].sucs;
			delete[] transMats[i].nSucs;
		}
		delete[] HMMtransMat;
		delete[] transMats;
		delete[] GMMlookupTable;
		delete[] HMMlookupTable;
		delete[] HMMnames;
	    DeleteHeap( &hmmHeap );
	}
}


/**
 * Set the name of the file that stores the model list e.g.:
 * /share/spandh.ami1/ami/amiasr/asrcore/sys06/rt05seval/ihm/hmms/P1/xwrd.clustered.mlist
 */
void HModels::SetHTKModelsList( const char * htkModelsList ){
	strcpy(HTKMList,htkModelsList);
}

/**
 * Implementing the Juicer Models API function
 * Load for hybrid modelling stuff
 */
void HModels::Load( const char *phonesListFName , const char *priorsFName , int statesPerModel )
{
	HError(9999,"HModels::Load() - Hybrid models not supported.");
	exit(1);
}

/**
 * Implementing the Juicer Models API function
 * Load HTK models
 */
void HModels::Load( const char *htkModelsFName , bool removeInitialToFinalTransitions )
{
        noAlias = TRUE;                        // zap aliases in hmmList
        // hmmDir = NULL ;                     // Input HMM definition directory
        // hmmExt = NULL ;                     // Input HMM file extension

	/*
	 * Allocate HTK Heaps for models and Observation vector
	 */
	CreateHeap(&hmmHeap,"Model Heap",MSTAK,1,1.0,100000,1000000);
	CreateHMMSet(&hSet,&hmmHeap,TRUE);
	hset=&hSet;

	/*
	 * First load the models, sort out adaptation transform paths and remove logical models if desired
	 */
	AddMMF(hset,(char*)htkModelsFName) ;                          // Input MMF file name
        if ( (inputXformDir!=NULL) && (inputXformDir[0]!='\0') )      // Input transform directory
        {
             useHAdapt = true;
             xfInfo->useInXForm = TRUE;
             LogFile::printf( "Using input transform directories:\n" );
             // We have a colon separated list. 
             // Make a copy of the list and carve it up
             char *str = new char[strlen(inputXformDir)+1] ;
             strcpy( str , inputXformDir ) ;
             // Extract the paths
             char *ptr = strtok( str , ":" ) ;
             while ( ptr != NULL )
             {
                 char *ixd = new char[strlen(ptr)+1] ;
                 strcpy( ixd , ptr ) ;
                 LogFile::printf( "\t%s\n" , ixd );
                 AddInXFormDir(hset,ixd);
                 ptr = strtok( NULL , ":" ) ;
             }
             delete [] str;
        }
        if ( xfInfo->usePaXForm == TRUE ) 
        {
             useHAdapt = true;
             LogFile::printf( "Using parent transform directory:\n\t%s" , xfInfo->paXFormDir );
        }
	LogFile::printf( "\nHModels::Load loading MMF %s ... " , htkModelsFName );
	if(MakeHMMSet(hset,HTKMList)<SUCCESS)
		HError(2628,"Initialise: MakeHMMSet failed");
	if (noAlias) ZapAliases();
	if(LoadHMMSet(hset,hmmDir,hmmExt)<SUCCESS)
		HError(2628,"Initialise: LoadHMMSet failed");
	/* INVDIAGC is much faster than DIAGC, due to use * instead of / */
	ConvDiagC(hset,TRUE);

	/*
	 * Make the Observation structure for storing feature vectors.
	 */
	Boolean saveAsVQ = FALSE;
	Boolean eSep = FALSE;
	currentFrameData = MakeObservation(&hmmHeap,hSet.swidth,hSet.pkind,saveAsVQ, eSep);

	/*
	 * Set some useful variables
	 */
	maxStates = MaxStatesInSet(hset);
	maxMixes = MaxMixInSet(hset);
	nHMMs = hset->numPhyHMM;
	LogFile::printf("\n%d logical/%d physical models loaded [%d states max, %d mixes max]\n",
			hset->numLogHMM,nHMMs,maxStates,maxMixes);

	/*
	 * Copy the transition probability matrices out of HTK structures for faster access
	 * and create a look up table for converting indices to HMM pointers.
	 */
	InitialiseHModels( removeInitialToFinalTransitions );
	createTrP();
	createSEIndex();

	/*
	 * HModels is now initialised
	 */
	isHModelsinitialised = true;
}


/**
 * Allocate and initialise memory for transition matrices and HMM look up tables.
 */
void HModels::InitialiseHModels( bool removeInitialToFinalTransitions )
{
	/*
	 * First allocate memory
	 */
	HMMlookupTable = new int[ nHMMs * maxStates ];
	GMMlookupTable = new StreamElem*[ hSet.numStates + 1 ];
	HMMnames = new char*[ nHMMs ];
	HMMtransMat = new HModelsTransMatrix*[ nHMMs ];
	transMats = new HModelsTransMatrix[ hSet.numTransP ];
	for( int i=0;i<hSet.numTransP;i++ )
	{
		transMats[i].nStates  = -1;
		transMats[i].nSucs    = new int[ maxStates ];
		transMats[i].sucs     = new int*[ maxStates ];
		transMats[i].logProbs = new real*[ maxStates ];
		for( int j=0;j<maxStates;j++ )
		{
			assert(j < maxStates);
			transMats[i].sucs[j] = new int[ maxStates ];
			transMats[i].logProbs[j] = new real[ maxStates ];
		}
	}

	/*
	 * Now scan through hset creating a look up table of pointers to the HMMs (HLink)
	 * and reordering the transition matrices data for faster lookup.
	 */
	HMMScanState hss;
	LabId id;
	int h=0;
	NewHMMScan(hset,&hss);
	do
	{
		if ( h >= nHMMs ) HError( 9999 , "Index exceeds number of physical HMMs");
		HMMtransMat[h] = getTransMatPtr( hss.hmm , removeInitialToFinalTransitions );
		id = hss.mac->id;
		HMMnames[h] = id->name;
		/*
		 * Scan through the HMM creating a look up table of pointers to
		 * the GMMs of each state (StateElem)
		 */
		for (int s=1;s<hss.hmm->numStates-1;s++)
		{
			int gmmInd=hss.hmm->svec[s+1].info->sIdx;
			HMMlookupTable[h*maxStates+s] = gmmInd;
			GMMlookupTable[gmmInd]=&(hss.hmm->svec[s+1].info->pdf[1]);
		}
		h++;
	}
	while(GoNextHMM(&hss));
	/*
	 * Allocate memory for state probability caching
	 * Add one because HTK counts its states from 1 not 0.
	 */
	stateProbCache = new HModelCacheElement[ hset->numStates + 1 ];
	for ( int i=0 ; i<=hSet.numStates ; i++ )
	{
		stateProbCache[i].isEmpty = true;
	}
}


/**
 * Implementing the Juicer Models API function
 * Read a binary models file
 */
void HModels::readBinary( const char *fName )
{
	HError(9999,"HModels::readBinary(%s) - Not implemented.",fName);
	exit(1);
}


/**
 * Implementing the Juicer Models API function
 * Write a binary models file.
 */
void HModels::output( const char *fName , bool outputBinary )
{
    LogFile::printf( "\nHModels::output(%s) - Not implemented ... " , fName );
}


/**
 * Implementing the Juicer Models API function
 * Write model statistics to specified file.
 */
void HModels::outputStats( FILE *fd )
{
    LogFile::puts( "\nHModels::outputStats() - Not implemented.\n" );
}

/**
 * Implementing the Juicer Models API function
 * Tells this class that the decoder has moved to a new frame.
 */
void HModels::newFrame( int frame , real **input, int nFrame)
{
    assert(nFrame > 0);
	/*
	 * Store the index of this frame
	 */
	currentFrameIndex = frame;
	/*
	 * Copy the new frame of data
	 *     In HTK fv[1] refers to stream 1
	 *            fv[1][0] recast as an int is the size of the feature vector
	 *            fv[1][1...size] stores the feature vector
	 */

	int *dim = (int*)currentFrameData.fv[1];
	*dim = hSet.vecSize;
	memcpy(currentFrameData.fv[1]+1,input[0],hSet.vecSize*sizeof(real));
	/*
	 * Clear the state probability cache
	 */
	for ( int i=0 ; i<=hSet.numStates ; i++ )
		stateProbCache[i].isEmpty = true;
}


/**
 * Implementing the Juicer Models API function
 */
real HModels::calcOutput( int hmmInd , int stateInd )
{
	/*
	 * Get the gmmInd of the state and calculate its output probability
	 */
	return calcOutput( HMMlookupTable[hmmInd*maxStates+stateInd] );
}


/**
 * Implementing the Juicer Models API function
 * Calculate the output probability given an index to a GMM
 */
real HModels::calcOutput( int gmmInd )
{
    /*
     * 1: Check if the output probability has been cached.
     */
    if ( stateProbCache[gmmInd].isEmpty )
    {
        /*
         * 2: Get the GMM corresponding to gmmInd from GMMlookupTable.
         * 3: Get the probability distribution function of stream 1.
         * 4: Call HTK's SoutP to calculate the output probability for
         *    stream 1 of currentFrameData using that pdf.
         */
        stateProbCache[gmmInd].isEmpty = false;

        // If we are adapting then we need to tell HTK to apply any feature based transforms
        if ( useHAdapt )
        {
            LogFloat det , wt , px;
            StreamElem *se = GMMlookupTable[gmmInd];
            MixtureElem *me = se->spdf.cpdf+1;
            if ( se->nMix == 1 ) 
            {
                stateProbCache[gmmInd].logProb = MOutP( ApplyCompFXForm( me->mpdf , currentFrameData.fv[1] ,
                                                                         xfInfo->inXForm , &det , currentFrameIndex ) , 
                                                        me->mpdf );
                stateProbCache[gmmInd].logProb += det;
            } else {
                stateProbCache[gmmInd].logProb = LZERO;
                for (int m=1; m<=se->nMix; m++,me++) {
                    wt = MixLogWeight(hset, me->weight);
                    if (wt>LMINMIX) {   
                        px = MOutP( ApplyCompFXForm( me->mpdf , currentFrameData.fv[1] ,                                                          
                                                     xfInfo->inXForm , &det , currentFrameIndex ) ,
                                    me->mpdf );
                        px += det;
                        stateProbCache[gmmInd].logProb=LAdd(stateProbCache[gmmInd].logProb,wt+px);
                    }
                }
            }
        } else {
            stateProbCache[gmmInd].logProb = SOutP( hset , 1 , &currentFrameData , GMMlookupTable[gmmInd] );
        }
    }
    return stateProbCache[gmmInd].logProb;
}


/**
 * Implementing the Juicer Models API function
 * Return the number of HMMs
 *
 * HTK has physical models and logical models.  We only need to return the number
 * of physical models for Juicer. And if htkargs in main() has the -z option then
 * the logical HMMs are removed anyway.
 */
int HModels::getNumHMMs()
{
	return nHMMs;
}


/**
 * Implementing the Juicer Models API function
 * Returns the current frame index. This is the value specified by
 * the last call to newFrame( int frame , const real * input )
 */
int HModels::getCurrFrame()
{
	return currentFrameIndex;
}


/**
 * Implementing the Juicer Models API function
 */
const char* HModels::getHMMName( int hmmInd )
{
	return HMMnames[hmmInd];
}


/**
 * Implementing the Juicer Models API function
 * Returns the size of the input feature vector required by the HMMs.
 */
int HModels::getInputVecSize()
{
	return hSet.vecSize;
}


/**
 * Implementing the Juicer Models API function
 * Returns the number of states in the specified HMM
 */
int HModels::getNumStates(int hmmInd)
{
	return HMMtransMat[hmmInd]->nStates;
}


/**
 * Implementing the Juicer Models API function
 */
int HModels::getNumSuccessors(int hmmInd, int stateInd)
{
    return HMMtransMat[hmmInd]->nSucs[stateInd];
}


/**
 * Implementing the Juicer Models API function
 */
int HModels::getSuccessor(int hmmInd, int stateInd, int sucInd)
{
    return HMMtransMat[hmmInd]->sucs[stateInd][sucInd];
}


/**
 * Implementing the Juicer Models API function
 */
real HModels::getSuccessorLogProb(int hmmInd, int stateInd, int sucInd)
{
    return HMMtransMat[hmmInd]->logProbs[stateInd][sucInd];
}


/**
 * Translate the HTK transition matrices to speed up Juicer access.
 */
HModelsTransMatrix *HModels::getTransMatPtr( HLink hmm , bool removeInitialToFinalTransitions_ )
{
	int t = hmm->tIdx-1;
	/*
	 * Check if transition matrix has already been initialised
	 */
	if ( transMats[t].nStates > -1 ) return &transMats[t];
	/*
	 * If not then initalise it
	 */
	transMats[t].nStates  = hmm->numStates;
	/*
	 * For each state in the HMM
	 */
	for ( int ss=0 ; ss<hmm->numStates ; ss++ )
	{
		/*
		 * Count the number of successors for each state
		 */
		int successors = 0;
		for (int es=0 ; es < hmm->numStates ; es ++ )
			if ( hmm->transP[ss+1][es+1] > LZERO )
				if (!( (ss==0)&&(es==(hmm->numStates-1))&&removeInitialToFinalTransitions_ ))
					successors++;
		transMats[t].nSucs[ss] = successors;
		/*
		 * Loop over the successor states and copy the non-zero transition probabilities
		 */
		successors=0;
		for (int es=0 ; es < hmm->numStates ; es++ )
		{
			if ( hmm->transP[ss+1][es+1] > LZERO )
			{
				if ( ( (ss==0)&&(es==(hmm->numStates-1))&&removeInitialToFinalTransitions_ ) ){
					if ( hmm->transP[ss+1][es+1] >= 0.0 )
						HError( -9999 , "HModels::getTransMatPtr - initial-final transition had prob. >= 1.0");
				}
				else
				{
					transMats[t].logProbs[ss][successors] = hmm->transP[ss+1][es+1];
					transMats[t].sucs[ss][successors] = es;
					successors++;
				}
			}
		}
	}
	return &transMats[t];
}

/**
 *  ZapAliases: reduce hList to just distinct physical HMM's
 *  Unmodified copy from HHEd.c
 */
void HModels::ZapAliases(void)
{
   int h;
   MLink q;
   HLink hmm;
   int fidx=0;            /* current macro file id */

   for (h=0; h<MACHASHSIZE; h++)
      for (q=hset->mtab[h]; q!=NULL; q=q->next)
         if (q->type=='l')
            DeleteMacro(hset,q);

   for (h=0; h<MACHASHSIZE; h++)
      for (q=hset->mtab[h]; q!=NULL; q=q->next)
         if (q->type=='h') {
            hmm=(HLink) q->structure;
            NewMacro(hset,fidx,'l',q->id,hmm);
            hmm->nUse=1;
         }
}

void HModels::createTrP() {
    // adhoc function, maybe better integerated later
    for (int i = 0; i < hSet.numTransP; ++i) {
        HModelsTransMatrix& tm = transMats[i];
        int n = tm.nStates;
        real** trP = new real*[n];
        for (int j = 0; j < n; ++j) {
            trP[j] = new real[n];
            for (int k = 0; k < n; ++k)
                trP[j][k] = LOG_ZERO;
        }

        for (int j = 0; j < n; ++j) {
            for (int sucId = 0; sucId < tm.nSucs[j]; ++sucId) {
                trP[j][tm.sucs[j][sucId]] = tm.logProbs[j][sucId];
            }
        }
        tm.trP = trP;
    }
}

void HModels::createSEIndex() {
    real** trP;
    SEIndex* se;
    int N, min, max;
    for (int i = 0; i < hSet.numTransP; ++i) {
        HModelsTransMatrix& tm = transMats[i];
        trP = tm.trP;
        assert(trP);
        N = tm.nStates;
        se = new SEIndex[N-1];
        --se; /* we index from 1, as state 0 does not need a SEIndex */
        for (int j = 1; j < N; ++j) {
            int min, max;
            for (min = (j == N-1?1:0); min < N-1; ++min) /* tee transition is not dealt with here */
                if (trP[min][j] > LOG_ZERO)
                    break;
            for (max = N-1; max >= 1; --max)
                if (trP[max][j] > LOG_ZERO)
                    break;
            se[j].start = min;
            se[j].end = max+1;
        }
        tm.seIndexes = se;
    }
}

void HModels::setBlockSize(int bs) {
    fprintf(stderr,"HModels::setBlockSize(int bs) not implemented\n");
    exit(-1);
}
}
