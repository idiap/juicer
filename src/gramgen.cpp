/*
 * Copyright 2004 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#include "general.h"
#include "CmdLine.h"
#include "DecVocabulary.h"
#include "ARPALM.h"
#include "WFSTGramGen.h"
#include "WFSTNetwork.h"

#include "MonophoneLookup.h"
#include "LogFile.h"

/*
	Author :		Darren Moore (moore@idiap.ch)
	Date :		11 Nov 2004
	$Id: gramgen.cpp,v 1.8 2005/08/26 01:16:35 moore Exp $
*/


using namespace Juicer ;
using namespace Torch ;

// Lexicon Parameters
char 		      *lexFName=NULL ;
char		      *sentStartWord=NULL ;
char		      *sentEndWord=NULL ;
//char		      *silWord=NULL ;

// Language Model Parameters
char           *lmFName=NULL ;
real           lmScaleFactor=1.0 ;
real           wordInsPen=0.0 ;
char           *unkWord=NULL ;
bool           addSilenceArcs=false ;
bool           phiBackoff=false ;

// FST-related Parameters
char           *gramTypeStr=NULL ;
WFSTGramType   gramType=WFST_GRAM_TYPE_INVALID ;
char           *fsmFName=NULL ;
char           *inSymsFName=NULL ;
char           *outSymsFName=NULL ;

// Test parameters
bool           genTestSeqs=false ;


void processCmdLine( CmdLine *cmd , int argc , char *argv[] )
{
   // Lexicon Parameters
   cmd->addText("\nLexicon Options:") ;
   cmd->addSCmdOption( "-lexFName" , &lexFName , "" , 
         "the dictionary filename" ) ;
   cmd->addSCmdOption( "-sentStartWord" , &sentStartWord , "" ,
         "the name of the dictionary word that will start every sentence" ) ;
   cmd->addSCmdOption( "-sentEndWord" , &sentEndWord , "" ,
         "the name of the dictionary word that will end every sentence" ) ;
//   cmd->addSCmdOption( "-silWord" , &silWord , "" ,
//         "the name of the silence dictionary word" ) ;

	// Language Model Parameters
	cmd->addText("\nLanguage Model Options:") ;
	cmd->addSCmdOption( "-lmFName" , &lmFName , "" ,
			"the LM file (in ARPA LM format or BBN word-pair format)" ) ;
	cmd->addRCmdOption( "-lmScaleFactor" , &lmScaleFactor , 1.0 ,
			"the factor by which log LM probs are scaled during decoding" ) ;
	cmd->addRCmdOption( "-wordInsPen" , &wordInsPen , 0.0 ,
			"word insertion penalty" ) ;
	cmd->addSCmdOption( "-unkWord" , &unkWord , "" ,
			"the name of the unknown word in the LM" ) ;
	cmd->addBCmdOption( "-addSilenceArcs" , &addSilenceArcs , false ,
			"adds silence self-loop arcs to most states (only when -gramType == ngram)" ) ;
	cmd->addBCmdOption( "-phiBackoff" , &phiBackoff , false ,
						"labels back-off transitions with aux. label #phi instead of <eps>" ) ;

	// Transducer Parameters
	cmd->addText("\nTransducer Options:") ;
	cmd->addSCmdOption( "-gramType" , &gramTypeStr , "" ,
		   "the type of grammar to generate "
         "(silwordloopsil,ngram,wordpair)" ) ;
	cmd->addSCmdOption( "-fsmFName" , &fsmFName , "" ,
			"the FSM output filename" ) ;
	cmd->addSCmdOption( "-inSymsFName" , &inSymsFName , "" ,
			"the input symbols output filename" ) ;
	cmd->addSCmdOption( "-outSymsFName" , &outSymsFName , "" ,
			"the output symbols output filename" ) ;

   // Test parameters
	cmd->addBCmdOption( "-genTestSeqs" , &genTestSeqs , false ,
			"generates some test sequences from the grammar transducer just constructed." ) ;

	cmd->read( argc , argv ) ;
   
	// Interpret the grammar type
	if ( strcmp( gramTypeStr , "" ) == 0 )
		error("gramgen: gramType undefined") ;
	if ( strcmp( gramTypeStr , "wordloop" ) == 0 )
	{
      gramType = WFST_GRAM_TYPE_WORDLOOP ;
	}
	else if ( strcmp( gramTypeStr , "silwordloopsil" ) == 0 )
	{
      gramType = WFST_GRAM_TYPE_SIL_WORDLOOP_SIL ;
	}
	else if ( strcmp( gramTypeStr , "ngram" ) == 0 )
	{
      gramType = WFST_GRAM_TYPE_NGRAM ;
	}
	else if ( strcmp( gramTypeStr , "wordpair" ) == 0 )
	{
      gramType = WFST_GRAM_TYPE_WORDPAIR ;
	}
	else
		error("-gramType %s : unrecognised type" , gramTypeStr ) ;

	// Basic parameter checks
	if ( strcmp( lexFName , "" ) == 0 )
		error("gramgen: lexFName undefined") ;
	if ( strcmp( fsmFName , "" ) == 0 )
		error("gramgen: fsmFName undefined") ;
	if ( strcmp( inSymsFName , "" ) == 0 )
		error("gramgen: inSymsFName undefined") ;
	if ( strcmp( outSymsFName , "" ) == 0 )
		error("gramgen: outSymsFName undefined") ;

	// Some 2 parameter dependencies
   if ( ((gramType == WFST_GRAM_TYPE_NGRAM) || (gramType == WFST_GRAM_TYPE_WORDPAIR)) && 
        (strcmp( lmFName , "" ) == 0) )
   {
		error("gramgen: gramType is N-Gram or Word-pair but no LM file specified") ;
   }
}


int main( int argc , char *argv[] )
{
   CmdLine cmd ;

   LogFile::open("stderr") ;

   // process command line
   processCmdLine( &cmd , argc , argv ) ;

   // create vocabulary
   DecVocabulary *vocab = new DecVocabulary(
       lexFName , '\0' , sentStartWord , sentEndWord
   ) ;

   // create grammar generator
   WFSTGramGen *gramGen = new WFSTGramGen(
       vocab , gramType , lmScaleFactor , wordInsPen , lmFName , unkWord
   ) ;

   // use grammar generator to create output FSM file (+ symbol files).
   gramGen->writeFSM(
	   fsmFName , inSymsFName , outSymsFName, addSilenceArcs, phiBackoff
   ) ;

   if ( genTestSeqs )
   {
      WFSTNetwork net( fsmFName , inSymsFName , outSymsFName ) ;
      net.generateSequences() ;
   }

   delete gramGen ;
   delete vocab ;

   LogFile::close() ;

   return 0 ;
} 

