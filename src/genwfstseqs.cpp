/*
 * Copyright 2004 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#include "general.h"
#include "CmdLine.h"
#include "WFSTNetwork.h"


/*
	Author :		Darren Moore (moore@idiap.ch)
	Date :		26 Nov 2004
	$Id: genwfstseqs.cpp,v 1.3 2005/08/26 01:16:35 moore Exp $
*/


using namespace Juicer ;
using namespace Torch ;

// FST-related Parameters
char           *fsmFName=NULL ;
char           *inSymsFName=NULL ;
char           *outSymsFName=NULL ;

// Test-related parameters
int            nSeqs=0 ;


void processCmdLine( CmdLine *cmd , int argc , char *argv[] )
{
	// Transducer Parameters
	cmd->addText("\nTransducer Options:") ;
	cmd->addSCmdOption( "-fsmFName" , &fsmFName , "" ,
			"the FSM output filename" ) ;
	cmd->addSCmdOption( "-inSymsFName" , &inSymsFName , "" ,
			"the input symbols output filename" ) ;
	cmd->addSCmdOption( "-outSymsFName" , &outSymsFName , "" ,
			"the output symbols output filename" ) ;
         
   // Test parameters
	cmd->addICmdOption( "-nSeqs" , &nSeqs , 10 ,
			"number of test sequences to output." ) ;

	cmd->read( argc , argv ) ;
   
	// Basic parameter checks
	if ( strcmp( fsmFName , "" ) == 0 )
		error("genwfstseqs: fsmFName undefined") ;
	if ( strcmp( inSymsFName , "" ) == 0 )
		error("genwfstseqs: inSymsFName undefined") ;
	if ( strcmp( outSymsFName , "" ) == 0 )
		error("genwfstseqs: outSymsFName undefined") ;

   if ( nSeqs <= 0 )
      nSeqs = 1 ;
}


int main( int argc , char *argv[] )
{
   CmdLine cmd ;

   // process command line
   processCmdLine( &cmd , argc , argv ) ;

   WFSTNetwork net( fsmFName , inSymsFName , outSymsFName ) ;
//net.outputText() ;
   net.generateSequences( nSeqs ) ;

   return 0 ;
}



