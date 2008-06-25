/*
 * Copyright 2004 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#include "DecoderSingleTest.h"
#include "DiskFile.h"
#include "time.h"
#include "sys/types.h"
#include "sys/stat.h"


/*
   Author:		Darren Moore (moore@idiap.ch)
                        Modified by John Dines to support LNA16bit
   Date:		2004
   $Id: DecoderSingleTest.cpp,v 1.23 2006/06/08 15:25:07 juicer Exp $
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

    // The data file hasn't been loaded yet - load it
    //loadDataFile() ;
    assert(dataFName);
    frontend->SetSource(dataFName);

    // See if the vecSize in the input file agreed with our expectations.
    //if ( vecSize != expVecSize )
    //    error("DecoderSingleTest::run - vecSize != expVecSize") ;

    // Timer for the decoding
    startTime = clock() ;

    // Run the decoder over the whole available input
    decoder->init() ;
    //for ( int t=0 ; t<nFrames ; t++ )
    //{
    //    decoder->processFrame( decoderInput[t], t ) ;
    //}
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

#if 0
    // Free up some memory
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
#endif
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

// Remove all data loading crap
#if 0

void DecoderSingleTest::loadDataFile()
{
   // Make sure that the test_filename and data_format member variables
   //   have been configured.
   if ( dataFName == NULL )
      error("DecoderSingleTest::loadDataFile - dataFName is NULL") ;
   if ( (inputFormat < 0) || (inputFormat >= DST_NOFORMAT) )
      error("DecoderSingleTest::loadDataFile - invalid inputFormat") ;

   // Free any existing data and reset the size-related member variables
   if ( decoderInput != NULL )
      error("DecoderSingleTest::loadDataFile - decoderInput is not NULL") ;

   nFrames=0 ;
   vecSize = 0 ;

   switch ( inputFormat )
   {
   case DST_FEATS_HTK:
   {
       loadHTK( dataFName ) ;
       inputsAreFeatures = true ;
       break ;
   }
   case DST_FEATS_ONLINE_FTRS:
   {
       loadOnlineFtrs( dataFName ) ;
       inputsAreFeatures = true ;
       break ;
   }
      case DST_PROBS_LNA8BIT:
      {
         loadLNA8bit( dataFName ) ;
         inputsAreFeatures = false ;
         break ;
      }
      case DST_PROBS_LNA16BIT:
      {
	loadLNA16bit( dataFName ) ;
	inputsAreFeatures = false ;
	break;
      }
      default:
         error("DecoderSingleTest::loadDataFile - data_format not recognised") ;
   }

   if ( (nFrames <= 0) || (vecSize <= 0) )
      error("DST::loadDataFile - no data loaded") ;
}


void DecoderSingleTest::loadHTK( const char *htkFName )
{
   DiskFile *file ;

#ifdef DEBUG
   if ( (htkFName == NULL) || (htkFName[0] == '\0') )
      error("DecoderSingleTest::loadHTK - invalid filename") ;
#endif

   // Open the input file in big endian mode
   file = new DiskFile( htkFName , "rb" , true ) ;

   // Read the 12-byte header
   // 1. Number of samples in file (4-byte integer)
   int totalNFrames ;
   if ( file->read( &totalNFrames , sizeof(int) , 1 ) != 1 )
      error("DecoderSingleTest::loadHTK - error reading num samples") ;
   if ( totalNFrames <= 0 )
      error("DecoderSingleTest::loadHTK - nFrames <= 0") ;
   if ( (extEndFrame > 0) && (extEndFrame >= totalNFrames) )
      error("DecoderSingleTest::loadHTK - extEndFrame >= nFrames") ;

   if ( extStartFrame >= 0 )
   {
      // We only want to read a chunk of this file
      nFrames = extEndFrame - extStartFrame + 1 ;
   }
   else
   {
      nFrames = totalNFrames ;
   }

   // 2. Sample period (4-byte integer - read and discard)
   int dummy ;
   if ( file->read( &dummy , 4 , 1 ) != 1 )
      error("DecoderSingleTest::loadHTK - error reading sample period") ;

   // 3. Number of bytes per sample (2-byte integer)
   short bytesPerSamp ;
   if ( file->read( &bytesPerSamp , 2 , 1 ) != 1 )
      error("DecoderSingleTest::loadHTK - error reading bytes per sample") ;
   if ( bytesPerSamp <= 0 )
      error("DecoderSingleTest::loadHTK - bytesPerSamp <= 0") ;

   // 4. Read parameter type
   short parmKind ;
   if ( file->read( &parmKind , 2 , 1 ) != 1 )
      error("DecoderSingleTest::loadHTK - error reading parm kind") ;

   // Calculate number of features in each vector
   vecSize = bytesPerSamp / 4 ;

   // Allocate array of pointers to feature vectors
   decoderInput = new real*[nFrames] ;
   decoderInput[0] = new real[vecSize * nFrames] ;
   int i ;
   for ( i=1 ; i<nFrames ; i++ )
   {
      decoderInput[i] = decoderInput[0] + ( i * vecSize ) ;
   }

   if ( extStartFrame > 0 )
   {
      // We need to fseek to the place in the file where the start frame is.
      file->seek( extStartFrame * bytesPerSamp , SEEK_CUR ) ;
   }

#ifndef USE_DOUBLE
   if ( file->read( decoderInput[0] , sizeof(float) , vecSize*nFrames ) != (vecSize*nFrames) )
      error("DecoderSingleTest::loadHTK - error reading feature vectors") ;
#else
   float *tmpBuf = new float[vecSize*nFrames] ;
   if ( file->read( tmpBuf , sizeof(float) , vecSize*nFrames ) != (vecSize*nFrames) )
      error("DecoderSingleTest::loadHTK - error reading feature vectors") ;
   for ( i=0 ; i<(vecSize*nFrames) ; i++ )
      decoderInput[0][i] = (real)( tmpBuf[i] ) ;
   delete [] tmpBuf ;
#endif

   // Close the input file
   delete file ;
}


void DecoderSingleTest::loadLNA8bit( char *lnaFName )
{
   FILE *lnaFD ;
   int bufSize , stepSize , i ;
   unsigned char *buf ;
   real sum=0.0 ;

#ifdef DEBUG
   if ( sizeof(unsigned char) != 1 )
      error("DecoderSingleTest::loadLNA8bit - unsigned char not 1 byte") ;
   if ( (lnaFName == NULL) || (strcmp(lnaFName,"")==0) )
      error("DecoderSingleTest::loadLNA8bit - lnaFName undefined") ;
   if ( expVecSize <= 0 )
      error("DecoderSingleTest::loadLNA8bit - expVecSize <= 0") ;
#endif

   if ( extStartFrame >= 0 )
      warning("DST::loadLNA8bit - extended filenames not supported for LNA input") ;

   buf = new unsigned char[10000] ;

   nFrames = 0 ;
   vecSize = expVecSize ;
   decoderInput = NULL ;
   stepSize = (1 + vecSize) * sizeof(unsigned char) ;

   if ( (lnaFD = fopen( lnaFName , "rb" )) == NULL )
      error("DecoderSingleTest::loadLNA8bit - error opening LNA file") ;
   do
   {
      if ( (bufSize=(int)fread( buf , 1 , stepSize , lnaFD )) != stepSize )
         error("DecoderSingleTest::loadLNA8bit - error reading prob vector") ;

      if ( (buf[0] != 0x00) && (buf[0] != 0x80) )
         error("DecoderSingleTest::loadLNA8bit - flag byte error %s %d",lnaFName,nFrames) ;

      nFrames++ ;
      decoderInput = (real **)realloc( decoderInput , nFrames*sizeof(real *) ) ;
      decoderInput[nFrames-1] = new real[vecSize] ;

      // Convert from the 8-bit integer in the file to the equivalent floating point
      //   log probability.
      sum = 0.0 ;
      for ( i=0 ; i<vecSize ; i++ )
      {
         decoderInput[nFrames-1][i] = -((real)buf[i+1] + (real)0.5) / (real)24.0 ;
         sum += (real)exp( decoderInput[nFrames-1][i] ) ;
      }

      if ( (sum < 0.97) || (sum > 1.03) )
      {
         printf("LNA Error on %s\n",lnaFName) ;
         printf("%d:",nFrames-1) ;
         for ( i=0 ; i<vecSize ; i++ )
            printf(" %.3f" , (real)exp( decoderInput[nFrames-1][i] ) ) ;
         printf("\n") ;
         error("DecoderSingleTest::loadLNA8bit - sum of probs=%.4f not in [0.97,1.03]",sum) ;
      }
   }
   while ( buf[0] != 0x80 ) ;

   // We're done.
   fclose( lnaFD ) ;

   delete [] buf ;
}

void DecoderSingleTest::loadLNA16bit( char *lnaFName )
{
   FILE *lnaFD ;
   int bufSize , stepSize , i ;
   unsigned short *buf ;
   real sum=0.0 ;

#ifdef DEBUG
   if ( sizeof(unsigned short) != 1 )
      error("DecoderSingleTest::loadLNA16bit - unsigned short not 1 byte") ;
   if ( (lnaFName == NULL) || (strcmp(lnaFName,"")==0) )
      error("DecoderSingleTest::loadLNA16bit - lnaFName undefined") ;
   if ( expVecSize <= 0 )
      error("DecoderSingleTest::loadLNA16bit - expVecSize <= 0") ;
#endif

   if ( extStartFrame >= 0 )
      warning("DST::loadLNA16bit - extended filenames not supported for LNA input") ;

   buf = new unsigned short[10000] ;

   nFrames = 0 ;
   vecSize = expVecSize ;
   decoderInput = NULL ;
   stepSize = (1 + vecSize)* sizeof(unsigned short) ;

   if ( (lnaFD = fopen( lnaFName , "rb" )) == NULL )
      error("DecoderSingleTest::loadLNA16bit - error opening LNA file") ;
   do
   {
      if ( (bufSize=(int)fread( buf , 1, stepSize , lnaFD )) != stepSize )
         error("DecoderSingleTest::loadLNA16bit - error reading prob vector") ;

      if ( (buf[0] != 0x00) && (buf[0] != 0x80) )
         error("DecoderSingleTest::loadLNA16bit - flag byte error %s %d",lnaFName,nFrames) ;

      nFrames++ ;
      decoderInput = (real **)realloc( decoderInput , nFrames*sizeof(real *) ) ;
      decoderInput[nFrames-1] = new real[vecSize] ;

      // Convert from the 16-bit integer in the file to the equivalent floating point
      //   log probability.
      sum = 0.0 ;
      for ( i=0 ; i<vecSize ; i++ )
      {
         decoderInput[nFrames-1][i] = -((real)buf[i+1] + (real)0.5) / (real)5120.0 ;
         sum += (real)exp( decoderInput[nFrames-1][i] ) ;
      }

      if ( (sum < 0.97) || (sum > 1.03) )
      {
         printf("LNA Error on %s\n",lnaFName) ;
         printf("%d:",nFrames-1) ;
         for ( i=0 ; i<vecSize ; i++ )
            printf(" %.3f" , (real)exp( decoderInput[nFrames-1][i] ) ) ;
         printf("\n") ;
         error("DecoderSingleTest::loadLNA16bit - sum of probs=%.4f not in [0.97,1.03]",sum) ;
      }
   }
   while ( buf[0] != 0x80 ) ;

   // We're done.
   fclose( lnaFD ) ;

   delete [] buf ;
}


void DecoderSingleTest::loadOnlineFtrs( char *onlineFtrsFName )
{
   // Only read the first sentence in the file.
   DiskFile *file ;
   unsigned char *buf , flag ;

#ifdef DEBUG
   if ( decoderInput != NULL )
      error("DecoderSingleTest::loadOnlineFtrs - already have decoder input data") ;
   if ( (sizeof(unsigned char) != 1) || (sizeof(float) != 4) )
      error("DecoderSingleTest::loadOnlineFtrs - types have unexpected sizes") ;
#endif

   if ( extStartFrame >= 0 )
      warning("DST::loadOnlineFtrs - extended filenames not supported for online_ftrs input") ;

   buf = new unsigned char[10000] ;

   nFrames = 0 ;
   decoderInput = NULL ;
   vecSize = expVecSize ;

   // Open the file in big endian mode
   file = new DiskFile( onlineFtrsFName , "rb" , true ) ;

   do
   {
      // read the flag byte
      file->read( &flag , sizeof(unsigned char) , 1 ) ;

      // read the features
      if ( file->read( buf , sizeof(float) , vecSize ) != vecSize )
         error("DecoderSingleTest::loadOnlineFtrs - error reading feature vector\n") ;

      if ( (flag != 0x00) && (flag != 0x80) )
         error("DecoderSingleTest::loadOnlineFtrs - flag byte error\n") ;

      nFrames++ ;
      decoderInput = (real **)realloc( decoderInput, nFrames*sizeof(real *) ) ;
      decoderInput[nFrames-1] = new real[vecSize] ;
#ifdef USE_DOUBLE
      for ( int i=0 ; i<vecSize ; i++ )
         decoderInput[nFrames-1][i] = (real)((float *)buf)[i] ;
#else
      memcpy( decoderInput[nFrames-1] , buf , vecSize*sizeof(float) ) ;
#endif
   }
   while ( flag != 0x80 ) ;

   // We're done.
   delete file ;
   delete [] buf ;
}

#endif
// Above is data loading crap

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
