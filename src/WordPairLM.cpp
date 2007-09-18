/*
 * Copyright 2005 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#include "WordPairLM.h"

/*
	Author:	Darren Moore (moore@idiap.ch)
	Date:		5 Jan 2005
	$Id: WordPairLM.cpp,v 1.2 2005/08/26 01:16:35 moore Exp $
*/

using namespace Torch ;

namespace Juicer {


enum
{
   WPLM_READ_STATE_START=0 ,
   WPLM_READ_STATE_INCOMMENTS ,
   WPLM_READ_STATE_INWORDS
};

WordPairLM::WordPairLM( const char *fName , DecVocabulary *vocab_ )
{
   if ( (vocab = vocab_) == NULL )
      error("WordPairLM::WordPairLM - vocab_ is NULL") ;

   if ( vocab->sentStartIndex < 0 )
      error("WordPairLM::WordPairLM - vocab->sentStartIndex not defined") ;
   if ( vocab->sentEndIndex < 0 )
      error("WordPairLM::WordPairLM - vocab->sentEndIndex < not defined") ;

   // Allocate and initialise the words array
   words = new WordPairLMEntry[vocab->nWords] ;
   for ( int i=0 ; i<vocab->nWords ; i++ )
   {
      words[i].word = i ;
      words[i].nSucWords = 0 ;
      words[i].sucWords = NULL ;
   }

   currPrevWordInd = -1 ;

   // Open the input word pair lm file
   FILE *fd ;
   if ( (fd = fopen( fName , "rb" )) == NULL )
      error("WordPairLM::WordPairLM - error opening input word pair LM file") ;

   // Ignore comments section if it is present
   char *str ;
   char *line = new char[10000] ;
   int readState = WPLM_READ_STATE_START ;
   while ( fgets( line , 10000 , fd ) != NULL )
   {
      switch ( readState )
      {
      case WPLM_READ_STATE_START:
         // We can get either the start of comment section, or the first word pair entry.
         if ( strstr( line , "/*" ) != NULL )
         {
            readState = WPLM_READ_STATE_INCOMMENTS ;
         }
         else
         {
            // Isolate first word in line
            str = strtok( line , "\r\n\t " ) ;
            if ( (str != NULL) && (strcmp( str , "" ) != 0) )
            {
               if ( str[0] != '>' )
                  error("WordPairLM::WordPairLM - in STATE_START, 1st word does not have >") ;
               str++ ;

               // Add this new prev word
               addPrevWord( str ) ;
               readState = WPLM_READ_STATE_INWORDS ;
            }
            // else empty line - stay in same state and read next line
         }
         break ;
      case WPLM_READ_STATE_INCOMMENTS:
         // We stay in this state until we get a "*/"
         if ( strstr( line , "*/" ) != NULL )
         {
            readState = WPLM_READ_STATE_INWORDS ;
         }
         break ;
      case WPLM_READ_STATE_INWORDS:
         // Isolate first word in line
         str = strtok( line , "\r\n\t " ) ;
         if ( (str != NULL) && (strcmp( str , "" ) != 0) )
         {
            if ( str[0] == '>' )
            {
               // New prev word
               str++ ;
               addPrevWord( str ) ;
            }
            else
            {
               addSucWord( str ) ;
            }
         }
         break ;
      default:
         error("WordPairLM::WordPairLM - invalid readState") ;
         break ;
      }
   }  
   delete [] line ;

   if ( readState != WPLM_READ_STATE_INWORDS )
      error("WordPairLM::WordPairLM - end of file but no entries encountered") ;

   fclose( fd ) ;
}


WordPairLM::~WordPairLM()
{
   for ( int i=0 ; i<vocab->nWords ; i++ )
   {
      if ( words[i].sucWords != NULL )
         free( words[i].sucWords ) ;
   }
   
   delete [] words ;
}


void WordPairLM::outputText( FILE *fd )
{
   for ( int i=0 ; i<vocab->nWords ; i++ )
   {
      if ( words[i].nSucWords > 0 )
      {
         if ( i == vocab->sentStartIndex )
            fprintf( fd , ">SENTENCE-END\n" ) ;
         else
            fprintf( fd , ">%s\n" , vocab->getWord(i) ) ;

         for ( int j=0 ; j<words[i].nSucWords ; j++ )
         {
            if ( words[i].sucWords[j] == vocab->sentEndIndex )
               fprintf( fd , " SENTENCE-END\n" ) ;
            else
               fprintf( fd , " %s\n" , vocab->getWord( words[i].sucWords[j] ) ) ;
         }
      }
   }
}


int WordPairLM::getNumSucWords( int word )
{
   if ( (word < 0) || (word >= vocab->nWords) )
      error("WordPairLM::getNumSucWords - word out of range") ;

   return words[word].nSucWords ;
}


int *WordPairLM::getSucWords( int word )
{
   if ( (word < 0) || (word >= vocab->nWords) )
      error("WordPairLM::getSucWords - word out of range") ;

   return words[word].sucWords ;
}


void WordPairLM::addPrevWord( const char *word )
{
   if ( (word == NULL) || (word[0] == '\0') )
      error("WordPairLM::addPrevWord - word is NULL or empty") ;
  
   // Retrieve the vocab index of the new prev word
   int ind ;
   if ( strcmp( word , "SENTENCE-END" ) == 0 )
   {
      if ( vocab->sentStartIndex < 0 )
         error("WordPairLM::addPrevWord - SENTENCE-END but vocab->sentStartIndex < 0") ;
      ind = vocab->sentStartIndex ;
   }
   else
   {
      if ( (ind = vocab->getIndex( word )) < 0 )
         error("WordPairLM::addPrevWord - word %s not in vocab" , word ) ;
   }
  
   // Do we already have some entries for this prev word ?
   if ( words[ind].nSucWords != 0 )
      error("WordPairLM::addPrevWord - entry for %s already has sucs" , word ) ;

   if ( currPrevWordInd == ind )
      error("WordPairLM::addPrevWord - currPrevWordInd == ind") ;

   currPrevWordInd = ind ;
}


void WordPairLM::addSucWord( const char *word )
{
   if ( (word == NULL) || (word[0] == '\0') )
      error("WordPairLM::addSucWord - word is NULL or empty") ;

   if ( currPrevWordInd < 0 )
      error("WordPairLM::addSucWord - currPrevWordInd < 0") ;

   // Retrieve the vocab index of the new suc word
   int ind ;
   if ( strcmp( word , "SENTENCE-END" ) == 0 )
   {
      if ( vocab->sentEndIndex < 0 )
         error("WordPairLM::addSucWord - SENTENCE-END but vocab->sentEndIndex < 0") ;
      ind = vocab->sentEndIndex ;
   }
   else
   {
      if ( (ind = vocab->getIndex( word )) < 0 )
         error("WordPairLM::addSucWord - word %s not in vocab" , word ) ;
   }

   // Make sure that the currPrevWord does not already have the new suc
   for ( int i=0 ; i<words[currPrevWordInd].nSucWords ; i++ )
   {
      if ( words[currPrevWordInd].sucWords[i] == ind )
         error("WordPairLM::addSucWord - duplicate detected") ;
   }

   // Add new suc word ind
   words[currPrevWordInd].nSucWords++ ;
   words[currPrevWordInd].sucWords = (int *)realloc( words[currPrevWordInd].sucWords ,
                                                 words[currPrevWordInd].nSucWords*sizeof(int) ) ;
   words[currPrevWordInd].sucWords[words[currPrevWordInd].nSucWords-1] = ind ;
}


}
