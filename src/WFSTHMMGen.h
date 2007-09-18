/*
 * Copyright 2007 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#ifndef WFSTHMMGEN_H
#define WFSTHMMGEN_H

#include "Models.h"

namespace Juicer
{
    /**
     * Converts an HMM to a transducer
     */
    class WFSTHMMGen
    {
    public:

        WFSTHMMGen(const char* htkModelsFName);

        void Write(
            const char* fsmFName,
            const char* inSymbolsFName, 
            const char* outSymbolsFName
        );

    private:

        void writeFSM(FILE* iFD);
        int writeFSMModel(FILE* iFD, HTKHMM& iHMM, int iState);
        int findTransMat(const char* iName);
        int findHMMState(const char* iName);
        void writeInSymbols(FILE* iFD);
        void writeOutSymbols(FILE* iFD);
    };
}

#endif // WFSTHMMGEN_H
