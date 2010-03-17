/*
 * Copyright 2004 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#include <assert.h>

#include "DecoderBatchTest.h"
#include "DiskXFile.h" // For EditDistance
#include "EditDistance.h"
#include "time.h"
#include "string_stuff.h"
#include "WFSTLattice.h"
#include "LogFile.h"


/*
  Author:		Darren Moore (moore@idiap.ch)
  Date:			2004
  $Id: DecoderBatchTest.cpp,v 1.23 2006/05/23 08:53:37 juicer Exp $
*/

using namespace Torch;


Juicer::DecoderBatchTest::DecoderBatchTest(
    DecVocabulary *vocab_ , PhoneLookup *phoneLookup_ , FrontEnd *frontend_ ,
    IDecoder *wfstDecoder_ , const char *inputFName_ ,
    DSTDataFileFormat inputFormat_ ,
    int inputVecSize_ , const char *outputFName_ ,
    DBTOutputFormat outputFormat_ ,
    const char *expResultsFName_ , bool removeSil_ , int framesPerSec_
)
{

    if ( (wfstDecoder = wfstDecoder_) == NULL )
        error("DBT::DBT - wfstDecoder_ is NULL") ;

    assert(frontend_);
    frontend = frontend_;

    if ( wfstDecoder->modelLevelOutput() )
    {
        mode = DBT_MODE_WFSTDECODE_PHONES ;
    }
    else
    {
        mode = DBT_MODE_WFSTDECODE_WORDS ;
    }

    if ( ((vocab = vocab_) == NULL) && (wfstDecoder->modelLevelOutput() == false) )
        error("DBT::DBT - vocab_ is NULL when decoder outputting words") ;
    if ( ((phoneLookup = phoneLookup_) == NULL) && wfstDecoder->modelLevelOutput() )
        error("DBT::DBT - phoneLookup_ is NULL when decoder outputting phonemes") ;

    init( inputFName_ , inputFormat_ , inputVecSize_ , outputFName_ , outputFormat_ ,
          expResultsFName_ , removeSil_ , framesPerSec_ ) ;

    loop = false;
}


Juicer::DecoderBatchTest::~DecoderBatchTest()
{
    if ( (outputFD != NULL) && (outputFD != stdout) && (outputFD != stderr) )
        fclose( outputFD ) ;

    delete [] inputFName ;
    delete [] outputFName ;
    delete [] expResultsFName ;

    if ( tests != NULL )
    {
        for ( int i=0 ; i<nTests ; i++ )
            delete tests[i] ;
        free( tests ) ;
    }

    delete [] latticeDir ;
}


void Juicer::DecoderBatchTest::init(
    const char *inputFName_ , DSTDataFileFormat inputFormat_ ,
    int inputVecSize_ , const char *outputFName_ ,
    DBTOutputFormat outputFormat_ , const char *expResultsFName_ ,
    bool removeSil_ , int framesPerSec_
)
{
    nTests = 0 ;
    nTestsAlloc = 0 ;
    tests = NULL ;

    if ( (inputFName_ == NULL) || (strcmp(inputFName_ , "") == 0) )
        error("DBT::init - inputFName is undefined\n") ;
    inputFName = new char[strlen(inputFName_)+1] ;
    strcpy( inputFName , inputFName_ ) ;

    if ( (mode < 0) || (mode >= DBT_MODE_INVALID) )
        error("DBT::init - invalid mode") ;
    if ( (inputFormat_ < 0) || (inputFormat_ >= DST_NOFORMAT) )
        error("DBT::init - invalid inputFormat_") ;
    inputFormat = inputFormat_ ;

    if ( (inputVecSize = inputVecSize_) <= 0 )
        error("DBT::init - inputVecSize <= 0") ;
    if ( (framesPerSec = framesPerSec_) <= 0 )
        error("DBT::init - framesPerSec <= 0") ;
    DecoderSingleTest::setFramesPerSec( framesPerSec  ) ;
    decodeTime = 0.0 ;
    speechTime = 0.0 ;

    outputFName = NULL ;
    if ( outputFName_ != NULL )
    {
        outputFName = new char[strlen(outputFName_)+1] ;
        strcpy( outputFName , outputFName_ ) ;
    }
    outputFD = NULL ;

    if ( (outputFormat_ < 0) || (outputFormat_ >= DBT_OUTPUT_INVALID) )
        error("DBT::init - invalid outputFormat_") ;
    outputFormat = outputFormat_ ;

    if ( (expResultsFName_ == NULL) || (strcmp(expResultsFName_,"")==0) )
    {
        haveExpResults = false ;
        expResultsFName = NULL ;
    }
    else
    {
        haveExpResults = true ;
        expResultsFName = new char[strlen(expResultsFName_)+1] ;
        strcpy( expResultsFName , expResultsFName_ ) ;
    }

    removeSil = removeSil_ ;

    doLatticeGeneration = false ;
    latticeDir = NULL ;

    configureTests() ;
}


void Juicer::DecoderBatchTest::printStatistics( int i_cost , int d_cost , int s_cost )
{
    // Calculates the insertions, deletions, substitutions, accuracy
    //   and outputs. Also calculates the total time taken to decode (not including
    //   loading of datafiles).
    EditDistance totalRes , singleRes ;
    totalRes.setCosts( i_cost , d_cost , s_cost ) ;
    singleRes.setCosts( i_cost , d_cost , s_cost ) ;

    DiskXFile dxf(outputFD);

    int *buf=NULL , nAlloc=0 ;

    for (int i=0 ; i<nTests ; i++ )
    {
        if ( tests[i]->nActualWords > 0 )
        {
            singleRes.distance( tests[i]->actualWords , tests[i]->nActualWords ,
                                tests[i]->expectedWords , tests[i]->nExpectedWords ) ;
        }
        else
        {
            if ( nAlloc < tests[i]->nResultWords )
            {
                nAlloc = tests[i]->nResultWords ;
                buf = (int *)realloc( buf , nAlloc*sizeof(int) ) ;
            }

            if ( tests[i]->nResultLevels > 1 )
                warning("DBT::printStatistics - nResultLevels in test > 1, only using first level") ;

            for ( int j=0 ; j<tests[i]->nResultWords ; j++ )
                buf[j] = tests[i]->resultWords[0][j].index ;

            singleRes.distance( buf , tests[i]->nResultWords ,
                                tests[i]->expectedWords , tests[i]->nExpectedWords ) ;
        }
        //fprintf( outputFD , "%s: " , tests[i]->getTestFName() ) ;
        //singleRes.print( outputFD ) ;
        totalRes.add( &singleRes ) ;
    }

    fprintf( outputFD , "\nTotal time spent decoding = %.2f secs\n" , decodeTime ) ;
    fprintf( outputFD , "Total amount of speech    = %.2f secs\n" , speechTime ) ;
    fprintf( outputFD , "Real-time (RT) factor     = %.2f\n" , decodeTime / speechTime ) ;
    //totalRes.print( outputFD ) ;
    totalRes.print( &dxf ) ;
    //totalRes.printRatio( outputFD ) ;
    totalRes.printRatio( &dxf ) ;
    fprintf( outputFD , "\n" ) ;

    if ( buf != NULL )
        free( buf ) ;
}


void Juicer::DecoderBatchTest::activateLatticeGeneration( const char *latticeDir_ )
{
    if ( latticeDir_ == NULL )
        error("DBT::activateLatticeGeneration - latticeDir_ is NULL") ;

    latticeDir = new char[strlen(latticeDir_)+1] ;
    strcpy( latticeDir , latticeDir_ ) ;

    doLatticeGeneration = true ;
}


void Juicer::DecoderBatchTest::openOutputFile()
{
    // Setup the output file descriptor
    if ( (outputFName == NULL) || (strcmp(outputFName,"") == 0) ||
         (strcmp(outputFName,"stdout") == 0) )
    {
        outputFD = stdout ;
    }
    else if ( strcmp(outputFName,"stderr") == 0 )
        outputFD = stderr ;
    else
    {
        if ( (outputFD = fopen( outputFName , "wb" )) == NULL )
            error("DecoderBatchTest::setupOutputFile - error opening output file") ;
    }

    // Now output any format-specific header information
    if ( (outputFormat == DBT_OUTPUT_MLF) || (outputFormat == DBT_OUTPUT_XMLF) )
        fputs( "#!MLF!#\n" , outputFD ) ;

    fflush( outputFD ) ;
}


void Juicer::DecoderBatchTest::closeOutputFile()
{
    if ( outputFD == NULL )
        return ;

    // Place any format-specific footer output here
    if ( outputFormat == DBT_OUTPUT_VERBOSE )
    {
        if ( haveExpResults == true )
        {
            //printStatistics( 3 , 3 , 4 ) ;   // NIST settings for Ins, Del, Sub calculations
            printStatistics( 7 , 7 , 10 ) ;   // HTK settings for Ins, Del, Sub calculations
        }
    }

    // Close the output file
    if ( (outputFD != stdout) && (outputFD != stderr) )
    {
        fclose( outputFD ) ;
        outputFD = NULL ;
    }
}


void Juicer::DecoderBatchTest::outputResult( DecoderSingleTest *test )
{
    if ( outputFD == NULL )
        error("DecoderBatchTest::outputResult - output file not opened") ;

    if ( test->nActualWords > 0 )
    {
        if ( outputFormat == DBT_OUTPUT_REF )
        {
            for ( int j=0 ; j<test->nActualWords ; j++ )
                fprintf( outputFD , "%s " , vocab->words[test->actualWords[j]] ) ;
            fprintf( outputFD , "\n" ) ;
        }
        else if ( outputFormat == DBT_OUTPUT_TRANS )
        {
            for ( int j=0 ; j<test->nActualWords ; j++ )
                fprintf( outputFD , "%s " , vocab->words[test->actualWords[j]] ) ;
            // trans style line (for easy input to nerrcom)
            fprintf( outputFD , "(trans-%d)\n" , test->nActualWords ) ;
        }
        else if ( (outputFormat == DBT_OUTPUT_MLF) || (outputFormat == DBT_OUTPUT_XMLF) )
        {
            const char *ptr , *str ;
            char *ptr2 , str2[10000] ;

            // remove the extension and path from the filename
            str = test->getTestFName() ;
            if ( (ptr=strrchr( str , '/' )) != NULL )
                ptr++ ;
            else
                ptr = str ;

            strcpy( str2 , ptr ) ;
            if ( (ptr2=strrchr( str2 , '.' )) != NULL )
                *ptr2 = '\0' ;

            fprintf( outputFD , "\"*/%s.rec\"\n" , str2 ) ;

            for ( int j=0 ; j<test->nActualWords ; j++ )
                fprintf( outputFD , "%s\n" , vocab->words[test->actualWords[j]] ) ;
            fprintf( outputFD , ".\n" ) ;
        }
        else if ( outputFormat == DBT_OUTPUT_VERBOSE )
        {
            int i ;
            fprintf( outputFD , "%s\n" , test->getTestFName() ) ;

            if ( haveExpResults == true )
            {
                fprintf( outputFD , "\tExpected :  ") ;
                for ( i=0 ; i<test->nExpectedWords ; i++ )
                {
                    if ( test->expectedWords[i] < 0 )
                        fprintf( outputFD , "<OOV> ") ;
                    else
                        fprintf( outputFD , "%s " , vocab->words[test->expectedWords[i]] ) ;
                }
                fprintf( outputFD , "\n" ) ;
            }

            fprintf( outputFD , "\tActual :    ") ;
            for ( i=0 ; i<test->nActualWords ; i++ )
                fprintf( outputFD , "%s " , vocab->words[test->actualWords[i]] ) ;
            // (word-ending) time stamps
            fprintf( outputFD , "  [ ") ;
            for ( i=0 ; i<test->nActualWords ; i++ )
                fprintf( outputFD , "%d " , (test->actualWordsTimes[i])+1 ) ;
            fprintf( outputFD , "(%d) ]\n" , test->getNumFrames() ) ;
        }
    }
    else
    {
        if ( test->nResultLevels > 1 )
            error("DBT::outputResult - test->nResultLevels > 1") ;

        if ( outputFormat == DBT_OUTPUT_REF )
        {
            for ( int j=0 ; j<test->nResultWords ; j++ )
                fprintf( outputFD , "%s " , vocab->words[test->resultWords[0][j].index] ) ;
            fprintf( outputFD , "\n" ) ;
        }
        else if ( outputFormat == DBT_OUTPUT_TRANS )
        {
            for ( int j=0 ; j<test->nResultWords ; j++ )
                fprintf( outputFD , "%s " , vocab->words[test->resultWords[0][j].index] ) ;
            // trans style line (for easy input to nerrcom)
            fprintf( outputFD , "(trans-%d)\n" , test->nResultWords ) ;
        }
        else if ( (outputFormat == DBT_OUTPUT_MLF) || (outputFormat == DBT_OUTPUT_XMLF) )
        {
            const char *ptr , *str ;
            char *ptr2 , str2[10000] ;

            // remove the extension and path from the filename
            str = test->getTestFName() ;
            if ( (ptr=strrchr( str , '/' )) != NULL )
                ptr++ ;
            else
                ptr = str ;

            strcpy( str2 , ptr ) ;
            if ( (ptr2=strrchr( str2 , '.' )) != NULL )
                *ptr2 = '\0' ;

            fprintf( outputFD , "\"*/%s.rec\"\n" , str2 ) ;

            if ( outputFormat == DBT_OUTPUT_MLF )
            {
                for ( int j=0 ; j<test->nResultWords ; j++ )
                    fprintf( outputFD , "%s\n" , vocab->words[test->resultWords[0][j].index] ) ;
            }
            else if ( outputFormat == DBT_OUTPUT_XMLF )
            {
                double st , et ;
                for ( int j=0 ; j<test->nResultWords ; j++ ) {
                    st = ( (real)1.0e7 / (real)framesPerSec *
                           (real)test->resultWords[0][j].startTime );

                    if ( st > 0 )
                        st += ( (real)1.0e7 / (real)framesPerSec );

                    et = ( (real)1.0e7 / (real)framesPerSec *
                           (real)test->resultWords[0][j].endTime );

                    if ( et > 0 )
                        et += ( (real)1.0e7 / (real)framesPerSec );

                    // Offset is frameTime in ns, / 100 for HTK
                    double offset = (double)test->frameTime(0) / 100;
                    fprintf( outputFD , "%.0f %.0f %s %f\n",
                             st + offset, et + offset,
                             vocab->words[test->resultWords[0][j].index] ,
                             test->resultWords[0][j].acousticScore +
                             test->resultWords[0][j].lmScore );

                }
            }
            fprintf( outputFD , ".\n" ) ;
        }
        else if ( outputFormat == DBT_OUTPUT_VERBOSE )
        {
            int i ;
            fprintf( outputFD , "%s\n" , test->getTestFName() ) ;

            if ( haveExpResults == true )
            {
                fprintf( outputFD , "\tExpected :  ") ;
                for ( i=0 ; i<test->nExpectedWords ; i++ )
                {
                    if ( test->expectedWords[i] < 0 )
                        fprintf( outputFD , "<OOV> ") ;
                    else
                        fprintf( outputFD , "%s " , vocab->words[test->expectedWords[i]] ) ;
                }
                fprintf( outputFD , "\n" ) ;
            }

            fprintf( outputFD , "\tActual :    ") ;
            for ( i=0 ; i<test->nResultWords ; i++ )
                fprintf( outputFD , "%s " , vocab->words[test->resultWords[0][i].index] ) ;
            // (word-ending) time stamps
            fprintf( outputFD , "  [ ") ;
            for ( i=0 ; i<test->nResultWords ; i++ )
                fprintf( outputFD , "%d " , (test->resultWords[0][i].endTime)+1 ) ;
            fprintf( outputFD , "(%d) ]\n" , test->getNumFrames() ) ;
        }

        TimeType ft = test->frameTime(0);
        LogFile::puts("\nRecognition result:\n\n") ;
        LogFile::printf("Speaker: %s\n", test->speakerID()) ;
        LogFile::printf("Start time %lld ns = %.3f s\n\n", ft, (double)ft/ONEe9);
        for ( int j=0 ; j<test->nResultWords ; j++ )
        {
            LogFile::printf( "    %s  start=%d end=%d " , vocab->words[test->resultWords[0][j].index] ,
                             test->resultWords[0][j].startTime , test->resultWords[0][j].endTime ) ;
            LogFile::printf( "acousticScore=%.4f lmScore=%.4f\n" ,
                             test->resultWords[0][j].acousticScore , test->resultWords[0][j].lmScore );
        }

        if (test->resultWords)
        {
            ft = test->frameTime(
                test->resultWords[0][test->nResultWords-1].endTime
            );
            LogFile::printf("\nEnd time %lld ns = %.3f s\n\n",
                            ft, (double)ft/ONEe9);
        }
        else
            LogFile::printf("\nEnd time unknown\n\n") ;

        LogFile::printf("\ntotal scores: lm=%.3f ac=%.3f\n\n" ,
                        test->getTotalLMScore() , test->getTotalAcousticScore() ) ;
    }

    fflush( outputFD ) ;
}


void Juicer::DecoderBatchTest::outputResultPhones( DecoderSingleTest *test )
{
    if ( outputFD == NULL )
        error("DecoderBatchTest::outputResultPhones - output file not opened") ;

    if ( test->nActualWords > 0 )
    {
        if ( outputFormat == DBT_OUTPUT_REF )
        {
            for ( int j=0 ; j<test->nActualWords ; j++ )
                fprintf( outputFD , "%s " , phoneLookup->getModelStr(test->actualWords[j]) ) ;
            fprintf( outputFD , "\n" ) ;
        }
        else if ( outputFormat == DBT_OUTPUT_TRANS )
        {
            for ( int j=0 ; j<test->nActualWords ; j++ )
                fprintf( outputFD , "%s " , phoneLookup->getModelStr(test->actualWords[j]) ) ;
            // trans style line (for easy input to nerrcom)
            fprintf( outputFD , "(trans-%d)\n" , test->nActualWords ) ;
        }
        else if ( (outputFormat == DBT_OUTPUT_MLF) || (outputFormat == DBT_OUTPUT_XMLF) )
        {
            const char *ptr , *str ;
            char *ptr2 , str2[10000] ;

            // remove the extension and path from the filename
            str = test->getTestFName() ;
            if ( (ptr=strrchr( str , '/' )) != NULL )
                ptr++ ;
            else
                ptr = str ;

            strcpy( str2 , ptr ) ;
            if ( (ptr2=strrchr( str2 , '.' )) != NULL )
                *ptr2 = '\0' ;

            fprintf( outputFD , "\"*/%s.rec\"\n" , str2 ) ;

            for ( int j=0 ; j<test->nActualWords ; j++ )
                fprintf( outputFD , "%s\n" , phoneLookup->getModelStr(test->actualWords[j]) ) ;
            fprintf( outputFD , ".\n" ) ;
        }
        else if ( outputFormat == DBT_OUTPUT_VERBOSE )
        {
            int i ;
            fprintf( outputFD , "%s\n" , test->getTestFName() ) ;

            if ( haveExpResults == true )
            {
                fprintf( outputFD , "\tExpected :  ") ;
                for ( i=0 ; i<test->nExpectedWords ; i++ )
                {
                    if ( test->expectedWords[i] < 0 )
                        fprintf( outputFD , "<OOV> ") ;
                    else
                        fprintf( outputFD , "%s " , phoneLookup->getModelStr(test->expectedWords[i]) ) ;
                }
                fprintf( outputFD , "\n" ) ;
            }

            fprintf( outputFD , "\tActual :    ") ;
            for ( i=0 ; i<test->nActualWords ; i++ )
                fprintf( outputFD , "%s " , phoneLookup->getModelStr(test->actualWords[i]) ) ;
            // (word-ending) time stamps
            fprintf( outputFD , "  [ ") ;
            for ( i=0 ; i<test->nActualWords ; i++ )
                fprintf( outputFD , "%d " , (test->actualWordsTimes[i])+1 ) ;
            fprintf( outputFD , "(%d) ]\n" , test->getNumFrames() ) ;
        }
    }
    else
    {
        if ( (test->nResultLevels > 0) && (test->nResultLevels != 2) )
            error("DBT::outputResultPhones - test->nResultLevels != 0 or 2") ;

        if ( outputFormat == DBT_OUTPUT_REF )
        {
            // Only output phones i.e. first result level
            for ( int j=0 ; j<test->nResultWords ; j++ )
                fprintf( outputFD , "%s " , phoneLookup->getModelStr(test->resultWords[0][j].index) ) ;
            fprintf( outputFD , "\n" ) ;
        }
        else if ( outputFormat == DBT_OUTPUT_TRANS )
        {
            // Only output phones i.e. first result level
            for ( int j=0 ; j<test->nResultWords ; j++ )
                fprintf( outputFD , "%s " , phoneLookup->getModelStr(test->resultWords[0][j].index) ) ;
            // trans style line (for easy input to nerrcom)
            fprintf( outputFD , "(trans-%d)\n" , test->nResultWords ) ;
        }
        else if ( (outputFormat == DBT_OUTPUT_MLF) || (outputFormat == DBT_OUTPUT_XMLF) )
        {
            const char *ptr , *str ;
            char *ptr2 , str2[10000] ;

            // remove the extension and path from the filename
            str = test->getTestFName() ;
            if ( (ptr=strrchr( str , '/' )) != NULL )
                ptr++ ;
            else
                ptr = str ;

            strcpy( str2 , ptr ) ;
            if ( (ptr2=strrchr( str2 , '.' )) != NULL )
                *ptr2 = '\0' ;

            fprintf( outputFD , "\"*/%s.rec\"\n" , str2 ) ;

            if ( outputFormat == DBT_OUTPUT_MLF )
            {
                for ( int j=0 ; j<test->nResultWords ; j++ )
                {
                    fprintf( outputFD, "%s", phoneLookup->getModelStr(test->resultWords[0][j].index));
                    if ( test->resultWords[1][j].index >= 0 )
                    {
                        fprintf( outputFD , " %s" , vocab->words[test->resultWords[1][j].index] ) ;
                    }
                    fprintf( outputFD , "\n" ) ;
                }
            }
            else if ( outputFormat == DBT_OUTPUT_XMLF )
            {
                double st , et ;
                for ( int j=0 ; j<test->nResultWords ; j++ ) {
                    st = ( (real)1.0e7 / (real)framesPerSec *
                           (real)test->resultWords[0][j].startTime );
                    if ( st > 0 )
                        st += ( (real)1.0e7 / (real)framesPerSec  );
                    et = ( (real)1.0e7 / (real)framesPerSec *
                           (real)test->resultWords[0][j].endTime  );
                    if ( et > 0 )
                        et += ( (real)1.0e7 / (real)framesPerSec );

                    // change by John Dines
                    fprintf( outputFD , "%.0f %.0f %s %f" , st , et ,
                             phoneLookup->getModelStr(test->resultWords[0][j].index) ,
                             test->resultWords[0][j].acousticScore + test->resultWords[0][j].lmScore ) ;
                    if ( test->resultWords[1][j].index >= 0 )
                    {
                        fprintf( outputFD , " %s" , vocab->words[test->resultWords[1][j].index] ) ;
                    }
                    fprintf( outputFD , "\n" ) ;

                }
            }
            fprintf( outputFD , ".\n" ) ;
        }
        else if ( outputFormat == DBT_OUTPUT_VERBOSE )
        {
            // Only output phones i.e. first result level (for the moment?)
            int i ;
            fprintf( outputFD , "%s\n" , test->getTestFName() ) ;

            if ( haveExpResults == true )
            {
                fprintf( outputFD , "\tExpected :  ") ;
                for ( i=0 ; i<test->nExpectedWords ; i++ )
                {
                    if ( test->expectedWords[i] < 0 )
                        fprintf( outputFD , "<OOV> ") ;
                    else
                        fprintf( outputFD , "%s " , phoneLookup->getModelStr(test->expectedWords[i]) ) ;
                }
                fprintf( outputFD , "\n" ) ;
            }

            fprintf( outputFD , "\tActual :    ") ;
            for ( i=0 ; i<test->nResultWords ; i++ )
                fprintf( outputFD , "%s " , phoneLookup->getModelStr(test->resultWords[0][i].index) ) ;
            // (word-ending) time stamps
            fprintf( outputFD , "  [ ") ;
            for ( i=0 ; i<test->nResultWords ; i++ )
                fprintf( outputFD , "%d " , (test->resultWords[0][i].endTime)+1 ) ;
            fprintf( outputFD , "(%d) ]\n" , test->getNumFrames() ) ;
        }

        LogFile::puts("\nRecognition result:\n\n") ;
        for ( int j=0 ; j<test->nResultWords ; j++ )
        {
            LogFile::printf( "    %s  start=%d end=%d " ,
                             phoneLookup->getModelStr(test->resultWords[0][j].index) ,
                             test->resultWords[0][j].startTime , test->resultWords[0][j].endTime ) ;
            LogFile::printf( "acousticScore=%.6f lmScore=%.6f\n" ,
                             test->resultWords[0][j].acousticScore , test->resultWords[0][j].lmScore ) ;
        }

        LogFile::printf("\ntotal scores: lm=%.3f ac=%.3f\n\n" ,
                        test->getTotalLMScore() , test->getTotalAcousticScore() ) ;
    }

    fflush( outputFD ) ;
}


void Juicer::DecoderBatchTest::outputWFSTLattice( DecoderSingleTest *test )
{
#ifdef DEBUG
    if ( (mode != DBT_MODE_WFSTDECODE_WORDS) && (mode != DBT_MODE_WFSTDECODE_PHONES) )
        error("DBT::outputWFSTLattice - mode not DBT_MODE_WFSTDECODE_*");
    if ( (doLatticeGeneration == false) || (latticeDir == NULL) )
        error("DBT::outputWFSTLattice - doLatticeGeneration == false OR latticeDir == NULL") ;
#endif

    const char *ptr , *str ;
    char *ptr2 , str2[10000] ;

    // replace original testFName path with latticeDir
    str = test->getTestFName() ;
    if ( (ptr=strrchr( str , '/' )) != NULL )
        ptr++ ;
    else
        ptr = str ;
    sprintf( str2 , "%s/%s" , latticeDir , ptr ) ;

    // Replace existing extension with fsm
    if ( (ptr2=strrchr( str2 , '.' )) != NULL )
        *ptr2 = '\0' ;
    strcat( str2 , ".fsm" ) ;

    LogFile::printf("\nLattice FSM: %s" , str2 ) ;

    WFSTLattice *lattice = wfstDecoder->getLattice() ;
    lattice->writeLatticeFSM( str2 ) ;

    LogFile::printf(" ... done\n\n") ;
}


void Juicer::DecoderBatchTest::run()
{
    decodeTime = 0.0 ;
    speechTime = 0.0 ;

    // open the output file and output format-specific header info
    openOutputFile() ;

    // loop is public - bit of a hack
    if (loop)
    {
        LogFile::printf( "Single test file with factory: looping\n" ) ;
        LogFile::printf( "File: %s\n" , tests[0]->getTestFName() ) ;

        tests[0]->openSource(frontend);
        while (1)
        {
            // run the test
            if ( (mode == DBT_MODE_WFSTDECODE_WORDS) ||
                 (mode == DBT_MODE_WFSTDECODE_PHONES) )
                tests[0]->decodeUtterance( wfstDecoder , frontend, vocab ) ;
            else
                error("DecoderBatchTest::run - mode invalid") ;

            // output the result
            if ( mode != DBT_MODE_WFSTDECODE_PHONES )
                outputResult( tests[0] ) ;
            else
                outputResultPhones( tests[0] ) ;
            if ( doLatticeGeneration &&
                 ((mode == DBT_MODE_WFSTDECODE_WORDS) ||
                  (mode == DBT_MODE_WFSTDECODE_PHONES)) )
                outputWFSTLattice( tests[0] ) ;

            real uttTime = (real)(tests[0]->getNumFrames()) /
                (real)framesPerSec ;
            real decTime = tests[0]->getDecodeTime() ;
            decodeTime += decTime ;
            speechTime += uttTime ;

            LogFile::printf(
                "CPU time %.3f  speech time %.3f  RT factor %.3f\n" ,
                decTime , uttTime , decTime / uttTime
            ) ;
        }
    }


    for ( int i=0 ; i<nTests ; i++ )
    {
        LogFile::printf( "File: %s\n" , tests[i]->getTestFName() ) ;

        // run the test
        if ( (mode == DBT_MODE_WFSTDECODE_WORDS) || (mode == DBT_MODE_WFSTDECODE_PHONES) )
            tests[i]->run( wfstDecoder , frontend, vocab ) ;
        else
            error("DecoderBatchTest::run - mode invalid") ;

        // output the result
        if ( mode != DBT_MODE_WFSTDECODE_PHONES )
        {
            outputResult( tests[i] ) ;
        }
        else
        {
            outputResultPhones( tests[i] ) ;
        }

        if ( doLatticeGeneration &&
             ((mode == DBT_MODE_WFSTDECODE_WORDS) || (mode == DBT_MODE_WFSTDECODE_PHONES)) )
        {
            outputWFSTLattice( tests[i] ) ;
        }

        real uttTime = (real)(tests[i]->getNumFrames()) / (real)framesPerSec ;
        real decTime = tests[i]->getDecodeTime() ;
        decodeTime += decTime ;
        speechTime += uttTime ;

        LogFile::printf( "CPU time %.3f  speech time %.3f  RT factor %.3f\n" ,
                         decTime , uttTime , decTime / uttTime ) ;
    }

    LogFile::printf(
        "\n\n"
        "Total CPU time %.3f  Total speech time %.3f  Avg. RT factor %.3f\n" ,
        decodeTime , speechTime , decodeTime / speechTime
    ) ;

    // output any format-specific footer information and close the output file
    closeOutputFile() ;
}


void Juicer::DecoderBatchTest::configureTests()
{
    FILE *inputFD=NULL , *resultsFD=NULL ;
    char *line , *fname , *resWord , *ptr ;
    int *tempResultList ,  nSentWords=0 , i=0 , testIndex , wordCnt ;
    char **inputFNames=NULL ;
    bool haveMLF=false ;

    line = new char[100000] ;
    fname = new char[100000] ;
    resWord = new char[1000] ;
    tempResultList = new int[100000] ;

    // Open the file containing the names of the data files we want to run tests for.
    if ( (inputFD = fopen( inputFName , "rb" )) == NULL )
        error("DecoderBatchTest::configureTests - error opening input file") ;

    // Open the file containing the expected results for each test.
    // We assume that the format is as per HTK MLF format.
    // Note that the filename line must be enclosed in "".
    if ( haveExpResults )
    {
        if ( (resultsFD = fopen( expResultsFName , "rb" )) == NULL )
            error("DecoderBatchTest::configureTests - error opening results file") ;

        // Read the first line of the results file to determine its type
        fgets( line , 1000 , resultsFD ) ;
        if ( strstr( line , "MLF" ) )
            haveMLF = true ;
        else
        {
            haveMLF = false ;
            fseek( resultsFD , 0 , SEEK_SET ) ;
        }
    }

    // Determine the number of inputFNames present in the datafiles file
    nTests=0 ;
    while ( fgets( line , 100000 , inputFD ) != NULL )
    {
	
        if ( (strlen(line) == 0) || (line[0] == '#') || (line[0] == '\n') ||
             (line[0] == '\r') || (line[0] == ' ') || (line[0] == '\t') )
            continue ;

	strncpy(fname, line, 10000);
	fname[strlen(fname)-1]='\0';	

        if ( nTests >= nTestsAlloc )
        {
            nTestsAlloc += 100 ;
            tests = (DecoderSingleTest **)realloc( tests , nTestsAlloc*sizeof(DecoderSingleTest *) ) ;
            inputFNames = (char **)realloc( inputFNames , nTestsAlloc*sizeof(char *) ) ;
        }

        tests[nTests] = NULL ;
        inputFNames[nTests] = new char[strlen(fname)+1] ;
        strcpy( inputFNames[nTests] , fname ) ;

        nTests++ ;
    }

    if ( haveExpResults == true )
    {
        // Read each entry in the expected results file, find its corresponding
        //   filename in the temporary list of filename, create a DecoderSingleTest
        //   instance and add it to the list of tests.
        testIndex = 0 ;
        while ( fgets( line , 100000 , resultsFD ) != NULL )
        {
            if ( haveMLF == true )
            {
                if ( sscanf(line,"\"%[^\"]",fname) != 0 )
                {
                    // remove the extension and path from the filename
                    if ( (ptr=strrchr( fname , '/' )) != NULL )
                        memmove( fname , ptr+1 , strlen(ptr)+1 ) ;
                    if ( (ptr=strrchr( fname , '.' )) != NULL )
                        *(ptr+1) = '\0' ; // leave the '.' so that strstr works properly with all fnames

                    // find the filename in the temporary list of inputFNames
                    for ( i=0 ; i<nTests ; i++ )
                    {
                        if ( strstr( inputFNames[i] , fname ) )
                        {
                            // We found the filename in the temporary list of inputFNames.
                            // Read the expected words.
                            nSentWords = 0 ;
                            fgets( line , 100000 , resultsFD ) ;
                            while ( line[0] != '.' )
                            {
                                sscanf( line , "%s" , resWord ) ;
                                tempResultList[nSentWords] = vocab->getIndex( resWord ) ;
                                if ( tempResultList[nSentWords] >= 0 )
                                    nSentWords++ ;
                                else
                                    warning("Unknown word in ground truth for %s",inputFNames[i]) ;
                                fgets( line , 100000 , resultsFD ) ;
                            }

                            // Now configure the next DecoderSingleTest instance
                            //   with the details of the test.
                            if ( tests[i] != NULL )
                            {
                                error("DecoderBatchTest::configureTests - duplicate reference transcript"
                                      " %s" , fname );
                            }
                            tests[i] = new DecoderSingleTest() ;
                            tests[i]->configure( mode , inputVecSize , inputFNames[i] , nSentWords ,
                                                 tempResultList , inputFormat , removeSil ) ;
                            break ;
                        }
                    }
                }
            }
            else
            {
                // We have expected results in reference format
                // Extract the words in the sentence
                ptr = strtok( line , " \r\n\t" ) ;
                wordCnt = 0 ;
                while ( ptr != NULL )
                {
                    if ( (tempResultList[wordCnt] = vocab->getIndex( ptr )) < 0 )
                        printf("DBT::cfgTests - result word %s not in vocab for test %d\n",ptr,i+1) ;
                    wordCnt++ ;
                    ptr = strtok( NULL , " \r\n\t" ) ;
                }

                // Configure the DecoderSingleTest instance
                if ( testIndex >= nTests )
                    error("DBT::configureTests - testIndex out of range");
                if ( tests[testIndex] != NULL )
                    error("DBT::configureTests - duplicate exp results");
                tests[testIndex] = new DecoderSingleTest() ;
                tests[testIndex]->configure( mode , inputVecSize , inputFNames[testIndex] , wordCnt ,
                                             tempResultList , inputFormat , removeSil ) ;
                testIndex++ ;
            }
        }

        // Check that each element of 'tests' has been configured
        for ( i=0 ; i<nTests ; i++ )
        {
            if ( tests[i] == NULL )
            {
                error( "DBT::configureTests - ref transcription not found for file %s" ,
                       inputFNames[i] ) ;
            }

            // this is useful to generate an in-order ref transcription file from an MLF file.
            //for ( int j=0 ; j<tests[i]->nExpectedWords ; j++ )
            //    printf("%s ",vocab->getWord( tests[i]->expectedWords[j] ) ) ;
            //printf("\n");
        }
    }
    else
    {
        for ( i=0 ; i<nTests ; i++ )
        {
            tests[i] = new DecoderSingleTest() ;
            tests[i]->configure( mode , inputVecSize , inputFNames[i] , 0 , NULL , inputFormat ,
                                 removeSil ) ;
        }
    }

    delete [] line ;
    delete [] fname ;
    delete [] resWord ;
    delete [] tempResultList ;

    // Free the temporary list of inputFNames
    for ( i=0 ; i<nTests ; i++ )
    {
        delete [] inputFNames[i] ;
    }
    free( inputFNames ) ;

    if ( haveExpResults == true )
        fclose( resultsFD ) ;
    fclose( inputFD ) ;
}


void Juicer::DecoderBatchTest::outputText()
{
    printf("Number of tests = %d\n",nTests) ;
    for ( int i=0 ; i<nTests ; i++ )
    {
        printf("\nTest %d\n********\n",i) ;
        //tests[i]->outputText() ;
        printf("\n") ;
    }
}
