/*
 * Copyright 2004 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#include "DecoderSingleTest.h"
#include "time.h"
#include "sys/types.h"
#include "sys/stat.h"


/*
   Author:		Darren Moore (moore@idiap.ch)
                        Modified by John Dines to support LNA16bit
   Date:		2004
*/


using namespace Torch;

namespace Juicer {


int DecoderSingleTest::framesPerSec = 0 ;


DecoderSingleTest::DecoderSingleTest()
{
   mode = DBT_MODE_INVALID ;
   testFName = NULL ;
   expectedWords = NULL ;
   nExpectedWords = 0 ;
   actualWords = NULL ;
   actualWordsTimes = NULL ;
   nActualWords = 0 ;
   inputFormat = DST_NOFORMAT ;
   removeSentMarks = false ;

   nFrames=0 ;
   decodeTime = 0.0 ;
   totalLMScore = LOG_ZERO ;
   totalAcousticScore = LOG_ZERO ;

   decoderInput = NULL ;

   nResultLevels = 0 ;
   nResultWords = 0 ;
   resultWords = NULL ;

   inputsAreFeatures = false ;
   expVecSize = -1 ;
   vecSize = 0 ;
   dataFName = NULL ;
   extStartFrame = -1 ;
   extEndFrame = -1 ;

   startTimeStamp = 0;
   mSpeakerID = 0;
}


DecoderSingleTest::~DecoderSingleTest()
{
   delete [] testFName ;
   delete [] dataFName ;

   delete [] expectedWords ;

   if ( actualWords != NULL )
      free( actualWords ) ;
   if ( actualWordsTimes != NULL )
      free( actualWordsTimes ) ;

   if ( decoderInput != NULL )
   {
      if ( inputFormat == DST_FEATS_HTK )
      {
         delete [] decoderInput[0] ;
         delete [] decoderInput ;
      }
      else
      {
         for ( int i=0 ; i<nFrames ; i++ )
            delete [] decoderInput[i] ;
         free( decoderInput ) ;
      }
   }

   if ( resultWords != NULL )
   {
      delete [] resultWords[0] ;
      delete [] resultWords ;
   }
}


void DecoderSingleTest::configure(
    DBTMode mode_ , int expVecSize_ , char *dataFName_ , 
    int nExpectedWords_ , int *expectedWords_ ,
    DSTDataFileFormat inputFormat_ ,
    bool removeSentMarks_ ) 
{
   mode = mode_ ;
   if ( (expVecSize = expVecSize_) <= 0 )
      error("DST::configure - expVecSize_ <= 0") ;

   if ( strchr( dataFName_ , '=' ) != NULL )
   {
      // We have an extended HTK filename.
      // Make a copy of the extended filename that we can carve up.
      char *str = new char[strlen(dataFName_)+1] ;
      strcpy( str , dataFName_ ) ;

      // Extract symbolic name
      char *ptr = strtok( str , "=" ) ;
      testFName = new char[strlen(ptr)+1] ;
      strcpy( testFName , ptr ) ;

      // Extract real file name
      if ( (ptr = strtok( NULL , "[" )) == NULL )
         error("DST::configure - error isolating real filename from extended filename") ;
      dataFName = new char[strlen(ptr)+1] ;
      strcpy( dataFName , ptr ) ;

      // Extract start frame
      if ( (ptr = strtok( NULL , "," )) == NULL )
         error("DST::configure - error isolating start frame from extended filename") ;
      if ( sscanf( ptr , "%d" , &extStartFrame ) != 1 ) 
         error("DST::configure - error extracting start frame from extended filename") ;

      // Extract end frame
      if ( (ptr = strtok( NULL , "]" )) == NULL )
         error("DST::configure - error isolating end frame from extended filename") ;
      if ( sscanf( ptr , "%d" , &extEndFrame ) != 1 ) 
         error("DST::configure - error extracting end frame from extended filename") ;

      // Some basic error checking
      if ( extStartFrame < 0 )
         error("DST::configure - extStartFrame < 0") ;
      if ( extEndFrame <= 0 )
         error("DST::configure - extEndFrame <= 0") ;
      if ( extStartFrame >= extEndFrame )
         error("DST::configure - extStartFrame >= extEndFrame") ;

      delete [] str ;
   }
   else
   {
      // Simple filename - just make a copy
      dataFName = new char[strlen(dataFName_)+1] ;
      strcpy( dataFName , dataFName_ ) ;

      testFName = new char[strlen(dataFName_)+1] ;
      strcpy( testFName , dataFName_ ) ;
   }

   // Allocate memory to hold the array of word indices that constitute the
   //   expected result of the test and copy the results.
   if ( (nExpectedWords_ > 0) && (expectedWords_ != NULL) )
   {
      nExpectedWords = nExpectedWords_ ;
      expectedWords = new int[nExpectedWords] ;
      memcpy( expectedWords , expectedWords_ , nExpectedWords*sizeof(int) ) ;
   }
   else
   {
      nExpectedWords = 0 ;
      expectedWords = NULL ;
   }

   nActualWords = 0 ;
   actualWords = NULL ;
   actualWordsTimes = NULL ;

   inputFormat = inputFormat_ ;
   removeSentMarks = removeSentMarks_ ;

   if ( (inputFormat < 0) || (inputFormat >= DST_NOFORMAT) )
      error("DST::configure - invalid inputFormat_") ;
}


/**
 * Runs the actual decoding.  Loads data into memory, passes the whole
 * array to the decoder and gets a hypothesis back.
 */
void DecoderSingleTest::run(
    WFSTDecoder *decoder , FrontEnd *frontend , DecVocabulary *vocab
)
{
    clock_t startTime , endTime ;

#ifdef DEBUG
    if ( (mode != DBT_MODE_WFSTDECODE_WORDS) &&
         (mode != DBT_MODE_WFSTDECODE_PHONES) )
        error("DecoderSingleTest::run - incompatible mode") ;
#endif

    // Connect to the source
    assert(dataFName);
    frontend->SetSource(dataFName);
    startTimeStamp = frontend->TimeStamp(0);

    // Timer for the decoding
    startTime = clock() ;

    // Run the decoder over the whole available input
    decoder->init() ;
    nFrames = 0;
    float* array;
    int offset = extStartFrame < 0 ? 0 : extStartFrame;
    while(frontend->GetArray(array, nFrames+offset))
    {
        decoder->processFrame(array, nFrames++);
        if ((extEndFrame >= 0) && (nFrames+offset > extEndFrame))
            break;
    }
    DecHyp* hyp = decoder->finish() ;
    endTime = clock() ;
    decodeTime = (real)(endTime-startTime) / CLOCKS_PER_SEC ;

    mSpeakerID = frontend->GetSpeakerID(offset / 2);

    // post-process the decoding result
    if ( hyp == NULL )
    {
        nResultLevels = 0 ;
        nResultWords = 0 ;
        resultWords = NULL ;
    }
    else
    {
        extractResultsFromHyp( hyp , vocab ) ;
    }

    //if ( removeSentMarks )
    //   removeSentMarksFromActual( vocab ) ;

    decoderInput = NULL ;
    vecSize = 0 ;
}


/**
 * Opens the file for decoding.  No need to call if this is actually a
 * sound device.
 */
void DecoderSingleTest::openSource(FrontEnd *frontend)
{
    assert(dataFName);
    frontend->SetSource(dataFName);
}


/**
 * Runs the actual decoding.
 */
void DecoderSingleTest::decodeUtterance(
    WFSTDecoder *decoder , FrontEnd *frontend , DecVocabulary *vocab
)
{
    clock_t startTime , endTime ;

#ifdef DEBUG
    if ( (mode != DBT_MODE_WFSTDECODE_WORDS) &&
         (mode != DBT_MODE_WFSTDECODE_PHONES) )
        error("DecoderSingleTest::decodeUtterance - incompatible mode") ;
#endif

    // Timer for the decoding
    startTime = clock() ;

    // Run the decoder over the whole available input
    decoder->init() ;
    nFrames = 0;
    float* array;
    while(frontend->GetArray(array, nFrames))
        decoder->processFrame(array, nFrames++);
    DecHyp* hyp = decoder->finish() ;
    endTime = clock() ;
    decodeTime = (real)(endTime-startTime) / CLOCKS_PER_SEC ;

    // Time stamp and speaker ID
    // time stamp must be after the first frame of decode 
    startTimeStamp = frontend->TimeStamp(0);
    mSpeakerID = frontend->GetSpeakerID(nFrames / 2);

    // post-process the decoding result
    if ( hyp == NULL )
    {
        nResultLevels = 0 ;
        nResultWords = 0 ;
        resultWords = NULL ;
    }
    else
    {
        extractResultsFromHyp( hyp , vocab ) ;
    }

    decoderInput = NULL ;
    vecSize = 0 ;
}


void DecoderSingleTest::removeSentMarksFromActual( DecVocabulary *vocab )
{
   if ( (nActualWords == 0) || (vocab == NULL) )
      return ;

   // remove the sentence start word
   if ( actualWords[0] == vocab->sentStartIndex )
   {
      for ( int j=1 ; j<nActualWords ; j++ )
      {
         actualWords[j-1] = actualWords[j] ;
         actualWordsTimes[j-1] = actualWordsTimes[j] ;
      }
      nActualWords-- ;
   }

   // remove the sentence end word
   if ( actualWords[nActualWords-1] == vocab->sentEndIndex )
      nActualWords-- ;

   // remove any instances of silence
   if ( vocab->silIndex >= 0 )
   {
      for ( int j=0 ; j<nActualWords ; j++ )
      {
         while ( (j<nActualWords) && (actualWords[j] == vocab->silIndex) )
         {
            for ( int k=(j+1) ; k<nActualWords ; k++ )
            {
               actualWords[k-1] = actualWords[k] ;
               actualWordsTimes[k-1] = actualWordsTimes[k] ;
            }
            nActualWords-- ;
         }
      }
   }
}


void DecoderSingleTest::extractResultsFromHyp( DecHyp *hyp , DecVocabulary *vocab )
{
   if ( (mode != DBT_MODE_WFSTDECODE_WORDS) && (mode != DBT_MODE_WFSTDECODE_PHONES) )
      error("DST::extractResultsFromHyp - mode not DBT_MODE_WFSTDECODE_*") ;

   if ( (hyp == NULL) || (DecHypHistPool::isActiveHyp( hyp ) == false) )
   {
      nResultLevels = 0 ;
      nResultWords = 0 ;
      resultWords = NULL ;
      totalLMScore = LOG_ZERO ;
      totalAcousticScore = LOG_ZERO ;
   }
   else
   {
#ifdef DEBUG
      if ( hyp->hist == NULL )
         error("DST::extractResultsFromHyp - hyp->hist == NULL") ;
#endif

      // Extract total LM and acoustic scores
      totalLMScore = hyp->lmScore ;
      totalAcousticScore = hyp->acousticScore ;

      if ( mode == DBT_MODE_WFSTDECODE_PHONES )
         extractResultsFromHypPhoneMode( hyp , vocab ) ;
      else if ( mode == DBT_MODE_WFSTDECODE_WORDS )
         extractResultsFromHypWordMode( hyp , vocab ) ;
   }
}


void DecoderSingleTest::extractResultsFromHypWordMode( DecHyp *hyp , DecVocabulary *vocab )
{
   // Count the number of words in the history for this hypothesis.
   DecHypHist *hist = hyp->hist ;
   nResultWords = 0 ;
   while ( hist != NULL )
   {
      if ( hist->type != DHHTYPE )
         error("DST::extractResultsFromHypWordMode - hist type not DHHTYPE") ;
      if ( (removeSentMarks == false) || 
           (((hist->state-1)!=vocab->sentStartIndex) && ((hist->state-1) != vocab->sentEndIndex)) )
      {
         nResultWords++ ;
      }
      hist = hist->prev ;
   }

   // Allocate memory and populate the result arrays.
   if ( nResultWords > 0 )
   {
      nResultLevels = 1 ;
      resultWords = new DSTResultWord*[nResultLevels] ;
      resultWords[0] = new DSTResultWord[nResultLevels*nResultWords] ;
      for ( int i=1 ; i<nResultLevels ; i++ )
      {
         resultWords[i] = resultWords[0] + i*nResultWords ;
      }
      
      hist = hyp->hist ;
      int w = nResultWords - 1 ;
      while ( hist != NULL )
      {
         // TODO - make sure first and last word have correct values 
         // after <s> and </s> removed.
         if ( (removeSentMarks == false) || 
              (((hist->state-1)!=vocab->sentStartIndex) && ((hist->state-1)!=vocab->sentEndIndex)))
         {
            if ( w < 0 )
               error("DST::extractResultsFromHypWordMode - w < 0") ;

            resultWords[0][w].index = (hist->state - 1) ;   // correct for 0th being epsilon
            resultWords[0][w].acousticScore = hist->acousticScore ;
            resultWords[0][w].lmScore = hist->lmScore ;
            resultWords[0][w].endTime = hist->time ;

            if ( w < (nResultWords - 1) ) 
            {
               resultWords[0][w+1].startTime = resultWords[0][w].endTime ;
               resultWords[0][w+1].acousticScore -= resultWords[0][w].acousticScore ;
               resultWords[0][w+1].lmScore -= resultWords[0][w].lmScore ;
            }

            w-- ;
         }
         hist = hist->prev ;
      }

      resultWords[0][0].startTime = 0 ;
   }
   else
   {
      nResultLevels = 0 ;
      nResultWords = 0 ;
      resultWords = NULL ;
   }
}


void DecoderSingleTest::extractResultsFromHypPhoneMode( DecHyp *hyp , DecVocabulary *vocab )
{
   // Count the number of phones in the history for this hypothesis.
   DecHypHist *hist = hyp->hist ;
   nResultWords = 0 ;   // used to count number of result phones
   int nWords = 0 ;     // number of result words
   while ( hist != NULL )
   {
      if ( hist->type == DHHTYPE )
      {
         // Phone history
         nResultWords++ ;
         hist = hist->prev ;
      }
      else if ( hist->type == LABDHHTYPE )
      {
         // Word history
         nWords++ ;
         hist = ((LabDecHypHist *)hist)->prev ;
      }
      else
      {
         error("DST::extractResultsFromHypPhoneMode - hist type not DHHTYPE or LABDHHTYPE") ;
      }
   }

   if ( nResultWords < nWords )
      error("DST::extractResultsFromHypPhoneMode - number of words exceeded num phones in result");

   // Allocate memory and populate the result arrays.
   if ( nResultWords > 0 )
   {
      if ( nWords == 0 )
         error("DST::extractResultsFromHypPhoneMode - no words found in result") ;
         
      nResultLevels = 2 ;
      resultWords = new DSTResultWord*[nResultLevels] ;
      resultWords[0] = new DSTResultWord[nResultLevels*nResultWords] ;
      int i ;
      for ( i=1 ; i<nResultLevels ; i++ )
      {
         resultWords[i] = resultWords[0] + i*nResultWords ;
      }

      // reset result arrays
      for ( i=0 ; i<(nResultLevels*nResultWords) ; i++ )
      {
         resultWords[0][i].index = -1 ;
         resultWords[0][i].startTime = -1 ;
         resultWords[0][i].endTime = -1 ;
         resultWords[0][i].acousticScore = LOG_ZERO ;
         resultWords[0][i].lmScore = LOG_ZERO ;
      }
      
      hist = hyp->hist ;
      int p = nResultWords - 1 ;
      int w = nResultWords - 1 ;

      while ( hist != NULL )
      {
         if ( hist->type == DHHTYPE )
         {
            if ( p < 0 )
               error("DST::extractResultsFromHypPhoneMode - p < 0") ;
            resultWords[0][p].index = (hist->state - 1) ;   // correct for 0th being epsilon
            resultWords[0][p].acousticScore = hist->acousticScore ;
            resultWords[0][p].lmScore = hist->lmScore ;
            resultWords[0][p].endTime = hist->time ;

            if ( p < (nResultWords - 1) ) 
            {
               resultWords[0][p+1].startTime = resultWords[0][p].endTime ;
               resultWords[0][p+1].acousticScore -= resultWords[0][p].acousticScore ;
               resultWords[0][p+1].lmScore -= resultWords[0][p].lmScore ;
            }

            p-- ;
            hist = hist->prev ;
         }
         else if ( hist->type == LABDHHTYPE )
         {
            if ( w < 0 )
               error("DST::extractResultsFromHypPhoneMode - w < 0") ;
            resultWords[1][w].index = ((LabDecHypHist *)hist)->label - 1 ;
            w-- ;
            hist = ((LabDecHypHist *)hist)->prev ;
         }
         else
         {
            error("DST::extractResultsFromHypPhoneMode - hist type not DHHTYPE or LABDHHTYPE (2)") ;
         }
      }

      resultWords[0][0].startTime = 0 ;
   }
   else
   {
      nResultLevels = 0 ;
      nResultWords = 0 ;
      resultWords = NULL ;
   }
}


}
