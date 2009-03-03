/*
 * Copyright 2002 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#ifndef DECPHONEINFO_INC
#define DECPHONEINFO_INC

#include "general.h"
#include "MonophoneLookup.h"

namespace Juicer {


/*
	This class contains the names of the phonemes that make up
	the words in the lexicon.

	Author:	Darren Moore (moore@idiap.ch)
	Date:		2002
	$Id: DecPhoneInfo.h,v 1.9 2005/08/26 01:16:34 moore Exp $
*/


typedef enum	// these should be in ascending order
{
	CD_TYPE_NONE=0 ,
	CD_TYPE_MONOPHONE ,			// Don't change the top two items !!
	CD_TYPE_BIPHONE_LEFT ,
	CD_TYPE_BIPHONE_RIGHT ,
	CD_TYPE_TRIPHONE ,
	CD_TYPE_INVALID				// always last !!
} CDType ;


typedef enum
{
	CDPL_STATE_UNINITIALISED=0 ,
	CDPL_STATE_ADD_MONOPHONES ,
	CDPL_STATE_ADD_CD_ENTRIES ,
	CDPL_STATE_INITIALISED
} CDPhoneLookupState ;


// This sole purpose of this class is to allow a user to lookup a context dependent
//   phone index by inputting the base monophone index, along with its left and right
//   context monophone indices.
// Implemented specifically for triphones at the moment.
class CDPhoneLookup
{
public:
	// Constructors / destructor
	CDPhoneLookup( MonophoneLookup *monoLookup_ ) ;
	CDPhoneLookup( MonophoneLookup *monoLookup_ , CDType cdType_ , const char *cdSepChars_ ) ;
	virtual ~CDPhoneLookup() ;
	
	// Public methods
	int getMonophoneIndex( char *monophoneName ) ;
	int getCDPhoneIndex( int base , int nLeft , int *left ,	int nRight , int *right ) ;
	const char *getMonophoneStr( int index ) ;
	int getNumMonophones() { return nMonophones ; } ;
	bool splitIntoMonophones( char *name , int *cnt , int *monoInds ) ;
	void splitIntoMonophones( char *name , int *base , int *nLeft , int *left , 
									  int *nRight , int *right ) ;
	void addCDPhone( char *name , int index ) ;
	void extractAndCheckMonophones( char *name ) ;
	void outputText() ;


	void writeBinary( FILE *fd ) ;
	void readBinary( FILE *fd ) ;

private:
	// Private member variables
	CDPhoneLookupState	state ;
	CDType					cdType ;
	char						*cdSepChars ;	// buffer of characters used to separate monophone names
													//   in the name of a CD phone (e.g. {'-','+'} for triphones)
													// there are 'maxCD'-1 of these separating chars
   MonophoneLookup      *monoLookup ;
	int						nMonophones ;

	int						*monoIndices ;
	int						**leftBiIndices ;
	int						**rightBiIndices ;
	int						***triIndices ;

	bool						fromBinFile ;
	
	// Private methods
	void addEntry( int cdPhoneIndex , int base , int nLeft , int *left , int nRight , int *right ) ;
};


class DecPhoneInfo
{
public:
	int		nPhones ;
	char		**phoneNames ;
	int		silIndex ;
	int		silMonophoneIndex ;
	int		pauseIndex ;
	bool		fromBinFile ;
	CDType	cdType ;

	// Creates an empty DecPhoneInfo instance
	DecPhoneInfo() ;

	// Reads the phone information from 'phones_fname' file. 
	// Looks at the first line of the file. 
	// If it is "PHONE" then the file is assumed to be in Noway phone models format. 
	// If it is "~o" then the file is assumed to be in HTK model definition format.
	// Otherwise the file is assumed to contain just a straight list of phone names.
	// If the sil_name and pause_name params are set, then the indices of the
	//   silence and pause phonemes in the list are set.
	DecPhoneInfo( const char *monoListFName , char *phonesFName , char *silName , 
                 char *pauseName , CDType cdType_ , const char *cdSepChars_ ) ;
	virtual ~DecPhoneInfo() ;

	// Returns a pointer to the phoneme name at position "index" in the list.
	char *getPhone( int index ) ;

	// Does a linear search through the list and returns the index of the
	//   phoneme name supplied as a parameter.
	// If the phoneme was not in the list returns -1.
	int getIndex( const char *phone_name ) ;
	int getMonophoneIndex( const char *monophoneName ) ;
	const char *getMonophone( int index ) ;
	int getCDPhoneIndex( int base , int nLeft , int *left ,	int nRight , int *right ) ;
	int getTriModelIndex( int base , int left , int right ) ;

	void writeBinary( FILE *fd ) ;
	void readBinary( FILE *fd ) ;
	void outputText() ;
	void testCDPhoneLookup() ;
	int getNumMonophones() 
		{ 
			if ( cdPhoneLookup != NULL ) 
				return cdPhoneLookup->getNumMonophones() ;
			else
				return monoLookup->getNumMonophones() ;
		};

private:
	// Private member variables
   MonophoneLookup   *monoLookup ;
   PhoneLookup       *modelLookup ;
	CDPhoneLookup		*cdPhoneLookup ;

   // Private methods
	void readPhonesFromASCII( FILE *phonesFD , char *silName=NULL, char *pauseName=NULL ) ;
	void readPhonesFromNoway( FILE *phones_fd, char *sil_name=NULL, char *pause_name=NULL ) ;
	void readPhonesFromHTK( FILE *phones_fd, char *sil_name=NULL, char *pause_name=NULL ) ;
};


}

#endif
