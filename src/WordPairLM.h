/*
 * Copyright 2005 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#ifndef WORD_PAIR_LM_INC
#define WORD_PAIR_LM_INC

#include "general.h"
#include "DecVocabulary.h"

/*
	Author:	Darren Moore (moore@idiap.ch)
	Date:		5 Jan 2005
	$Id: WordPairLM.h,v 1.2 2005/08/26 01:16:34 moore Exp $
*/

namespace Juicer {


struct WordPairLMEntry
{
   int      word ;
   int      nSucWords ;
   int      *sucWords ;
};


class WordPairLM
{
public:
   WordPairLM( const char *fName , DecVocabulary *vocab_ ) ;
   virtual ~WordPairLM() ;

   void outputText( FILE *fd=stdout ) ;
   int getNumSucWords( int word ) ;
   int *getSucWords( int word ) ;

private:
   DecVocabulary     *vocab ;
   WordPairLMEntry   *words ;

   int               currPrevWordInd ;

   void addPrevWord( const char *word ) ;
   void addSucWord( const char *word ) ;
};


}

#endif
