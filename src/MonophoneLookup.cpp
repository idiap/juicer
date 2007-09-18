/*
 * Copyright 2004 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#include "MonophoneLookup.h"

using namespace Torch;

/*
	Author:	Darren Moore (moore@idiap.ch)
	Date:		11 November 2004
	$Id: MonophoneLookup.cpp,v 1.13 2005/10/20 02:03:44 moore Exp $
*/

namespace Juicer {


#define  MAX_CHAR    127


MonophoneLookup::MonophoneLookup()
{
   firstLevelNodes = NULL ;
   nNodes = 0 ;
   nNodesAlloc = 0 ;
   nodes = NULL ;
   
   nMonophones = 0 ;
   nMonophoneStrsAlloc = 0 ;
   monophoneStrs = NULL ;
   silMonoInd = -1 ;
   pauseMonoInd = -1 ;
   
   // Allocate memory of the top-level nodes.
   // Indexed by char value - ie. 0-127
   firstLevelNodes = new int[MAX_CHAR+1] ;
   for ( int i=0 ; i<=MAX_CHAR ; i++ )
      firstLevelNodes[i] = -1 ;

   fromBinFile = false ;
}


MonophoneLookup::MonophoneLookup( const char *monophoneListFName , const char *silMonoStr ,
                                  const char *pauseMonoStr )
{
   firstLevelNodes = NULL ;
   nNodes = 0 ;
   nNodesAlloc = 0 ;
   nodes = NULL ;
   nMonophones = 0 ;
   nMonophoneStrsAlloc = 0 ;
   monophoneStrs = NULL ;
   silMonoInd = -1 ;
   pauseMonoInd = -1 ;

   // Open the monophone list file.
   FILE *fd ;
   if ( (fd = fopen( monophoneListFName , "rb" )) == NULL )
      error("MonophoneLookup::MonophoneLookup - error opening monophone list file") ;

   // Allocate memory of the top-level nodes.
   // Indexed by char value - ie. 0-127
   firstLevelNodes = new int[MAX_CHAR+1] ;
   for ( int i=0 ; i<=MAX_CHAR ; i++ )
      firstLevelNodes[i] = -1 ;

   // Read the lines of the monophone list file
   char line[1000] , *ptr ;
   while ( fgets( line , 1000 , fd ) != NULL )
   {
      // Strip whitespace & check for blank line.
      if ( (ptr = strtok( line , " \r\n\t" )) == NULL )
         continue ;

      // Add monophone to lookup structure
      addMonophone( ptr ) ;
   }

   if ( (silMonoStr != NULL) && (silMonoStr[0] != '\0') )
   {
      silMonoInd = getIndex( silMonoStr ) ;
      if ( silMonoInd < 0 )
         error("MonophoneLookup::MonophoneLookup - silMonoStr not in monoList file");
   }
   if ( (pauseMonoStr != NULL) && (pauseMonoStr[0] != '\0') )
   {
      pauseMonoInd = getIndex( pauseMonoStr ) ;
      if ( pauseMonoInd < 0 )
         error("MonophoneLookup::MonophoneLookup - pauseMonoStr not in monoList file");
   }

   fclose(fd) ;
   fromBinFile = false ;
}


MonophoneLookup::~MonophoneLookup()
{
   if ( nodes != NULL )
   {
      if ( fromBinFile )
         delete [] nodes ;
      else
         free( nodes ) ;
   }

   delete [] firstLevelNodes ;

   if ( monophoneStrs != NULL )
   {
      for ( int i=0 ; i<nMonophones ; i++ )
         delete [] monophoneStrs[i] ;

      if ( fromBinFile )
         delete [] monophoneStrs ;
      else
         free( monophoneStrs ) ;
   }
}


int MonophoneLookup::getIndex( const char *monophoneStr )
{
   const char *chr = monophoneStr ;
   int node = -1 ;

   while ( *chr != '\0' )
   {
      node = getNode( node , *chr ) ;
      if ( node < 0 )
         return -1 ;
      chr++ ;
   }

   return nodes[node].monophone ;
}


int MonophoneLookup::getIndexWithAdd( const char *monophoneStr )
{
   int index = getIndex( monophoneStr ) ;
   if ( index < 0 )
   {
      addMonophone( monophoneStr ) ;
      return getIndex( monophoneStr ) ;
   }
   return index ;
}


void MonophoneLookup::addMonophone( const char *monophoneStr )
{
   const char *chr=monophoneStr ;
   int node = -1 ;
   while ( *chr != '\0' )
   {
      node = getNode( node , *chr , true ) ;
      chr++ ;
   }

   if ( nodes[node].monophone >= 0 )
      error("MonophoneLookup::addMonophone - duplicate monophone detected. %s",monophoneStr) ;
   nodes[node].monophone = nMonophones ;

   if ( nMonophones == nMonophoneStrsAlloc )
   {
      nMonophoneStrsAlloc += 100 ;
      monophoneStrs = (char **)realloc( monophoneStrs , nMonophoneStrsAlloc*sizeof(char *) ) ;
      for ( int i=nMonophones ; i<nMonophoneStrsAlloc ; i++ )
         monophoneStrs[i] = NULL ;
   }

   monophoneStrs[nMonophones] = new char[strlen(monophoneStr)+1] ;
   strcpy( monophoneStrs[nMonophones] , monophoneStr ) ;
   nMonophones++ ;
}

   // For word-internal CD phone systems, the "monophones" are in fact WI CD-phones.
   // The tied list could potentially contain a CD-phone name that is not a proper "monophone"
   //   (ie. not present in dictionary) but could be the name of a valid physical model, perhaps
   //   tied to one or more "monophones" from the dictionary.


int MonophoneLookup::getNode( int parentNode , char chr , bool addNew )
{
   int node=-1 ;

   if ( chr < 0 )
      error("MonophoneLookup::getNode - chr=%c out of range" , chr) ;

   if ( parentNode < 0 )
   {
      // look in firstLevelNodes
      if ( firstLevelNodes[chr] < 0 )
      {
         firstLevelNodes[chr] = allocNode( chr ) ;
      }  
      
      node = firstLevelNodes[chr] ; 
   }
   else
   {
      // search through the children of 'parentNode' looking for a node
      //   with chr field equal to 'chr'.
      node = nodes[parentNode].firstChild ;
      while ( node >= 0 )
      {
         if ( nodes[node].chr == chr )
            break ;         
         node = nodes[node].nextSib ;
      }

      if ( (node < 0) && addNew )
      {
         node = allocNode( chr ) ;
         nodes[node].nextSib = nodes[parentNode].firstChild ;
         nodes[parentNode].firstChild = node ;
      }
   }

   return node ;
}
   

int MonophoneLookup::allocNode( char chr )
{
   if ( chr < 0 )
      error("MonophoneLookup::allocNode - chr=%c out of range" , chr) ;
      
   if ( nNodes == nNodesAlloc )
   {
      nNodesAlloc += 1000 ;
      nodes = (MonophoneLookupNode *)realloc( nodes , nNodesAlloc*sizeof(MonophoneLookupNode) ) ;
      for ( int i=nNodes ; i<nNodesAlloc ; i++ )
         initNode( i ) ;
   }

   nodes[nNodes++].chr = chr ;
   return ( nNodes - 1 ) ;
}


void MonophoneLookup::initNode( int ind )
{
   if ( (ind < 0) || (ind >= nNodesAlloc) )
      error("MonophoneLookup::initNode - ind out of range") ;
   
   nodes[ind].chr = '\0' ;
   nodes[ind].monophone = -1 ;
   nodes[ind].isTemp = false ;
   nodes[ind].nextSib = -1 ;
   nodes[ind].firstChild = -1 ;
}


void MonophoneLookup::outputText()
{
   char *chrs = new char[30] ;
   int nChrs=0 , i ;
  
   printf("****** START MonophoneLookup ******\n") ;
   printf("nNodes=%d nNodesAlloc=%d nMonophones=%d nMonophoneStrsAlloc=%d\n" ,
           nNodes , nNodesAlloc , nMonophones , nMonophoneStrsAlloc ) ;
   printf("silMonoInd=%d pauseMonoInd=%d\n" , silMonoInd , pauseMonoInd ) ;
   printf("\nNodes Output\n-----------\n");
   for ( i=0 ; i<=MAX_CHAR ; i++ )
   {
      nChrs = 0 ;
      outputNode( firstLevelNodes[i] , &nChrs , chrs ) ;
   }
   printf("\nStrings Output\n-----------\n");
   for ( i=0 ; i<nMonophones ; i++ )
   {
      printf("%s\n" , monophoneStrs[i] ) ;
   }
   printf("****** END MonophoneLookup ******\n") ;

   delete [] chrs ;
}


void MonophoneLookup::outputNode( int node , int *nChrs , char *chrs )
{
   if ( node < 0 )
      return ;

   chrs[(*nChrs)++] = nodes[node].chr ;
 
   if ( nodes[node].monophone >= 0 )
   {
      // we want to output this node.
      chrs[*nChrs] = '\0' ;
      printf("%s = %d\n" , chrs , nodes[node].monophone ) ;
   }

   int next = nodes[node].firstChild ;
   while ( next >= 0 )
   {
      outputNode( next , nChrs , chrs ) ;
      next = nodes[next].nextSib ;
   }

   (*nChrs)-- ;
}


const char *MonophoneLookup::getString( int monophoneInd )
{
   if ( (monophoneInd < 0) || (monophoneInd >= nMonophones) )
      error("MonophoneLookup::getString - monophoneInd out of range") ;

   return monophoneStrs[monophoneInd] ;
}


void MonophoneLookup::writeBinary( FILE *fd )
{
   if ( fd == NULL )
      error("MonophoneLookup::writeBinary - fd is NULL") ;
   
   char id[5] ;
   strcpy( id , "TOML" ) ;

   // 1. Write ID
   fwrite( (int *)id , sizeof(int) , 1 , fd ) ;
   
   // 2. Write number of first level nodes
   int num = MAX_CHAR+1 ;
   fwrite( &num , sizeof(int) , 1 , fd ) ;

   // 3. Write the firstLevelNodes array
   fwrite( firstLevelNodes , sizeof(int) , num , fd ) ;
   
   // 4. Write nNodes
   fwrite( &nNodes , sizeof(int) , 1 , fd ) ;

   // 5. Write nodes
   fwrite( nodes , sizeof(MonophoneLookupNode) , nNodes , fd ) ;

   // 6. Write nMonophones
   fwrite( &nMonophones , sizeof(int) , 1 , fd ) ;

   // 7. Write monophoneStrs array
   int len ;
   for ( int i=0 ; i<nMonophones ; i++ )
   {
      // 7a. Write str length
      len = strlen( monophoneStrs[i] ) + 1 ;
      fwrite( &len , sizeof(int) , 1 , fd ) ;

      // 7b. Write str
      fwrite( monophoneStrs[i] , sizeof(char) , len , fd ) ;
   }

   // 8. Write silMonoInd and pauseMonoInd
   fwrite( &silMonoInd , sizeof(int) , 1 , fd ) ;
   fwrite( &pauseMonoInd , sizeof(int) , 1 , fd ) ;
}


void MonophoneLookup::writeBinary( const char *binFName )
{
   FILE *fd ;
   if ( (fd = fopen( binFName ,"wb" )) == NULL )
      error("MonophoneLookup::writeBinary(2) - error opening binFName") ;

   writeBinary( fd ) ;

   fclose( fd ) ;
}


void MonophoneLookup::readBinary( FILE *fd )
{
   char id[5] ;
   id[4] = '\0' ;

   // 1. Read ID
   if ( fread( (int *)id , sizeof(int) , 1 , fd ) != 1 )
      error("MonophoneLookup::readBinary - error reading ID") ;
   if ( strcmp( id , "TOML" ) != 0 )
      error("MonophoneLookup::readBinary - exp. TOML got %s",id) ;

   // 2. Read number of first level nodes and check
   int num ;
   if ( fread( &num , sizeof(int) , 1 , fd ) != 1 )
      error("MonophoneLookup::readBinary - error reading num first level nodes") ;
   if ( num != (MAX_CHAR + 1) )
      error("MonophoneLookup::readBinary - unexpected num first level nodes %d",num) ;
   
   // 3. Allocate and read the firstLevelNodes array
   firstLevelNodes = new int[num] ;
   if ( (int)fread( firstLevelNodes , sizeof(int) , num , fd ) != num )
      error("MonophoneLookup::readBinary - error reading firstLevelNodes") ;
      
   // 4. Read nNodes 
   if ( fread( &nNodes , sizeof(int) , 1 , fd ) != 1 )
      error("MonophoneLookup::readBinary - error reading nNodes") ;

   // 5. Allocate and read nodes array
   nNodesAlloc = nNodes ;
   nodes = new MonophoneLookupNode[nNodesAlloc] ;
   if ( (int)fread( nodes , sizeof(MonophoneLookupNode) , nNodes , fd ) != nNodes )
      error("MonophoneLookup::readBinary - error reading nodes array") ;

   // 6. Read the number of monophones
   if ( fread( &nMonophones , sizeof(int) , 1 , fd ) != 1 )
      error("MonophoneLookup::readBinary - error reading nMonophones") ;

   // 7. Allocate and read the monophoneStrs array
   nMonophoneStrsAlloc = nMonophones ;
   monophoneStrs = new char*[nMonophones] ;
   int len ;
   for ( int i=0 ; i<nMonophones ; i++ )
   {
      // 7a. Read the length of the string
      if ( fread( &len , sizeof(int) , 1 , fd ) != 1 )
         error("MonophoneLookup::readBinary - error reading monoStr[%d] len",i) ;
      
      // Allocate mem for string
      monophoneStrs[i] = new char[len] ;

      // 7b. Read string and check last char is nul
      if ( (int)fread( monophoneStrs[i] , sizeof(char) , len , fd ) != len )
         error("MonophoneLookup::readBinary - error reading monoStr[%d] chars",i) ;
      if ( monophoneStrs[i][len-1] != '\0' )
         error("MonophoneLookup::readBinary - last char of monoStr[%d] not nul",i) ;
   }

   // 8. Read silMonoInd and pauseMonoInd
   if ( fread( &silMonoInd , sizeof(int) , 1 , fd ) != 1 )
      error("MonophoneLookup::readBinary - error reading silMonoInd") ;
   if ( fread( &pauseMonoInd , sizeof(int) , 1 , fd ) != 1 )
      error("MonophoneLookup::readBinary - error reading pauseMonoInd") ;
   
   fromBinFile = true ;
}


void MonophoneLookup::readBinary( const char *binFName )
{
   FILE *fd ;
   if ( (fd = fopen( binFName ,"rb" )) == NULL )
      error("MonophoneLookup::readBinary(2) - error opening binFName") ;

   readBinary( fd ) ;

   fclose( fd ) ;
}


//------------------ PhoneLookup implementation -------------------


PhoneLookup::PhoneLookup( MonophoneLookup *monoLookup_ , const char *tiedListFName ,
                          const char *sepChars_ )
{
   if ( monoLookup_ == NULL )
      error("PhoneLookup::PhoneLookup - monoLookup_ is NULL") ;
   if ( tiedListFName == NULL )
      error("PhoneLookup::PhoneLookup - cdPhoneTiedListFName is NULL") ;

   monoLookup = monoLookup_ ;
   ownMonoLookup = false ;
   nSepChars = 0 ;
   sepChars = NULL ;

   nPhones = 0 ;
   nModelInds = 0 ;
   nModelIndsAlloc = 0 ;
   modelInds = NULL ;
   modelStrs = NULL ;

   firstLevelNodes = new int[monoLookup->getNumMonophones()] ;
   for ( int i=0 ; i<monoLookup->getNumMonophones() ; i++ )
      firstLevelNodes[i] = -1 ;
   nNodes = 0 ;
   nNodesAlloc = 0 ;
   nodes = NULL ;

   monophoneMode = false ;
   extraMonoLookup = NULL ;
   nExtraMLIndsAlloc = 0 ;
   extraMLInds = NULL ;
   
   // Setup the sepChars
   if ( (sepChars_ == NULL) || (sepChars_[0] == '\0') )
   {
      nSepChars = 0 ;
      sepChars = NULL ;
      monophoneMode = true ;
      extraMonoLookup = new MonophoneLookup() ;
   }
   else
   {
      nSepChars = strlen(sepChars_) ;
      if ( (nSepChars % 2) != 0 )
         error("PhoneLookup::PhoneLookup - there must be an even number of sep chars") ;
      sepChars = new char[nSepChars+1] ;
      strcpy( sepChars , sepChars_ ) ;
      monophoneMode = false ;
   }

   // Open the tied list file
   FILE *fd ;
   if ( (fd = fopen( tiedListFName , "rb" )) == NULL )
      error("PhoneLookup::PhoneLookup - error opening tied list file") ;
   
   char line[1000] , *ptr1 , *ptr2 ;
   while ( fgets( line , 1000 , fd ) != NULL )
   {
      // There are 1 or 2 CD phones on each line.
      if ( (ptr1 = strtok( line , " \r\n\t" )) == NULL )
         continue ; // blank line
      ptr2 = strtok( NULL , " \r\n\t" ) ;

      if ( ptr2 == NULL )
      {
         // Only 1 phone on line - add it + it gets its own modelInd
         addPhone( ptr1 ) ;
      }
      else
      {
         // 2 phones on line.
         // 1) add/retrieve the second.
         // 2) add the first using the modelLookupInd of the second.
         int ind = addPhone( ptr2 ) ;
         addPhone( ptr1 , ind ) ;
      }
   }

   fclose( fd ) ;
}


PhoneLookup::PhoneLookup( const char *monophoneListFName , const char *silMonoStr ,
                          const char *pauseMonoStr , const char *tiedListFName , 
                          const char *sepChars_ )
{
   if ( monophoneListFName == NULL )
      error("PhoneLookup::PhoneLookup(2) - monophoneListFName is NULL");
   if ( tiedListFName == NULL )
      error("PhoneLookup::PhoneLookup(2) - cdPhoneTiedListFName is NULL") ;

   monoLookup = new MonophoneLookup( monophoneListFName , silMonoStr , pauseMonoStr ) ;
   ownMonoLookup = true ;
   nSepChars = 0 ;
   sepChars = NULL ;
   
   nPhones = 0 ;
   nModelInds = 0 ;
   nModelIndsAlloc = 0 ;
   modelInds = NULL ;
   modelStrs = NULL ;

   firstLevelNodes = new int[monoLookup->getNumMonophones()] ;
   for ( int i=0 ; i<monoLookup->getNumMonophones() ; i++ )
      firstLevelNodes[i] = -1 ;
   nNodes = 0 ;
   nNodesAlloc = 0 ;
   nodes = NULL ;

   monophoneMode = false ;
   extraMonoLookup = NULL ;
   nExtraMLIndsAlloc = 0 ;
   extraMLInds = NULL ;
   
   // Setup the sepChars
   if ( (sepChars_ == NULL) || (sepChars_[0] == '\0') )
   {
      nSepChars = 0 ;
      sepChars = NULL ;
      monophoneMode = true ;
      extraMonoLookup = new MonophoneLookup() ;
   }
   else
   {
      nSepChars = strlen(sepChars_) ;
      if ( (nSepChars % 2) != 0 )
         error("PhoneLookup::PhoneLookup(2) - there must be an even number of sep chars") ;
      sepChars = new char[nSepChars+1] ;
      strcpy( sepChars , sepChars_ ) ;
      monophoneMode = false ;
   }

   // Open the tied list file
   FILE *fd ;
   if ( (fd = fopen( tiedListFName , "rb" )) == NULL )
      error("PhoneLookup::PhoneLookup(2) - error opening tied list file") ;
   
   char line[1000] , *ptr1 , *ptr2 ;
   while ( fgets( line , 1000 , fd ) != NULL )
   {
      // There are 1 or 2 CD phones on each line.
      if ( (ptr1 = strtok( line , " \r\n\t" )) == NULL )
         continue ; // blank line
      ptr2 = strtok( NULL , " \r\n\t" ) ;

      if ( ptr2 == NULL )
      {
         // Only 1 phone on line - add it + it gets its own modelInd
         addPhone( ptr1 ) ;
      }
      else
      {
         // 2 phones on line.
         // 1) add/retrieve the second.
         // 2) add the first using the modelLookupInd of the second.
         int ind = addPhone( ptr2 ) ;
         addPhone( ptr1 , ind ) ;
      }
   }

   fclose( fd ) ;
}


PhoneLookup::~PhoneLookup()
{
   if ( ownMonoLookup )
      delete monoLookup ;
   delete [] sepChars ;
   delete [] firstLevelNodes ;

   if ( modelInds != NULL )
      free( modelInds ) ;
   if ( modelStrs != NULL )
   {
      for ( int i=0 ; i<nModelInds ; i++ )
         delete [] modelStrs[i] ;
      free( modelStrs ) ;
   }
   if ( nodes != NULL )
      free( nodes ) ;
   
   delete extraMonoLookup ;
   if ( extraMLInds != NULL )
      free( extraMLInds ) ;
}


int PhoneLookup::addPhone( char *phoneStr , int modelLookupInd )
{
   char *ptr=phoneStr ;
   char *pSepChar=NULL , sepChar='\0' , prevSepChar='\0' ;
   int sepCharInd=-1 , monoInd=-1 , existingMLInd=-1 ;
   
   if ( sepChars != NULL )
   {
      // Isolate the first monophone.
      // Determine what the first separating character is.
      pSepChar = strpbrk( ptr , sepChars ) ;
      if ( pSepChar != NULL )
      {
         // determine which sepChar was found
         for ( int i=0 ; i<nSepChars ; i++ )
         {
            if ( *pSepChar == sepChars[i] )
            {
               sepChar = sepChars[i] ;
               sepCharInd = i ;
               break ;
            }
         }

         // replace *pSepChar with '\0'
         *pSepChar = '\0' ;
      }
   }

   int node = -1 ;
   while ( ptr != NULL )
   {
      if ( (monoInd = monoLookup->getIndex( ptr )) < 0 )
      {
         if ( monophoneMode )
         {
            // This phoneme is in the tied list, but is not in the dictionary.
            // Check in the extraMonoLookup, add if not already there.
            monoInd = extraMonoLookup->getIndexWithAdd( ptr ) ;
            if ( monoInd >= nExtraMLIndsAlloc )
            {
               extraMLInds = (int *)realloc( extraMLInds , (monoInd+1000)*sizeof(int) ) ;
               for ( int i=nExtraMLIndsAlloc ; i<(monoInd+1000) ; i++ )
                  extraMLInds[i] = -1 ;
               nExtraMLIndsAlloc = monoInd + 1000 ;
            }
            existingMLInd = extraMLInds[monoInd] ;
            break ;
         }
         else
            error("PhoneLookup::addPhone - monophone %s not found" , ptr ) ;
      }
      else
      {
         // Get the node corresponding to the current monophone and the previous
         //   separating character.
         node = getNode( node , prevSepChar , monoInd , true ) ;
         existingMLInd = nodes[node].modelLookupInd ;

         // Isolate the next monophone in the input string
         if ( pSepChar != NULL )
         {
            ptr = pSepChar + 1 ;
            if ( *ptr == '\0' )
               error("PhoneLookup::addPhone - sepChar is last char of phone string") ;

            // Find and check the next separating character
            pSepChar = strpbrk( ptr , sepChars ) ;
            if ( pSepChar != NULL )
            {
               // Check that we got the expected separating character.
               if ( *pSepChar != sepChars[sepCharInd+1] )
                  error("PhoneLookup::addPhone - unexpected separating character") ;

               sepCharInd++ ;
               prevSepChar = sepChar ;
               sepChar = *pSepChar ;

               // replace *pSepChar with '\0'
               *pSepChar = '\0' ;
            }
            else
            {
               prevSepChar = sepChar ;
               sepChar = '\0' ;
            }
         }
         else
            break ;
      }
   }

   if ( (modelLookupInd < 0) && (existingMLInd < 0) )
   {
      // Allocate more modelInds if required.
      if ( nModelInds == nModelIndsAlloc )
      {
         nModelIndsAlloc += 1000 ;
         modelInds = (int *)realloc( modelInds , nModelIndsAlloc*sizeof(int) ) ;
         modelStrs = (char **)realloc( modelStrs , nModelIndsAlloc*sizeof(char *) ) ;
         for ( int i=nModelInds ; i<nModelIndsAlloc ; i++ )
         {
            modelInds[i] = -1 ;
            modelStrs[i] = NULL ;
         }
      }
      
      if ( (node < 0) && monophoneMode )
         extraMLInds[monoInd] = nModelInds++ ;
      else
         nodes[node].modelLookupInd = nModelInds++ ;
      nPhones++ ;
   }
   else if ( modelLookupInd >= 0 )
   {
      if ( (node < 0) && monophoneMode )
      {
         if ( extraMLInds[monoInd] >= 0 )
            error("PhoneLookup::addPhone - trying to add tied phone - duplicate detected") ;
         extraMLInds[monoInd] = modelLookupInd ;
      }
      else
      {
         if ( nodes[node].modelLookupInd >= 0 )
            error("PhoneLookup::addPhone - trying to add tied phone - duplicate detected (2)") ;
         nodes[node].modelLookupInd = modelLookupInd ;
      }
      nPhones++ ;
   }
 
   if ( (node < 0) && monophoneMode )
   {
      if ( extraMLInds[monoInd] < 0 )
         error("PhoneLookup::addPhone - lookup failed") ;
      return extraMLInds[monoInd] ;
   }
   else
   {
      if ( nodes[node].modelLookupInd < 0 )
         error("PhoneLookup::addPhone - lookup failed (2)") ;
      return nodes[node].modelLookupInd ;
   }
}


int PhoneLookup::getNode( int parentNode , char sepChar , int monoInd , bool addNew )
{
   if ( (monoInd < 0) || (monoInd >= monoLookup->getNumMonophones()) )
      error("PhoneLookup::allocNode - monoInd=%d out of range" , monoInd) ;
      
   int node=-1 ;

   if ( parentNode < 0 )
   {
      if ( sepChar != '\0' )
         error("PhoneLookup::getNode - expect sepChar==nul when parentNode<0") ;

      // look in firstLevelNodes
      if ( (firstLevelNodes[monoInd] < 0) && addNew )
      {
         firstLevelNodes[monoInd] = allocNode( '\0' , monoInd ) ;
      }  
      
      node = firstLevelNodes[monoInd] ;
   }
   else
   {
      // search through the children of 'parentNode' looking for a node
      //   with monoInd field equal to 'monoInd' and sepChar field equal to 'sepChar'.
      node = nodes[parentNode].firstChild ;
      while ( node >= 0 )
      {
         if ( (nodes[node].monoInd == monoInd) && (nodes[node].sepChar == sepChar) )
            break ;
         node = nodes[node].nextSib ;
      }

      if ( (node < 0) && addNew )
      {
         node = allocNode( sepChar , monoInd ) ;
         nodes[node].nextSib = nodes[parentNode].firstChild ;
         nodes[parentNode].firstChild = node ;
      }
   }

   return node ;
}
   

int PhoneLookup::addModelInd( const char *phoneStr , int modelInd )
{
   if ( (phoneStr == NULL) || (phoneStr[0] == '\0') )
      error("PhoneLookup::addModelInd - phoneStr is NULL or empty") ;

   // All phones have been added, but the entries of modelInds have not
   //   been configured.
   char *str = new char[1000] ;
   strcpy( str , phoneStr ) ;

   char *ptr = str ;   
   char *pSepChar=NULL , sepChar='\0' , prevSepChar='\0' ;
   int sepCharInd=-1 , monoInd=-1 , existingMLInd=-1 ;

   if ( sepChars != NULL )
   {
      // Isolate the first monophone.
      // Determine what the first separating character is.
      pSepChar = strpbrk( ptr , sepChars ) ;
      if ( pSepChar != NULL )
      {
         // determine which sepChar was found
         for ( int i=0 ; i<nSepChars ; i++ )
         {
            if ( *pSepChar == sepChars[i] )
            {
               sepChar = sepChars[i] ;
               sepCharInd = i ;
               break ;
            }
         }

         // replace *pSepChar with '\0'
         *pSepChar = '\0' ;
      }
   }

   int node = -1 ;
   while ( ptr != NULL )
   {
      if ( (monoInd = monoLookup->getIndex( ptr )) < 0 )
      {
         if ( monophoneMode )
         {
            // This phoneme is in the tied list, but is not in the dictionary.
            // Check in the extraMonoLookup, add if not already there.
            if ( (monoInd = extraMonoLookup->getIndex( ptr )) < 0 )
            {
               if ( modelInd >= 0 )
                  error("PhoneLookup::addModelInd - %s not found" , phoneStr ) ;
               else
                  return -1 ;
            }
            existingMLInd = extraMLInds[monoInd] ;
            break ;
         }
         else
            error("PhoneLookup::addModelInd - monophone %s not found (2)" , ptr ) ;
      }
      else
      {
         // Get the node corresponding to the current monophone and the previous
         //   separating character.
         if ( (node = getNode( node , prevSepChar , monoInd , false )) < 0 )
         {
            if ( modelInd >= 0 )
               error("PhoneLookup::addModelInd - %s not found" , phoneStr ) ;
            else
               return -1 ;
         }
         existingMLInd = nodes[node].modelLookupInd ;

         // Isolate the next monophone in the input string
         if ( pSepChar != NULL )
         {
            ptr = pSepChar + 1 ;
            if ( *ptr == '\0' )
               error("PhoneLookup::addModelInd - sepChar is last char of phone string") ;

            // Find and check the next separating character
            pSepChar = strpbrk( ptr , sepChars ) ;
            if ( pSepChar != NULL )
            {
               // Check that we got the expected separating character.
               if ( *pSepChar != sepChars[sepCharInd+1] )
                  error("PhoneLookup::addModelInd - unexpected separating character") ;

               sepCharInd++ ;
               prevSepChar = sepChar ;
               sepChar = *pSepChar ;

               // replace *pSepChar with '\0'
               *pSepChar = '\0' ;
            }
            else
            {
               prevSepChar = sepChar ;
               sepChar = '\0' ;
            }
         }
         else
            break ;
      }
   }

   if ( existingMLInd < 0 )
   {
      if ( modelInd >= 0 )
         error("PhoneLookup::addModelInd - modelLookupInd for %s < 0" , phoneStr ) ;
      else // only doing a lookup
         return -1 ;
   }
      
   if ( modelInd >= 0 )
   {
      // We have the modelLookupInd for this phone.
      // Check to make sure that the entry in 'modelLookupInd'th entry in modelInds 
      //   is < 0 (ie. we have not already added a model index for this phone).
      if ( modelInds[existingMLInd] >= 0 )
         error("PhoneLookup::addModelInd - modelInd for %s already exists" , phoneStr ) ;

      modelInds[existingMLInd] = modelInd ;
      modelStrs[existingMLInd] = new char[strlen(phoneStr)+1] ;
      strcpy( modelStrs[existingMLInd] , phoneStr ) ;
   }
   else
   {
      // Doing a lookup NOT adding.
      if ( modelInds[existingMLInd] < 0 )
         error("PhoneLookup::addModelInd - modelInd retrieve for %s failed" , phoneStr ) ;
      modelInd = modelInds[existingMLInd] ;
   }
   
   delete [] str ;
   return modelInd ;
}


int PhoneLookup::getModelInd( const char *phoneStr )
{
   return addModelInd( phoneStr , -1 ) ;
}


int PhoneLookup::getMaxCD()
{
   // Returns the maximum "order" or "degree of context dependency" 
   //   of models that we have.
   // Based solely on the number of sepChars that are specified.
   //   (ie. if sepChars is "-+" then returns 3, if sepChars is "" then returns 1).
   return nSepChars + 1 ;
}


const char *PhoneLookup::getModelStr( int index )
{
   if ( (index < 0) || (index >= nModelInds) )
      error("PhoneLookup::getModelStr - index out of range") ;

   for ( int i=0 ; i<nModelInds ; i++ )
   {
      if ( modelInds[i] == index )
         return modelStrs[i] ;
   }

   return NULL ;
}


void PhoneLookup::verifyAllModels()
{
   // Just checks to see if all modelInds have been filled in
   for ( int i=0 ; i<nModelInds ; i++ )
   {
      if ( modelInds[i] < 0 )
         error("PhoneLookup::verifyAllModels - model %d not present" , i ) ;
   }
}

void PhoneLookup::getAllModelInfo( int **phoneMonoInds , int *phoneModelInds )
{
   // Assume input arrays pre-allocated
   // monoInds is (nPhones x (nSepChars+1)).
   // modelInds is (nPhones x 1).

   char *tmpSepChars = new char[30] ;
   int *tmpMonoInds = new int[30] ;
   int n=0 ;
   int phnCnt=0 ;
   
   for ( int i=0 ; i<monoLookup->getNumMonophones() ; i++ )
   {
      n = 0 ;
      getAllModelInfoNode( firstLevelNodes[i] , &n , tmpSepChars , tmpMonoInds , 
                           &phnCnt , phoneMonoInds , phoneModelInds ) ;
   }

   if ( phnCnt != nPhones )
      error("PhoneLookup::getAllModelInfo - phnCnt != nPhones") ;
   /*
   // ----- START TEST CODE -----
   bool gotFirst ;
   int i , j ;
   for ( i=0 ; i<nPhones ; i++ )
   {
      gotFirst = false ;
      for ( j=0 ; j<=nSepChars ; j++ )
      {
         if ( phoneMonoInds[i][j] >= 0 )
         {
            if ( gotFirst )
               printf(" %c %s" , sepChars[j-1] , monoLookup->getString(phoneMonoInds[i][j]) ) ;
            else
            {
               gotFirst = true ;
               printf("%s" , monoLookup->getString(phoneMonoInds[i][j]) ) ;
            }
         }
      }
      printf(" = %s\n" , getModelStr( phoneModelInds[i] ) ) ;
   }
   // ----- END TEST CODE -----
   */

   delete [] tmpSepChars ;
   delete [] tmpMonoInds ;
}


void PhoneLookup::getAllModelInfoNode( int node , int *n , char *tmpSepChars , int *tmpMonoInds ,
                                       int *phnCnt , int **phoneMonoInds , int *phoneModelInds )
{
   if ( node < 0 )
      return ;

   tmpSepChars[*n] = nodes[node].sepChar ;
   tmpMonoInds[*n] = nodes[node].monoInd ;
   (*n)++ ;
 
   if ( nodes[node].modelLookupInd >= 0 )
   {
      if ( modelInds[nodes[node].modelLookupInd] < 0 )
         error("PhoneLookup::getAllModelInfoNode - modelInd for phone was < 0") ;
      if ( *phnCnt >= nPhones )
         error("PhoneLookup::getAllModelInfoNode - *phnCnt >= nPhones") ;

      // Fill-in the monoInds entry and the modelInds entry.
      int i ;
      if ( *n == 1 )
      {
         for ( i=0 ; i<(nSepChars/2) ; i++ )
         {
            phoneMonoInds[*phnCnt][i] = -1 ;
            phoneMonoInds[*phnCnt][nSepChars-i] = -1 ;            
         }
         phoneMonoInds[*phnCnt][nSepChars/2] = tmpMonoInds[0] ;
         phoneModelInds[*phnCnt] = modelInds[nodes[node].modelLookupInd] ;
      }
      else
      {
         int startInd=-1 ;
         for ( i=0 ; i<nSepChars ; i++ )
         {
            if ( sepChars[i] == tmpSepChars[1] )
            {
               startInd = i ;
               break ;
            }
         }
         if ( (startInd < 0) || ((startInd+(*n)) > (nSepChars+1)) )
            error("PhoneLookup::getAllModelInfoNode - startInd invalid - see code") ;
         
         for ( i=0 ; i<=nSepChars ; i++ )
            phoneMonoInds[*phnCnt][i] = -1 ;
         for ( i=startInd ; i<(startInd+(*n)) ; i++ )
            phoneMonoInds[*phnCnt][i] = tmpMonoInds[i-startInd] ;
         phoneModelInds[*phnCnt] = modelInds[nodes[node].modelLookupInd] ;         
      }

      (*phnCnt)++ ;
   }

   int next = nodes[node].firstChild ;
   while ( next >= 0 )
   {
      getAllModelInfoNode( next , n , tmpSepChars , tmpMonoInds , 
                           phnCnt , phoneMonoInds , phoneModelInds ) ;
      next = nodes[next].nextSib ;
   }

   (*n)-- ;
}


bool PhoneLookup::haveCISilence()
{
   if ( getCISilenceModelInd() < 0 )
      return false ;
   else
      return true ;
}


int PhoneLookup::getCISilenceModelInd()
{
   int silMono , silModel ;
   
   if ( (silMono = monoLookup->getSilMonophone()) < 0 )
      return -1 ;

   silModel = getModelInd( monoLookup->getString( silMono ) ) ;
   if ( silModel >= 0 )
      return silModel ;
   else
      return -1 ;
}


bool PhoneLookup::haveCIPause()
{
   if ( getCIPauseModelInd() < 0 )
      return false ;
   else
      return true ;
}


int PhoneLookup::getCIPauseModelInd()
{
   int pauseMono , pauseModel ;
   
   if ( (pauseMono = monoLookup->getPauseMonophone()) < 0 )
      return -1 ;

   pauseModel = getModelInd( monoLookup->getString( pauseMono ) ) ;
   if ( pauseModel >= 0 )
      return pauseModel ;
   else
      return -1 ;
}


int PhoneLookup::allocNode( char sepChar , int monoInd )
{
   if ( (monoInd < 0) || (monoInd >= monoLookup->getNumMonophones()) )
      error("PhoneLookup::allocNode - monoInd=%d out of range" , monoInd) ;
      
   if ( nNodes == nNodesAlloc )
   {
      nNodesAlloc += 1000 ;
      nodes = (PhoneLookupNode *)realloc( nodes , nNodesAlloc*sizeof(PhoneLookupNode) ) ;
      for ( int i=nNodes ; i<nNodesAlloc ; i++ )
         initNode( i ) ;
   }

   nodes[nNodes].sepChar = sepChar ;
   nodes[nNodes].monoInd = monoInd ;
   nNodes++ ;
   return ( nNodes - 1 ) ;
}


void PhoneLookup::initNode( int ind )
{
   if ( (ind < 0) || (ind >= nNodesAlloc) )
      error("MonophoneLookup::initNode - ind out of range") ;
   
   nodes[ind].sepChar = '\0' ;
   nodes[ind].monoInd = -1 ;
   nodes[ind].modelLookupInd = -1 ;
   nodes[ind].nextSib = -1 ;
   nodes[ind].firstChild = -1 ;
}


void PhoneLookup::outputText()
{
   char *tmpSepChars = new char[30] ;
   int *tmpMonoInds = new int[30] ;
   int n=0 ;
   
   printf("****** START PhoneLookup ******\n") ;
   printf("nSepChars=%d nPhones=%d nModelInds=%d nNodes=%d\n" , nSepChars , nPhones , 
          nModelInds , nNodes ) ;
   for ( int i=0 ; i<monoLookup->getNumMonophones() ; i++ )
   {
      n = 0 ;
      outputNode( firstLevelNodes[i] , &n , tmpSepChars , tmpMonoInds ) ;
   }

   printf("****** END PhoneLookup ******\n") ;
   delete [] tmpSepChars ;
   delete [] tmpMonoInds ;
}


void PhoneLookup::outputNode( int node , int *n , char *tmpSepChars , int *tmpMonoInds )
{
   if ( node < 0 )
      return ;

   tmpSepChars[*n] = nodes[node].sepChar ;
   tmpMonoInds[*n] = nodes[node].monoInd ;
   (*n)++ ;
 
   if ( nodes[node].modelLookupInd >= 0 )
   {
      // we want to output this node.
      printf("%s",monoLookup->getString(tmpMonoInds[0])) ;
      for ( int i=1 ; i<(*n) ; i++ )
      {
         printf("%c%s" , tmpSepChars[i] , monoLookup->getString(tmpMonoInds[i]) ) ;
      }
      printf( " = %d(%d,%s)\n" , nodes[node].modelLookupInd , 
              modelInds[nodes[node].modelLookupInd] , modelStrs[nodes[node].modelLookupInd] ) ;
   }

   int next = nodes[node].firstChild ;
   while ( next >= 0 )
   {
      outputNode( next , n , tmpSepChars , tmpMonoInds ) ;
      next = nodes[next].nextSib ;
   }

   (*n)-- ;
}


}
