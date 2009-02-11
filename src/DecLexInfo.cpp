/*
 * Copyright 2002 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#include "DecLexInfo.h"

/*
	Author:	Darren Moore (moore@idiap.ch)
	Date:		2002
	$Id: DecLexInfo.cpp,v 1.20 2005/08/26 01:16:34 moore Exp $
*/

using namespace Torch;

namespace Juicer {


DecLexInfo::DecLexInfo()
{
	nEntries = 0 ;
	entries = NULL ;
	sentStartIndex = -1 ;
	sentEndIndex = -1 ;
	silIndex = -1 ;
	phoneInfo = NULL ;
	vocabulary = NULL ;
	vocabToLexMap = NULL ;
	fromBinFile = false ;
	cdType = CD_TYPE_NONE ;
	rescoreMode = false ;
   monoOnlyMode = false ;
   monoLookup = NULL ;
}


DecLexInfo::DecLexInfo(
    const char *monophoneListFName , char *phonesFName , char *silPhone , 
    char *pausePhone , CDType phonesCDType , char *lexFName , 
    char *sentStartWord , char *sentEndWord , char *silWord , 
    CDType cdType_ , bool rescoreMode_
)
{
	nEntries = 0 ;
	entries = NULL ;
	sentStartIndex = -1 ;
	sentEndIndex = -1 ;
	silIndex = -1 ;
	phoneInfo = NULL ;
	vocabulary = NULL ;
	vocabToLexMap = NULL ;
	fromBinFile = false ;
	cdType = cdType_ ;
    monoLookup = NULL ;
    monoOnlyMode = false ;

	// When rescoreMode is true, we do not fill in the 'phones' field
	// of each lexical entry.  Only the 'monophones' field is
	// configured, enabling (x-word) CD phone model indices to be
	// retrieved on demand.
	rescoreMode = rescoreMode_ ;

	readFromASCII(
        monophoneListFName , phonesFName , silPhone , pausePhone ,
        phonesCDType , lexFName , sentStartWord , sentEndWord , silWord
    ) ;
}


DecLexInfo::DecLexInfo(
    const char *monophoneListFName , const char *silPhone , 
    const char *pausePhone , const char *lexFName , 
    const char *sentStartWord , const char *sentEndWord , 
    const char *silWord
)
{
	nEntries = 0 ;
	entries = NULL ;
	sentStartIndex = -1 ;
	sentEndIndex = -1 ;
	silIndex = -1 ;
	phoneInfo = NULL ;
	vocabulary = NULL ;
	vocabToLexMap = NULL ;
	fromBinFile = false ;
	cdType = CD_TYPE_MONOPHONE ;
    rescoreMode = false ;
    monoOnlyMode = true ;

    monoLookup = new MonophoneLookup( monophoneListFName , silPhone , pausePhone ) ;

    // Hard-code the use of '!' at the start of a word to designate a
    // special word (for now).
    char specialChar='!' ;
    vocabulary = new DecVocabulary(
        lexFName , specialChar , sentStartWord , sentEndWord , silWord
    ) ;
                                   
	// Allocate memory for mappings between vocab entries and
	// dictionary entries.
	vocabToLexMap = new VocToLexMapEntry[vocabulary->nWords] ;
	for ( int i=0 ; i<vocabulary->nWords ; i++ )
	{
		vocabToLexMap[i].nPronuns = 0 ;
		vocabToLexMap[i].pronuns = NULL ;
	}

   FILE *lexFD ;
	if ( (lexFD = fopen( lexFName , "rb" )) == NULL )
		error("DecLexInfo::DecLexInfo(2) - error opening lex file") ;
	
	// Now read the file and add entries.
   int vocInd=-1 , nFlds , *inds = new int[1000] , temp ;
   char *line=new char[10000] , *currWord=new char[1000] , *currPhone , ch ;
   DecLexInfoEntry *currEntry ;
   float prior ;
	nEntries = 0 ;
	while ( fgets( line , 1000 , lexFD ) != NULL )
	{
		if ( (line[0] == '(') || (line[0] == '#') || (strtok( line , "\r\n\t " ) == NULL) )
			continue ;

#ifdef USE_DOUBLE
		if ( (nFlds = sscanf( line , "%[^( \t]%c%lf" , currWord , &ch , &prior)) == 0 )
			continue ;
#else
		if ( (nFlds = sscanf( line , "%[^( \t]%c%f" , currWord , &ch , &prior)) == 0 )
			continue ;
#endif

		if ( nFlds < 3 )
			prior = 1.0 ;

		// Find the vocab index of the new word
		vocInd = vocabulary->getIndex( currWord , vocInd ) ;
		if ( vocInd < 0 )
			error("DecLexInfo::DecLexInfo(2) - word %s not found in vocabulary",currWord) ;

		// Allocate memory for the new lexicon entry.
		entries = (DecLexInfoEntry *)realloc( entries , (nEntries+1) * sizeof(DecLexInfoEntry) ) ;
		currEntry = entries + nEntries ;
		initLexInfoEntry( currEntry ) ;

		currEntry->vocabIndex = vocInd ;
		currEntry->logPrior = (real)log( prior ) ;

		// read in the (mono)phones for the new pronunciation
		while ( (currPhone=strtok( NULL , "\r\n\t " )) != NULL )
		{
			inds[currEntry->nPhones] = monoLookup->getIndex( currPhone ) ;

			if ( inds[currEntry->nPhones] < 0 )
				error("DecLexInfo::DecLexInfo(2) - %s not found in phone list" , currPhone ) ;
			currEntry->nPhones++ ;
		}

		if ( currEntry->nPhones == 0 )
			error("DecLexInfo::DecLexInfo - %s had no phones",currWord) ;

		// Allocate memory to hold phone model and monophone indices
        currEntry->monophones = new int[currEntry->nPhones] ;
        currEntry->phones = NULL ;
	
		// Copy monophone indices into the entry
		memcpy( currEntry->monophones , inds , currEntry->nPhones*sizeof(int) ) ;

		// Update the appropriate vocabToLexMap entry
		temp = ++(vocabToLexMap[vocInd].nPronuns) ;
		vocabToLexMap[vocInd].pronuns = (int *)realloc( 
                                   vocabToLexMap[vocInd].pronuns , temp*sizeof(int) ) ;
		vocabToLexMap[vocInd].pronuns[temp-1] = nEntries ;

		// Check if these are 'special' words
		if ( vocInd == vocabulary->sentStartIndex )
		{
			if ( sentStartIndex >= 0 )
				error("DecLexInfo::DecLexInfo - cannot have >1 pronuns of the start word") ;
			sentStartIndex = nEntries ;
		}
		if ( vocInd == vocabulary->sentEndIndex )
		{
			if ( sentEndIndex >= 0 )
				error("DecLexInfo::DecLexInfo - cannot have >1 pronuns of the end word") ;
			sentEndIndex = nEntries ;
		}
		if ( vocInd == vocabulary->silIndex )
		{
			if ( silIndex >= 0 )
				error("DecLexInfo::DecLexInfo - cannot have >1 pronuns of the sil word") ;
			silIndex = nEntries ;
		}

		nEntries++ ;
	}

	fclose( lexFD ) ;

	if ( (sentEndIndex >= 0) && (sentStartIndex == sentEndIndex) )
	{
		// Create a separate, identical entry for the sentEndIndex
		//   so that there will be a separate model for the sentence end word.
		entries = (DecLexInfoEntry *)realloc( entries , (nEntries+1)*sizeof(DecLexInfoEntry) ) ;
		currEntry = entries + nEntries ;
		initLexInfoEntry( currEntry ) ;
		currEntry->vocabIndex = entries[sentStartIndex].vocabIndex ;
		currEntry->logPrior = entries[sentStartIndex].logPrior ;
		currEntry->nPhones = entries[sentStartIndex].nPhones ;
		currEntry->monophones = new int[currEntry->nPhones] ;
		memcpy( currEntry->monophones , entries[sentStartIndex].monophones ,
				  currEntry->nPhones*sizeof(int) ) ;

		// Update the appropriate vocabToLexMap entry
		temp = ++(vocabToLexMap[currEntry->vocabIndex].nPronuns) ;
		vocabToLexMap[currEntry->vocabIndex].pronuns = (int *)realloc( 
						vocabToLexMap[currEntry->vocabIndex].pronuns , temp*sizeof(int) ) ;
		vocabToLexMap[currEntry->vocabIndex].pronuns[temp-1] = nEntries ;

		sentEndIndex = nEntries++ ;
	}

	if ( (silIndex >= 0) && ((silIndex==sentStartIndex) || (silIndex==sentEndIndex)) )
	{
      int ind ;
		if ( silIndex == sentEndIndex )
			ind = sentEndIndex ;
		else
			ind = sentStartIndex ;

		// Create a separate, identical entry for the silIndex
		//   so that there will be a separate model for the silence word.
		entries = (DecLexInfoEntry *)realloc( entries , (nEntries+1)*sizeof(DecLexInfoEntry) ) ;
		currEntry = entries + nEntries ;
		initLexInfoEntry( currEntry ) ;
		currEntry->vocabIndex = entries[ind].vocabIndex ;
		currEntry->logPrior = entries[ind].logPrior ;
		currEntry->nPhones = entries[ind].nPhones ;
		currEntry->monophones = new int[currEntry->nPhones] ;
		memcpy( currEntry->monophones, entries[ind].monophones, currEntry->nPhones*sizeof(int) ) ;

		// Update the appropriate vocabToLexMap entry
		temp = ++(vocabToLexMap[currEntry->vocabIndex].nPronuns) ;
		vocabToLexMap[currEntry->vocabIndex].pronuns = (int *)realloc( 
						vocabToLexMap[currEntry->vocabIndex].pronuns , temp*sizeof(int) ) ;
		vocabToLexMap[currEntry->vocabIndex].pronuns[temp-1] = nEntries ;

		silIndex = nEntries++ ;
	}
   delete [] inds ;
   delete [] line ;
   delete [] currWord ;
}


void DecLexInfo::readFromASCII(
    const char *monophoneListFName , char *phonesFName , 
    char *silPhone , char *pausePhone , CDType phonesCDType , 
    char *lexFName , char *sentStartWord , 
    char *sentEndWord , char *silWord
)
{
	FILE *lexFD ;
	char line[1000] , ch , currWord[1000] , *currPhone ;
	real prior=1.0 ;
	int nFlds , vocabIndex=0 , temp , ind ;
	DecLexInfoEntry *currEntry ;

	// Create the DecPhoneInfo and DecVocabulary objects

	// Hard-code the use of '-' and '+' as context separators in
	// triphone model names (for now).
	char *cdSepChars=(char *)"-+" ;

   // Hard-code the use of '!' at the start of a word to designate a special word (for now).
   char specialChar='!' ;

	phoneInfo = new DecPhoneInfo( monophoneListFName , phonesFName , silPhone , pausePhone , 
                                 phonesCDType , cdSepChars ) ; 
	vocabulary = new DecVocabulary( lexFName , specialChar , sentStartWord , sentEndWord , 
                                   silWord ) ;

	// Allocate memory for mappings between vocab entries and dictionary entries.
	//vocabToLexMap = new VocToLexMapEntry[vocabulary->nWords+2] ;
	vocabToLexMap = new VocToLexMapEntry[vocabulary->nWords] ;
	for ( int i=0 ; i<vocabulary->nWords ; i++ )
	{
		vocabToLexMap[i].nPronuns = 0 ;
		vocabToLexMap[i].pronuns = NULL ;
	}

	if ( (lexFD = fopen( lexFName , "rb" )) == NULL )
		error("DecLexInfo::DecLexInfo - error opening lex file") ;
	
	// Now read the file and add entries.
	nEntries = 0 ;
	while ( fgets( line , 1000 , lexFD ) != NULL )
	{
		if ( (line[0] == '(') || (line[0] == '#') || (strtok( line , "\r\n\t " ) == NULL) )
			continue ;

#ifdef USE_DOUBLE
		if ( (nFlds = sscanf( line , "%[^( \t]%c%lf" , currWord , &ch , &prior)) == 0 )
			continue ;
#else
		if ( (nFlds = sscanf( line , "%[^( \t]%c%f" , currWord , &ch , &prior)) == 0 )
			continue ;
#endif

		if ( nFlds < 3 )
			prior = 1.0 ;

		// Find the vocab index of the new word
		vocabIndex = vocabulary->getIndex( currWord , vocabIndex ) ;
		if ( vocabIndex < 0 )
			error("DecLexInfo::DecLexInfo - word %s not found in vocabulary",currWord) ;

		// Allocate memory for the new lexicon entry.
		entries = (DecLexInfoEntry *)realloc( entries , (nEntries+1) * sizeof(DecLexInfoEntry) ) ;
		currEntry = entries + nEntries ;
		initLexInfoEntry( currEntry ) ;

		currEntry->vocabIndex = vocabIndex ;
		currEntry->logPrior = (real)log( prior ) ;

		// read in the (mono)phones for the new pronunciation
		int *inds = new int[1000] ;
		while ( (currPhone=strtok( NULL , "\r\n\t " )) != NULL )
		{
			inds[currEntry->nPhones] = phoneInfo->getMonophoneIndex( currPhone ) ;

			if ( inds[currEntry->nPhones] < 0 )
				error("DecLexInfo::DecLexInfo - %s not found in phone list" , currPhone ) ;
			currEntry->nPhones++ ;
		}

		if ( currEntry->nPhones == 0 )
			error("DecLexInfo::DecLexInfo - %s had no phones",currWord) ;

		// Allocate memory to hold phone model and monophone indices
		if ( rescoreMode == false )
		{
			currEntry->monophones = new int[currEntry->nPhones * 2] ;
			currEntry->phones = currEntry->monophones + currEntry->nPhones ;
		}
		else
		{
			currEntry->monophones = new int[currEntry->nPhones] ;
			currEntry->phones = NULL ;
		}
	
		// Copy monophone indices into the entry
		memcpy( currEntry->monophones , inds , currEntry->nPhones*sizeof(int) ) ;
		delete [] inds ;

		// Convert the list of monophones into context-dependent phones, and store the
		//   corresponding model indices in the 'phones' field of the entry.
		// Don't do this if rescoreMode is true.
		if ( rescoreMode == false )
			monoToCDPhones( currEntry ) ;
		
		// Update the appropriate vocabToLexMap entry
		temp = ++(vocabToLexMap[vocabIndex].nPronuns) ;
		vocabToLexMap[vocabIndex].pronuns = (int *)realloc( 
											vocabToLexMap[vocabIndex].pronuns , temp*sizeof(int) ) ;
		vocabToLexMap[vocabIndex].pronuns[temp-1] = nEntries ;

		// Check if these are 'special' words
		if ( vocabIndex == vocabulary->sentStartIndex )
		{
			if ( sentStartIndex >= 0 )
				error("DecLexInfo::DecLexInfo - cannot have >1 pronuns of the start word\n") ;
			sentStartIndex = nEntries ;
		}
		if ( vocabIndex == vocabulary->sentEndIndex )
		{
			if ( sentEndIndex >= 0 )
				error("DecLexInfo::DecLexInfo - cannot have >1 pronuns of the end word\n") ;
			sentEndIndex = nEntries ;
		}
		if ( vocabIndex == vocabulary->silIndex )
		{
			if ( silIndex >= 0 )
				error("DecLexInfo::DecLexInfo - cannot have >1 pronuns of the sil word\n") ;
			silIndex = nEntries ;
		}

		nEntries++ ;
	}

	fclose( lexFD ) ;

	if ( (sentEndIndex >= 0) && (sentStartIndex == sentEndIndex) )
	{
		// Create a separate, identical entry for the sentEndIndex
		//   so that there will be a separate model for the sentence end word.
		entries = (DecLexInfoEntry *)realloc( entries , (nEntries+1)*sizeof(DecLexInfoEntry) ) ;
		currEntry = entries + nEntries ;
		initLexInfoEntry( currEntry ) ;
		currEntry->vocabIndex = entries[sentStartIndex].vocabIndex ;
		currEntry->logPrior = entries[sentStartIndex].logPrior ;
		currEntry->nPhones = entries[sentStartIndex].nPhones ;
		if ( rescoreMode == false )
		{
			currEntry->phones = new int[currEntry->nPhones] ;
			memcpy( currEntry->phones , entries[sentStartIndex].phones ,
					  currEntry->nPhones*sizeof(int) ) ;
		}
		currEntry->monophones = new int[currEntry->nPhones] ;
		memcpy( currEntry->monophones , entries[sentStartIndex].monophones ,
				  currEntry->nPhones*sizeof(int) ) ;

		// Update the appropriate vocabToLexMap entry
		temp = ++(vocabToLexMap[currEntry->vocabIndex].nPronuns) ;
		vocabToLexMap[currEntry->vocabIndex].pronuns = (int *)realloc( 
						vocabToLexMap[currEntry->vocabIndex].pronuns , temp*sizeof(int) ) ;
		vocabToLexMap[currEntry->vocabIndex].pronuns[temp-1] = nEntries ;

		sentEndIndex = nEntries++ ;
	}

	if ( (silIndex >= 0) && ((silIndex==sentStartIndex) || (silIndex==sentEndIndex)) )
	{
		if ( silIndex == sentEndIndex )
			ind = sentEndIndex ;
		else
			ind = sentStartIndex ;

		// Create a separate, identical entry for the silIndex
		//   so that there will be a separate model for the silence word.
		entries = (DecLexInfoEntry *)realloc( entries , (nEntries+1)*sizeof(DecLexInfoEntry) ) ;
		currEntry = entries + nEntries ;
		initLexInfoEntry( currEntry ) ;
		currEntry->vocabIndex = entries[ind].vocabIndex ;
		currEntry->logPrior = entries[ind].logPrior ;
		currEntry->nPhones = entries[ind].nPhones ;
		if ( rescoreMode == false )
		{
			currEntry->phones = new int[currEntry->nPhones] ;
			memcpy( currEntry->phones , entries[ind].phones , currEntry->nPhones*sizeof(int) ) ;
		}
		
		currEntry->monophones = new int[currEntry->nPhones] ;
		memcpy( currEntry->monophones, entries[ind].monophones, currEntry->nPhones*sizeof(int) ) ;

		// Update the appropriate vocabToLexMap entry
		temp = ++(vocabToLexMap[currEntry->vocabIndex].nPronuns) ;
		vocabToLexMap[currEntry->vocabIndex].pronuns = (int *)realloc( 
						vocabToLexMap[currEntry->vocabIndex].pronuns , temp*sizeof(int) ) ;
		vocabToLexMap[currEntry->vocabIndex].pronuns[temp-1] = nEntries ;

		silIndex = nEntries++ ;
	}
}


void DecLexInfo::monoToCDPhones( DecLexInfoEntry *entry )
{
#ifdef DEBUG
	if ( (rescoreMode == true) || (monoOnlyMode == true) )
		error("DecLexInfo::monoToCDPhones - rescoreMode or monoOnlyMode is true") ;
	if ( (entry == NULL) || (entry->monophones == NULL) || (entry->phones == NULL) )
		error("DecLexInfo::monoToCDPhones - entry or entry fields NULL") ;
#endif

	int i ;

	// This will need to be updated if > triphones are added in future
	int base , nLeft , nRight , left , right ;
	for ( i=0 ; i<entry->nPhones ; i++ )
	{
		base = entry->monophones[i] ;

		nLeft = 0 ;
		if ( (i == 0) || (cdType == CD_TYPE_BIPHONE_RIGHT) || (cdType == CD_TYPE_MONOPHONE) )
		{
			left = -1 ;
			// This fragment will use sil-ph1+ph2 for the first CD phone
			//if ( i == 0 )
			//{
			//	nLeft = 1 ;
			//	left = phoneInfo->sil_monophone_index ;
			//}
		}
		else
		{
			nLeft = 1 ;
			left = entry->monophones[i-1] ;
		}

		nRight = 0 ;
		if ( (i == (entry->nPhones-1)) || 
			  (cdType == CD_TYPE_BIPHONE_LEFT) || (cdType == CD_TYPE_MONOPHONE)  )
		{
			right = -1 ;
			// This fragment will use ph4-ph5+sil for the last CD phone
			//if ( i == (entry->nPhones-1) )
			//{
			//	nRight = 1 ;
			//	right = phoneInfo->sil_monophone_index ;
			//}
		}
		else
		{
			nRight = 1 ;
			right = entry->monophones[i+1] ;
		}

		entry->phones[i] = phoneInfo->getCDPhoneIndex( base , nLeft , &left , nRight , &right ) ;
		if ( entry->phones[i] < 0 )
		{
			const char *l=NULL,*r=NULL,*b=NULL,*w=NULL ;
			w = vocabulary->getWord( entry->vocabIndex ) ;
			b = phoneInfo->getMonophone(base) ;
			if ( left >= 0 )
				l = phoneInfo->getMonophone(left) ;
			if ( right >= 0 )
				r = phoneInfo->getMonophone(right) ;

			if ( (left < 0) && (right < 0) )
				error( "DecLexInfo::monoToCDPhones - %s CD phone not found %s" , w , b ) ;
			else if ( left < 0 )
				error( "DecLexInfo::monoToCDPhones - %s CD phone not found %s+%s" , w , b , r ) ;
			else if ( right < 0 )
				error( "DecLexInfo::monoToCDPhones - %s CD phone not found %s-%s" , w , l , b ) ;
			else
				error( "DecLexInfo::monoToCDPhones - %s CD phone not found %s-%s+%s", w, l, b, r ) ;
		}
	}
}	


DecLexInfo::~DecLexInfo()
{
	if ( entries != NULL )
	{
		for ( int i=0 ; i<nEntries ; i++ )
			delete [] entries[i].monophones ;

		if ( fromBinFile == false )
			free( entries ) ;
		else
			delete [] entries ;
	}

	if ( vocabToLexMap != NULL )
	{
		if ( fromBinFile == false )
		{
			for ( int i=0 ; i<vocabulary->nWords ; i++ )
			{
				if ( vocabToLexMap[i].pronuns != NULL )
					free( vocabToLexMap[i].pronuns ) ;
			}
		}
		else
			delete [] vocabToLexMap[0].pronuns ;
			
		delete [] vocabToLexMap ;
	}

	delete phoneInfo ;
	delete vocabulary ;
   delete monoLookup ;
}


void DecLexInfo::initLexInfoEntry( DecLexInfoEntry *entry )
{
	entry->nPhones = 0 ;
	entry->phones = NULL ;
	entry->monophones = NULL ;
	entry->logPrior = 0.0 ;
	entry->vocabIndex = -1 ;
}



/********* BINARY FILE I/O FUNCTIONS **********/

void DecLexInfo::writeBinary( const char *binFName )
{
	FILE *fd ;

	// Open the output file
	if ( (fd=fopen( binFName , "wb" )) == NULL )
		error("DecLexInfo::writeBinary - error opening output file") ;

	fwrite( &nEntries , sizeof(int) , 1 , fd ) ;
	fwrite( &sentStartIndex , sizeof(int) , 1 , fd ) ;
	fwrite( &sentEndIndex , sizeof(int) , 1 , fd ) ;
	fwrite( &silIndex , sizeof(int) , 1 , fd ) ;
	fwrite( &cdType , sizeof(CDType) , 1 , fd ) ;
	fwrite( &rescoreMode , sizeof(bool) , 1 , fd ) ;
	
   // Write the monoOnlyMode flag
   fwrite( &monoOnlyMode , sizeof(bool) , 1 , fd ) ;

   if ( monoOnlyMode )
   {
      // Write the monoLookup
      monoLookup->writeBinary( fd ) ;
   }
   else
   {
   	// Write the phoneInfo
	   phoneInfo->writeBinary( fd ) ;
   }

	// Write the vocabulary
	vocabulary->writeBinary( fd ) ;

	// Write the vocabToLexMap
	writeBinaryVTLM( fd ) ;

	// Write the entries
	writeBinaryEntries( fd ) ;

	fclose(fd) ;
}


void DecLexInfo::readBinary( const char *binFName )
{
	FILE *fd ;

	// Open the input file
	if ( (fd=fopen( binFName , "rb" )) == NULL )
		error("DecLexInfo::readBinary - error opening input file\n") ;

	if ( fread( &nEntries , sizeof(int) , 1 , fd ) != 1 )
		error("DecLexInfo::readBinary - error reading nEntries") ;
	if ( fread( &sentStartIndex , sizeof(int) , 1 , fd ) != 1 )
		error("DecLexInfo::readBinary - error reading sentStartIndex") ;
	if ( fread( &sentEndIndex , sizeof(int) , 1 , fd ) != 1 )
		error("DecLexInfo::readBinary - error reading sentEndIndex") ;
	if ( fread( &silIndex , sizeof(int) , 1 , fd ) != 1 )
		error("DecLexInfo::readBinary - error reading silIndex") ;
	if ( fread( &cdType , sizeof(CDType) , 1 , fd ) != 1 )
		error("DecLexInfo::readBinary - error reading cdType") ;
	if ( fread( &rescoreMode , sizeof(bool) , 1 , fd ) != 1 )
		error("DecLexInfo::readBinary - error reading rescoreMode") ;
	
   // Read the monoOnlyMode flag
	if ( fread( &monoOnlyMode , sizeof(bool) , 1 , fd ) != 1 )
		error("DecLexInfo::readBinary - error reading monoOnlyMode") ;
  
   if ( monoOnlyMode )
   {
      monoLookup = new MonophoneLookup() ;
      monoLookup->readBinary( fd ) ;
   }
   else
   {
   	// Read the phoneInfo
   	phoneInfo = new DecPhoneInfo() ;
   	phoneInfo->readBinary( fd ) ;
   }

	// Read the vocabulary
	vocabulary = new DecVocabulary() ;
	vocabulary->readBinary( fd ) ;

	// Read the vocabToLexMap
	readBinaryVTLM( fd ) ;

	// Read the entries
	readBinaryEntries( fd ) ;

	fromBinFile = true ;
	fclose( fd ) ;
}


void DecLexInfo::writeBinaryVTLM( FILE *fd )
{
	int i , total ;

	// Write all of the nPronuns fields.
	// Calculate total number of pronuns.
	for ( i=0,total=0 ; i<vocabulary->nWords ; i++ )
	{
		total += vocabToLexMap[i].nPronuns ;
		fwrite( &(vocabToLexMap[i].nPronuns) , sizeof(int) , 1 , fd ) ;
	}

	// Write the total number of pronuns
	fwrite( &total , sizeof(int) , 1 , fd ) ;
	
	// Write the pronuns arrays of each vtlm entry
	for ( i=0 ; i<vocabulary->nWords ; i++ )
		fwrite( vocabToLexMap[i].pronuns , sizeof(int) , vocabToLexMap[i].nPronuns , fd ) ;
}


void DecLexInfo::readBinaryVTLM( FILE *fd )
{
	int i , total ;
	
	// Allocate memory for the VTLM
	vocabToLexMap = new VocToLexMapEntry[vocabulary->nWords] ;

	// Read the nPronuns values
	int *tmp=new int[vocabulary->nWords] ;
	if ( (int)fread( tmp , sizeof(int) , vocabulary->nWords , fd ) != vocabulary->nWords )
		error("DecLexInfo::readBinaryVTLM - error reading nPronuns entries") ;
	
	// Read the total number of pronuns
	if ( (int)fread( &total , sizeof(int) , 1 , fd ) != 1 )
		error("DecLexInfo::readBinaryVTLM - error reading total num pronuns") ;

   // Check that the values in tmp sum total
   int tot=0 ;
   for ( i=0 ; i<vocabulary->nWords ; i++ )
      tot += tmp[i] ;
   if ( tot != total )
      error("DecLexInfo::readBinaryVTLM - unexpected value for total num pronuns") ;

	// Allocate memory for and read pronun indices in one big block
	vocabToLexMap[0].nPronuns = tmp[0] ;
	vocabToLexMap[0].pronuns = new int[total] ;
	if ( (int)fread( vocabToLexMap[0].pronuns , sizeof(int) , total , fd ) != total )
		error("DecLexInfo::readBinaryVTLM - error reading pronuns array") ;

	// Finish configuring all other entries
	for ( i=1 ; i<vocabulary->nWords ; i++ )
	{
		vocabToLexMap[i].nPronuns = tmp[i] ;
		vocabToLexMap[i].pronuns = vocabToLexMap[i-1].pronuns+vocabToLexMap[i-1].nPronuns ;
	}
	
	delete [] tmp ;
}


void DecLexInfo::writeBinaryEntries( FILE *fd )
{
	int i ;
	float prior ;

	for ( i=0 ; i<nEntries ; i++ )
	{
		fwrite( &(entries[i].nPhones) , sizeof(int) , 1 , fd ) ;
		fwrite( entries[i].monophones , sizeof(int) , entries[i].nPhones , fd ) ;
		if ( rescoreMode == false )
			fwrite( entries[i].phones , sizeof(int) , entries[i].nPhones , fd ) ;

		prior = (float)( entries[i].logPrior ) ;
		fwrite( &prior , sizeof(float) , 1 , fd ) ;
		fwrite( &(entries[i].vocabIndex) , sizeof(int) , 1 , fd ) ;
	}
}


void DecLexInfo::readBinaryEntries( FILE *fd )
{
	int i ;
	float prior ;

	// Allocate memory for the entries
	entries = new DecLexInfoEntry[nEntries] ;
	
	// Read the entries
	for ( i=0 ; i<nEntries ; i++ )
	{
		if ( fread( &(entries[i].nPhones) , sizeof(int) , 1 , fd ) != 1 )
			error("DecLexInfo::readBinaryEntries - error reading nPhones field") ;
		if ( rescoreMode == false )
		{
			entries[i].monophones = new int[entries[i].nPhones * 2] ;
			entries[i].phones = entries[i].monophones + entries[i].nPhones ;
			if ( (int)fread( entries[i].monophones , sizeof(int) , entries[i].nPhones*2 , fd ) !=
																								(entries[i].nPhones*2) )
			{
				error("DecLexInfo::readBinaryEntries - error reading monophones field") ;
			}
		}
		else
		{
			entries[i].monophones = new int[entries[i].nPhones] ;
			entries[i].phones = NULL ;
			if ( (int)fread( entries[i].monophones , sizeof(int) , entries[i].nPhones , fd ) != 
																								entries[i].nPhones )
			{
				error("DecLexInfo::readBinaryEntries - error reading monophones field(2)") ;
			}
		}

		if ( fread( &prior , sizeof(float) , 1 , fd ) != 1 )
			error("DecLexInfo::readBinaryEntries - error reading prior") ;
		entries[i].logPrior = (real)prior ;

		if ( fread( &(entries[i].vocabIndex) , sizeof(int) , 1 , fd ) != 1 )
			error("DecLexInfo::readBinaryEntries - error reading vocabIndex") ;
	}
}


/*** END *** BINARY FILE I/O FUNCTIONS *** END ***/


void DecLexInfo::outputText()
{
	int i , j , k , pron ;

   if ( phoneInfo != NULL )
      phoneInfo->outputText() ;
   else if ( monoLookup != NULL )
      monoLookup->outputText() ;
   else
      error("DecLexInfo::outputText - both phoneInfo and monoLookup are NULL") ;

	vocabulary->outputText() ;

	printf("****** START DecLexInfo ******\n") ;
	printf("nEntries=%d  startInd=%d  endInd=%d  silInd=%d\n" , 
			nEntries , sentStartIndex , sentEndIndex , silIndex ) ;
	
   if ( monoOnlyMode == false )
   {
      for ( i=0 ; i<nEntries ; i++ )
      {
         printf("%-16s",vocabulary->getWord( entries[i].vocabIndex )) ;
         for ( j=0 ; j<entries[i].nPhones ; j++ )
            printf(" %s" , phoneInfo->getPhone( entries[i].phones[j] )) ;
         printf("\n") ;
      }

      printf("\n\n") ;

      for ( i=0 ; i<vocabulary->nWords ; i++ )
      {
         for ( j=0 ; j<vocabToLexMap[i].nPronuns ; j++ )
         {
            pron = vocabToLexMap[i].pronuns[j] ;
            printf("%-16s(%.3f)",vocabulary->getWord( i ),entries[pron].logPrior) ;
            for ( k=0 ; k<entries[pron].nPhones ; k++ )
               printf(" %s" , phoneInfo->getMonophone( entries[pron].monophones[k] )) ;
            printf("\n") ;
         }
      }
   }
   else
   {
      for ( i=0 ; i<vocabulary->nWords ; i++ )
      {
         for ( j=0 ; j<vocabToLexMap[i].nPronuns ; j++ )
         {
            pron = vocabToLexMap[i].pronuns[j] ;
            printf("%-16s(%.3f)",vocabulary->getWord( i ),entries[pron].logPrior) ;
            for ( k=0 ; k<entries[pron].nPhones ; k++ )
               printf(" %s" , monoLookup->getString( entries[pron].monophones[k] )) ;
            printf("\n") ;
         }
      }
   }
	
	printf("****** END DecLexInfo ******\n") ;
}


void DecLexInfo::writeWFST( const char *wfstFName , const char *inSymbolsFName ,
                            const char *outSymbolsFName )
{
   // Creates the lexicon transducer that is the union of the transducers
   //   for all pronunciations
   // Input labels are CI phonemes, output labels are words.

   // NOTE: We add 1 to all phone and word indices, because the 0th index is
   //       always reserved for the epsilon symbol.
   // NOTE: We do not output weights - assume that omission results in default.
   //       (i.e. 1.0 in prob semiring or 0.0 in log/tropical semiring).

   int   i , j ;
   FILE  *wfstFD=NULL , *inSymFD=NULL , *outSymFD=NULL ;
   int   initState=0 ;
   int   epsilon=0 ;
   int   nStates=1 ; // 1 for the initial state
   int   nFinalStates=0 , nFinalStatesAlloc=0 , *finalStates=NULL ;
      
   // Open the WFST file.
   if ( (wfstFD = fopen( wfstFName , "wb" )) == NULL )
      error("DecLexInfo::outputWFST - error opening WFST output file: %s",wfstFName) ;
      
   for ( i=0 ; i<nEntries ; i++ )
   {
      // Write the first arc - output label is the word
      fprintf( wfstFD , "%-10d %-10d %-10d %-10d 0.000\n" , 
               initState , nStates , (entries[i].monophones[0])+1 , (entries[i].vocabIndex)+1 ) ;
      nStates++ ;

      // Write the remaining arcs - output labels are all epsilon
      for ( j=1 ; j<entries[i].nPhones ; j++ )
      {
         fprintf( wfstFD , "%-10d %-10d %-10d %-10d 0.000\n" , 
                  nStates-1 , nStates , (entries[i].monophones[j])+1 , epsilon ) ;
         nStates++ ;
      }

      // Make a note of the final state for this pronunciation
      if ( nFinalStates == nFinalStatesAlloc )
      {
         finalStates = (int *)realloc( finalStates , (nFinalStatesAlloc+1000)*sizeof(int) ) ;
         nFinalStatesAlloc += 1000 ;
      }
      finalStates[nFinalStates++] = nStates - 1 ;
   }
  
   // Write the final states
   for ( i=0 ; i<nFinalStates ; i++ )
   {
      fprintf( wfstFD , "%d\n" , finalStates[i] ) ;
   }
   
   // Close the WFST file.
   fclose( wfstFD ) ;

   // Deallocate the finalStates memory
   if ( finalStates != NULL )
      free( finalStates ) ;

   // Now write the input (phonemes) symbols file
   if ( (inSymFD = fopen( inSymbolsFName , "wb" )) == NULL )
      error("DecLexInfo::outputWFST - error opening input symbols file: %s",inSymbolsFName) ;
   
   // epsilon symbol is always 0th symbol
   fprintf( inSymFD , "%-25s %d\n" , "<eps>" , 0 ) ;
   for ( i=0 ; i<phoneInfo->getNumMonophones() ; i++ )
   {
      fprintf( inSymFD , "%-25s %d\n" , phoneInfo->getMonophone(i) , i+1 ) ;
   }
   
   fclose( inSymFD ) ;
   
   // Now write the output (words) symbols files
   if ( (outSymFD = fopen( outSymbolsFName , "wb" )) == NULL )
      error("DecLexInfo::outputWFST - error opening output symbols file: %s",outSymbolsFName) ;
   
   // epsilon symbol is always 0th symbol
   fprintf( outSymFD , "%-25s %d\n" , "<eps>" , 0 ) ;
   for ( i=0 ; i<vocabulary->nWords ; i++ )
   {
      fprintf( outSymFD , "%-25s %d\n" , vocabulary->words[i] , i+1 ) ;
   }
   
   fclose( outSymFD ) ;
}


MonophoneLookup *DecLexInfo::getMonoLookup()
{
   if ( monoOnlyMode == false )
      error("DecLexInfo::getMonoLookup - monoOnlyMode false") ;
   
   return monoLookup ;
}

/**
 * Fix up the pronunciation probabilities so those for a given word
 * sum to unity.
 */
void DecLexInfo::normalisePronuns()
{
    // Loop over each word in the vocabulary
    for (int i=0; i<vocabulary->nWords; i++)
    {
        VocToLexMapEntry& lexMap = vocabToLexMap[i];

        // Loop 1: accumulate the sum of the probabilities
        double sum = 0.0;
        for (int j=0; j<lexMap.nPronuns; j++)
        {
            DecLexInfoEntry& lexInfo = entries[lexMap.pronuns[j]];
            sum += exp(lexInfo.logPrior);
        }

        // Loop 2: normalise the probabilities
        real logSum = log(sum);
        for (int j=0; j<lexMap.nPronuns; j++)
        {
            DecLexInfoEntry& lexInfo = entries[lexMap.pronuns[j]];
            lexInfo.logPrior -= logSum;
        }
    }
}

}

