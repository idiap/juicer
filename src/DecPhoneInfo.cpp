/*
 * Copyright 2002 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#include "DecPhoneInfo.h"

/*
	Author:	Darren Moore (moore@idiap.ch)
	Date:		2002
	$Id: DecPhoneInfo.cpp,v 1.13 2005/08/26 01:16:35 moore Exp $
*/

using namespace Torch;

namespace Juicer {


DecPhoneInfo::DecPhoneInfo()
{
	nPhones = 0 ;
	phoneNames = NULL ;
	silIndex = -1 ;
	silMonophoneIndex = -1 ;
	pauseIndex = -1 ;
	fromBinFile = false ;

	cdType = CD_TYPE_NONE ;
   monoLookup = NULL ;
	cdPhoneLookup = NULL ;
}


DecPhoneInfo::DecPhoneInfo( const char *monoListFName , char *phonesFName , char *silName , 
                            char *pauseName , CDType cdType_ , const char *cdSepChars_ )
{
   FILE *phones_fd ;
   char line[1000] ;

   if ( (phonesFName == NULL) || (strcmp(phonesFName,"")==0) )
      error("DecPhoneInfo::DecPhoneInfo(2) - phonesFName undefined") ;

   nPhones = 0 ;
   phoneNames = NULL ;
   silIndex = -1 ;
   silMonophoneIndex = -1 ;
   pauseIndex = -1 ;
   fromBinFile = false ;

   cdType = cdType_ ;

   monoLookup = new MonophoneLookup( monoListFName , silName , pauseName ) ;

   cdPhoneLookup = NULL ;
   if ( cdType >= CD_TYPE_INVALID )
      error("DecPhoneInfo::DecPhoneInfo(2) - invalid cdType") ;
   else	
      cdPhoneLookup = new CDPhoneLookup( monoLookup , cdType , cdSepChars_ ) ;

   if ( (phones_fd = fopen( phonesFName , "rb" )) == NULL )
      error("DecPhoneInfo::DecPhoneInfo(2) - error opening phones file") ;

   // read the first non-blank line of the file and use it to determine the file type
   do 
   {
      fgets( line , 1000 , phones_fd ) ;
   } 
   while ( strtok( line , " \r\n\t" ) == NULL ) ;

   if ( strstr( line , "PHONE" ) ) 
   {
      // This is a NOWAY format phone models file
      readPhonesFromNoway( phones_fd , silName , pauseName ) ;
   }
   else if ( line[0] == '~' )
   {
      // This is a HTK model definition file
      readPhonesFromHTK( phones_fd , silName , pauseName ) ;
   }
   else
   {
      // Assume that the file contains just a list of phone names
      //   with 1 phone name per line.
      fseek( phones_fd , 0 , SEEK_SET ) ;
      readPhonesFromASCII( phones_fd , silName , pauseName ) ;
   }

   if ( (silName != NULL) && (strcmp(silName,"")!=0) && (silIndex<0) )
      error("DecPhoneInfo::DecPhoneInfo(2) - silence phone not found") ;
   if ( (pauseName != NULL) && (strcmp(pauseName,"")!=0) && (pauseIndex<0) ) 
      error("DecPhoneInfo::DecPhoneInfo(2) - pause phone not found") ;

   // Setup the silMonophoneIndex
   if ( (silName != NULL) && (silName[0] != '\0') )
   {
      if ( (silMonophoneIndex = monoLookup->getIndex( silName )) < 0 )
         error("DecPhoneInfo::DecPhoneInfo(2) - silName defined but monophone not found") ;
   }

   fclose( phones_fd ) ;
}


DecPhoneInfo::~DecPhoneInfo()
{
   if ( phoneNames != NULL )
   {
      for ( int i=0 ; i<nPhones ; i++ )
         delete [] phoneNames[i] ;
      delete [] phoneNames ;
   }

   delete monoLookup ;
   delete cdPhoneLookup ;
}


char *DecPhoneInfo::getPhone( int index )
{
   if ( (index < 0) || (index >= nPhones) )
      error("DecPhoneInfo::getPhone - index out of range\n") ;

   return phoneNames[index] ;
}


int DecPhoneInfo::getIndex( const char *phone_name )
{
   if ( phone_name == NULL )
      error("DecPhoneInfo::getIndex - phone_name is NULL\n") ;

   // Just do a linear search.
   for ( int i=0 ; i<nPhones ; i++ )
   {
      if ( strcmp( phone_name , phoneNames[i] ) == 0 )
         return i ;
   }

   return -1 ;
}


int DecPhoneInfo::getMonophoneIndex( const char *monophoneName )
{
   if ( cdType <= CD_TYPE_MONOPHONE )
      return monoLookup->getIndex( monophoneName ) ;
   else
   {
      int base , nLeft , left[10] , nRight , right[10] ;
      char *str = new char[1000] ;
      strcpy( str , monophoneName ) ;

      cdPhoneLookup->splitIntoMonophones( str , &base , &nLeft , left , &nRight , right ) ;

      delete [] str ;
      return base ;
   }
}


const char *DecPhoneInfo::getMonophone( int index )
{
   return monoLookup->getString( index ) ;
}


int DecPhoneInfo::getCDPhoneIndex( int base , int nLeft , int *left , int nRight , int *right )
{
   int ind=-1 ;

   switch ( cdType )
   {
      case CD_TYPE_MONOPHONE:
         ind = cdPhoneLookup->getCDPhoneIndex( base , 0 , NULL , 0 , NULL ) ;
         break ;
      case CD_TYPE_BIPHONE_LEFT:
         ind = cdPhoneLookup->getCDPhoneIndex( base , nLeft , left , 0 , NULL ) ;
         break ;
      case CD_TYPE_BIPHONE_RIGHT:
         ind = cdPhoneLookup->getCDPhoneIndex( base , 0 , NULL , nRight , right ) ;
         break ;
      case CD_TYPE_TRIPHONE:
         ind = cdPhoneLookup->getCDPhoneIndex( base , nLeft , left , nRight , right ) ;
         break ;
      default:
         error("DecPhoneInfo::getCDPhoneIndex - invalid cdType") ;
         break ;
   }

   return ind ;		
}


int DecPhoneInfo::getTriModelIndex( int base , int left , int right )
{
#ifdef DEBUG
   if ( cdType != CD_TYPE_TRIPHONE )
      error("DecPhoneInfo::getTriModelIndex - cdType is not CD_TYPE_TRIPHONE") ;
   if ( base < 0 )
      error("DecPhoneInfo::getTriModelIndex - base < 0") ;
   if ( left < 0 )
      error("DecPhoneInfo::getTriModelIndex - left < 0") ;
   if ( right < 0 )
      error("DecPhoneInfo::getTriModelIndex - right < 0") ;
#endif

   return cdPhoneLookup->getCDPhoneIndex( base , 1 , &left , 1 , &right ) ;
}


void DecPhoneInfo::readPhonesFromASCII( FILE *phonesFD , char *silName , char *pauseName )
{
   // Assume we have a file that only has phoneme names (1 line per phoneme name)
   // Do not support CD phones.
   if ( cdType != CD_TYPE_MONOPHONE )
      error("DecPhoneInfo::readPhonesFromASCII - cdType is not CD_TYPE_MONOPHONE") ;

   char line[1000] , *str ;
   int totalNPhones=0 ;

   // Do a first pass to determine the number of phones in the file
   while ( fgets( line , 1000 , phonesFD ) != NULL )
   {
      // Ignore comment lines or blank lines
      if ( (line[0] == '#') || ((str = strtok( line , "\r\n\t ")) == NULL) )
         continue ;
      totalNPhones++ ;
   }

   phoneNames = new char*[totalNPhones] ;

   // Return to start of file and read in the phoneme names
   fseek( phonesFD , 0 , SEEK_SET ) ;
   while ( fgets( line , 1000 , phonesFD ) != NULL )
   {
      // Ignore comment lines or blank lines
      if ( (line[0] == '#') || ((str = strtok( line , "\r\n\t ")) == NULL) )
         continue ;

      // Make sure this phone is not already in the list
      if ( getIndex( str ) >= 0 )
         error("DecPhoneInfo::readPhonesFromASCII - duplicate phone %s detected",str) ;

      // Allocate memory to hold new phone name
      if ( nPhones >= totalNPhones )
         error("DecPhoneInfo::readPhonesFromASCII - nPhones %d exceeded totalNPhones %d" ,
               nPhones , totalNPhones ) ;
      phoneNames[nPhones] = new char[strlen(str)+1] ;
      strcpy( phoneNames[nPhones] , str ) ;

      // Check if this phone is a special phone
      if ( (silName != NULL) && (strcmp( phoneNames[nPhones] , silName ) == 0) )
      {
         if ( silIndex >= 0 )
            error("DecPhoneInfo::readPhonesFromASCII - silIndex already defined") ;
         else
            silIndex = nPhones ;
      }
      if ( (pauseName != NULL) && (strcmp( phoneNames[nPhones] , pauseName ) == 0) )
      {
         if ( pauseIndex >= 0 )
            error("DecPhoneInfo::readPhonesFromASCII - pauseIndex already defined") ;
         else
            pauseIndex = nPhones ;
      }

      nPhones++ ;
   }

   // Check we got the expected number of phone names
   if ( nPhones != totalNPhones )
      error("DecPhoneInfo::readPhonesFromASCII - nPhones / totalNPhones mismatch") ;

   if ( cdType <= CD_TYPE_MONOPHONE )
   {
      // All of the phones that we just read should be in monoLookup
      for ( int i=0 ; i<nPhones ; i++ )
      {
         if ( monoLookup->getIndex( phoneNames[i] ) < 0 )
            error("DecPhoneInfo::readPhonesFromASCII - %s not in monoLookup" , phoneNames[i] ) ;
      }
   }
}


void DecPhoneInfo::readPhonesFromNoway( FILE *phones_fd , char *silName , char *pauseName )
{
   char line[1000] , str[1000] ;
   int cnt=0 , n_states , index ;

   error("DecPhoneInfo::readPhonesFromNoway - implementation is not current") ;

   // Assume the first line of the file has already been read.
   // The second line contains the number of phones in the file.
   fgets( line , 1000 , phones_fd ) ;
   if ( sscanf( line , "%d" , &nPhones ) != 1 )
      error("DecPhoneInfo::readPhonesFromNoway - error reading nPhones") ;

   phoneNames = new char*[nPhones] ;

   while ( fgets( line , 1000 , phones_fd ) != NULL )
   {
      // interpret the line containing the index, n_states, name fields
      if ( sscanf( line , "%d %d %s" , &index , &n_states , str ) != 3 )
         error("DecPhoneInfo::readPhonesFromNoway - error reading index,n_st,name line") ;
      if ( index != (cnt+1) )
         error("DecPhoneInfo::readPhonesFromNoway - phone index mismatch") ;

      // add the phone to our list
      phoneNames[cnt] = new char[strlen(str)+1] ;
      strcpy( phoneNames[cnt] , str ) ;

      if ( (silName != NULL) && (strcmp(silName,str)==0) )
      {
         if ( silIndex >= 0 )
            error("DecPhoneInfo::readPhonesFromNoway - silIndex already defined") ;
         silIndex = cnt ;
      }
      if ( (pauseName != NULL) && (strcmp(pauseName,str)==0) )
      {
         if ( pauseIndex >= 0 )
            error("DecPhoneInfo::readPhonesFromNoway - pauseIndex already defined") ;
         pauseIndex = cnt ;
      }

      // There are (n_states+1) lines before the next line containing a phone name.
      // Read and discard.
      for ( int i=0 ; i<(n_states+1) ; i++ )
         fgets( line , 1000 , phones_fd ) ;

      cnt++ ;
   }

   if ( cnt != nPhones )
      error("DecPhoneInfo::readPhonesFromNoway - nPhones mismatch") ;
      
   if ( cdType <= CD_TYPE_MONOPHONE )
   {
      // All of the phones that we just read should be in monoLookup
      for ( int i=0 ; i<nPhones ; i++ )
      {
         if ( monoLookup->getIndex( phoneNames[i] ) < 0 )
            error("DecPhoneInfo::readPhonesFromNoway - %s not in monoLookup" , phoneNames[i] ) ;
      }
   }
}


void DecPhoneInfo::readPhonesFromHTK( FILE *phones_fd , char *silName , char *pauseName )
{
   char line[1000] , *str ;
   int totalNPhones=0 ;

   // Assume the first line of the file has already been read.
   // Do a first pass of the file to determine the number of phones.
   while ( fgets( line , 1000 , phones_fd ) != NULL )
   {
      if ( (line[0] == '~') && (line[1] == 'h') )
      {
         totalNPhones++ ;
         // Make sure all monophones in this phone name are in the cdPhoneLookup
         strtok( line , "\"" ) ; // get past the ~h
         if ( (str = strtok( NULL , "\"" )) == NULL )
            error("DecPhoneInfo::readPhonesFromHTK - could not locate phone name") ;

         cdPhoneLookup->extractAndCheckMonophones( str ) ;
      }				
   }

   // Allocate memory
   phoneNames = new char*[totalNPhones] ;
   fseek( phones_fd , 0 , SEEK_SET ) ;

   nPhones = 0 ;
   while ( fgets( line , 1000 , phones_fd ) != NULL )
   {
      if ( (line[0] == '~') && (line[1] == 'h') )
      {
         strtok( line , "\"" ) ; // get past the ~h
         if ( (str = strtok( NULL , "\"" )) == NULL )
            error("DecPhoneInfo::readPhonesFromHTK - could not locate phone name(2)") ;

         if ( nPhones >= totalNPhones )
            error("DecPhoneInfo::readPhonesFromHTK - nPhones exceeds expected") ;

         phoneNames[nPhones] = new char[strlen(str)+1] ;
         strcpy( phoneNames[nPhones] , str ) ;

         // We have a string with the name of either a monophone or a context dependent phone.
         // Context dependent phone names are assumed to consist of monophone names
         //   separated by characters from the 'cdSepChars' array.
         // Find the indices of each of the individual contexts in the name.
         cdPhoneLookup->addCDPhone( str , nPhones ) ;

         if ( (silName != NULL) && (strcmp(silName,str)==0) )
         {
            if ( silIndex >= 0 )
               error("DecPhoneInfo::readPhonesFromHTK - silIndex already defined") ;
            silIndex = nPhones ;
         }
         if ( (pauseName != NULL) && (strcmp(pauseName,str)==0) )
         {
            if ( pauseIndex >= 0 )
               error("DecPhoneInfo::readPhonesFromHTK - pauseIndex already defined") ;
            pauseIndex = nPhones ;
         }

         nPhones++ ;
      }
   }

   if ( totalNPhones != nPhones )
      error("DecPhoneInfo::readPhonesFromHTK - unexpected nPhones\n") ;

   if ( cdType <= CD_TYPE_MONOPHONE )
   {
      // All of the phones that we just read should be in monoLookup
      for ( int i=0 ; i<nPhones ; i++ )
      {
         if ( monoLookup->getIndex( phoneNames[i] ) < 0 )
            error("DecPhoneInfo::readPhonesFromHTK - %s not in monoLookup" , phoneNames[i] ) ;
      }
   }
}


void DecPhoneInfo::writeBinary( FILE *fd )
{
   int i , len ;

   fwrite( &nPhones , sizeof(int) , 1 , fd ) ;
   for ( i=0 ; i<nPhones ; i++ )
   {
      len = strlen( phoneNames[i] ) ;
      fwrite( &len , sizeof(int) , 1 , fd ) ;
      fwrite( phoneNames[i] , sizeof(char) , len , fd ) ;
   }

   fwrite( &silIndex , sizeof(int) , 1 , fd ) ;
   fwrite( &silMonophoneIndex , sizeof(int) , 1 , fd ) ;
   fwrite( &pauseIndex , sizeof(int) , 1 , fd ) ;
   fwrite( &cdType , sizeof(CDType) , 1 , fd ) ;

   monoLookup->writeBinary( fd ) ;
   cdPhoneLookup->writeBinary( fd ) ;
}	


void DecPhoneInfo::readBinary( FILE *fd )
{
   int i , len ;

   fread( &nPhones , sizeof(int) , 1 , fd ) ;
   phoneNames = new char*[nPhones] ;
   for ( i=0 ; i<nPhones ; i++ )
   {
      fread( &len , sizeof(int) , 1 , fd ) ;
      phoneNames[i] = new char[len+1] ;
      fread( phoneNames[i] , sizeof(char) , len , fd ) ;
      phoneNames[i][len] = '\0' ;
   }

   fread( &silIndex , sizeof(int) , 1 , fd ) ;
   fread( &silMonophoneIndex , sizeof(int) , 1 , fd ) ;
   fread( &pauseIndex , sizeof(int) , 1 , fd ) ;
   fread( &cdType , sizeof(CDType) , 1 , fd ) ;

   monoLookup = new MonophoneLookup() ;
   monoLookup->readBinary( fd ) ;
   cdPhoneLookup = new CDPhoneLookup( monoLookup ) ;
   cdPhoneLookup->readBinary( fd ) ;

   fromBinFile = true ;
}


void DecPhoneInfo::outputText()
{
   printf("****** START DecPhoneInfo ******\n") ;
   printf("nPhones=%d silIndex=%d silMonophoneIndex=%d pauseIndex=%d\n",nPhones,silIndex,silMonophoneIndex,pauseIndex) ;
   printf("cdType=%d\n",cdType);
   monoLookup->outputText() ;
   cdPhoneLookup->outputText() ;
   for ( int i=0 ; i<nPhones ; i++ )
      printf("%s\n",phoneNames[i]) ;
   printf("\n") ;
   printf("****** END DecPhoneInfo ******\n") ;
}


void DecPhoneInfo::testCDPhoneLookup()
{
   int i , j , base , nLeft , left[100] , nRight , right[100] ;
   int monoInd , modelInd ;
   char str[1000] ;
   const char *name ;

   if ( cdType < CD_TYPE_MONOPHONE )
      return ;

   for ( i=0 ; i<nPhones ; i++ )
   {
      strcpy( str , phoneNames[i] ) ;
      cdPhoneLookup->splitIntoMonophones( str , &base , &nLeft , left , &nRight , right ) ;
      printf("%s :" , phoneNames[i] ) ;
      for ( j=0 ; j<nLeft ; j++ )
      {
         name = cdPhoneLookup->getMonophoneStr( left[j] ) ;
         monoInd = getMonophoneIndex( name ) ;
         if ( monoInd != left[j] )
            error("DecPhoneInfo::testCDPhoneLookup - left context monophone lookup error") ;
         printf(" %s" , name ) ;
      }
      printf(" | ") ;
      name = cdPhoneLookup->getMonophoneStr( base ) ;
      monoInd = getMonophoneIndex( name ) ;
      if ( monoInd != base )
         error("DecPhoneInfo::testCDPhoneLookup - base monophone lookup error") ;
      printf(" %s | " , name ) ;
      for ( j=0 ; j<nRight ; j++ )
      {
         name = cdPhoneLookup->getMonophoneStr( right[j] ) ;
         monoInd = getMonophoneIndex( name ) ;
         if ( monoInd != right[j] )
            error("DecPhoneInfo::testCDPhoneLookup - right context monophone lookup error") ;
         printf(" %s" , name ) ;
      }

      modelInd = cdPhoneLookup->getCDPhoneIndex( base , nLeft , left , nRight , right ) ;
      if ( modelInd != i )	
         error("DecPhoneInfo::testCDPhoneLookup - CD model lookup error") ;
      printf(" OK\n") ;
   }
}


//------------------ CDPhoneLookup Implementation ------------------


CDPhoneLookup::CDPhoneLookup( MonophoneLookup *monoLookup_ )
{
   if ( monoLookup_ == NULL )
      error("CDPhoneLookup::CDPhoneLookup(2) - monoLookup_ is NULL") ;

	state = CDPL_STATE_UNINITIALISED ;
	
	cdType = CD_TYPE_NONE ;
	cdSepChars = NULL ;
		
   monoLookup = monoLookup_ ;
	nMonophones = 0 ;

	monoIndices = NULL ;
	leftBiIndices = NULL ;
	rightBiIndices = NULL ;
	triIndices = NULL ;

	fromBinFile = false ;
}


CDPhoneLookup::CDPhoneLookup(
    MonophoneLookup *monoLookup_ , CDType cdType_ , const char *cdSepChars_
)
{
   if ( monoLookup_ == NULL )
      error("CDPhoneLookup::CDPhoneLookup - monoLookup_ is NULL") ;

	state = CDPL_STATE_ADD_MONOPHONES ;

	cdType = cdType_ ;
   cdSepChars = NULL ;
	if ( cdType <= CD_TYPE_NONE )
		error("CDPhoneLookup::CDPhoneLookup - invalid cdType") ;
   if ( ((cdSepChars_==NULL) || (cdSepChars_[0]=='\0')) && (cdType > CD_TYPE_MONOPHONE) )
		error("CDPhoneLookup::CDPhoneLookup - cdType != CD_TYPE_MONOPHONE but no sepChars_") ;
	if ( cdType >= CD_TYPE_INVALID )
		error("CDPhoneLookup::CDPhoneLookup - invalid cdType") ;
	
   int len=0 ;
   if ( cdType > CD_TYPE_MONOPHONE )
   	len = (int)strlen( cdSepChars_ ) ;
	switch ( cdType )
	{
      case CD_TYPE_MONOPHONE:
         break ;
		case CD_TYPE_BIPHONE_LEFT:
		case CD_TYPE_BIPHONE_RIGHT:
			if ( len < 1 )
				error("CDPhoneLookup::CDPhoneLookup - cdSepChars_ length incorrect (biphone)") ;
			len = 1 ;
			break ;
		case CD_TYPE_TRIPHONE:
			if ( len < 2 )
				error("CDPhoneLookup::CDPhoneLookup - cdSepChars_ length incorrect (triphone)") ;
			len = 2 ;
			break ;
		default:
			error("CDPhoneLookup::CDPhoneLookup - invalid cdType") ;
			break ;
	}

   cdSepChars = NULL ;
   if ( cdType > CD_TYPE_MONOPHONE )
   {
	   cdSepChars = new char[len+1] ;
	   strncpy( cdSepChars , cdSepChars_ , len ) ;
	   cdSepChars[len] = '\0' ;
   }

   monoLookup = monoLookup_ ;
   nMonophones = monoLookup->getNumMonophones() ;

	monoIndices = NULL ;
	leftBiIndices = NULL ;
	rightBiIndices = NULL ;
	triIndices = NULL ;

	fromBinFile = false ;
}


CDPhoneLookup::~CDPhoneLookup()
{
	int i , j ;

	delete [] cdSepChars ;

	delete [] monoIndices ;
	if ( leftBiIndices != NULL )
	{
		for ( i=0 ; i<nMonophones ; i++ )
			delete [] leftBiIndices[i] ;
		delete [] leftBiIndices ;
	}
	if ( rightBiIndices != NULL )
	{
		for ( i=0 ; i<nMonophones ; i++ )
			delete [] rightBiIndices[i] ;
		delete [] rightBiIndices ;
	}
	if ( triIndices != NULL )
	{
		for ( i=0 ; i<nMonophones ; i++ )
		{	
			for ( j=0 ; j<nMonophones ; j++ )
				delete [] triIndices[i][j] ;
			delete [] triIndices[i] ;
		}
		delete [] triIndices ;
	}
}


void CDPhoneLookup::addCDPhone( char *name , int index )
{
	int base , nLeft , left[100] , nRight , right[100] ;
	char str[1000] ;

	if ( state == CDPL_STATE_ADD_MONOPHONES )
	{
		// This is the first time this function is called after
		//   adding all monophones.
		// Allocate memory for lookup tables.
		int i , j , k ;

		monoIndices = new int[nMonophones] ;
		for ( i=0 ; i<nMonophones ; i++ )
			monoIndices[i] = -1 ;

      leftBiIndices = NULL ;
      rightBiIndices = NULL ;
      triIndices = NULL ;

		if ( (cdType == CD_TYPE_BIPHONE_LEFT) || (cdType == CD_TYPE_TRIPHONE) )
		{
			leftBiIndices = new int*[nMonophones] ;
			for ( i=0 ; i<nMonophones ; i++ )
			{
				leftBiIndices[i] = new int[nMonophones] ;
				for ( j=0 ; j<nMonophones ; j++ )
					leftBiIndices[i][j] = -1 ;
			}
		}
		
		if ( (cdType == CD_TYPE_BIPHONE_RIGHT) || (cdType == CD_TYPE_TRIPHONE) )
		{
			rightBiIndices = new int*[nMonophones] ;
			for ( i=0 ; i<nMonophones ; i++ )
			{
				rightBiIndices[i] = new int[nMonophones] ;
				for ( j=0 ; j<nMonophones ; j++ )
					rightBiIndices[i][j] = -1 ;
			}
		}
	
		if ( cdType == CD_TYPE_TRIPHONE )
		{
			triIndices = new int**[nMonophones] ;
			for ( i=0 ; i<nMonophones ; i++ )
			{
				triIndices[i] = new int*[nMonophones] ;
				for ( j=0 ; j<nMonophones ; j++ )
				{
					triIndices[i][j] = new int[nMonophones] ;
					for ( k=0 ; k<nMonophones ; k++ )
						triIndices[i][j][k] = -1 ;
				}
			}
		}

		state = CDPL_STATE_ADD_CD_ENTRIES ;
	}
	
#ifdef DEBUG
	if ( strlen(name) >= 1000 )
		error("CDPhoneLookup::addCDPhone - bad string length assumption") ;
#endif

	if ( state != CDPL_STATE_ADD_CD_ENTRIES )
		error("CDPhoneLookup::addCDPhone - invalid state") ;

	strcpy( str , name ) ;

	// First split the CD name into its monophones
	splitIntoMonophones( str , &base , &nLeft , left , &nRight , right ) ;

   if ( (cdType == CD_TYPE_MONOPHONE) && ((nLeft != 0) || (nRight != 0)) )
      error("CDPhoneLookup::addCDPhone - > monophone detected") ;
	else if ( (cdType < CD_TYPE_TRIPHONE) && ( (nLeft+nRight) > 1 ) )
		error("CDPhoneLookup::addCDPhone - > biphone detected") ;

	// If a valid number of monophones were found, add a lookup entry.
	addEntry( index , base , nLeft , left , nRight , right ) ;
}


void CDPhoneLookup::extractAndCheckMonophones( char *name )
{
#ifdef DEBUG
	if ( triIndices != NULL )
		error("CDPhoneLookup::extractAndAddMonophones - cannot call after addCDPhone") ;
	if ( strlen(name) >= 1000 )
		error("CDPhoneLookup::extractAndAddMonophones - bad string length assumption") ;
#endif

	char str[1000] ;
	strcpy( str , name ) ;
	if ( splitIntoMonophones( str , NULL , NULL ) == false )
      error("CDPhoneLookup::extractAndCheckMonophones - unrecognised monophone in %s",name) ;
}


bool CDPhoneLookup::splitIntoMonophones( char *name , int *cnt , int *monoInds )
{
	// NOTE: DESTROYS THE CONTENTS OF 'name' !!!!!
	
	// Process the context dependent phone name in 'name'.
	// Split the name up into monophone parts by looking for separating characters
	//   as defined in 'cdSepChars'.
	// Return the number of monophones found in 'cnt'.
	// Return the indices into 'monoPhones' of the monophones found.
	//   (update 'monoPhones' if new monophones are discovered).

#ifdef DEBUG
	if ( (name == NULL) || (name[0] == '\0') )
		error("CDPhoneLookup::splitIntoMonophones - name is NULL") ;
#endif

	if ( state != CDPL_STATE_ADD_MONOPHONES )
		error("CDPhoneLookup::splitIntoMonophones - invalid state") ;

   int ind ;
   
   if ( cdType == CD_TYPE_MONOPHONE )
   {
      if ( (ind = getMonophoneIndex( name )) < 0 )
         return false ;
   }
   else
   {
      char *ptr ;

      if ( (ptr = strtok( name , cdSepChars )) == NULL )
         error("CDPhoneLookup::splitIntoMonophones - nothing in name") ;

      if ( cnt != NULL )
         *cnt = 0 ;

      while ( ptr != NULL )
      {
         // We have isolated a monophone.
         // Determine its index. (add to list if not already there).
         if ( (ind = getMonophoneIndex( ptr )) < 0 )
            return false ;

         if ( cnt != NULL )
         {
            monoInds[*cnt] = ind ;
            (*cnt)++ ;
         }

         ptr = strtok( NULL , cdSepChars ) ;
      }
   }

   return true ;
}


void CDPhoneLookup::splitIntoMonophones( char *name , int *base , int *nLeft , int *left , 
													  int *nRight , int *right )
{
	// NOTE: DESTROYS THE CONTENTS OF 'name' !!!!!

	*nLeft = 0 ;
	*nRight = 0 ;
	*base = -1 ;

	int ind ;
   if ( cdType == CD_TYPE_MONOPHONE )
   {
      if ( (ind = getMonophoneIndex( name )) < 0 )
         error("CDPhoneLookup::splitIntoMonophones(2) - monophone not found(0)") ;
      *base = ind ;
      return ;
   }
   
	// Search through the characters of 'name' until we find a separating character.
	char *ptr = strchr( name , cdSepChars[0] ) , *prev ;
	if ( ptr == NULL )
	{
		// The first separating char was not in the string.
		// If cdType is CD_TYPE_TRIPHONE, check for 2nd separating char.
		if ( cdType == CD_TYPE_TRIPHONE )
			ptr = strchr( name , cdSepChars[1] ) ;

		if ( ptr == NULL )
		{
			// This is a monophone
			if ( (ind = getMonophoneIndex( name )) < 0 )
				error("CDPhoneLookup::splitIntoMonophones(2) - monophone not found(1)") ;
			*base = ind ;
		}
		else
		{
			// This is a right biphone with cdType==CD_TYPE_TRIPHONE
			prev = name ;
			*ptr = '\0' ;	// isolate the first monophone in the biphone name
			if ( (ind = getMonophoneIndex( prev )) < 0 )
				error("CDPhoneLookup::splitIntoMonophones(2) - monophone not found(2)") ;
			*base = ind ;

			prev = ptr + 1 ; // access the 2nd monophone in the biphone name
			if ( (ind = getMonophoneIndex( prev )) < 0 )
				error("CDPhoneLookup::splitIntoMonophones(2) - monophone not found(3)") ;
			*nRight = 1 ;
			right[0] = ind ;
		}
	}
	else
	{
		// The first separating char was in the string so this is either
		//   a biphone or the first part of a triphone
		prev = name ;
		*ptr = '\0' ;	// isolate the first monophone in the name
		if ( (ind = getMonophoneIndex( prev )) < 0 )
			error("CDPhoneLookup::splitIntoMonophones(2) - monophone not found(4)") ;

		if ( cdType == CD_TYPE_BIPHONE_RIGHT )
		{
			*base = ind ;
			prev = ptr + 1 ;
			if ( (ind = getMonophoneIndex( prev )) < 0 )
				error("CDPhoneLookup::splitIntoMonophones(2) - monophone not found(5)") ;
			*nRight = 1 ;
			right[0] = ind ;
		}
		else
		{
			// left biphone or triphone
			*nLeft = 1 ;
			left[0] = ind ;

			prev = ptr + 1 ;
			if ( cdType == CD_TYPE_BIPHONE_LEFT )
			{
				if ( (ind = getMonophoneIndex( prev )) < 0 )
					error("CDPhoneLookup::splitIntoMonophones(2) - monophone not found(6)") ;
				*base = ind ;
			}
			else if ( cdType == CD_TYPE_TRIPHONE )
			{
				if ( (ptr = strchr( prev , cdSepChars[1] )) != NULL )
				{
					// found 2nd sep char - triphone
					*ptr = '\0' ;	// isolate 2nd monophone in triphone name
					if ( (ind = getMonophoneIndex( prev )) < 0 )
						error("CDPhoneLookup::splitIntoMonophones(2) - monophone not found(7)") ;
					*base = ind ;

					prev = ptr + 1 ;
					if ( (ind = getMonophoneIndex( prev )) < 0 )
						error("CDPhoneLookup::splitIntoMonophones(2) - monophone not found(8)") ;
					*nRight = 1 ;
					right[0] = ind ;
				}
				else
				{
					// no 2nd sep char - left biphone
					if ( (ind = getMonophoneIndex( prev )) < 0 )
						error("CDPhoneLookup::splitIntoMonophones(2) - monophone not found(6)") ;
					*base = ind ;
				}
			}
		}
	}
}


void CDPhoneLookup::addEntry( int cdPhoneIndex , int base , int nLeft , int *left ,
										int nRight , int *right )
{
	if ( (nLeft < 0) || (nLeft > 1) || (nRight < 0) || (nRight > 1) )
		error("CDPhoneLookup::addEntry - only triphones supported at the moment") ;

	if ( state != CDPL_STATE_ADD_CD_ENTRIES )
		error("CDPhoneLookup::addEntry - invalid state") ;

#ifdef DEBUG
	if ( (base < 0) || (base >= nMonophones) )
		error("CDPhoneLookup::addEntry - monophone index out of range") ;
	if ( nRight == 1 )
	{
		if ( (right[0] < 0) || (right[0] >= nMonophones) )
			error("CDPhoneLookup::addEntry - right[0] out of range") ;
	}
#endif

	if ( nLeft == 1 )
	{
#ifdef DEBUG
		if ( (left[0] < 0) || (left[0] >= nMonophones) )
			error("CDPhoneLookup::addEntry - left[0] out of range") ;
#endif

		if ( nRight == 1 )
		{
			if ( cdType < CD_TYPE_TRIPHONE )
				error("CDPhoneLookup::addEntry - nRight & nLeft are 1 but type < triphone") ;
				
			// We have a triphone entry
			if ( triIndices[base][left[0]][right[0]] >= 0 )
				error("CDPhoneLookup::addEntry - duplicate triphone entry") ;
			triIndices[base][left[0]][right[0]] = cdPhoneIndex ;
		}
		else if ( nRight == 0 )
		{
			if ( cdType == CD_TYPE_BIPHONE_RIGHT )
				error("CDPhoneLookup::addEntry - nLeft is 1 but type == right-biphone") ;
				
			// We have a left biphone entry
			if ( leftBiIndices[base][left[0]] >= 0 )
				error("CDPhoneLookup::addEntry - duplicate left biphone entry") ;
			leftBiIndices[base][left[0]] = cdPhoneIndex ;
		}
		else
			error("CDPhoneLookup::addEntry - invalid nRight") ;
	}
	else if ( nLeft == 0 )
	{
		if ( nRight == 1 )
		{
			if ( cdType == CD_TYPE_BIPHONE_LEFT )
				error("CDPhoneLookup::addEntry - nRight is 1 but type == left-biphone") ;

			// We have a right biphone entry
			if ( rightBiIndices[base][right[0]] >= 0 )
				error("CDPhoneLookup::addEntry - duplicate right biphone entry") ;
			rightBiIndices[base][right[0]] = cdPhoneIndex ;
		}
		else if ( nRight == 0 )
		{
			// We have a monophone entry
			if ( monoIndices[base] >= 0 )
				error("CDPhoneLookup::addEntry - duplicate monophone entry") ;
			monoIndices[base] = cdPhoneIndex ;
		}
		else
			error("CDPhoneLookup::addEntry - invalid nRight(2)") ;
	}
	else
		error("CDPhoneLookup::addEntry - invalid nLeft") ;
}


int CDPhoneLookup::getMonophoneIndex( char *monophoneName )
{
#ifdef DEBUG
	if ( (monophoneName == NULL) || (monophoneName[0] == '\0') )
		error("CDPhoneLookup::getMonophoneIndex - monophoneName undefined") ;
#endif

   return monoLookup->getIndex( monophoneName ) ;
}


const char *CDPhoneLookup::getMonophoneStr( int index )
{
#ifdef DEBUG
	if ( (index < 0) || (index >= nMonophones) )
		error("CDPhoneLookup::getMonophoneStr - index %d out of range [0,%d]",index,nMonophones) ;
#endif
	
	return monoLookup->getString( index ) ;
}


int CDPhoneLookup::getCDPhoneIndex( int base , int nLeft , int *left ,	int nRight , int *right )
{
#ifdef DEBUG
	if ( (base < 0) || (base >= nMonophones) )
		error("CDPhoneLookup::getCDPhoneIndex - base monophone index out of range") ;
#endif

	if ( state == CDPL_STATE_ADD_CD_ENTRIES )
		state = CDPL_STATE_INITIALISED ;

	if ( state != CDPL_STATE_INITIALISED )
		error("CDPhoneLookup::getCDPhoneIndex - invalid state") ;

	int ind=-1 ;

	if ( nLeft == 1 )
	{
		if ( nRight == 1 )
		{
			if ( cdType < CD_TYPE_TRIPHONE )
				error("CDPhoneLookup::getCDPhoneIndex - nRight & nLeft are 1 but type < triphone") ;

			ind = triIndices[base][left[0]][right[0]] ;
		}
		else if ( nRight == 0 )
		{
			if ( cdType == CD_TYPE_BIPHONE_RIGHT )
				error("CDPhoneLookup::getCDPhoneIndex - nLeft is 1 but type == right-biphone") ;
			
			ind = leftBiIndices[base][left[0]] ;
		}
		else
			error("CDPhoneLookup::getCDPhoneIndex - nRight invalid") ;
	}
	else if ( nLeft == 0 )
	{
		if ( nRight == 1 )
		{
			if ( cdType == CD_TYPE_BIPHONE_LEFT )
				error("CDPhoneLookup::getCDPhoneIndex - nRight is 1 but type == left-biphone") ;

			ind = rightBiIndices[base][right[0]] ;
		}
		else if ( nRight == 0 )
		{
			ind = monoIndices[base] ;
		}
		else
			error("CDPhoneLookup::getCDPhoneIndex - nRight invalid(2)") ;
	}
	else
		error("CDPhoneLookup::getCDPhoneIndex - nLeft invalid") ;

	return ind ;
}


void CDPhoneLookup::writeBinary( FILE *fd )
{
	int len , i , j ;

	if ( (state != CDPL_STATE_ADD_CD_ENTRIES) && (state != CDPL_STATE_INITIALISED) )
		error("CDPhoneLookup::writeBinary - invalid state") ;

	fwrite( &cdType , sizeof(CDType) , 1 , fd ) ;
	
	len = strlen( cdSepChars ) + 1 ;
	fwrite( &len , sizeof(int) , 1 , fd ) ;
	fwrite( cdSepChars , sizeof(char) , len , fd ) ;

	fwrite( &nMonophones , sizeof(int) , 1 , fd ) ;

	// Always write monoIndices
	fwrite( monoIndices , sizeof(int) , nMonophones , fd ) ;

	if ( (cdType == CD_TYPE_BIPHONE_LEFT) || (cdType == CD_TYPE_TRIPHONE) )
	{
		// Write leftBiIndices
		for ( i=0 ; i<nMonophones ; i++ )
			fwrite( leftBiIndices[i] , sizeof(int) , nMonophones , fd ) ;
	}

	if ( (cdType == CD_TYPE_BIPHONE_RIGHT) || (cdType == CD_TYPE_TRIPHONE) )
	{
		// Write rightBiIndices
		for ( i=0 ; i<nMonophones ; i++ )
			fwrite( rightBiIndices[i] , sizeof(int) , nMonophones , fd ) ;
	}

	if ( cdType == CD_TYPE_TRIPHONE )
	{
		// Write triIndices
		for ( i=0 ; i<nMonophones ; i++ )
		{
			for ( j=0 ; j<nMonophones ; j++ )
				fwrite( triIndices[i][j] , sizeof(int) , nMonophones , fd ) ;
		}
	}
}


void CDPhoneLookup::readBinary( FILE *fd )
{
	int len , i , j ;

	if ( state != CDPL_STATE_UNINITIALISED )
		error("CDPhoneLookup::readBinary - state != CDPL_STATE_UNINITIALISED") ;

	fread( &cdType , sizeof(CDType) , 1 , fd ) ;
	fread( &len , sizeof(int) , 1 , fd ) ;
	cdSepChars = new char[len] ;
	fread( cdSepChars , sizeof(char) , len , fd ) ;

	fread( &nMonophones , sizeof(int) , 1 , fd ) ;
   if ( nMonophones != monoLookup->getNumMonophones() )
		error("CDPhoneLookup::readBinary - nMonophones mistmatch with monoLookup") ;

	monoIndices = new int[nMonophones] ;
	fread( monoIndices , sizeof(int) , nMonophones , fd ) ;

	if ( (cdType == CD_TYPE_BIPHONE_LEFT) || (cdType == CD_TYPE_TRIPHONE) )
	{
		// Read leftBiIndices
		leftBiIndices = new int*[nMonophones] ;
		for ( i=0 ; i<nMonophones ; i++ )
		{
			leftBiIndices[i] = new int[nMonophones] ;
			fread( leftBiIndices[i] , sizeof(int) , nMonophones , fd ) ;
		}
	}

	if ( (cdType == CD_TYPE_BIPHONE_RIGHT) || (cdType == CD_TYPE_TRIPHONE) )
	{
		// Read leftBiIndices
		rightBiIndices = new int*[nMonophones] ;
		for ( i=0 ; i<nMonophones ; i++ )
		{
			rightBiIndices[i] = new int[nMonophones] ;
			fread( rightBiIndices[i] , sizeof(int) , nMonophones , fd ) ;
		}
	}

	if ( cdType == CD_TYPE_TRIPHONE )
	{
		// Read triIndices
		triIndices = new int**[nMonophones] ;
		for ( i=0 ; i<nMonophones ; i++ )
		{
			triIndices[i] = new int*[nMonophones] ;
			for ( j=0 ; j<nMonophones ; j++ )
			{
				triIndices[i][j] = new int[nMonophones] ;
				fread( triIndices[i][j] , sizeof(int) , nMonophones , fd ) ;
			}
		}
	}

	fromBinFile = true ;
	state = CDPL_STATE_INITIALISED ;
}


void CDPhoneLookup::outputText()
{
	printf("****** START CDPhoneLookup ******\n") ;
	printf("cdType=%d nMonophones=%d\n",cdType,nMonophones) ;
	printf("****** END CDPhoneLookup ******\n") ;
}



}
