/*
 * Copyright 2004 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#ifndef MONOPHONE_LOOKUP_INC
#define MONOPHONE_LOOKUP_INC

#include "general.h"

namespace Juicer {


/*
   Author:		Darren Moore (moore@idiap.ch)
	Date:			11 Nov 2004
	$Id: MonophoneLookup.h,v 1.12 2005/08/26 01:16:34 moore Exp $
*/


struct MonophoneLookupNode
{
   char  chr ;
   int   monophone ;
   bool  isTemp ;
   int   nextSib ;
   int   firstChild ;
};

/**
 * Lookup monophones
 */
class MonophoneLookup
{
public:
   MonophoneLookup() ;
   MonophoneLookup( const char *monophoneListFName , const char *silMonoStr , 
                    const char *pauseMonoStr ) ;
   virtual ~MonophoneLookup() ;

   void outputText() ;
   int getIndex( const char *monophoneStr ) ;
   int getIndexWithAdd( const char *monophoneStr ) ;
   const char *getString( int monophoneInd ) ;
   int getNumMonophones() { return nMonophones ; } ;
   int getSilMonophone() { return silMonoInd ; } ;
   int getPauseMonophone() { return pauseMonoInd ; } ;

   void writeBinary( FILE *fd ) ;
   void writeBinary( const char *binFName ) ;
   void readBinary( FILE *fd ) ;
   void readBinary( const char *binFName ) ;

private:
   int                  *firstLevelNodes ;
   int                  nNodes ;
   int                  nNodesAlloc ;
   MonophoneLookupNode  *nodes ;
   
   int                  nMonophones ;
   int                  nMonophoneStrsAlloc ;
   char                 **monophoneStrs ;
   int                  silMonoInd ;
   int                  pauseMonoInd ;

   bool                 fromBinFile ;

   void addMonophone( const char *monophoneStr ) ;
   int getNode( int parentNode , char chr , bool addNew=false ) ;
   int allocNode( char chr ) ;
   void initNode( int ind ) ;
   void outputNode( int node , int *nChrs , char *chrs ) ;
};



struct PhoneLookupNode
{
   char  sepChar ;
   int   monoInd ;
   int   modelLookupInd ;
   int   nextSib ;
   int   firstChild ;
};

/**
 * Phone lookup
 */
class PhoneLookup
{
public:
   PhoneLookup( MonophoneLookup *monoLookup_ , const char *tiedListFName ,
                const char *sepChars_ ) ;
   PhoneLookup( const char *monophoneListFName , const char *silMonoStr , 
                const char *pauseMonoStr , const char *tiedListFName , 
                const char *sepChars_ ) ;
   virtual ~PhoneLookup() ;

   void outputText() ;
   int addModelInd( const char *phoneStr , int modelInd ) ;
   int getModelInd( const char *phoneStr ) ;
   int getNumModels() { return nModelInds ; } ;
   int getNumPhones() { return nPhones ; } ;
   int getMaxCD() ;
   const char *getModelStr( int index ) ;
   void verifyAllModels() ;
   void getAllModelInfo( int **phoneMonoInds , int *phoneModelInds ) ;
   bool haveCISilence() ;
   int getCISilenceModelInd() ;
   bool haveCIPause() ;
   int getCIPauseModelInd() ;

private:
   bool              ownMonoLookup ;
   MonophoneLookup   *monoLookup ;
   int               nSepChars ;
   char              *sepChars ;

   int               nPhones ;
   int               nModelInds ;
   int               nModelIndsAlloc ;
   int               *modelInds ;   // nodes refer to entry in this array.
                                    // entries in this array refer to index of actual model.
   char              **modelStrs ;
                                    
   int               *firstLevelNodes ;
   int               nNodes ;
   int               nNodesAlloc ;
   PhoneLookupNode   *nodes ;

   bool              monophoneMode ; // if true, model/phone names are treated as whole entities.

   // These member variables are only used if monophoneMode is true
   MonophoneLookup   *extraMonoLookup ;
   int               nExtraMLIndsAlloc ;
   int               *extraMLInds ;
   
   int addPhone( char *phoneStr , int modelLookupInd=-1 ) ;
   int getNode( int parentNode , char sepChar , int monoInd , bool addNew=false ) ;
   int allocNode( char sepChar , int monoInd ) ;
   void initNode( int ind ) ;
   void outputNode( int node , int *n , char *tmpSepChars , int *tmpMonoInds ) ;
   void getAllModelInfoNode( int node , int *n , char *tmpSepChars , int *tmpMonoInds ,
                             int *phnCnt , int **phoneMonoInds , int *phoneModelInds ) ;
};


}

#endif
