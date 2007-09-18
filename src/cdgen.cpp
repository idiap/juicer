/*
 * Copyright 2004 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#include "general.h"
#include "CmdLine.h"
#include "WFSTCDGen.h"
#include "WFSTNetwork.h"


/*
	Author :		Darren Moore (moore@idiap.ch)
	Date :		18 Nov 2004
	$Id: cdgen.cpp,v 1.6 2005/10/20 01:20:03 moore Exp $
*/


using namespace Juicer ;
using namespace Torch ;


// Context-Independent Phoneme Parameters
char           *monoListFName=NULL ;
char           *silMonophone=NULL ;
char           *pauseMonophone=NULL ;

// Context-Dependent Phoneme Parameters
char           *tiedListFName=NULL ;
char           *htkModelsFName=NULL ;
char           *cdSepChars=NULL ;
char           *cdType_s=NULL ;
WFSTCDType     cdType=WFST_CD_TYPE_INVALID ;
bool           ndixt=false ;

// Hybrid stuff
char           *priorsFName=NULL ;
int            statesPerModel=0 ;

// FST-related Parameters
char           *fsmFName=NULL ;
char           *inSymsFName=NULL ;
char           *outSymsFName=NULL ;
char           *lexInSymsFName=NULL ;

// Test-related parameters
bool           genTestSeqs=false ;


void processCmdLine( CmdLine *cmd , int argc , char *argv[] )
{
	// CI Phone Parameters
	cmd->addText("\nContext-Independent Phone Options:") ;
	cmd->addSCmdOption( "-monoListFName" , &monoListFName , "" ,
			"the file containing the list of monophones" ) ;
	cmd->addSCmdOption( "-silMonophone" , &silMonophone , "" ,
			"the name of the silence phoneme" ) ;
	cmd->addSCmdOption( "-pauseMonophone" , &pauseMonophone , "" ,
			"the name of the pause phoneme" ) ;

	// CD Phone Parameters
	cmd->addText("\nContext-Dependent Phone Options:") ;
	cmd->addSCmdOption( "-tiedListFName" , &tiedListFName , "" ,
			"the file containing the CD phone tied list" ) ;
	cmd->addSCmdOption( "-htkModelsFName" , &htkModelsFName , "" ,
			"the file containing the acoustic models in HTK MMF format" ) ;
   cmd->addSCmdOption( "-cdSepChars" , &cdSepChars , "" ,
         "the characters that separate monophones in CD phone names (in order)" ) ;
   cmd->addSCmdOption( "-cdType" , &cdType_s , "mono" ,
         "the type of context-dependency for output WFST (mono,monoann,xwrdtri)" ) ;
	cmd->addBCmdOption( "-ndixt", &ndixt, false,
			"generates a x-word triphone transducer with a non-deterministic inverse. "
         "ignored unless cdType is xwrdtri." );
  
   // Hybrid HMM/ANN Parameters
   cmd->addText("\nHybrid HMM/ANN related options:") ;
	cmd->addSCmdOption( "-priorsFName" , &priorsFName , "" ,
			"the file containing the phone priors" ) ;
   cmd->addICmdOption( "-statesPerModel" , &statesPerModel , 0 ,
         "the number of states in each HMM used in the hybrid system" ) ;
   
	// Transducer Parameters
	cmd->addText("\nTransducer Options:") ;
	cmd->addSCmdOption( "-fsmFName" , &fsmFName , "" ,
			"the FSM output filename" ) ;
	cmd->addSCmdOption( "-inSymsFName" , &inSymsFName , "" ,
			"the input symbols output filename" ) ;
	cmd->addSCmdOption( "-outSymsFName" , &outSymsFName , "" ,
			"the output symbols output filename" ) ;
	cmd->addSCmdOption( "-lexInSymsFName" , &lexInSymsFName , "" ,
			"the (pre-existing) input symbols filename for the lexicon transducer" ) ;
         
   // Test parameters
	cmd->addBCmdOption( "-genTestSeqs" , &genTestSeqs , false ,
			"generates some test sequences from the CD transducer just constructed." ) ;

	cmd->read( argc , argv ) ;
   
   // Interpet the cdType_s
   if ( strcmp( cdType_s , "mono" ) == 0 )
      cdType = WFST_CD_TYPE_MONOPHONE ;
   else if ( strcmp( cdType_s , "monoann" ) == 0 )
      cdType = WFST_CD_TYPE_MONOPHONE_ANN ;
   else if ( strcmp( cdType_s , "xwrdtri" ) == 0 ) {
       //if ( ndixt ) {
       // cdType = WFST_CD_TYPE_XWORD_TRIPHONE_NDI;
       //} else {
       cdType = WFST_CD_TYPE_XWORD_TRIPHONE ;
       //}
   } else
      error("cdgen: invalid cdType") ;

	// Basic parameter checks
   if ( (htkModelsFName != NULL) && (htkModelsFName[0] != '\0') )
   {
      // HMM/GMM system
      if ( (cdType != WFST_CD_TYPE_MONOPHONE) && 
           (cdType != WFST_CD_TYPE_XWORD_TRIPHONE) )
      {
         error("cdgen: cdType invalid for HMM/GMM system") ;
      }
      if ( strcmp( tiedListFName , "" ) == 0 )
         error("cdgen: tiedListFName undefined") ;
   }
   else if ( (priorsFName != NULL) && (priorsFName[0] != '\0') )
   {
      // Hybrid HMM/ANN system
      if ( cdType != WFST_CD_TYPE_MONOPHONE_ANN )
      {
         error("cdgen: cdType invalid for HMM/ANN system") ;
      }
      if ( statesPerModel <= 2 )
         error("cdgen: statesPerModel <= 2") ;
   }
   else
   {
      error("cdgen: both htkModelsFName and priorsFName are undefined") ;
   }
   
   if ( strcmp( monoListFName , "" ) == 0 )
      error("cdgen: monoListFName undefined") ;
	if ( strcmp( fsmFName , "" ) == 0 )
		error("cdgen: fsmFName undefined") ;
	if ( strcmp( inSymsFName , "" ) == 0 )
		error("cdgen: inSymsFName undefined") ;
	if ( strcmp( outSymsFName , "" ) == 0 )
		error("cdgen: outSymsFName undefined") ;
}


int main( int argc , char *argv[] )
{
   CmdLine cmd ;

   // process command line
   processCmdLine( &cmd , argc , argv ) ;

   // create cdGen
   WFSTCDGen *cdGen = new WFSTCDGen( cdType , htkModelsFName , monoListFName , silMonophone ,
                                     pauseMonophone , tiedListFName , cdSepChars , 
                                     priorsFName , statesPerModel ) ;

   // write lex FSM
   cdGen->writeFSM( fsmFName , inSymsFName , outSymsFName , lexInSymsFName ) ;
   if ( genTestSeqs )
   {
      WFSTNetwork net( fsmFName , inSymsFName , outSymsFName ) ;
      net.generateSequences() ;
   }

   // cleanup and exit
   delete cdGen ;

   return 0 ;
}


