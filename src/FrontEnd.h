 /*
 * Copyright 2007,2008 by IDIAP Research Institute
 *                        http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */
#ifndef FRONTEND_H
#define FRONTEND_H

#include "ASRFactory.h"
#include "HTKSource.h"
#include "LNASource.h"
#include "ArraySink.h"
#include "ALSASource.h"

#include "config.h"

using namespace Tracter;

namespace Juicer
{
    /**
     * Format of front-end.
     * All front-ends are based on Tracter.
     */
    enum FrontEndFormat
    {
        FRONTEND_HTK,    ///< HTK file
        FRONTEND_LNA,    ///< LNA file
        FRONTEND_FACTORY ///< Tracter::ASRFactory
    };

    class FrontEnd
    {
    public:
        FrontEnd(int iInputVecSize, FrontEndFormat iFormat)
        {
            Plugin<float>* source;
            switch (iFormat)
            {
            case FRONTEND_HTK:
            {
                HTKSource* tmp = new HTKSource();
                source = tmp;
                mSource = tmp;
                break;
            }
            case FRONTEND_LNA:
            {
                LNASource* tmp = new LNASource();
                source = tmp;
                mSource = tmp;
                break;
            }
            case FRONTEND_FACTORY:
            {
                ASRFactory factory;
                source = factory.CreateSource(mSource);
                source = factory.CreateFrontend(source);
                break;
            }
            default:
                assert(0);
            }
            mSink = new ArraySink<float>(source);
            assert(iInputVecSize == mSink->GetArraySize());
        }

        ~FrontEnd()
        {
            delete mSink;
        }

        bool GetArray(float*& ioData, int iIndex)
        {
            if (iIndex == 0)
                mSink->Reset();
            return mSink->GetArray(ioData, iIndex);
        }

        void SetSource(char* iFileName)
        {
            assert(iFileName);
            mSource->Open(iFileName);
            mSink->Reset();
        }

    private:
        Tracter::Source* mSource;
        ArraySink<float>* mSink;
    };
}

#endif /* FRONTEND_H */
