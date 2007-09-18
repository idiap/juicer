/*
 * Copyright 2004 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#include "general.h"
#include "CmdLine.h"
#include "DecLexInfo.h"
#include "WFSTLexGen.h"


/*
	Author :		Darren Moore (moore@idiap.ch)
	Date :		11 Nov 2004
	$Id: lexgen.cpp,v 1.9 2005/08/26 01:16:35 moore Exp $
*/


using namespace Juicer ;
using namespace Torch ;

// Lexicon Parameters
char 		      *lexFName=NULL ;
char		      *sentStartWord=NULL ;
char		      *sentEndWord=NULL ;

// Context-Independent Phoneme Parameters
char           *monoListFName ;
char           *silMonophone ;
char           *pauseMonophone ;

// FST-related Parameters
char           *fsmFName=NULL ;
char           *inSymsFName=NULL ;
char           *outSymsFName=NULL ;
bool           addPronunsWithStartSil=false ;
bool           addPronunsWithEndSil=false ;
bool           addPronunsWithStartPause=false ;
bool           addPronunsWithEndPause=false ;
real           pauseTeeTransProb=0.0 ;
bool           addPhiLoop=false ;
bool           outputAuxPhones=false ;

void processCmdLine( CmdLine *cmd , int argc , char *argv[] )
{
   // Lexicon Parameters
   cmd->addText("\nLexicon Options:") ;
   cmd->addSCmdOption( "-lexFName" , &lexFName , "" , 
         "the dictionary filename - monophone transcriptions" ) ;
   cmd->addSCmdOption( "-sentStartWord" , &sentStartWord , "" ,
         "the name of the dictionary word that will start every sentence" ) ;
   cmd->addSCmdOption( "-sentEndWord" , &sentEndWord , "" ,
         "the name of the dictionary word that will end every sentence" ) ;

	// Monophone options
	cmd->addText("\nContext-Independent Phone Options:") ;
	cmd->addSCmdOption( "-monoListFName" , &monoListFName , "" ,
			"the file containing the list of monophones" ) ;
	cmd->addSCmdOption( "-silMonophone" , &silMonophone , "" ,
			"the name of the silence phoneme" ) ;
	cmd->addSCmdOption( "-pauseMonophone" , &pauseMonophone , "" ,
			"the name of the pause phoneme" ) ;
   cmd->addRCmdOption( "-pauseTeeTransProb" , &pauseTeeTransProb , 0.0 ,
         "the initial to final state transition probability of the pause model, "
         "if addPronunsWithEndPause is defined, then this prob becomes the "
         "pronun. prob for the base pronun (without pause at end), and 1-(this value) "
         "becomes the pronun. prob for the pronun with pause at the end." ) ;

	// Transducer Parameters
	cmd->addText("\nTransducer Options:") ;
	cmd->addSCmdOption( "-fsmFName" , &fsmFName , "" ,
			"the FSM output filename" ) ;
	cmd->addSCmdOption( "-inSymsFName" , &inSymsFName , "" ,
			"the input symbols output filename" ) ;
	cmd->addSCmdOption( "-outSymsFName" , &outSymsFName , "" ,
                        "the output symbols output filename" ) ;
   cmd->addBCmdOption(
       "-addPronunsWithStartSil" , &addPronunsWithStartSil , false ,
       "indicates that an additional pronunciation with silence monophone "
       "at the start is added for each word."
   ) ;
    cmd->addBCmdOption(
        "-addPronunsWithEndSil" , &addPronunsWithEndSil , false ,
        "indicates that an additional pronunciation with silence monophone "
        "at end is added for each word."
    ) ;
   cmd->addBCmdOption(
       "-addPronunsWithStartPause" , &addPronunsWithStartPause , false ,
       "indicates that an additional pronunciation with pause monophone "
       "at the start is added for each word."
   ) ;
   cmd->addBCmdOption(
       "-addPronunsWithEndPause" , &addPronunsWithEndPause , false ,
       "indicates that an additional pronunciation with pause monophone "
       "at end is added for each word."
   ) ;
   cmd->addBCmdOption(
       "-addPhiLoop" , &addPhiLoop , false ,
       "a phi loop should be added on the initial state to match phi labels"
       "in the grammar.  Phi labels make the grammar deterministic"
   ) ;
   cmd->addBCmdOption(
       "-outputAuxPhones" , &outputAuxPhones , false ,
       "indicates that auxiliary phones should be added to pronunciations"
       "in the lexicon in order to disambiguate distinct words with"
       "identical pronunciations"
   ) ;

	cmd->read( argc , argv ) ;
   
	// Basic parameter checks
	if ( strcmp( lexFName , "" ) == 0 )
		error("lexgen: lexFName undefined") ;
	if ( strcmp( monoListFName , "" ) == 0 )
		error("lexgen: monoListFName undefined") ;
	if ( strcmp( fsmFName , "" ) == 0 )
		error("lexgen: fsmFName undefined") ;
	if ( strcmp( inSymsFName , "" ) == 0 )
		error("lexgen: inSymsFName undefined") ;
	if ( strcmp( outSymsFName , "" ) == 0 )
		error("lexgen: outSymsFName undefined") ;

   if ( (pauseTeeTransProb > 0.0) && (addPronunsWithEndPause == false) )
   {
      warning("lexgen: pauseTeeTransProb ignored because addPronunsWithEndPause not defined") ;
      pauseTeeTransProb = 0.0 ;
   }
}


int main( int argc , char *argv[] )
{
   CmdLine cmd ;

   // process command line
   processCmdLine( &cmd , argc , argv ) ;

   // create lexInfo
   DecLexInfo *lexInfo = new DecLexInfo(
       monoListFName , silMonophone , pauseMonophone ,
       lexFName , sentStartWord , sentEndWord , NULL
   ) ;
   //lexInfo->outputText() ;                                       

   // Normalise the pronunciations for each word to add to one
   lexInfo->normalisePronuns();

   // create lexGen
   if ( pauseTeeTransProb > 0.0 )
      pauseTeeTransProb = log( pauseTeeTransProb ) ;
   else
      pauseTeeTransProb = LOG_ZERO ;

   WFSTLexGen *lexGen = new WFSTLexGen(
       lexInfo ,
       addPronunsWithEndSil , addPronunsWithEndPause ,
       addPronunsWithStartSil , addPronunsWithStartPause, 
       pauseTeeTransProb, outputAuxPhones
   ) ;
   //lexGen->outputText() ;

   // write lex FSM
   lexGen->writeFSM(
       fsmFName , inSymsFName , outSymsFName , outputAuxPhones , addPhiLoop
   ) ;
   
   // cleanup and exit
   delete lexGen ;
   delete lexInfo ;

   return 0 ;
}
