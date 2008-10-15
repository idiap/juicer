/*
 * Copyright 2004 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#include "general.h"
#include "CmdLine.h"
#include "DecVocabulary.h"
#include "WFSTNetwork.h"
#include "WFSTDecoder.h"
#include "DecoderBatchTest.h"
#include "MonophoneLookup.h"
#include "LogFile.h"

#ifdef HAVE_HTKLIB
# include "HModels.h"
#else
# ifdef OPTIMISE_FLATMODEL
#  include "HTKFlatModels.h"
# else
#  include "HTKModels.h"
# endif
#endif

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

// Consistency checking parameters
char           *monoListFName=NULL ;
char           *silMonophone=NULL ;
char           *pauseMonophone=NULL ;
char           *tiedListFName=NULL ;
char           *cdSepChars=NULL ;

void checkConsistency() ;

void processCmdLine( CmdLine *cmd , int argc , char *argv[] )
{
    // General Parameters
    cmd->addText("\nGeneral Options:") ;
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
                        "outputs recognised models instead of recognised words." ) ;
    cmd->addSCmdOption( "-latticeDir" , &latticeDir , "" ,
                        "the directory where output lattices will be placed" ) ;

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

    cmd->read( argc , argv ) ;

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
    }

    if ( (latticeDir != NULL) && (strcmp( latticeDir , "") != 0) )
    {
        latticeGeneration = true ;
        // make sure the output lattice directory exists
        char str[10000] ;
        sprintf( str , "mkdir -p %s" , latticeDir ) ;
        system( str ) ;
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

void setupModels( Models **models ) ;


int main( int argc , char *argv[] )
{
    CmdLine cmd ;

    // process command line
    processCmdLine( &cmd , argc , argv ) ;

    LogFile::open( logFName ) ;
    LogFile::date( "juicer started on" ) ;
    LogFile::hostname( "on host" ) ;

    // load vocabulary
    LogFile::puts( "loading vocab .... " ) ;
    DecVocabulary *vocab = new DecVocabulary(
        lexFName , '!' , sentStartWord , sentEndWord );
    LogFile::puts( "done\n" ) ;

    // load models
    LogFile::puts( "loading acoustic models .... " ) ;
    Models *models=NULL ;
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

#ifndef HAVE_HTKLIB
    if ( doModelsIOTest )
    {
        testModelsIO(
            htkModelsFName , monoListFName , priorsFName , statesPerModel ) ;
    }
#endif

    // load network
    LogFile::puts( "loading transducer network .... " ) ;
    WFSTNetwork *network=NULL ;
#ifdef USE_BINARY_WFST
    char *netBinFName = new char[strlen(fsmFName)+5] ;
    sprintf( netBinFName , "%s.bin" , fsmFName ) ;
    char *clNetBinFName = NULL ;
    char *gNetBinFName = NULL ;
#endif

    // Networks for on-the-fly composition
    // Changes
    WFSTNetwork *clNetwork = NULL ;
    WFSTSortedInLabelNetwork *gNetwork = NULL ;

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

    // create decoder
    LogFile::puts( "creating WFSTDecoder .... " ) ;
    WFSTDecoder *decoder = NULL ;
    if ( !onTheFlyComposition )  {
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

    if ( latticeGeneration )
    {
        tester->activateLatticeGeneration( latticeDir ) ;
        LogFile::puts( "lattice generation activated ..." ) ;
    }
    LogFile::puts( "done\n\njuicer initialisation complete\n\n" ) ;

    // run the decoder
    tester->run() ;

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


void setupModels( Models **models )
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
        *models = new HModels() ;
#else
# ifdef OPTIMISE_FLATMODEL
        *models = new HTKFlatModels() ;
# else
        *models = new HTKModels() ;
# endif
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
        assert(0);  // makes no sense right now to have HTK models and LNA
        *models = new HModels() ;
#else
# ifdef OPTIMISE_FLATMODEL
        *models = new HTKFlatModels();
# else
        *models = new HTKModels() ;
# endif
#endif
        (*models)->Load( monoListFName , priorsFName , statesPerModel ) ;
    }
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
