/*
 * Copyright 2009 by Idiap Research Institute, http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#ifndef DECODER_H
#define DECODER_H

#include "WFSTLattice.h"
#include "DecHypHistPool.h"

namespace Juicer
{
    /**
     * Interface for decoders
     */
    class IDecoder
    {
    public:
        virtual ~IDecoder () {}

        virtual bool modelLevelOutput() = 0;
        virtual WFSTLattice* getLattice() = 0;
        virtual void init() = 0;
        virtual void processFrame(
            float** inputVec, int currFrame_, int nFrames
        ) = 0;
        virtual DecHyp *finish() = 0;
    };
}

#endif /* DECODER_H */
