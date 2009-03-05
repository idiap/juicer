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
                source = mFactory.CreateSource(mSource);
                source = mFactory.CreateFrontend(source);
                break;
            }
            default:
                assert(0);
            }
            mSink = new ArraySink<float>(source);
            mSpeakerIDSink = 0;
            printf("iInputVecSize %d GetArraySize %d\n", iInputVecSize, mSink->GetArraySize());
            assert(iInputVecSize == mSink->GetArraySize());

#ifdef HAVE_HTKLIB
            // Set flag in frontend to true if the environment variable ASRFactory_Source=HTKLib  
            isHTKLibSource = false;
            if ( iFormat == FRONTEND_FACTORY )
            {
                const char* ret = getenv("ASRFactory_Source");
                printf("ASRFactory_Source = %s\n" , ret);
                isHTKLibSource = ( strcmp(ret,"HTKLib") == 0 );
            }
#endif
        }

        ~FrontEnd()
        {
            delete mSink;
            if (mSpeakerIDSink)
                delete mSpeakerIDSink;
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

        TimeType TimeStamp(IndexType iIndex)
        {
            return mSink->TimeStamp(iIndex);
        }

        const char* GetSpeakerID(int iIndex)
        {
            //printf("GetSpeakerID for index %d\n", iIndex);
            if (!mSpeakerIDSink)
            {
                Plugin<float>* p = mFactory.GetSpeakerIDSource();
                if (p)
                {
                    mSpeakerIDSink = new ArraySink<float>(p);
                    mSpeakerIDSink->Reset();
                }
                else
                    return "xxx";
            }
            assert(mSpeakerIDSink);
            float* data;
            int get = mSpeakerIDSink->GetArray(data, iIndex);
            if (get)
                return (char*)data;
            return "yyy";
        }

#ifdef HAVE_HTKLIB
        bool isHTKLibSource;
#endif

    private:
        Tracter::Source* mSource;
        ArraySink<float>* mSink;
        ArraySink<float>* mSpeakerIDSink;
        ASRFactory mFactory;
    };
}

#endif /* FRONTEND_H */
