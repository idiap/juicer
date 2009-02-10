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

#include "HModels.h"
#include "LogFile.h"

namespace Juicer{

/**
 * Includes from HHEd.c
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
	HTKCFG = new char[1024];
	HTKMMF = new char[1024];
	HTKMList = new char[1024];
	HTKCFG[0] = '\0';
	HTKMMF[0] = '\0';
	HTKMList[0] = '\0';
	hmmDir = NULL;
	hmmExt = NULL;
	noAlias = FALSE;
	isHTKinitialised = false;

	HError(-9999,"Juicer::HModels::HModels() - TODO SetHTKCFG()");
	// TODO: The next function should be called by juicer not here in the constructor.
	SetHTKCFG("data/HModels.cfg");
}

/**
 * Free up memory
 */
HModels::~HModels()
{
    delete[] HTKCFG;
    delete[] HTKMMF;
    delete[] HTKMList;
    if (isHTKinitialised) {
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
 * Set the name of the file that stores the HTK configuration e.g.:
 */
void HModels::SetHTKCFG( const char * configFName ){
	strcpy(HTKCFG,configFName);
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
	/*
	 * If HTKCFG is not set then exit
	 */
	if (HTKCFG[0]=='\0')
	{
		HError(9999,"\nJuicer::HModels::Load() - HTK config not set");
	}
	/*
	 * Store MMF filename in class
	 */
	strcpy(HTKMMF,htkModelsFName);
	/*
	 * If HTKMList is not set then assume that the file is htkModelsFName + ".mlist"
	 */
	if (HTKMList[0]=='\0')
	{
		strcpy(HTKMList,htkModelsFName);
		strcat(HTKMList,".mlist");
		LogFile::printf( "\nHModels::Load() HTKMList not set - using default %s ... " , HTKMList );
	}
    /*
     * Initialise HTK.
     */
    InitialiseHTK();
	/*
	 * Now load the models and make the Observation structure for storing feature vectors
	 */
    InitialiseHMMSet();
	/*
	 * Copy the transition probability matrices out of HTK structures for faster access
	 * and create a look up table for converting indices to HMM pointers.
	 */
	InitialiseHModels( removeInitialToFinalTransitions );
	/*
	 * HTK is now initialised
	 */
	isHTKinitialised = true;
    createTrP();
    createSEIndex();
}

/**
 *  -------------------- Initialisation ---------------------
 *  Copied and modified from HHEd.c
 */
void HModels::InitialiseHTK()
{
	/**
     * Simulate command line options
	 * 	USAGE: HHEd [options] hmmList
	 *
	 *	 Option                                       Default
	 *
	 *	 -d s    dir to find hmm definitions          current
	 *	 -x s    extension for hmm files              none
	 *	 -z      zap aliases in hmmList
	 *	 -A      Print command line arguments         off
	 *	 -B      Save HMMs/transforms as binary       off
	 *	 -C cf   Set config file to cf                default
	 *	 -D      Display configuration variables      off
	 *	 -H mmf  Load HMM macro file mmf
	 *	 -S f    Set script file to f                 none
	 **/
    int argc = 6;
    char *argv[argc];
    argv[0] = "Juicer::HModels";
    argv[1] = "-C";
    argv[2] = HTKCFG;
    argv[3] = "-H";
    argv[4] = HTKMMF;
    argv[5] = "-z";
    /*
     * Standard HTK initialisation
     */
    char *hhed_version = "!HVER!HHEd:   3.4 [CUED 25/04/06]";
    char *hhed_vc_id = "$Id: HHEd.c,v 1.2 2006/12/07 11:09:08 mjfg Exp $";
    if(InitShell(argc,argv,hhed_version,hhed_vc_id)<SUCCESS)
		HError(2600,"HHEd: InitShell failed");
	InitMem();
	InitLabel();
	InitMath();
	InitSigP();
	InitWave();
	InitAudio();
	InitVQ();
	InitModel();
	if(InitParm()<SUCCESS)
		HError(2600,"HHEd: InitParm failed");
	InitUtil();
    /*
	 * Allocate HTK Heaps for models and Observation vector
	 */
	CreateHeap(&hmmHeap,"Model Heap",MSTAK,1,1.0,100000,1000000);
	CreateHMMSet(&hSet,&hmmHeap,TRUE);
	hset=&hSet;
	/*
	 * Parse simulated command line options argc/argv
	 * that are not handled by HTKLib
	 */
	ParseArgs();
}

/**
 *  -------------------- Initialisation ---------------------
 *  Copied and modified from HHEd.c
 *  Parse simulated command line options argc/argv
 *  that are not handled by HTKLib
 */
void HModels::ParseArgs()
{
	char *s;
	while (NextArg() == SWITCHARG) {
		s = GetSwtArg();
		if (strlen(s)!=1)
			HError(2619,"HHEd: Bad switch %s; must be single letter",s);
		switch(s[0]) {
		case 'd':
			if (NextArg()!=STRINGARG)
				HError(2619,"HHEd: Input HMM definition directory expected");
			hmmDir = GetStrArg();
			break;
		case 'x':
			if (NextArg()!=STRINGARG)
				HError(2619,"HHEd: Input HMM file extension expected");
			hmmExt = GetStrArg();
			break;
		case 'z':
			noAlias = TRUE;
			break;
		case 'J':
			if (NextArg()!=STRINGARG)
				HError(2319,"HERest: input transform directory expected");
			AddInXFormDir(hset,GetStrArg());
			break;
		case 'H':
			if (NextArg()!=STRINGARG)
				HError(2619,"HHEd: Input MMF file name expected");
			AddMMF(hset,GetStrArg());
			break;
		default:
			HError(2619,"HHEd: Unknown switch %s",s);
		}
	}
	if (NumArgs()>0)
		HError(2619,"HHEd: Unexpected extra args on command line");
}

/**
 *  -------------------- Initialisation ---------------------
 *  Copied and modified from HHEd.c
 *  Load the HMMs from disk
 */
void HModels::InitialiseHMMSet()
{
	/*
	 * Load the models, removing logical models if desired
	 */
    LogFile::printf( "\nHModels::Load loading MMF %s ... " , HTKMMF );
	if(MakeHMMSet(&hSet,HTKMList)<SUCCESS)
		HError(2628,"Initialise: MakeHMMSet failed");
	if (noAlias) ZapAliases();
	if(LoadHMMSet(&hSet,hmmDir,hmmExt)<SUCCESS)
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
	LogFile::printf("\n%d logical/%d physical models loaded [%d states max, %d mixes max] ... ",
			hset->numLogHMM,nHMMs,maxStates,maxMixes);
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
void HModels::newFrame( int frame , const real *input )
{
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
	memcpy(currentFrameData.fv[1]+1,input,hSet.vecSize*sizeof(real));
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
		stateProbCache[gmmInd].logProb = SOutP( hset , 1 , &currentFrameData , GMMlookupTable[gmmInd] );
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

}
