/*
 * Copyright 2004 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#ifndef DECODERBATCHTEST_INC
#define DECODERBATCHTEST_INC

#include "general.h"

#include "FrontEnd.h"
#include "DecLexInfo.h"
#include "DecVocabulary.h"
#include "DecoderSingleTest.h"
#include "Decoder.h"
#include "MonophoneLookup.h"


/*
	Author:		Darren Moore (moore@idiap.ch)
	Date:			2004
	$Id: DecoderBatchTest.h,v 1.12 2005/08/26 01:16:34 moore Exp $
*/

namespace Juicer {


typedef enum
{
    DBT_OUTPUT_VERBOSE=0 ,
    DBT_OUTPUT_TRANS ,
    DBT_OUTPUT_REF ,
    DBT_OUTPUT_MLF ,
    DBT_OUTPUT_XMLF ,
    DBT_OUTPUT_HUB ,
    DBT_OUTPUT_INVALID
} DBTOutputFormat ;


/**
 * This class is used to decode a set of test files and display
 * statistics and results for each file (ie. expected and actual *
 * words recognised) and also for the entire test set (ie. insertions,
 * deletions, substitutions, accuracy, total time).
 *
 *  @author Darren Moore (moore@idiap.ch)
 */
class DecoderBatchTest
{
public:
	DecVocabulary				*vocab ;

	int							nTests ;
	int							nTestsAlloc ;
	DecoderSingleTest			**tests ;

	// Constructors / destructor
	DecoderBatchTest(
        DecVocabulary *vocab_, PhoneLookup *phoneLookup_,
        FrontEnd *frontend_ , IDecoder *wfstDecoder_ ,
        const char *inputFName_ , DSTDataFileFormat inputFormat_ , int inputVecSize_ ,
        const char *outputFName_ , DBTOutputFormat outputFormat_ ,
        const char *expResultsFName_ , bool removeSil_ , int framesPerSec_ ) ;
	~DecoderBatchTest() ;

	// Public methods
   void activateLatticeGeneration( const char *latticeDir_ ) ;
	void run() ;
	void outputText() ;

    bool loop;  // public for now as a quick hack

private:
	// Private member variables
	DBTMode						mode ;
    IDecoder             *wfstDecoder ;
    FrontEnd    *frontend;
    PhoneLookup             *phoneLookup ;
	
	char							*inputFName ;
	DSTDataFileFormat			inputFormat ;
   int                     inputVecSize ;
   int                     framesPerSec ;
   real                    decodeTime ;
   real                    speechTime ;

	DBTOutputFormat			outputFormat ;
	char							*outputFName ;
	FILE							*outputFD ;

	bool							haveExpResults ;
	char							*expResultsFName ;
	bool							removeSil ;
	
   bool                    doLatticeGeneration ;
   char                    *latticeDir ;

	// Private methods
	void init( const char *inputFName_ , DSTDataFileFormat inputFormat_ , int inputVecSize_ , 
              const char *outputFName_ , DBTOutputFormat outputFormat_ , 
              const char *expResultsFName_ , bool removeSil_ , int framesPerSec_ ) ;
	void openOutputFile() ;
	void outputResult( DecoderSingleTest *test ) ;
	void outputResultPhones( DecoderSingleTest *test ) ;
   void outputWFSTLattice( DecoderSingleTest *test ) ;
	void closeOutputFile() ;
	void configureTests() ; 
	void printStatistics( int i_cost , int d_cost , int s_cost ) ;
};


}

#endif
