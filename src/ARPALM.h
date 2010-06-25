/*
 * Copyright 2002 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#ifndef ARPALM_INC
#define ARPALM_INC

#include "general.h"
#include "DecVocabulary.h"

/*
	Author:	Darren Moore (moore@idiap.ch)
	Date:		2002
	$Id: ARPALM.h,v 1.5 2005/08/26 01:16:34 moore Exp $
*/

namespace Juicer
{
    typedef struct
    {
        real        log_prob ;
        real        log_bo ;
        int         *words ;
    } ARPALMEntry ;


    /**
     * Representation of an ARPA N-gram language model.
     */
    class ARPALM
    {
    public:
        DecVocabulary *vocab ;

        int            order ;
        ARPALMEntry    **entries ;
        int            *n_ngrams ;

        char           *unk_wrd ;
        int            unk_id ;

        int            n_unk_words ;
        int            *unk_words ;

        ARPALM( DecVocabulary *vocab_ ) ;
        ARPALM( const char *arpa_fname , DecVocabulary *vocab_ ,
                const char *unk_wrd_ ) ;
        virtual ~ARPALM() ;

        void readARPA( const char *arpa_fname ) ;
        void calcUnkWords() ;
        void printWarning() ;
        void allocateEntries() ;
        void writeBinary( const char *fname ) ;
        void readBinary( const char *fname ) ;
        void outputText() ;

        void Normalise();

    private:
        real RecursiveNormalise(int iLevel, int* iContext, real iBackOff);
        bool           *words_in_lm ;
    };
}

#endif
