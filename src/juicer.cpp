/*
 * Copyright 2004 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * Copyright 2008 by The University of Sheffield
 *
 * Copyright 2008 by The University of Edinburgh
 *
 * See the file COPYING for the licence associated with this software.
 */

#include "CmdLine.h"
#include "DecVocabulary.h"
#include "WFSTNetwork.h"
#include "Decoder.h"
#include "DecoderBatchTest.h"
#include "MonophoneLookup.h"
#include "LogFile.h"

#include "config.h"

#include <HTKLib.h>
#ifdef HAVE_HTKLIB
  namespace Juicer 
  {
#   include "HShell.h"
#   include "HMem.h"
#   include "HMath.h"
#   include "HAudio.h"
#   include "HWave.h" 
#   include "HParm.h"
#   include "HLabel.h"
#   include "HModel.h"
#   include "HSigP.h"
#   include "HVQ.h"
#   include "HUtil.h"
#   include "HTrain.h"
#   include "HAdapt.h"
  }
# include "HModels.h"
#endif
#ifdef OPT_FLATMODEL
# include "HTKFlatModels.h"
# include "HTKFlatModelsThreading.h"
#else
# include "HTKModels.h"
#endif

#include "WFSTDecoder.h"
#include "WFSTDecoderLite.h"
#include "WFSTDecoderLiteThreading.h"

#ifdef WITH_ONTHEFLY
# include "WFSTOnTheFlyDecoder.h"
#endif

/*
  Author :      Darren Moore (moore@idiap.ch)
  Date :        19 Nov 2004
  $Id: juicer.cpp,v 1.16.4.1 2006/06/21 12:34:13 juicer Exp $
*/

/*
  Author:       Octavian Cheng
  Date:     16 June 2006
*/


using namespace Torch ;
using namespace Juicer ;


typedef struct GMMThreadParam_ {
    // pointers to global resources
    HTKFlatModelsThreading* models;

    // per-thread resources
    bool running; 
} GMMThreadParam;

void* GMMThread(void* arg) {
    GMMThreadParam* p = (GMMThreadParam*)arg;
    LogFile::printf("GMM Thread up and running...\n");
    p->models->calcStates();
    LogFile::printf("GMM Thread quitting...\n");
    pthread_exit(NULL);
}

// Version string
bool version = false;

// decoding core selection, default to WFSTDecoderLite
bool useBasicCore = false;

// General parameters
char           *logFName=NULL ;
int            framesPerSec=0 ;

// Vocabulary parameters
char              *lexFName=NULL ;
char              *sentStartWord=NULL ;
char              *sentEndWord=NULL ;

// GMM Model parameters
char           *htkModelsFName=NULL ;
bool           doModelsIOTest=false ;
#ifdef HAVE_HTKLIB
char           *htkConfigFName=NULL ;
bool           useHModels=false ;
char           *speakerNamePattern=NULL ;
char           *parentXformDir=NULL ;
char           *parentXformExt=NULL ;
char           *inputXformDir=NULL ;
char           *inputXformExt=NULL ;
#endif

// Hybrid HMM/ANN parameters
char           *priorsFName=NULL ;
int            statesPerModel=0 ;

// WFST network parameters
char           *fsmFName=NULL ;
char           *inSymsFName=NULL ;
char           *outSymsFName=NULL ;
bool           genTestSeqs=false ;
bool           writeBinaryFiles=false;

// Changes Octavian
// WFST network parameters for on-the-fly composition
char           *gramFsmFName=NULL ;
char           *gramInSymsFName=NULL ;
char           *gramOutSymsFName=NULL ;
bool           onTheFlyComposition=false ;
bool           doLabelAndWeightPushing=false ;

// Decoder Parameters
float          mainBeam=0.0 ;
float          phoneEndBeam=0.0 ;
float          phoneStartBeam=0.0 ;
float          wordEmitBeam=0.0 ;
int            maxHyps=0 ;
int            blockSize = 5;
char           *inputFormat_s=NULL ;
DSTDataFileFormat inputFormat ;
char           *outputFormat_s=NULL ;
DBTOutputFormat outputFormat ;
char           *inputFName=NULL ;
char           *outputFName=NULL ;
char           *refFName=NULL ;
real           lmScaleFactor=1.0 ;
real           insPenalty=0.0 ;
bool           removeSentMarks=false ;
bool           modelLevelOutput=false ;
bool           latticeGeneration=false ;
char           *latticeDir=NULL ;

bool           use2Threads = false;

// Consistency checking parameters
char           *monoListFName=NULL ;
char           *silMonophone=NULL ;
char           *pauseMonophone=NULL ;
char           *tiedListFName=NULL ;
char           *cdSepChars=NULL ;

// Hacks
bool dbtLoop;

void checkConsistency() ;

void processCmdLine( CmdLine *cmd , int argc , char *argv[] )
{
    // General Parameters
    cmd->addText("\nGeneral Options:") ;
    cmd->addBCmdOption( "-version" , &version , false ,
                        "print version and exit" ) ;
    cmd->addBCmdOption( "-basicCore" , &useBasicCore , false ,
                        "use the basic WFSTDecoder core instead of WFSTDecoderLite, which can be slower" ) ;
    cmd->addSCmdOption( "-logFName" , &logFName , "" ,
                        "the name of the log file" ) ;
    cmd->addICmdOption( "-framesPerSec" , &framesPerSec , 100 ,
                        "Number of feature vectors per second (used to output timings in recognised words, "
                        "and also to calculate RT factor" ) ;
    cmd->addBCmdOption( "-writeBinaryFiles" , &writeBinaryFiles , false ,
                        "write binary WFST and model files if they don't already exist" ) ;

    // Vocabulary Parameters
    cmd->addText("\nVocabulary Options:") ;
    cmd->addSCmdOption( "-lexFName" , &lexFName , "" ,
                        "the dictionary filename - monophone transcriptions" ) ;
    cmd->addSCmdOption( "-sentStartWord" , &sentStartWord , "" ,
                        "the name of the dictionary word that will start every sentence" ) ;
    cmd->addSCmdOption( "-sentEndWord" , &sentEndWord , "" ,
                        "the name of the dictionary word that will end every sentence" ) ;

    // Model parameters
    cmd->addText("\nAcoustic Model Options:") ;
    cmd->addSCmdOption( "-htkModelsFName" , &htkModelsFName , "" ,
                        "the file containing the acoustic models in HTK MMF format" ) ;
    cmd->addBCmdOption( "-doModelsIOTest" , &doModelsIOTest , false ,
                        "tests the text and binary acoustic models load/save" ) ;
#ifdef HAVE_HTKLIB
    cmd->addText("\nHTK Options:") ;
    cmd->addSCmdOption( "-htkConfig" , &htkConfigFName , "" ,
                        "the HTK config file that initialises HTKLib if required (optional)" ) ;
    cmd->addBCmdOption( "-useHModels" , &useHModels , false ,
                        "use HTKLib for HMM likelihood calculation" ) ;
    cmd->addSCmdOption( "-speakerNamePattern" , &speakerNamePattern , "" ,
                        "speaker name pattern syntax: outPat[:inPat[:parentPat]]" ) ;
    cmd->addSCmdOption( "-parentXformDir" , &parentXformDir , "" ,
                        "directory for parent transform" ) ;
    cmd->addSCmdOption( "-parentXformExt" , &parentXformExt , "" ,
                        "parent transform filename extension" ) ;
    cmd->addSCmdOption( "-inputXformDir" , &inputXformDir , "" ,
                        "colon separated list of directories to search for input transforms" ) ;
    cmd->addSCmdOption( "-inputXformExt" , &inputXformExt , "" ,
                        "input transform filename extension" ) ;
#endif

    // Hybrid HMM/ANN Parameters
    cmd->addText("\nHybrid HMM/ANN related options:") ;
    cmd->addSCmdOption( "-priorsFName" , &priorsFName , "" ,
                        "the file containing the phone priors" ) ;
    cmd->addICmdOption( "-statesPerModel" , &statesPerModel , 0 ,
                        "the number of states in each HMM used in the hybrid system" ) ;

    // WFST network parameters
    cmd->addText("\nWFST network parameters:") ;
    cmd->addSCmdOption( "-fsmFName" , &fsmFName , "" ,
                        "the FSM filename" ) ;
    cmd->addSCmdOption( "-inSymsFName" , &inSymsFName , "" ,
                        "the input symbols (ie. CD phone names) filename" ) ;
    cmd->addSCmdOption( "-outSymsFName" , &outSymsFName , "" ,
                        "the output symbols (ie. words) filename" ) ;
    cmd->addBCmdOption( "-genTestSeqs" , &genTestSeqs , false ,
                        "generates some test sequences using the network." ) ;

    // Changes Octavian WFST network parameters for on-the-fly composition
    cmd->addSCmdOption( "-gramFsmFName" , &gramFsmFName , "" , "the grammar FSM filename" ) ;
    cmd->addSCmdOption( "-gramInSymsFName" , &gramInSymsFName , "" , "the grammar FSM input symbols" ) ;
    cmd->addSCmdOption( "-gramOutSymsFName" , &gramOutSymsFName , "" , "the grammar FSM output symbols" ) ;
    cmd->addBCmdOption( "-pushing", &doLabelAndWeightPushing, false,
                        "Do weight pushing for on-the-fly composition" ) ;

    // Decoder parameters
    cmd->addText("\nDecoder Options:") ;
    cmd->addRCmdOption( "-mainBeam" , &mainBeam , LOG_ZERO ,
                        "the (+ve log) window used for pruning emitting state hypotheses" ) ;
    cmd->addRCmdOption( "-phoneStartBeam" , &phoneStartBeam , LOG_ZERO ,
                        "the (+ve log) window used for pruning phone-start state hypotheses" ) ;
    cmd->addRCmdOption( "-phoneEndBeam" , &phoneEndBeam , LOG_ZERO ,
                        "the (+ve log) window used for pruning phone-end state hypotheses" ) ;
    cmd->addRCmdOption( "-wordEmitBeam" , &wordEmitBeam , LOG_ZERO ,
                        "the (+ve log) window used for pruning word-emitting-state hypotheses" ) ;
    cmd->addICmdOption( "-maxHyps" , &maxHyps , 0 ,
                        "Upper limit on the number of active emitting state hypotheses" ) ;
    cmd->addICmdOption( "-blockSize" , &blockSize , 5 ,
                        "speed up GMM output calculation by computing a sequence of frames (1-20) a time.");
    cmd->addBCmdOption( "-threading" , &use2Threads , false,
                        "speed up decoding via threading, where GMM calculation is handled in a separate thread." ) ;
    cmd->addSCmdOption( "-inputFName" , &inputFName , "" ,
                        "the file containing the list of files to be decoded" ) ;
    cmd->addSCmdOption( "-inputFormat" , &inputFormat_s , "" ,
                        "the format of the input files (htk,lna)" ) ;
    cmd->addSCmdOption( "-outputFName" , &outputFName , "" ,
                        "the file where decoding results are written (stdout,stderr,<filename>). default=stdout" ) ;
    cmd->addSCmdOption( "-outputFormat" , &outputFormat_s , "" ,
                        "the format used for recognition results output (ref,mlf,xmlf,verbose). default=ref" ) ;
    cmd->addSCmdOption( "-refFName" , &refFName , "" ,
                        "the file containing word-level reference transcriptions" ) ;
    cmd->addRCmdOption( "-lmScaleFactor" , &lmScaleFactor , 1.0 ,
                        "the language model scaling factor" ) ;
    cmd->addRCmdOption( "-insPenalty" , &insPenalty , 0.0 ,
                        "the word insertion penalty" ) ;
    cmd->addBCmdOption( "-removeSentMarks" , &removeSentMarks , false ,
                        "removes sentence start and end markers from decoding result before outputting." ) ;
    cmd->addBCmdOption( "-modelLevelOutput" , &modelLevelOutput , false ,
                        "outputs recognised models instead of recognised words (only available in -basicCore)." ) ;
    cmd->addSCmdOption( "-latticeDir" , &latticeDir , "" ,
                        "the directory where output lattices will be placed (only available in -basicCore)" ) ;

    // modelLevelOutput == true related parameters
    cmd->addSCmdOption( "-monoListFName" , &monoListFName , "" ,
                        "the file containing the list of monophones" ) ;
    cmd->addSCmdOption( "-silMonophone" , &silMonophone , "" ,
                        "the name of the silence monophone" ) ;
    cmd->addSCmdOption( "-pauseMonophone" , &pauseMonophone , "" ,
                        "the name of the pause monophone" ) ;
    cmd->addSCmdOption( "-tiedListFName" , &tiedListFName , "" ,
                        "the file containing the (tied) model list" ) ;
    cmd->addSCmdOption( "-cdSepChars" , &cdSepChars , "" ,
                        "the characters that separate monophones in context-dependent phone names (in order)") ;
    cmd->addBCmdOption( "-loop" , &dbtLoop , false ,
                        "Loops around the first file / device" ) ;

    cmd->read( argc , argv ) ;

    // Version request
    if (version)
    {
        printf("Juicer version %s\n", PACKAGE_VERSION);
        exit(0);
    }

    // First interpret the inputFormat
    if ( strcmp( inputFormat_s , "factory" ) == 0 )
        inputFormat = DST_FEATS_FACTORY ;
    else if ( strcmp( inputFormat_s , "htk" ) == 0 )
        inputFormat = DST_FEATS_HTK ;
    else if ( strcmp( inputFormat_s , "lna" ) == 0 )
        inputFormat = DST_PROBS_LNA8BIT ;
    else
        error("juicer: -inputFormat %s ... unrecognised format" , inputFormat_s ) ;

    // Basic parameter checks
    if ( strcmp( lexFName , "" ) == 0 )
        error("juicer: lexFName undefined") ;
    if ( strcmp( inputFName , "" ) == 0 )
        error("juicer: inputFName undefined") ;
    if ( strcmp( fsmFName , "" ) == 0 )
        error("juicer: fsmFName undefined") ;
    if ( strcmp( inSymsFName , "" ) == 0 )
        error("juicer: inSymsFName undefined") ;
    if ( strcmp( outSymsFName , "" ) == 0 )
        error("juicer: outSymsFName undefined") ;

    if ( (strcmp(gramFsmFName, "")==0) &&
         ( (strcmp(gramInSymsFName, "")!=0) || (strcmp(gramOutSymsFName, "")!=0) ) )
        error("juicer: gramFsmFName undefined while gramIn/OutSymsFName defined") ;
    if ( (strcmp(gramFsmFName, "")!=0) &&
         ( (strcmp(gramInSymsFName, "")==0) || (strcmp(gramOutSymsFName, "")==0) ) )
        error("juicer: gramFsmFName defined while gramIn/OutSymsFName undefined") ;

    if ( strcmp(gramFsmFName, "") != 0 )
        onTheFlyComposition = true ;

    if ( (htkModelsFName != NULL) && (htkModelsFName[0] != '\0') )
    {
        // HMM/GMM system
        if ( (inputFormat != DST_FEATS_HTK) &&
             (inputFormat != DST_FEATS_FACTORY) )
            error("juicer: invalid inputFormat for HMM/GMM decoding") ;
    }
    else if ( (priorsFName != NULL) && (priorsFName[0] != '\0') )
    {
        // Hybrid HMM/ANN system
        if ( inputFormat != DST_PROBS_LNA8BIT )
            error("juicer: invalid inputFormat for hybrid HMM/ANN decoding") ;
        if ( statesPerModel <= 2 )
            error("juicer: invalid statesPerModel (should be > 2)") ;
        if ( (monoListFName == NULL) || (monoListFName[0] == '\0') )
            error("juicer: monoListFName undefined for hybrid HMM/ANN decoding") ;
    }
    else
    {
        error("juicer: both htkModelsFName and priorsFName are undefined") ;
    }

    // Interpret the input and output format strings
    // Interpret the (optional) outputFormat
    if ( (outputFormat_s == NULL) || (strcmp( outputFormat_s , "" ) == 0) )
        outputFormat = DBT_OUTPUT_REF ;
    else if ( strcmp( outputFormat_s , "ref" ) == 0 )
        outputFormat = DBT_OUTPUT_REF ;
    else if ( strcmp( outputFormat_s , "mlf" ) == 0 )
        outputFormat = DBT_OUTPUT_MLF ;
    else if ( strcmp( outputFormat_s , "xmlf" ) == 0 )
        outputFormat = DBT_OUTPUT_XMLF ;
    else if ( strcmp( outputFormat_s , "trans" ) == 0 )
        outputFormat = DBT_OUTPUT_TRANS ;
    else if ( strcmp( outputFormat_s , "verbose" ) == 0 )
        outputFormat = DBT_OUTPUT_VERBOSE ;
    else
        error("juicer: -outputFormat %s ... unrecognised format" , outputFormat_s ) ;

    if ( modelLevelOutput )
    {
        if ( strcmp( monoListFName , "" ) == 0 )
            error("juicer: -monoListFName not specified when using -modelLevelOutput") ;
        if ( strcmp( tiedListFName , "" ) == 0 )
            error("juicer: -tiedListFName not specified when using -modelLevelOutput") ;
        if (useBasicCore == false) {
            fprintf(stderr, "Warning: -modelLevelOutput only available in basicCore, switched to basicCore from now on.\n");
            useBasicCore = true;
        }
    }

    if ( (latticeDir != NULL) && (strcmp( latticeDir , "") != 0) )
    {
        latticeGeneration = true ;
        // make sure the output lattice directory exists
        char str[10000] ;
        sprintf( str , "mkdir -p %s" , latticeDir ) ;
        system( str ) ;
        if (useBasicCore == false) {
            fprintf(stderr, "Warning: -latticeDir only available in basicCore, switched to basicCore from now on.\n");
            useBasicCore = true;
        }
    }

    if (use2Threads) {
        if (useBasicCore == true) {
            fprintf(stderr, "Warning: 2 thread decoding is not available in basicCore, switched to default core from now on.\n");
            useBasicCore = false;
        }
    }

}

bool fileExists( const char *fname )
{
    FILE *pFile ;
    if ( (pFile = fopen( fname , "rb" )) == NULL )
        return false ;
    fclose( pFile ) ;
    return true ;
}

void setupModels( IModels **models ) ;
void setupNetworks(WFSTNetwork** network_, WFSTNetwork** clNetwork_, WFSTSortedInLabelNetwork** gNetwork_);

#ifdef HAVE_HTKLIB
/** 
 * ---------------- Information about transforms ------------
 */
XFInfo xfInfo;
/**
 *  -------------------- Initialisation ---------------------
 */
void InitialiseHTK()
{
        /**
         * Simulate command line options
         *       Option                                       Default
         *
         *       -C cf   Set config file to cf                default
         *       -D      Display configuration variables      off
         *       -S f    Set script file to f                 none
         **/
    int argc = 6;
    char *argv[argc];
    argv[0] = "Juicer::HTKLib";
    argv[1] = "-C";
    argv[2] = htkConfigFName;
    argv[3] = "-S";
    argv[4] = inputFName;
    argv[5] = "-D";

    /*
     * Standard Global HTK initialisation
     */
    char *hvite_version = "!HVER!HVite:   3.4 [CUED 25/04/06]";
    char *hvite_vc_id = "$Id: HVite.c,v 1.1.1.1 2006/10/11 09:55:02 jal58 Exp $";
    if(InitShell(argc,argv,hvite_version,hvite_vc_id)<SUCCESS)
                HError(2600,"HTKLib: InitShell failed");
        InitMem();
        InitLabel();
        InitMath();
        InitSigP();
        InitWave();
        InitAudio();
        InitVQ();
        InitModel();
        if(InitParm()<SUCCESS)
                HError(2600,"HTKLib: InitParm failed");
        InitUtil();
        InitAdapt(&xfInfo);

        if (NumArgs()>0) {
                LogFile::printf( "HTK config loaded:\n\t%s\n\t%d segments in %s\n" , htkConfigFName , NumArgs() , inputFName );
        }
}
#endif

int main( int argc , char *argv[] )
{
    CmdLine cmd ;

    // process command line
    processCmdLine( &cmd , argc , argv ) ;

    LogFile::open( logFName ) ;
    LogFile::printf( "Juicer version %s\n", PACKAGE_VERSION ) ;
    LogFile::date( "started on" ) ;
    LogFile::hostname( "on host" ) ;

#ifdef HAVE_HTKLIB
    if ( (htkConfigFName != NULL) && (htkConfigFName[0] != '\0') )
    {
        InitialiseHTK();
    }
#endif

    // load vocabulary
    LogFile::puts( "loading vocab .... " ) ;
    DecVocabulary *vocab = new DecVocabulary(
        lexFName , '!' , sentStartWord , sentEndWord );
    LogFile::puts( "done\n" ) ;

    // load models
    LogFile::puts( "loading acoustic models .... " ) ;
    IModels *models=NULL ;
    setupModels( &models ) ;
    LogFile::puts( "done\n" ) ;

#if 0
    // Check model order
    for (int i=0; i<models->getNumHMMs(); i++)
    {
        printf("Model %d is %s\n", i, models->getHMMName(i));
    }
    assert(0);
#endif

    if ( doModelsIOTest )
    {
        testModelsIO(
            htkModelsFName , monoListFName , priorsFName , statesPerModel ) ;
    }

    if (use2Threads) {
        #ifndef OPT_FLATMODEL
            #error GMM threading code requires HTKFlatModels
        #endif
        // let GMM thread run before loading network to ensure it will be in running state when
        // decoding starts
        pthread_t gmm_thread;
        GMMThreadParam gmm_thread_param;
        gmm_thread_param.models = (HTKFlatModelsThreading*)models;
        gmm_thread_param.running = true;
        if (pthread_create(&gmm_thread, NULL, GMMThread, &gmm_thread_param)) {
            fprintf(stderr, "juicer.cpp fail to create gmm_thread\n");
            exit(-1);
        }
    }

    // load network
    LogFile::puts( "loading transducer network .... " ) ;
    WFSTNetwork *network=NULL ;
    WFSTNetwork *clNetwork = NULL ;
    WFSTSortedInLabelNetwork *gNetwork = NULL ;
    setupNetworks(&network, &clNetwork, &gNetwork);

    // Create front-end.
    FrontEndFormat source;
    switch (inputFormat)
    {
    case DST_FEATS_FACTORY:
        source = FRONTEND_FACTORY;
        break;
    case DST_FEATS_HTK:
        source = FRONTEND_HTK;
        break;
    case DST_PROBS_LNA8BIT:
        source = FRONTEND_LNA;
        break;
    default:
        assert(0);
    }
    FrontEnd *frontend = new FrontEnd(models->getInputVecSize(), source);
#ifdef HAVE_HTKLIB
    if (sHTKLib.mHTKLibSource)
    {
        if ( (htkConfigFName == NULL) || (htkConfigFName[0] == '\0') )
            HError( 9999 , "Juicer: HTKLibSource selected but no HTK config provided" );
    }
#endif

    // create decoder
    LogFile::puts( "creating Decoder .... " ) ;
    IDecoder *decoder = NULL ;
    if ( !onTheFlyComposition )  {
        if (!useBasicCore) {
            if (use2Threads)
                decoder = new WFSTDecoderLiteThreading(
                        network , models , phoneStartBeam, mainBeam , phoneEndBeam , wordEmitBeam ,
                        maxHyps);
            else
                decoder = new WFSTDecoderLite(
                        network , models , phoneStartBeam, mainBeam , phoneEndBeam , wordEmitBeam ,
                        maxHyps);
        } else

        decoder = new WFSTDecoder(
	    network , models , phoneStartBeam, mainBeam , phoneEndBeam , wordEmitBeam ,
            maxHyps , modelLevelOutput , latticeGeneration ) ;
    }
    else  {
#ifdef WITH_ONTHEFLY
        decoder = new WFSTOnTheFlyDecoder(
            clNetwork, gNetwork, models, mainBeam, phoneEndBeam,
            maxHyps, modelLevelOutput, latticeGeneration,
            doLabelAndWeightPushing, true ) ;
#else
        printf("On the fly not compiled in\n");
        assert(0);
#endif
    }
    LogFile::puts( "done\n" ) ;

    // setup phoneLookup
    PhoneLookup *phoneLookup=NULL ;
    if ( modelLevelOutput )
    {
        phoneLookup = new PhoneLookup(
            monoListFName , silMonophone , pauseMonophone ,
            tiedListFName , cdSepChars ) ;

        // Add the model indices to the PhoneLookup
        int i ;
        for ( i=0 ; i<models->getNumHMMs() ; i++ )
        {
            const char* hmmName = models->getHMMName( i ) ;
            phoneLookup->addModelInd( hmmName , i ) ;
        }
        phoneLookup->verifyAllModels() ;
    }

    // create batch tester
    LogFile::puts( "creating DecoderBatchTest .... " ) ;
    DecoderBatchTest *tester = new DecoderBatchTest(
        vocab , phoneLookup , frontend , decoder , inputFName , inputFormat ,
        models->getInputVecSize() , outputFName , outputFormat , refFName ,
        removeSentMarks , framesPerSec ) ;
    tester->loop = dbtLoop;

    if ( latticeGeneration )
    {
        tester->activateLatticeGeneration( latticeDir ) ;
        LogFile::puts( "lattice generation activated ..." ) ;
    }
    LogFile::puts( "done\n\njuicer initialisation complete\n\n" ) ;


    // run the decoder
    tester->run();

    if (use2Threads)
        ((HTKFlatModelsThreading*)models)->stop();

    // cleanup and exit
    delete tester ;
    delete phoneLookup ;
    delete decoder ;
    delete network ;
    delete models ;
    delete vocab ;

    // Clean up for on-the-fly composition
    delete clNetwork ;
    delete gNetwork ;

    LogFile::date( "juicer finished at" ) ;
    LogFile::close() ;
    return 0 ;
}


void setupModels( IModels **models )
{
    if ( (htkModelsFName != NULL) && (htkModelsFName[0] != '\0') )
    {
        // Check for correct input format
        if ( (inputFormat != DST_FEATS_HTK) &&
             (inputFormat != DST_FEATS_FACTORY) )
            error("juicer: setupModels - "
                  "htkModelsFName defined but inputFormat not htk") ;

        // HTK MMF model input - i.e. a HMM/GMM system
#ifdef HAVE_HTKLIB
        if ( useHModels ) 
        {
            if ( (tiedListFName == NULL) || (tiedListFName[0] == '\0') ) 
            {
                useHModels=false;
                LogFile::puts( "\nWARNING: Can't use HTKLib's likelihood calculation because no (tied) model list was provided\n" );
            }
            if ( (htkConfigFName == NULL) || (htkConfigFName[0] == '\0') )
            {
                useHModels=false;
                LogFile::puts( "\nWARNING: Can't use HTKLib's likelihood calculation because no HTK config was provided\n" );
            }
        } else {
            if ( ( (speakerNamePattern!=NULL) && (speakerNamePattern[0]!='\0') ) ||
                 ( (parentXformDir!=NULL) && (parentXformDir[0]!='\0') ) ||
                 ( (inputXformDir!=NULL) && (inputXformDir[0]!='\0') )  )
                error( "speakerNamePattern, inputXformDir and parentXformDir require the -useHModels option\n" );

        }
        if ( useHModels )
        {
            LogFile::printf( "Using HModels.\n");
            sHModels = new HModels(tiedListFName);
            xfInfo.usePaXForm = FALSE;
            xfInfo.useInXForm = FALSE;
            sHModels->xfInfo = &xfInfo;
            *models = sHModels;
            if ( (speakerNamePattern!=NULL) && (speakerNamePattern[0]!='\0') )
            {
                // We have a colon separated list.
                // Make a copy of the list and carve it up
                char *str = new char[strlen(speakerNamePattern)+1] ;
                strcpy( str , speakerNamePattern ) ;
                // Extract the paths
                char *ptr = strtok( str , ":" ) ;
                char *pattern = new char[strlen(ptr)+1] ;
                strcpy( pattern , ptr ) ;
                xfInfo.outSpkrPat = pattern;
                LogFile::printf("\tOut speaker pattern    = %s\n", pattern);
                ptr = strtok( NULL , ":" ) ;
                if ( ptr !=NULL )
                {
                    char *pattern = new char[strlen(ptr)+1] ;
                    strcpy( pattern , ptr ) ;
                    xfInfo.inSpkrPat = pattern;
                    LogFile::printf("\tIn speaker pattern     = %s\n", pattern);
                    ptr = strtok( NULL , ":" ) ;
                    if ( ptr !=NULL )
                    {
                        char *pattern = new char[strlen(ptr)+1] ;
                        strcpy( pattern , ptr ) ;
                        xfInfo.paSpkrPat = pattern;
                        LogFile::printf("\tParent speaker pattern = %s\n", pattern);
                    }
                }
                delete [] str;
                if (xfInfo.inSpkrPat == NULL) 
                {
                    xfInfo.inSpkrPat = xfInfo.outSpkrPat; 
                    LogFile::printf("\tIn speaker pattern     = %s\n", xfInfo.inSpkrPat);
                }
                if (xfInfo.paSpkrPat == NULL)
                {
                    xfInfo.paSpkrPat = xfInfo.outSpkrPat; 
                    LogFile::printf("\tParent speaker pattern = %s\n", xfInfo.paSpkrPat);
                }
            }
            if ( (parentXformDir!=NULL) && (parentXformDir[0]!='\0') )
            {
                if ( xfInfo.paSpkrPat == NULL )
                    error("Parent transform specified without a corresponding mask");
                xfInfo.usePaXForm = TRUE;
                xfInfo.paXFormDir = parentXformDir; 
                if ( (parentXformExt!=NULL) && (parentXformExt[0]!='\0') )
                    xfInfo.paXFormExt = parentXformExt; 
            }
            if ( (inputXformDir!=NULL) && (inputXformDir[0]!='\0') )
            {
                if ( xfInfo.inSpkrPat == NULL )
                    error("Input transform specified without a corresponding mask");
                sHModels->inputXformDir=inputXformDir;
                if ( (inputXformExt!=NULL) && (inputXformExt[0]!='\0') )
                    xfInfo.inXFormExt = inputXformExt; 
            }
        } else {
#endif
# ifdef OPT_FLATMODEL
        if (use2Threads)
            *models = new HTKFlatModelsThreading() ;
        else
            *models = new HTKFlatModels() ;
        (*models)->setBlockSize(blockSize);
# else
        *models = new HTKModels() ;
# endif

        (*models)->setBlockSize(blockSize);

#ifdef HAVE_HTKLIB
        }
#endif

#ifdef USE_BINARY_MODELS
        char *modelsBinFName = new char[strlen(htkModelsFName)+5] ;
        sprintf( modelsBinFName , "%s.bin" , htkModelsFName ) ;
        if ( fileExists( modelsBinFName ) )
        {
            LogFile::puts( "from pre-existing binary file .... " ) ;
            (*models)->readBinary( modelsBinFName ) ;
        }
        else
        {
#endif
            LogFile::puts( "from ascii HTK MMF file .... " ) ;
            (*models)->Load( htkModelsFName , false /*fixTeeModels*/ ) ;
#ifdef USE_BINARY_MODELS
            if ( writeBinaryFiles )
            {
                LogFile::puts( "writing new binary file .... " ) ;
                (*models)->output( modelsBinFName , true ) ;
            }
            else
            {
                LogFile::puts( "binary file writing inhibited ....\n");
            }
        }
        delete [] modelsBinFName ;
#endif
    }
    else if ( (priorsFName != NULL) && (priorsFName[0] != '\0') )
    {
        // File containing priors for ANN outputs - a hybrid HMM/ANN
        // system In this case we assume that the ordering in
        // monoListFName corresponds to the order of values in LNA
        // input frames.

        // Check for correct input format
        if ( inputFormat != DST_PROBS_LNA8BIT )
            error("juicer: setupModels - "
                  "aNNPriorsFName defined but inputFormat not lna") ;
        if ( (monoListFName == NULL) || (monoListFName[0] == '\0') )
            error("juicer: setupModels - "
                  "aNNPriorsFName defined but no monoListFName defined") ;
        if ( statesPerModel <= 2 )
            error("juicer: setupModels - "
                  "aNNPriorsFName defined but statesPerModel <= 2") ;

#ifdef HAVE_HTKLIB
        if ( useHModels ) 
        {
            if ( (tiedListFName == NULL) || (tiedListFName[0] == '\0') )
                LogFile::puts( "\nWARNING: Can't use HTKLib's likelihood calculation because no (tied) model list was provided\n" );
            if ( (htkConfigFName == NULL) || (htkConfigFName[0] == '\0') )
                LogFile::puts( "\nWARNING: Can't use HTKLib's likelihood calculation because no HTK config was provided\n" );
        }
        if ( useHModels && (htkConfigFName != NULL) && (htkConfigFName[0] != '\0') && (tiedListFName != NULL) && (tiedListFName[0] != '\0') )
        {
            assert(0);  // makes no sense right now to have HTK models and LNA
        } else {
#endif
#ifdef OPT_FLATMODEL
        //  not implemented, use HTKModels instead now
        *models = new HTKModels();
#else
        *models = new HTKModels() ;
#endif
#ifdef HAVE_HTKLIB
        }
#endif
        (*models)->Load( monoListFName , priorsFName , statesPerModel ) ;
    }
}

void setupNetworks(WFSTNetwork** network_, WFSTNetwork** clNetwork_, WFSTSortedInLabelNetwork** gNetwork_) {
    WFSTNetwork *network=NULL ;
    WFSTNetwork *clNetwork = NULL ;
    WFSTSortedInLabelNetwork *gNetwork = NULL ;
#ifdef USE_BINARY_WFST
    char *netBinFName = new char[strlen(fsmFName)+5] ;
    sprintf( netBinFName , "%s.bin" , fsmFName ) ;
    char *clNetBinFName = NULL ;
    char *gNetBinFName = NULL ;
#endif

    // Load network: Either static or on-the-fly composition
    if ( !onTheFlyComposition )  {
#ifdef USE_BINARY_WFST
        if ( fileExists( netBinFName ) )
        {
            LogFile::puts( "from pre-existing binary file .... " ) ;
            network = new WFSTNetwork( lmScaleFactor, insPenalty ) ;
            network->readBinary( netBinFName ) ;
        }
        else
        {
#endif
            LogFile::puts( "from ascii FSM file .... " ) ;
            // Changes Octavian 20060523
            network = new WFSTNetwork(
                fsmFName , inSymsFName , outSymsFName ,
                lmScaleFactor , insPenalty,
                REMOVEBOTH
            ) ;
#ifdef USE_BINARY_WFST
            if ( writeBinaryFiles )
            {
                LogFile::puts( "writing new binary file .... " ) ;
                network->writeBinary( netBinFName ) ;
            }
            else
            {
                LogFile::puts( "binary file writing inhibited .... \n");
            }
        }
        delete [] netBinFName ;
#endif

        LogFile::printf( "nStates=%d nTrans=%d ... done\n" ,
                         network->getNumStates() , network->getNumTransitions() ) ;

        if ( genTestSeqs )
        {
            network->generateSequences( 10 ) ;
            //network->writeFSM("blah.fsm","blah.insyms","blah.outsyms") ;
            //exit(1) ;
        }
    }
    else  {
        // On-the-fly composition
#ifdef USE_BINARY_WFST
        clNetBinFName = netBinFName ;
        gNetBinFName = new char[strlen(gramFsmFName)+5] ;
        sprintf( gNetBinFName, "%s.bin", gramFsmFName );
#endif

        // Load C o L network
        LogFile::puts( "loading C o L network .... " ) ;
#ifdef USE_BINARY_WFST
        if ( fileExists(clNetBinFName) )  {
            LogFile::puts( "from pre-existing binary file .... " ) ;

            if ( doLabelAndWeightPushing )  {
                LogFile::puts( "With label and weight pushing .... " ) ;
                clNetwork = new WFSTLabelPushingNetwork( 1.0 ) ;
            }
            else  {
                LogFile::puts( "No pushing .... " ) ;
                clNetwork = new WFSTNetwork( 1.0 , 0.0 ) ;
            }

            clNetwork->readBinary( clNetBinFName );
        }
        else  {
#endif
            LogFile::puts( "from ascii FSM file .... " ) ;

            if ( doLabelAndWeightPushing )  {
                LogFile::puts( "With label and weight pushing ... " ) ;
                clNetwork = new WFSTLabelPushingNetwork(
                    fsmFName, inSymsFName, outSymsFName, 1.0 , REMOVEINPUT ) ;
            }
            else  {
                LogFile::puts( "No pushing ... " ) ;
                // Changes Octavian 20060523
                clNetwork = new WFSTNetwork (
                    fsmFName, inSymsFName, outSymsFName, 1.0 , REMOVEINPUT ) ;
            }

#ifdef USE_BINARY_WFST
            LogFile::puts( "writing new binary file .... " ) ;
            clNetwork->writeBinary( clNetBinFName ) ;
        }
#endif
        // Changes Octavian 20060616
        // After loading CoL transducer. Print the number of max outLabels
        if ( doLabelAndWeightPushing )  {
            LogFile::printf(
                "Max number of output labels in a set = %d\n",
                ((WFSTLabelPushingNetwork*) clNetwork)->getMaxOutLabels() ) ;
        }

        // Load G network
        LogFile::puts( "loading G network .... " ) ;
#ifdef USE_BINARY_WFST
        if ( fileExists(gNetBinFName) )  {
            LogFile::puts( "from pre-existing binary file .... " ) ;
            gNetwork = new WFSTSortedInLabelNetwork( lmScaleFactor ) ;
            gNetwork->readBinary( gNetBinFName );
        }
        else  {
#endif
            LogFile::puts( "from ascii FSM file .... " ) ;
            // Changes Octavian 20060523
            gNetwork = new WFSTSortedInLabelNetwork(
                gramFsmFName, gramInSymsFName, gramOutSymsFName,
                lmScaleFactor , NOTREMOVE ) ;
#ifdef USE_BINARY_WFST
            LogFile::puts( "writing new binary file .... " ) ;
            gNetwork->writeBinary( gNetBinFName );
        }
        delete [] clNetBinFName ;
        delete [] gNetBinFName ;
#endif

        // Changes Octavian 20060616
        // After loeading the G transducer, print the max number of transitions
        // out of a state
        LogFile::printf(
            "Max number of transitions from a state in G = %d\n",
            gNetwork->getMaxOutTransitions() ) ;
        LogFile::printf(
            "C o L: nStates=%d nTrans=%d ... done\n" ,
            clNetwork->getNumStates() , clNetwork->getNumTransitions()
        ) ;
        LogFile::printf(
            "G: nStates=%d nTrans=%d ... done\n" ,
            gNetwork->getNumStates() , gNetwork->getNumTransitions()
        ) ;
    }

    *network_ = network;
    *clNetwork_ = clNetwork;
    *gNetwork_ = gNetwork;
}


void checkConsistency()
{
    // 1. Check the consistency of our inSymsFName symbols file
    // (containing CD phone names).  We should be able to configure a
    // PhoneLookup instance from scratch and its indices should match
    // those in the inSymsFName symbols filename.
    PhoneLookup modelLookup( monoListFName , silMonophone , pauseMonophone ,
                             tiedListFName , cdSepChars ) ;
    WFSTAlphabet cdPhones( inSymsFName ) ;

    int i , ind ;
    const char *str ;
    for ( i=1 ; i<=cdPhones.getMaxLabel() ; i++ )
    {
        // start at 1 to ignore the epsilon label.
        str = cdPhones.getLabel( i ) ;
        if ( str == NULL )
            continue ;
        ind = modelLookup.getModelInd( str ) ;
        if ( ind != (i-1) ) // account for <eps> being index 0
            error("juicer: checkConsistency ... "
                  "symbol %s=%d != %d in modelLookup" , str , i , ind ) ;
    }

    for ( i=0 ; i<modelLookup.getNumModels() ; i++ )
    {
        str = modelLookup.getModelStr( i ) ;
        if ( str == NULL )
            error("juicer: checkConsistency ... modelLookup str is NULL") ;
        ind = cdPhones.getIndex( str ) ;
        if ( (ind-1) != i )
            error("juicer: checkConsistency ... "
                  "mdlLookup %s=%d != %d in cdPhones" , str , i , ind ) ;
    }

    // 2. Check the consistency of our outSymsFName symbols file
    // (containing words).  We should be able to configure a
    // DecVocabulary instance from scratch and its indices should
    // match those in the outSymsFName symbols filename.
    DecVocabulary vocab( lexFName , '!' , sentStartWord , sentEndWord ) ;
    WFSTAlphabet words( outSymsFName ) ;
    for ( i=1 ; i<=words.getMaxLabel() ; i++ )
    {
        str = words.getLabel( i ) ;
        if ( str == NULL )
            continue ;
        ind = vocab.getIndex( str ) ;
        if ( ind != (i-1) )
            error("juicer: checkConsistency ... "
                  "symbol %s=%d != %d in vocab" , str , i , ind ) ;
    }

    for ( i=0 ; i<vocab.nWords ; i++ )
    {
        str = vocab.words[i] ;
        ind = words.getIndex( str ) ;
        if ( (ind-1) != i )
            error("juicer: checkConsistency ... "
                  "vocab word %s=%d != %d in words" , str , i , ind ) ;
    }
}
