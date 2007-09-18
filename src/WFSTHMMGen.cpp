/*
 * Copyright 2007 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#include "math.h"
#include "assert.h"
#include "WFSTHMMGen.h"
#include "WFSTGeneral.h"
#include "general.h"
#include "htkparse.h"

using namespace Torch;

namespace Juicer
{
    /**
     * Constructor.
     * Uses the bison MMF parser to load a given HTK model file.
     */
    WFSTHMMGen::WFSTHMMGen(
        const char* htkModelsFName ///< HTK MMF to load
    )
    {
        assert(htkModelsFName);

        /*
         * This loads an HTK definition accessible by these:
         *
         * extern HTKDef htk_def;
         * extern int htkparse( void * );
         * extern void cleanHTKDef();
         */
        FILE *fd = fopen(htkModelsFName, "rb");
        if (!fd)
            error("WFSTHMMGen::WFSTHMMGen - error opening htkModelsFName");
        if (htkparse( (void *)fd ))
            error("WFSTHMMGen::WFSTHMMGen - htkparse failed");
        fclose(fd);
        printf("Got %d HMMs\n", htk_def.n_hmms);

        // Check that all states are shared
        for (int i=0; i<htk_def.n_sh_states; i++ )
        {
            HTKHMMState& state = *htk_def.sh_states[i];
            if (!state.sh_name)
                error("WFSTHMMGen::writeFSM"
                      " - htk_def.sh_states[i]->sh_name undefined");
        }
        printf("Got %d shared states\n", htk_def.n_sh_states);
    }

    /**
     * Writes the currently loaded MMF as a transducer
     */
    void WFSTHMMGen::Write(
        const char* fsmFName,       ///< Output FSM file name
        const char* inSymbolsFName, ///< Output FSM input symbols file name
        const char* outSymbolsFName ///< Output FSM output symbols file name
    )
    {
        assert(fsmFName);
        assert(inSymbolsFName);
        assert(outSymbolsFName);

        // FSM file
        FILE* fsmFD = fopen(fsmFName, "wb");
        if (!fsmFD)
            error("WFSTHMMGen::writeFSM"
                  " - error opening FSM file: %s", fsmFName);
        writeFSM(fsmFD);
        fclose(fsmFD);

        // inSyms file
        FILE* inFD = fopen(inSymbolsFName, "wb");
        if (!inFD)
            error("WFSTHMMGen::writeFSM"
                  " - error opening input symbols file: %s", inSymbolsFName);
        writeInSymbols(inFD);
        fclose(inFD);

        // outSyms file
        FILE* outFD = fopen(outSymbolsFName, "wb");
        if (!outFD)
            error("WFSTHMMGen::writeFSM"
                  " - error opening output symbols file: %s", outSymbolsFName);
        writeOutSymbols(outFD);
        fclose(outFD);
    }

    /**
     * Write FSM to a file descriptor
     */
    void WFSTHMMGen::writeFSM(FILE* iFD)
    {
        assert(iFD);

        /*
         * Keep a running count of states used
         *
         * 0 - Initial state
         * 1 - Final state
         *
         * The counter holds the last state used rather than the next
         * free one.
         */
        int fsmState = 1;

        for (int h=0; h<htk_def.n_hmms; h++)
        {
            HTKHMM& hmm = *htk_def.hmms[h];
            if (!hmm.name)
                error("WFSTHMMGen::writeFSM"
                      " - htk_def.hmms[h]->name undefined");

            // All HMMs start at the initial state.  fsmState becomes
            // the entry state of the model.
            writeFSMTransition(iFD, 0, ++fsmState, WFST_EPSILON, h+1);

            // Write the states
            fsmState = writeFSMModel(iFD, hmm, fsmState);

            // Add an arc from the last HMM state to the final transducer state
            writeFSMTransition(
                iFD, fsmState, 1,  WFST_EPSILON, WFST_EPSILON
            );
        }

        // Final state
        writeFSMFinalState(iFD, 1);
    }


    /**
     * Write HMM as FSM links to a file descriptor
     */
    int WFSTHMMGen::writeFSMModel(FILE* iFD, HTKHMM& iHMM, int iState)
    {
        assert(iFD);

        // The matrix could be shared, in which case we have to find it
        assert(iHMM.transmat);
        HTKTransMat& trans = (
            iHMM.transmat->sh_name
            ? *htk_def.sh_transmats[findTransMat(iHMM.transmat->sh_name)]
            : *iHMM.transmat
        );

        // Loop over the matrix to find transitions to write
        for (int i=0; i<iHMM.n_states; i++)
        {
            for (int j=0; j<iHMM.n_states; j++)
            //for (int j=i+1; j<(i==(iHMM.n_states-1) ? iHMM.n_states : i+2); j++)
            {
                // Loop again if there is no transition
                if (trans.transp[i][j] <= 0.0)
                    continue;

                // else: There is a transition from state i to
                // state j.  The arc output label is the target
                // state index plus 1
                int label = WFST_EPSILON;
                if ((j != 0) && (j != iHMM.n_states-1))
                    label = findHMMState(iHMM.emit_states[j-1]->sh_name) + 1;
                writeFSMTransition(
                    iFD, iState+i, iState+j,
                    label, WFST_EPSILON, -logf(trans.transp[i][j])
                );
            }
        }

        // Step the counter by the number of states
        return iState + iHMM.n_states - 1;
    }

    /**
     * Find a transition matrix with a given string
     */
    int WFSTHMMGen::findTransMat(const char* iName)
    {
        assert(iName);

        // Linear search
        for (int i=0; i<htk_def.n_sh_transmats; i++)
        {
            HTKTransMat& trans = *htk_def.sh_transmats[i];
            if (strcmp(iName, trans.sh_name) == 0)
                return i;
        }

        // If the loop terminates, it failed
        error("WFSTHMMGen::findTransMat: failed to find %s\n", iName);
        return -1;
    }

    /**
     * Find a state with a given string
     */
    int WFSTHMMGen::findHMMState(const char* iName)
    {
        assert(iName);

        // Linear search
        for (int i=0; i<htk_def.n_sh_states; i++)
        {
            HTKHMMState& state = *htk_def.sh_states[i];
            if (strcmp(iName, state.sh_name) == 0)
                return i;
        }

        // If the loop terminates, it failed
        error("WFSTHMMGen::findHMMState: failed to find %s\n", iName);
        return -1;
    }

    /**
     * Write out the input symbols (the state names)
     */
    void WFSTHMMGen::writeInSymbols(FILE* iFD)
    {
        assert(iFD);

        writeFSMSymbol(iFD, WFST_EPSILON_STR, 0);
        for (int i=0; i<htk_def.n_sh_states; i++)
        {
            HTKHMMState& state = *htk_def.sh_states[i];
            writeFSMSymbol(iFD, state.sh_name, i+1);
        }
    }

    /**
     * Write out the output symbols (the model names)
     */
    void WFSTHMMGen::writeOutSymbols(FILE* iFD)
    {
        assert(iFD);

        writeFSMSymbol(iFD, WFST_EPSILON_STR, 0);
        for (int i=0; i<htk_def.n_hmms; i++)
        {
            HTKHMM& hmm = *htk_def.hmms[i];
            writeFSMSymbol(iFD, hmm.name, i+1);
        }
    }
}
