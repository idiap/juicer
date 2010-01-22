/*
 * Copyright 2002 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#include <ctype.h>
#include "DecVocabulary.h"
#include "log_add.h"

/*
	Author:	Darren Moore (moore@idiap.ch)
	Date:		2002
	$Id: DecVocabulary.cpp,v 1.14 2005/08/26 01:16:35 moore Exp $
*/

using namespace Torch;

namespace Juicer {


DecVocabulary::DecVocabulary()
{
	nWords = 0 ;
   nWordsAlloc = 0 ;
	words = NULL ;
   special = NULL ;
   nPronuns = NULL ;

   nNormWords = 0 ;
   normWordInds = NULL ;

   specWordChar = '\0' ;
   nSpecWords = 0 ;
   specWordInds = NULL ;

	sentStartIndex = -1 ;
	sentEndIndex = -1 ;
	silIndex = -1 ;
	fromBinFile = false ;
}


DecVocabulary::DecVocabulary( const char *lexFName , char specWordChar_ ,
                  const char *sentStartWord , const char *sentEndWord , const char *silWord )
{
	FILE *lexFD ;
	char line1[1000] , line2[1000] , *line , *prevLine ;

	if ( (lexFName == NULL) || (strcmp(lexFName,"")==0) )
		error("DecVocabulary::DecVocabulary - lexicon filename undefined") ;

	nWords = 0 ;
   nWordsAlloc = 0 ;
	words = NULL ;
   special = NULL ;
   nPronuns = NULL ;

   nNormWords = 0 ;
   normWordInds = NULL ;

   specWordChar = specWordChar_ ;
   nSpecWords = 0 ;
   specWordInds = NULL ;

	sentStartIndex = -1 ;
	sentEndIndex = -1 ;
	silIndex = -1 ;
	fromBinFile = false ;

	if ( (lexFD = fopen( lexFName , "rb" )) == NULL )
		error("DecVocabulary::DecVocabulary - error opening lex file -%s-" , lexFName ) ;

	// Add words to the vocabulary.
	// Do not add duplicates.
	// Check that vocab file is in alphabetical order.
	// entries starting with 'specWordChar' mark words which should not
	// be taken into account by the LM and treated specially
	// in search (first occ of this word determines this quality)
	line = line1 ;
	prevLine = line2 ;
	prevLine[0] = '\0' ;
   int prevInd = -1 ;
	while ( fgets( line , 1000 , lexFD ) != NULL )
	{
		if ( (line[0] == '(') || (strtok( line , "(\r\n\t " ) == NULL) )
			continue ;

		// Is this a new word ?
		if ( strcmp( line , prevLine ) != 0 )
      {
         prevInd = addWord( line ) ;
      }
      else
      {
         nPronuns[prevInd]++ ;
      }

		if ( line == line1 )
		{
			line = line2 ;
			prevLine = line1 ;
		}
		else
		{
			line = line1 ;
			prevLine = line2 ;
		}
	}

	// Sentence start and sentence end are special - they do not necessarily
	//   have to be in the lexical dictionary to be included in the vocab.
   // They also do not have to have the special char as their first char
	// If they have been specified, then add them to the list of words.
	if ( (sentStartWord != NULL) && (strcmp(sentStartWord,"") != 0) )
	{
		if ( (sentStartIndex = getIndex( sentStartWord )) < 0 )
		{
			// add an additional entry
			addWord( sentStartWord , false ) ;
		}
	}

	if ( (sentEndWord != NULL) && (strcmp(sentEndWord,"") != 0) )
	{
		if ( (sentEndIndex = getIndex( sentEndWord )) < 0 )
		{
			// add an additional entry
			addWord( sentEndWord , false ) ;
		}
	}

	sentEndIndex = getIndex( sentEndWord ) ;
	sentStartIndex = getIndex( sentStartWord ) ;
	silIndex = getIndex( silWord ) ;

   // We've added all the unique words to the words list
   // Now create lists of indices of normal and special words
   // First determine how many of each there is
   int i , tmpNNorm=0 , tmpNSpec=0 ;

   special = new bool[nWords] ;

   nNormWords = 0 ;
   nSpecWords = 0 ;
   for ( i=0 ; i<nWords ; i++ )
   {
      if ( (i == sentStartIndex) || (i == sentEndIndex) || (words[i][0] == specWordChar) )
      {
         nSpecWords++ ;
         special[i] = true ;
      }
      else
      {
         nNormWords++ ;
         special[i] = false ;
      }
   }

   if ( (nNormWords + nSpecWords) != nWords )
      error("DecVocabulary::DecVocabulary - nNormWords/nSpecWords unexpected") ;

   if ( nNormWords > 0 )
      normWordInds = new int[nNormWords] ;
   if ( nSpecWords > 0 )
      specWordInds = new int[nSpecWords] ;

   for ( i=0 ; i<nWords ; i++ )
   {
      if ( (tmpNNorm > nNormWords) || (tmpNSpec > nSpecWords) )
         error("DecVocabulary::DecVocabulary - tmpNNorm/tmpNSpec unexpected") ;

      if ( (i == sentStartIndex) || (i == sentEndIndex) || (words[i][0] == specWordChar) )
         specWordInds[tmpNSpec++] = i ;
      else
         normWordInds[tmpNNorm++] = i ;
   }

   if ( (tmpNNorm != nNormWords) || (tmpNSpec != nSpecWords) )
      error("DecVocabulary::DecVocabulary - final tmpNNorm/tmpNSpec values incorrect") ;

   // Make sure that the sentence-end and sentence-start words (if specified) are special words
   if ( (sentEndIndex >= 0) && (special[sentEndIndex] == false) )
      error("DecVocabulary::DecVocabulary - sentence end word is not a special word") ;
   if ( (sentStartIndex >= 0) && (special[sentStartIndex] == false) )
      error("DecVocabulary::DecVocabulary - sentence start word is not a special word") ;

	fclose( lexFD ) ;
}


DecVocabulary::~DecVocabulary()
{
	if ( words != NULL )
	{
      for ( int i=0 ; i<nWords ; i++ )
         delete [] words[i] ;

		if ( fromBinFile == true )
      {
         delete [] nPronuns ;
			delete [] words ;
      }
		else
      {
         free( nPronuns ) ;
			free( words ) ;
      }
	}

   delete [] special ;
   delete [] normWordInds ;
   delete [] specWordInds ;
}


int DecVocabulary::addWord( const char *word , bool registerPronun )
{
	int cmpResult=0 , ind=-1 ;

   // Allocate enough memory in the list of words for the new word
   if ( nWords == nWordsAlloc )
   {
      nWordsAlloc += 100 ;
      words = (char **)realloc( words , nWordsAlloc*sizeof(char *) ) ;
      nPronuns = (int *)realloc( nPronuns , nWordsAlloc*sizeof(int) ) ;
      for ( int i=nWords ; i<nWordsAlloc ; i++ )
      {
         words[i] = NULL ;
         nPronuns[i] = 0 ;
      }
   }

	// Words starting with 'specWordChar' are added to the 'specWords' list
   // All other words are added to 'words' list.

   if ( (word == NULL) || (word[0] == '\0') )
		return -1 ;

	if ( nWords > 0 )
		cmpResult = strcasecmp( words[nWords-1] , word ) ;

	if ( (cmpResult < 0) || (nWords == 0) )
	{
		// The new word belongs at the end of the list
		words[nWords] = new char[strlen(word)+1] ;
      nPronuns[nWords] = 0 ;
		strcpy( words[nWords] , word ) ;
      ind = nWords ;
      nWords++ ;
	}
	else if ( cmpResult > 0 )
	{
		// Find the place in the list of words where we want to insert the new word
		for ( int i=0 ; i<nWords ; i++ )
		{
			cmpResult = strcasecmp( words[i] , word ) ;
			if ( cmpResult > 0 )
			{
            nWords++ ;

				// Shuffle down all words from i onwards and place the
				//   new word in position i.
				for ( int j=(nWords-1) ; j>i ; j-- )
            {
					words[j] = words[j-1] ;
               nPronuns[j] = nPronuns[j-1] ;
            }

				words[i] = new char[strlen(word)+1] ;
				strcpy( words[i] , word ) ;
            nPronuns[i] = 0 ;
            ind = i ;
            break ;
			}
			else if ( cmpResult == 0 )
			{
				// the word is already in our vocab - don't duplicate
            ind = i ;
				break ;
			}
		}
	}

   if ( ind < 0 )
      error("DecVocabulary::addWord - ind < 0") ;

   if ( registerPronun )
   {
      (nPronuns[ind])++ ;
   }

   return ind ;
}


int DecVocabulary::getNumPronuns( int index )
{
#ifdef DEBUG
   if ( (index < 0) || (index >= nWords) )
      error("DecVocabulary::getNumPronuns - index out of range") ;
#endif

   return nPronuns[index] ;
}


void DecVocabulary::writeBinary( FILE *fd )
{
	int nChars , i ;
   char id[]="TOVO" ;

   // Write the ID bytes
   if ( fwrite( id , sizeof(char) , 4 , fd ) != 4 )
      error("DecVocabulary::writeBinary - error writing ID") ;

   // Write the number of words
   fwrite( &nWords , sizeof(int) , 1 , fd ) ;
   if ( nWords > 0 )
   {
      // Write the nPronuns array
      fwrite( nPronuns , sizeof(int) , 1 , fd ) ;

      // Write the special array
      fwrite( special , sizeof(bool) , nWords , fd ) ;
   }

   // Write each word
   for ( i=0 ; i<nWords ; i++ )
   {
      nChars = strlen( words[i] ) + 1 ;
      fwrite( &nChars , sizeof(int) , 1 , fd ) ;
      fwrite( words[i] , sizeof(char) , nChars , fd ) ;
   }

   // Write the number of normal words
   fwrite( &nNormWords , sizeof(int) , 1 , fd ) ;

   // Write the normal word indices
   if ( nNormWords > 0 )
      fwrite( normWordInds , sizeof(int) , nNormWords , fd ) ;

   // Write the specWordChar
   fwrite( &specWordChar , sizeof(char) , 1 , fd ) ;

   // Write the number of special words
   fwrite( &nSpecWords , sizeof(int) , 1 , fd ) ;

   // Write the special word indices
   if ( nSpecWords > 0 )
      fwrite( specWordInds , sizeof(int) , nSpecWords , fd ) ;

   // Write the sentStartIndex, sentEndIndex, silIndex
   fwrite( &sentStartIndex , sizeof(int) , 1 , fd ) ;
   fwrite( &sentEndIndex , sizeof(int) , 1 , fd ) ;
   fwrite( &silIndex , sizeof(int) , 1 , fd ) ;
}


void DecVocabulary::readBinary( FILE *fd )
{
	char id[5] ;
	int i , nChars ;

   // Read And verify the ID line
   id[4] = '\0' ;
   if ( fread( id , sizeof(char) , 4 , fd ) != 4 )
      error("DecVocabulary::readBinary - error reading ID") ;
   if ( strcmp( id , "TOVO" ) != 0 )
      error("DecVocabulary::readBinary - invalid ID %s",id) ;

   // Read the number of words and allocate words array
   if ( fread( &nWords , sizeof(int) , 1 , fd ) != 1 )
      error("DecVocabulary::readBinary - error reading nWords") ;

   words = NULL ;
   if ( nWords > 0 )
   {
      words = new char*[nWords] ;

      // Read the number of pronuns of each word
      nPronuns = new int[nWords] ;
      if ( (int)fread( nPronuns , sizeof(int) , nWords , fd ) != nWords )
         error("DecVocabulary::readBinary - error reading nPronuns array") ;

      // Allocate memory for special array and read from file
      special = new bool[nWords] ;
      if ( (int)fread( special , sizeof(bool) , nWords , fd ) != nWords )
         error("DecVocabulary::readBinary - error reading special array") ;
   }
   nWordsAlloc = nWords ;

   // Read each word from file
   for ( i=0 ; i<nWords ; i++ )
   {
      // Read the number of chars in the word
      if ( fread( &nChars , sizeof(int) , 1 , fd ) != 1 )
         error("DecVocabulary::readBinary - error reading nChars for word %d",i) ;
      if ( nChars <= 0 )
         error("DecVocabulary::readBinary - nChars <= 0 for word %d",i) ;

      // Allocate memory
      words[i] = new char[nChars] ;

      // Read word string
      if ( (int)fread( words[i] , sizeof(char) , nChars , fd ) != nChars )
         error("DecVocabulary::readBinary - error reading string for word %d",i) ;

      // Check that word string is NULL terminated
      if ( words[i][nChars-1] != '\0' )
         error("DecVocabulary::readBinary - string for word %d not null terminated",i) ;
   }

   // Read the number of normal words and allocate normWordInds array
   if ( fread( &nNormWords , sizeof(int) , 1 , fd ) != 1 )
      error("DecVocabulary::readBinary - error reading nNormWords") ;

   normWordInds = NULL ;
   if ( nNormWords > 0 )
   {
      normWordInds = new int[nNormWords] ;

      // Read the normWordInds array
      if ( (int)fread( normWordInds , sizeof(int) , nNormWords , fd ) != nNormWords )
         error("DecVocabulary::readBinary - error reading normWordInds array") ;
   }

   // Read the specWordChar
   if ( fread( &specWordChar , sizeof(char) , 1 , fd ) != 1 )
      error("DecVocabulary::readBinary - error reading specWordChar") ;

   // Read the number of special words and allocate specWordInds array
   if ( fread( &nSpecWords , sizeof(int) , 1 , fd ) != 1 )
      error("DecVocabulary::readBinary - error reading nSpecWords") ;

   specWordInds = NULL ;
   if ( nSpecWords > 0 )
   {
      specWordInds = new int[nSpecWords] ;

      // Read the specWordInds array
      if ( (int)fread( specWordInds , sizeof(int) , nSpecWords , fd ) != nSpecWords )
         error("DecVocabulary::readBinary - error reading specWordInds array") ;
   }

   // Read the sentStartIndex, sentEndIndex, silIndex
   if ( fread( &sentStartIndex , sizeof(int) , 1 , fd ) != 1 )
      error("DecVocabulary::readBinary - error reading sentStartIndex") ;
   if ( fread( &sentEndIndex , sizeof(int) , 1 , fd ) != 1 )
      error("DecVocabulary::readBinary - error reading sentEndIndex") ;
   if ( fread( &silIndex , sizeof(int) , 1 , fd ) != 1 )
      error("DecVocabulary::readBinary - error reading silIndex") ;

   // Set the flag indicating that we read from binary
	fromBinFile = true ;
}


char *DecVocabulary::getWord( int index )
{
	if ( (index < 0) || (index >= nWords) )
		error("DecVocabulary::getWord - index out of range") ;

	return words[index] ;
}


bool DecVocabulary::isSpecial( int index )
{
	if ( (index < 0) || (index >= nWords) )
		error("DecVocabulary::isSpecial - index out of range") ;

	return special[index] ;
}


bool DecVocabulary::getIgnoreLM( int index )
{
   error("DecVocabulary::getIgnoreLM - function has been deprecated") ;
   return false ;
}


int DecVocabulary::getIndex( const char *word , int guess )
{
	// We assume that the list of words is in ascending order so
	//   that we can do a binary search.
	int min=0 , max=(nWords-1) , curr_pos=0 ;
	int cmp_result=0 ;

	if ( (word == NULL) || (word[0] == '\0') )
		return -1 ;

	// If guess is valid, do a quick check to see if the word is where
	//   the caller expects it to be - either at guess or at guess+1
	if ( (guess >= 0) && (guess<nWords) )
	{
		if ( strcasecmp(word,words[guess]) == 0 )
			return guess ;
		else if ( ((guess+1)<nWords) && (strcasecmp(word,words[guess+1])==0) )
			return guess+1 ;
	}

	while (1)
	{
		curr_pos = min+(max-min)/2 ;
		cmp_result = strcasecmp( word , words[curr_pos] ) ;
		if ( cmp_result < 0 )
			max = curr_pos-1 ;
		else if ( cmp_result > 0 )
			min = curr_pos+1 ;
		else
			return curr_pos ;

		if ( min > max )
			return -1 ;
	}

	return -1 ;
}


void DecVocabulary::outputText()
{
   int i ;

	printf("\n****** START VOCABULARY ******\n") ;
   printf("nWords=%d nNormWords=%d nSpecWords=%d specWordChar=%c sentStartInd=%d "
          "sentEndInd=%d silInd=%d\n" , nWords , nNormWords , nSpecWords , specWordChar ,
          sentStartIndex , sentEndIndex , silIndex ) ;
   printf("\n--- words ---\n") ;
	for ( i=0 ; i<nWords ; i++ )
   {
      if ( special[i] )
		   printf("%s (%d)(S)\n",words[i],nPronuns[i]) ;
      else
		   printf("%s (%d)(N)\n",words[i],nPronuns[i]) ;
   }
   printf("\n--- normWordInds ---\n") ;
	for ( i=0 ; i<nNormWords ; i++ )
		printf("%d ",normWordInds[i]) ;
   printf("\n") ;
   printf("\n--- specWordInds ---\n") ;
	for ( i=0 ; i<nSpecWords ; i++ )
		printf("%d ",specWordInds[i]) ;
   printf("\n") ;
	printf("\n****** END VOCABULARY ******\n") ;
}


}
