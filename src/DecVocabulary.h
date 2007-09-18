/*
 * Copyright 2002 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#ifndef DECVOCABULARY_INC
#define DECVOCABULARY_INC

#include "general.h"

/*
	Author:	Darren Moore (moore@idiap.ch)
	Date:		2002
	$Id: DecVocabulary.h,v 1.11 2005/08/26 01:16:34 moore Exp $
*/

namespace Juicer {


/**
 * This class contains the list of words we want our recogniser to
 * recognise plus a few "special" words (eg. sentence markers, silence
 * word). There are no duplicates in the list, and the list is sorted
 * alphabetically.
 */
class DecVocabulary
{
public:
	int         nWords ;       // total number of words
	char        **words ;      // ordered list of all word strings (normal + special)
   int         nNormWords ;
   int         *normWordInds ;
   
   char        specWordChar ; // special words start with this character (eg. '!')
   int         nSpecWords ;
   int         *specWordInds ;
   
	int         sentStartIndex ;
	int         sentEndIndex ;
	int         silIndex ;
	bool        fromBinFile ;

	/* Constructors / Destructor */
	DecVocabulary() ;

	// Creates the vocabulary.
	// 'lex_fname' is the name of the lexicon file containing the pronunciations to be
	//   recognised.  The format is the standard "word(prior) ph ph ph" format
	//   where the (prior) is optional.
	// 'sent_start_word' and 'sent_end_word' are the words that will start and
	//   end every recognised utterance. 
	DecVocabulary(
        const char *lexFName , char specWordChar_='\0' ,
        const char *sentStartWord=NULL , const char *sentEndWord=NULL ,
        const char *silWord=NULL
    ) ;

	virtual ~DecVocabulary() ;

	/* Methods */

	// Returns the word given the index into the vocabulary
	char *getWord( int index ) ;
   int getNumPronuns( int index ) ;
   bool isSpecial( int index ) ;

	// get the LM flag set for this word
	bool getIgnoreLM( int index );

	// Returns the index of a given word.  If 'guess' is defined, then the
	//  words at indices of 'guess' and 'guess+1' are checked for a match
	//   before the rest of the vocab is searched.
	int getIndex( const char *word , int guess=-1 ) ;

	void writeBinary( FILE *fd ) ;
	void readBinary( FILE *fd ) ;

	void outputText() ;

private:
   int         nWordsAlloc ;
   bool        *special ;     // indicate whether each word is special or not
   int         *nPronuns ;    // number of pronunciations for each word.

	// Adds a word to the vocabulary. Maintains alphabetic order. Does not add
	//   duplicate entries.
	int addWord( const char *word , bool registerPronun=true ) ;
   
};


}

#endif
