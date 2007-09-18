/*
 * Copyright 2007 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#include "general.h"
#include "CmdLine.h"
#include "WFSTHMMGen.h"

using namespace Juicer;
using namespace Torch;

// MMF Parameters
char* htkModelsFName = 0;

// FST-related Parameters
char* fsmFName = 0;
char* inSymsFName = 0;
char* outSymsFName = 0;


/**
 * Process command line options
 */
void processCmdLine(CmdLine& cmd, int argc, char *argv[])
{
    // MMF options
	cmd.addText("\nModel Options:");
	cmd.addSCmdOption(
        "-htkModelsFName", &htkModelsFName, "",
        "the file containing the acoustic models in HTK MMF format"
    );

	// Transducer Parameters
	cmd.addText("\nTransducer Options:");
	cmd.addSCmdOption(
        "-fsmFName", &fsmFName, "",
        "the FSM output filename"
    );
	cmd.addSCmdOption(
        "-inSymsFName", &inSymsFName, "",
        "the input symbols output filename"
    );
	cmd.addSCmdOption(
        "-outSymsFName", &outSymsFName, "",
        "the output symbols output filename"
    );

    // The actual processing
	cmd.read(argc, argv);
}


int main(int argc, char *argv[])
{
    // process command line
    CmdLine cmd;
    processCmdLine(cmd, argc, argv);

    WFSTHMMGen hmmGen(htkModelsFName);
    hmmGen.Write(fsmFName, inSymsFName, outSymsFName);

    return 0;
}
