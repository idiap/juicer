/*
 * Copyright 2002 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#include <assert.h>

#include "ARPALM.h"
#include "string_stuff.h"
#include "log_add.h"
#include "LogFile.h"

/*
	Author:	Darren Moore (moore@idiap.ch)
	Date:		2002
	$Id: ARPALM.cpp,v 1.11 2005/08/26 01:16:34 moore Exp $
*/

using namespace Torch;

namespace Juicer {


ARPALM::ARPALM( DecVocabulary *vocab_ )
{
	vocab = vocab_ ;
	order = 0 ;
	entries = NULL ;
	n_ngrams = NULL ;
	unk_wrd = NULL ;
	unk_id = -1 ;
	words_in_lm = NULL ;
	n_unk_words = 0 ;
	unk_words = NULL ;
}


ARPALM::ARPALM( const char *arpa_fname , DecVocabulary *vocab_ , const char *unk_wrd_ ) 
{
	FILE *fd ;
	int hdr ;
	char *tmp ;

	order = 0 ;
	entries = NULL ;
	n_ngrams = NULL ;
	unk_wrd = NULL ;
	unk_id = -1 ;
	words_in_lm = NULL ;
	n_unk_words = 0 ;
	unk_words = NULL ;

	vocab = vocab_ ;

	// Check the first word of the input file to see if it is a binary file
	if ( (fd = fopen( arpa_fname , "r" )) == NULL )
		error("ARPALM::ARPALM - error opening lm file\n") ;
	if ( fread( &hdr , sizeof(int) , 1 , fd ) != 1 )
		error("ARPALM::ARPALM - error reading 1st word\n") ;
	fclose( fd ) ;
	tmp = (char *)(&hdr) ;
	if ( (tmp[0]=='T') && (tmp[1]=='O') && (tmp[2]=='L') && (tmp[3]=='M') )
	{
		readBinary( arpa_fname ) ;
	}
	else
	{
		// allocate some memory so we can keep a record of which vocab words
		//   are in the language model.
		words_in_lm = new bool[vocab->nWords] ;
		for ( int i=0 ; i<vocab->nWords ; i++ )
			words_in_lm[i] = false ;
		n_unk_words = 0 ;
		unk_words = NULL ;

		if ( (unk_wrd_ == NULL) || (unk_wrd_[0]=='\0') )
      {
         unk_wrd = NULL ;
         unk_id = -1 ;
      }
      else
      {
         if ( vocab->getIndex(unk_wrd_) >= 0 )
			   error("ARPALM::ARPALM - unk word invalid - already in vocab") ;
         unk_wrd = new char[strlen(unk_wrd_)+1] ;
         strcpy( unk_wrd , unk_wrd_ ) ;
         unk_id = vocab->nWords ;
      }

		readARPA( arpa_fname ) ;
		calcUnkWords() ;
	}

#ifdef DEBUG
	printWarning() ;
#endif
}


ARPALM::~ARPALM()
{
	for ( int i=0 ; i<order ; i++ )
	{
		delete [] entries[i][0].words ;
		delete [] entries[i] ;
	}
	delete [] entries ;

	delete [] n_ngrams ;
	delete [] unk_wrd ;
	delete [] words_in_lm ;
	delete [] unk_words ;
}


void ARPALM::writeBinary( const char *fname )
{
	FILE *fd ;
	int i , j , ind , hdr ;

	((char *)&hdr)[0] = 'T' ;
	((char *)&hdr)[1] = 'O' ;
	((char *)&hdr)[2] = 'L' ;
	((char *)&hdr)[3] = 'M' ;

	if ( (fd = fopen( fname , "wb" )) == NULL )
		error("ARPALM::writeBinary - error opening output file") ;

	setvbuf(fd, 0, 16384, _IOFBF);

	// 1. Write the LM header
	fwrite( &hdr , sizeof(int) , 1 , fd ) ;

	// 2. Write the order
   fwrite( &order , sizeof(int) , 1 , fd ) ;

	// 3. Write the Vocabulary
	vocab->writeBinary( fd ) ;

	// 4. Write the words_in_lm array
	fwrite( words_in_lm , sizeof(bool) , vocab->nWords , fd ) ;

   // 5. Write the unknown word stuff
   bool haveUnk = true ;
   if ( unk_wrd != NULL )
   {
      haveUnk = true ;
      int len = strlen(unk_wrd) + 1 ;
      fwrite( &haveUnk , sizeof(bool) , 1 , fd ) ; 
      fwrite( &len , sizeof(int) , 1 , fd ) ;
      fwrite( unk_wrd , sizeof(char) , len , fd ) ;
      fwrite( &unk_id , sizeof(int) , 1 , fd ) ;
      fwrite( &n_unk_words , sizeof(int) , 1 , fd ) ;
      fwrite( unk_words , sizeof(int) , n_unk_words , fd ) ;
   }
   else
   {
      haveUnk = false ;
      fwrite( &haveUnk , sizeof(bool) , 1 , fd ) ;
   }

	// 6. Write all entries
	for ( i=0 ; i<order ; i++ )
	{
      // write the value of N
      int val=(i+1) ;
      fwrite( &val , sizeof(int) , 1 , fd ) ;
      fwrite( &(n_ngrams[i]) , sizeof(int) , 1 , fd ) ;

		// write logprobs and bo weights
		if ( i == (order-1) )
		{   
			// no backoff probs for 'order'-grams
			for ( j=0 ; j<n_ngrams[i] ; j++ )
				fwrite( &(entries[i][j].log_prob) , sizeof(float) , 1 , fd ) ;
		}
		else
		{
			for ( j=0,ind=0 ; j<n_ngrams[i] ; j++ )
			{
				fwrite( &(entries[i][j].log_prob) , sizeof(float) , 1 , fd ) ;
				fwrite( &(entries[i][j].log_bo) , sizeof(float) , 1 , fd ) ;
			}
		}

		fwrite( entries[i][0].words , sizeof(int) , (i+1)*(n_ngrams[i]) , fd ) ;
	}
	fclose(fd) ;
}


void ARPALM::readBinary( const char *fname )
{
   FILE *fd ;
   char *tmp ;
   int i , j , hdr , ind ;
   float *fbuf ;

   if ( (fd = fopen( fname , "rb" )) == NULL )
      error("ARPALM::readBinary - error opening input file") ;

   // 1. Read and check the header
   if ( fread( &hdr , sizeof(int) , 1 , fd ) != 1 )
      error("ARPALM::readBinary - error reading header word") ;
   tmp = (char *)(&hdr) ;
   if ( (tmp[0]!='T') || (tmp[1]!='O') || (tmp[2]!='L') || (tmp[3]!='M') )
      error("ARPALM::readBinary - invalid header word - not TOLM") ;

   // 2. Read the order
   if ( fread( &order , sizeof(int) , 1 , fd ) != 1 )
      error("ARPALM::readBinary - error reading order") ;

   // 3. Read the vocab and do some checks against the real vocab
   DecVocabulary voc ;
   voc.readBinary( fd ) ;
   if ( (voc.nWords != vocab->nWords) || (voc.sentStartIndex != vocab->sentStartIndex) ||
        (voc.sentEndIndex != vocab->sentEndIndex) || (voc.silIndex != vocab->silIndex) )
   {
      error("ARPALM::readBinary - vocab mismatch") ;
   }

   // 4. words_in_lm array
   words_in_lm = new bool[vocab->nWords] ;
   if ( (int)fread( words_in_lm , sizeof(bool) , vocab->nWords , fd ) != vocab->nWords )
      error("ARPALM::readBinary - error reading words_in_lm") ;
   
   bool haveUnk ;
   if ( fread( &haveUnk , sizeof(bool) , 1 , fd ) != 1 )
      error("ARPALM::readBinary - error reading haveUnk") ;
   
   if ( haveUnk )
   {
      // Read the length of the <unk> string
      int len ;
      if ( fread( &len , sizeof(int) , 1 , fd ) != 1 )
         error("ARPALM::readBinary - error reading unk_word length") ;

      // Allocate memory for the unk word string
      unk_wrd = new char[len] ;

      // Read the unk_word string
      if ( (int)fread( unk_wrd , sizeof(char) , len , fd ) != len )
         error("ARPALM::readBinary - error reading unk_word string") ;

      // Read in the unk_id
      if ( fread( &unk_id , sizeof(int) , 1 , fd ) != 1 )
         error("ARPALM::readBinary - error reading unk_id") ;
      if ( unk_id < 0 )
         error("ARPALM::readBinary - unk_id <= 0") ;

      // Read the n_unk_words
      if ( fread( &n_unk_words , sizeof(int) , 1 , fd ) != 1 )
         error("ARPALM::readBinary - error reading n_unk_words") ;
      if ( n_unk_words < 0 )
         error("ARPALM::readBinary - n_unk_words < 0") ;
     
      // Read in the unk_words
      unk_words = new int[n_unk_words] ;
      if ( (int)fread( unk_words , sizeof(int) , n_unk_words , fd ) != n_unk_words )
         error("ARPALM::readBinary - unk_words binary chunk read error") ;
   }
   else
   {
      unk_id = -1 ;
      unk_wrd = NULL ;
      n_unk_words = 0 ;
      unk_words = NULL ;
   }
   
   // 6. Read in the NGram entries
   n_ngrams = new int[order] ;
   entries = new ARPALMEntry*[order] ;
   for ( i=0 ; i<order ; i++ )
   {
      // Read the current 'N' and check that it is (i+1)
      int val ;
      if ( fread( &val , sizeof(int) , 1 , fd ) != 1 )
         error("ARPALM::readBinary - error reading N value") ;
      if ( val != (i+1) )
         error("ARPALM::readBinary - N value invalid") ;

      // Read the number of N-grams
      if ( fread( &(n_ngrams[i]) , sizeof(int) , 1 , fd ) != 1 )
         error("ARPALM::readBinary - error reading n_grams value") ;

      // 6b. Binary probs and bo weights
      if ( i == (order-1) )
      {
         fbuf = new float[n_ngrams[i]] ;
         if ( (int)fread( fbuf , sizeof(float) , n_ngrams[i] , fd ) != n_ngrams[i] )
            error("ARPALM::readBinary - error reading probs for 'order'") ;
      }
      else
      {
         fbuf = new float[n_ngrams[i] * 2] ;
         if ( (int)fread( fbuf , sizeof(float) , 2*n_ngrams[i] , fd ) != (2*n_ngrams[i]) )
            error("ARPALM::readBinary - error reading probs and bo_wts") ;
      }

      // 6c. Words binary chunk
      ind = 0 ;
      entries[i] = new ARPALMEntry[n_ngrams[i]] ;
      entries[i][0].log_prob = fbuf[ind++] ;
      if ( i != (order-1) )
         entries[i][0].log_bo = fbuf[ind++] ;
      entries[i][0].words = new int[n_ngrams[i] * (i+1)] ;

      for ( j=1 ; j<n_ngrams[i] ; j++ )
      {
         entries[i][j].log_prob = fbuf[ind++] ;
         if ( i != (order-1) )
            entries[i][j].log_bo = fbuf[ind++] ;
         entries[i][j].words = entries[i][0].words + j*(i+1) ;
      }

      if ( (int)fread( entries[i][0].words , sizeof(int) , (i+1)*n_ngrams[i] , fd ) != 
            (i+1)*n_ngrams[i] )
      { 
         error("ARPALM::readBinary - words binary chunk read error") ;
      }

      delete [] fbuf ;
   }

   fclose(fd) ;
}


void ARPALM::allocateEntries()
{
	// pre: 'order' has been initialised.
	// pre: 'n_ngrams' has been initialised.
#ifdef DEBUG
	if ( order <= 0 )
		error("ARPALM::allocateEntries - order <= 0\n") ;
	if ( n_ngrams == NULL )
		error("ARPALM::allocateEntries - n_ngrams is NULL\n") ;
	if ( entries != NULL )
		error("ARPALM::allocateEntries - entries already allocated\n") ;
#endif

	entries = new ARPALMEntry*[order] ;
	for ( int i=0 ; i<order ; i++ )
	{
		entries[i] = new ARPALMEntry[n_ngrams[i]] ;
		entries[i][0].log_prob = LOG_ZERO ;
		entries[i][0].log_bo = 0.0 ;
		entries[i][0].words = new int[n_ngrams[i] * (i+1)] ;
		for ( int j=1 ; j<n_ngrams[i] ; j++ )
		{
			entries[i][j].log_prob = LOG_ZERO ;
			entries[i][j].log_bo = 0.0 ;
			entries[i][j].words = entries[i][0].words + j*(i+1) ;
		}
	}
}


void ARPALM::calcUnkWords()
{
	// Calculates which (normal) words in the Vocabulary are "unknown" to the LM.
	// (ie. they are nowhere to be found in the ARPA file.)

	n_unk_words = 0 ;
	delete [] unk_words ;
   unk_words = NULL ;

   if ( unk_wrd != NULL )
   {
      unk_words = new int[vocab->nWords] ;

      for ( int i=0 ; i<vocab->nWords ; i++ )
      {
         // Don't count special words
         if ( vocab->isSpecial( i ) )
            continue ;

         if ( words_in_lm[i] == false )
            unk_words[n_unk_words++] = i ;
      }
   }
   else
   {
      // Check that all our vocab words are in the LM - because there is no <unk> word
      for ( int i=0 ; i<vocab->nWords ; i++ )
      {
         // Don't count special words
         if ( vocab->isSpecial( i ) || (i == vocab->silIndex) )
            continue ;

         if ( words_in_lm[i] == false )
            error("ARPALM::calcUnkWords - no unk word defined but %s not in LM",vocab->words[i]);
      }
   }
}

    
void ARPALM::printWarning()
{
	vocab->outputText() ;

	if ( n_unk_words > 0 )
	{
		printf("ARPALM - Vocabulary words that are not in the LM\n") ;
		printf("         (not including silence word)\n") ;
		printf("------------------------------------------------\n\n") ;

		for ( int i=0 ; i<n_unk_words ; i++ )
			printf( "%s\n" , vocab->words[unk_words[i]] ) ;

		printf("\n------------------------------------------------\n") ;
		printf("A total of %d words were not in LM\n" , n_unk_words ) ;
		printf("(these words will use <UNK> LM probs for decoding)\n") ;
		printf("------------------------------------------------\n") ;
	}
}


void ARPALM::outputText()
{
	printf("\\data\\\n") ;
	for ( int i=0 ; i<order ; i++ )
		printf("ngram %d=%d\n",i+1,n_ngrams[i]) ;

	for ( int i=0 ; i<order ; i++ )
	{
		printf("\n\\%d-grams: (%d)\n",i+1,n_ngrams[i]) ;
		for ( int j=0 ; j<n_ngrams[i] ; j++ )
		{
			printf("%.4f ",entries[i][j].log_prob);
			for ( int k=0 ; k<(i+1) ; k++ )
			{
				if ( entries[i][j].words[k] == unk_id )
					printf("<unk> ") ;
				else
					printf("%s ",vocab->getWord(entries[i][j].words[k])) ;
			}
			if ( i == (order-1) )
				printf("\n") ;
			else
				printf("%.4f\n",entries[i][j].log_bo) ;
		}
	}

	printf("\n\\end\\\n") ;
}


typedef enum
{
	ARPA_BEFORE_DATA=0 ,
	ARPA_IN_DATA ,
	ARPA_IN_NGRAMS ,
	ARPA_EXPECT_NGRAM_HDR ,
	ARPA_EXPECT_END
} ARPAReadState ;


void ARPALM::readARPA( const char *arpa_fname )
{
	// Reads an ARPA file into memory.
	char line[1000] , *curr_word ;
	FILE *arpa_fd ;
	ARPAReadState read_state ;
	int tmp_n_ngrams[30] , tmp_inds[30] , tmp1 , tmp2 , curr_n=0 , ngram_cnt=0 , i ;
	real tmp_prob , tmp_bo , ln_10 ;
	bool error_flag ;
    bool haveReportedSentStartIgnore=false , haveReportedSentEndIgnore=false ;

	if ( (arpa_fd = fopen( arpa_fname , "rb" )) == NULL )
		error("ARPALM::readARPA - error opening ARPA file") ;

	order = 0 ;
	n_ngrams = NULL ;
	entries = NULL ;
	read_state = ARPA_BEFORE_DATA ;
	ln_10 = (real)log(10.0) ;
	error_flag = false ;

	while ( fgets( line , 1000 , arpa_fd ) != NULL )
	{
		if ( (line[0]==' ') || (line[0]=='\r') || (line[0]=='\n') || 
			  (line[0]=='\t') || (line[0]=='#') )
		{
			// Blank line or comment - just ignore
			continue ;
		}

		if ( read_state == ARPA_BEFORE_DATA )
		{
			// Ignore everything until we see the \data\ line
			if ( line[0] == '\\' )
			{
				strtoupper( line ) ;
				if ( strstr( line , "\\DATA\\" ) != NULL )
            {
					read_state = ARPA_IN_DATA ;
            }
			}
		}
		else if ( read_state == ARPA_IN_DATA )
		{
			strtoupper( line ) ;
			if ( strstr( line , "NGRAM" ) != NULL )
			{
				// Read the ngram x=y lines
				if ( sscanf( line , "%*s %d=%d" , &tmp1 , &tmp2 ) != 2 )
					error("ARPALM::readARPA - error reading ngram x=y line\n") ;

				if ( tmp1 != (order+1) )
					error("ARPALM::readARPA - unexpected x in ngram x=y line\n") ;
				order = tmp1 ;
				tmp_n_ngrams[order-1] = tmp2 ;
			}
			else if ( strstr( line , "-GRAMS:" ) != NULL )
			{
				// We have reached the start of the N-gram entries.
				// Now that we know how many entries we have, we can allocate memory
				n_ngrams = new int[order] ;
				for ( i=0 ; i<order ; i++ )
					n_ngrams[i] = tmp_n_ngrams[i] ;

				allocateEntries() ;

				// check that we are entering the 1-gram section
				if ( (curr_n = (line[1]-0x30)) != 1 )
					error("ARPALM::readARPA - did not get \\1-GRAMS: after \\data\\\n") ;
				ngram_cnt = 0 ;

				// We are now in the reading-ngram-entries state
				read_state = ARPA_IN_NGRAMS ;
			}
			else
				error("ARPALM::readARPA - unexpected line while in ARPA_IN_DATA state\n") ;
		}
		else if ( read_state == ARPA_IN_NGRAMS )
		{
			// The only thing we should have on this line is an N-gram entry.
			//   (where N is equal to 'curr_n')

			// 1. Read the log10 prob at the stary of line and convert to natural log.
			//    If the log10 prob is < -90 then treat it as -inf
#ifdef USE_DOUBLE
			if ( sscanf( line , "%lf" , &tmp_prob ) != 1 )
				error("ARPALM::readARPA - ARPA_IN_NGRAMS - tmp_prob sscanf failed\n") ; 
#else
			if ( sscanf( line , "%f" , &tmp_prob ) != 1 )
				error("ARPALM::readARPA - ARPA_IN_NGRAMS - tmp_prob sscanf failed\n") ; 
#endif

			if ( tmp_prob < -90.0 )
				tmp_prob = LOG_ZERO ;
			else
				tmp_prob *= ln_10 ;

			// 2. Get past the prob field so we can read the words
			strtok( line , " \r\n\t" ) ;

			// 3. Read wd_1 , ... , wd_n (ie. all predecessor words of wd_n).
			//    There should be 'curr_n' words on this line.
			for ( i=0 ; i<curr_n ; i++ )
			{
				// Extract the next word from the line
				if ( (curr_word = strtok( NULL , " \n\r\t" )) == NULL )
               error("ARPALM::readARPA - strtok returned NULL unexpectedly") ;

				// determine the index of the word in the vocabulary
				tmp1 = vocab->getIndex( curr_word ) ;

				if ( tmp1 < 0 ) 
				{
					if ( (unk_wrd != NULL) && (strstr( curr_word , unk_wrd ) != NULL) )
						tmp1 = unk_id ;
					else
					{
						// The word is not in our vocab - don't add
						// the entry to our LM.  Don't worry about the
						// memory that is now left unused.
						n_ngrams[curr_n-1]-- ;
						error_flag = true ;
						break ;
					}
				}
				else if ( (tmp1 == vocab->sentStartIndex) && (i > 0) )
            {
               // Sentence start word was detected at a non-first position
               // Don't add this entry to our LM
               if ( haveReportedSentStartIgnore == false )
               {
                  LogFile::printf("ARPALM::readARPA - ignored LM entries with sentence start word "
                                  "at non-first position\n") ;
                  haveReportedSentStartIgnore = true ;
               }
               n_ngrams[curr_n-1]-- ;
               error_flag = true ;
               break ;
            }
            else if ( (tmp1 == vocab->sentEndIndex) && (i < (curr_n-1)) )
            {
               // Sentence end word was detected at a non-last position
               // Don't add this entry to our LM
               if ( haveReportedSentEndIgnore == false )
               {
                  LogFile::printf("ARPALM::readARPA - ignored LM entries with sentence end word at "
                                  "non-last position\n") ;
                  haveReportedSentEndIgnore = true ;
               }
               n_ngrams[curr_n-1]-- ;
               error_flag = true ;
               break ;
            }
            else
				{
					if ( tmp1 == vocab->silIndex )
						error("ARPALM::readARPA - silence word in LM") ;
					words_in_lm[tmp1] = true ;
				}

				tmp_inds[i] = tmp1 ;
			}

			if ( error_flag == false )
			{
				// 4. Read the log10 backoff value and convert to natural log.
				//    Unless, the 'curr_n' is equal to 'order'.
				if ( curr_n < order )
				{
               if ( (curr_word = strtok( NULL , " \n\r\t" )) == NULL )
               {
                  // If there is no backoff weight present - assume 0.0
                  tmp_bo = 0.0 ;
               }
               else
               {    
#ifdef USE_DOUBLE
                   if ( sscanf( curr_word , "%lf" , &tmp_bo ) != 1 )
                       error("ARPALM::readARPA - could not read back off weight\n") ;
#else
                   if ( sscanf( curr_word , "%f" , &tmp_bo ) != 1 )
                       error("ARPALM::readARPA - could not read back off weight\n") ;
#endif
               }
               
               if ( tmp_bo < -90.0 )
                   tmp_bo = LOG_ZERO ;
               else
                   tmp_bo *= ln_10 ;
				}
				else
					tmp_bo = LOG_ZERO ;
               
				// 5. Add all the stuff we've read in steps 1-4 to the current entry.
				entries[curr_n-1][ngram_cnt].log_prob = tmp_prob ;
				entries[curr_n-1][ngram_cnt].log_bo = tmp_bo ;
				for ( i=0 ; i<curr_n ; i++ )            
					entries[curr_n-1][ngram_cnt].words[i] = tmp_inds[i] ;

				ngram_cnt++ ;
			}

			error_flag = false ;
			if ( ngram_cnt == n_ngrams[curr_n-1] )
			{
				if ( curr_n == order )
            {
					read_state = ARPA_EXPECT_END ;
            }
				else
            {
					read_state = ARPA_EXPECT_NGRAM_HDR ;
            }
			}
		}
		else if ( read_state == ARPA_EXPECT_NGRAM_HDR )
		{
			// The only thing we expect here is an \x-GRAMS: entry
			if ( line[0] == '\\' )
			{
				strtoupper( line ) ;
				if ( strstr( line , "-GRAMS:" ) != NULL )
				{
					if ( (line[1]-0x30) != (curr_n+1) )
						error("ARPALM::readARPA - ARPA_EXPECT_NGRAM_HDR: unexpected x\n") ;
					curr_n = line[1]-0x30 ;

					// Maybe we are expecting 0 entries ??
					if ( n_ngrams[curr_n-1] > 0 )
					{
						ngram_cnt = 0 ;
						read_state = ARPA_IN_NGRAMS ;
					}
					else
					{   
						if ( curr_n == order )
							read_state = ARPA_EXPECT_END ;
					}
				}
				else
					error("ARPALM::readARPA - ARPA_EXPECT_NGRAM_HDR: did not get \\x-GRAMS:\n") ;
			}
			else
				error("ARPALM::readARPA - ARPA_EXPECT_NGRAM_HDR: did not get \\x-GRAMS:\n") ;
		}
		else if ( read_state == ARPA_EXPECT_END )
		{
			// The only thing we expect here is a \\end\\ entry
			if ( line[0] == '\\' )
			{
				strtoupper( line ) ;
				if ( strstr( line , "\\END\\" ) == NULL )
					error("ARPALM::readARPA - ARPA_EXPECT_END: did not get end\n") ;
			}
			else
				error("ARPALM::readARPA - ARPA_EXPECT_END: did not get end\n") ;
		}
		else
			error("ARPALM::readARPA - unexpected read_state\n") ;
	}

	fclose( arpa_fd ) ;

	/*        
	// Issue warnings if the number of expected entries for each n-gram did
	//   not match the actual number read from the file
	for ( i=0 ; i<order ; i++ )
	{
		if ( tmp_n_ngrams[i] != n_ngrams[i] )
		{
			warning("ARPALM::readARPA - warning - %d-gram entry count mismatch\n" , i+1 ) ;
			warning("\t%d expected != %d actual\n",tmp_n_ngrams[i],n_ngrams[i]) ;
		}
	}
	*/
}

// The longest n-gram that we can deal with
const int MAX_CONTEXT = 5;

/**
 * Calls a recursive routine that normalises the language model in
 * such a way that when it is represented as a FSM all transition
 * probabilities out of a particular state will add to unity.
 */
void ARPALM::Normalise()
{
    //printf("Normalising, order=%d\n", order);
    int context[MAX_CONTEXT];
    assert(MAX_CONTEXT > order-1);
    RecursiveNormalise(0, context, LOG_ZERO);
    //outputText();
    //exit(1);
}

static bool ContextEqual(int* i1, int* i2, int iLength)
{
    if (iLength == 0)
        return true;

    for (int i=0; i<iLength; i++)
        if (i1[i] != i2[i])
            return false;
    return true;
}

//#define SCALE_NGRAMS

real ARPALM::RecursiveNormalise(int iLevel, int* iContext, real iBackOff)
{
    assert(iContext);
    assert(iLevel < order);

    // Diagnostics
#if 0
    printf("Level %d, Context:", iLevel);
    if (iLevel == 0)
        printf(" NULL\n");
    else
    {
        for (int w=0; w<iLevel; w++)
            printf(" %s", vocab->getWord(iContext[w]));
        printf("\n");
    }
#endif

    // Add up exit probabilities from this context (log_e at this stage)
#ifdef SCALE_NGRAMS
    // Include the back-off in the summation
    double sum = iBackOff > LOG_ZERO ? exp(iBackOff) : 0.0;
#else
    // Summation is just the NGRAMS
    double sum = 0.0;
#endif
    for (int i=0; i<n_ngrams[iLevel]; i++)
    {
        ARPALMEntry& e = entries[iLevel][i];
        if (ContextEqual(iContext, e.words, iLevel))
        {
            // Recurse the deeper levels first
            if (iLevel+1 < order)
            {
                //printf("Recursing from level %d on %s\n",
                //       iLevel, vocab->getWord(e.words[iLevel]));
                iContext[iLevel] = e.words[iLevel];
                e.log_bo = RecursiveNormalise(iLevel+1, iContext, e.log_bo);
                //printf("Done, log-bo: %f\n", e.log_bo);
            }

            // And add up the probabilities for this context
            if (e.log_prob > LOG_ZERO)
                sum += exp(e.log_prob);
            //printf("%s: log-prob %f\n",
            //       vocab->getWord(e.words[iLevel]),
            //       e.log_prob);
        }
    }
    //printf("linear sum = %f\n", sum);

#ifdef SCALE_NGRAMS
    // Normalise
    real logSum = log(sum);
    for (int i=0; i<n_ngrams[iLevel]; i++)
    {
        ARPALMEntry& e = entries[iLevel][i];
        if (ContextEqual(iContext, e.words, iLevel))
            if (e.log_prob > LOG_ZERO)
                e.log_prob -= logSum;
    }

    // Return the normalised back-off
    return iBackOff - logSum;
#else
    assert(sum <= 1.0);
    if (iLevel == 0)
    {
        real logSum = log(sum);
        for (int i=0; i<n_ngrams[0]; i++)
        {
            ARPALMEntry& e = entries[0][i];
            if (e.log_prob > LOG_ZERO)
                e.log_prob -= logSum;
        }
        return 0.0;
    }

    // else back-off is just 1 minus ngram probability mass
    return log(1.0 - sum);
#endif
}


}

