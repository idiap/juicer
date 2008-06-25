/*
 * Copyright 2004 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#ifndef DECODERSINGLETEST_INC
#define DECODERSINGLETEST_INC

#include "general.h"
#include "FrontEnd.h"
#include "DecVocabulary.h"
#include "WFSTDecoder.h"


/*
	Author:		Darren Moore (moore@idiap.ch)
	                Modified by John Dines to support LNA16bit (dines@idiap.ch)
	Date:			2004
	$Id: DecoderSingleTest.h,v 1.13 2006/06/20 17:05:15 juicer Exp $
*/

namespace Juicer {


typedef enum 
{
  DST_FEATS_HTK=0 ,
  DST_FEATS_ONLINE_FTRS ,
  DST_PROBS_LNA8BIT ,
  DST_PROBS_LNA16BIT , // modification John Dines
  DST_FEATS_FACTORY ,
  DST_NOFORMAT
} DSTDataFileFormat ;


typedef enum
{
  DBT_MODE_WFSTDECODE_WORDS=0 ,
  DBT_MODE_WFSTDECODE_PHONES ,
  DBT_MODE_INVALID
} DBTMode ;



struct DSTResultWord
{
   int   index ;
   int   startTime ;
   int   endTime ;
   real  acousticScore ;
   real  lmScore ;
};


/** 
 * This class is used to recognise a single input data file and
 * post-process the recognition result.  The DecoderBatchTest class
 * contains of an array of these objects.
 *
 * @author Darren Moore (moore@idiap.ch)
 */
class DecoderSingleTest
{
public:
  // Public member variables
  DBTMode			mode ;
  int				*expectedWords ;
  int				nExpectedWords ;
  int				*actualWords ;
  int				*actualWordsTimes ;
  int				nActualWords ;
  DSTDataFileFormat		inputFormat ;
  bool				removeSentMarks ;

  real				**decoderInput ;

  int                           nResultLevels ;
  int                           nResultWords ;
  DSTResultWord                 **resultWords ;

  // Constructors / destructor
  DecoderSingleTest() ;
  ~DecoderSingleTest() ;

  // Public array access
  inline int getActualWord(int i) { return actualWords[i]; }
  inline int getActualWordTime(int i) { return actualWordsTimes[i]; }
  inline DSTResultWord * getResultWord(int i, int j) { return &(resultWords[i][j]); }

  // Public methods
  void configure( DBTMode mode_ , int expVecSize_ , char *testFName_ , int nExpectedWords_ , 
		  int *expectedWords_ , DSTDataFileFormat inputFormat_ , 
		  bool removeSentMarks_ ) ;

  void run( WFSTDecoder *decoder , FrontEnd *frontend , DecVocabulary *vocab );

    // Like run(), but split in two
    void openSource(FrontEnd *frontend);
    void decodeUtterance(
        WFSTDecoder *decoder , FrontEnd *frontend , DecVocabulary *vocab
    );

  const char *getTestFName() { return testFName ; } ;
  int getNumFrames() { return nFrames ; } ;
  real getDecodeTime() { return decodeTime ; } ;
  real getTotalLMScore() { return totalLMScore ; } ;
  real getTotalAcousticScore() { return totalAcousticScore ; } ;
  
  static void setFramesPerSec( int framesPerSec_ ) { framesPerSec = framesPerSec_ ; } ;
   
private:
  // Private member variables
  static int           framesPerSec ;
  bool                 inputsAreFeatures ;
  int			expVecSize ;
  int			vecSize ;
  
  char			*testFName ;
  char                 *dataFName ;
  int                  extStartFrame ;
  int                  extEndFrame ;
  
  int			nFrames ;
  real			decodeTime ;
  real                 totalLMScore ;
  real                 totalAcousticScore ;

  // Private methods
  void removeSentMarksFromActual( DecVocabulary *vocab ) ;
    //void loadDataFile() ;
    //void loadHTK( const char *htkFName ) ;
    //void loadLNA8bit( char *lnaFName ) ;
    //void loadLNA16bit( char *lnaFName ) ;
    //void loadOnlineFtrs( char *onlineFtrsFName ) ;
  void extractResultsFromHyp( DecHyp *hyp , DecVocabulary *vocab ) ;
  void extractResultsFromHypWordMode( DecHyp *hyp , DecVocabulary *vocab ) ;
  void extractResultsFromHypPhoneMode( DecHyp *hyp , DecVocabulary *vocab ) ;
};


}

#endif
