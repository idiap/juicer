/*
 * Copyright 2002 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#ifndef DECLEXINFO_INC
#define DECLEXINFO_INC

#include "general.h"
#include "DecVocabulary.h"
#include "DecPhoneInfo.h"
#include "MonophoneLookup.h"


/*
	Author:	Darren Moore (moore@idiap.ch)
	Date:		2002
	$Id: DecLexInfo.h,v 1.11 2005/08/26 01:16:34 moore Exp $
*/


namespace Juicer {


typedef struct
{
	int      nPhones ;
	int      *phones ;
	int      *monophones ;
	real     logPrior ;
	int      vocabIndex ;
} DecLexInfoEntry ;


typedef struct
{
	int nPronuns ;
	int *pronuns ;
} VocToLexMapEntry ;


/**
 * This class stores information about how phonemes are assembled into
 * pronunciations. For each pronunciation, a list of indices into a
 * PhoneInfo instance is stored, along with a prior and a index into a
 * DecVocabulary instance. Information is also stored to map
 * DecVocabulary entries to one or more pronunciations.
 *
 * @author Darren Moore (moore@idiap.ch)
 */
class DecLexInfo
{
public:
	int 						nEntries ;
	DecLexInfoEntry		*entries ;
	int						sentStartIndex ;
	int						sentEndIndex ;
	int						silIndex ;
	DecPhoneInfo			*phoneInfo ;
	DecVocabulary			*vocabulary ;
	VocToLexMapEntry		*vocabToLexMap ;
	CDType					cdType ;
	bool						rescoreMode ;

	DecLexInfo(
        const char *monophoneListFName , char *phonesFName , char *silPhone , 
        char *pause_phone , CDType phonesCDType ,
        char *lexFName , char *sentStartWord=NULL , char *sentEndWord=NULL , 
        char *silWord=NULL , CDType cdType_=CD_TYPE_MONOPHONE ,
        bool rescoreMode_=false
    ) ;

    DecLexInfo(
        const char *monophoneListFName , const char *silPhone ,
        const char *pausePhone , const char *lexFName ,
        const char *sentStartWord , const char *sentEndWord , 
        const char *silWord
    ) ;
               
	DecLexInfo() ;
	virtual ~DecLexInfo() ;

	void monoToCDPhones( DecLexInfoEntry *entry ) ;

	void writeBinary( const char *binFName ) ;
	void readBinary( const char *binFName ) ;
	void outputText() ;
    void writeWFST( const char *wfstFName , const char *inSymbolsFName , 
                    const char *outSymbolsFName ) ;
    MonophoneLookup *getMonoLookup() ;
    void normalisePronuns();

private:
	bool						fromBinFile ;
    bool                 monoOnlyMode ;
    MonophoneLookup      *monoLookup ;
   
	void readFromASCII(
        const char *monophoneListFName , char *phonesFName , char *silPhone , 
        char *pausePhone , CDType phonesCDType , char *lexFName , 
        char *sentStartWord=NULL , char *sentEndWord=NULL , 
        char *silWord=NULL
    ) ;

	void initLexInfoEntry( DecLexInfoEntry *entry ) ;

	void writeBinaryVTLM( FILE *fd ) ;
	void readBinaryVTLM( FILE *fd ) ;
	void writeBinaryEntries( FILE *fd ) ;
	void readBinaryEntries( FILE *fd ) ;
	
};


}

#endif
