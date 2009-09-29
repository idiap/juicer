/*
 * Copyright 2007,2008 by IDIAP Research Institute
 *                        http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */
#ifndef FRONTEND_H
#define FRONTEND_H

#include <ASRFactory.h>
#include <HTKSource.h>
#include <LNASource.h>
#include <FrameSink.h>
#include <ALSASource.h>


#include "config.h"

#ifdef HAVE_HTKLIB
# include "HModels.h"
#endif

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
            Component<float>* source;
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
            mSink = new FrameSink<float>(source);
            mSpeakerIDSink = 0;
            printf("iInputVecSize %d FrameSize %d\n", iInputVecSize, mSink->Frame().size);
            assert(iInputVecSize == mSink->Frame().size);

#ifdef HAVE_HTKLIB
            // Set flag in frontend to true if the environment variable ASRFactory_Source=HTKLib  
            isHTKLibSource = false;
            if ( iFormat == FRONTEND_FACTORY )
            {
                const char* ret = getenv("ASRFactory_Source");
                printf("ASRFactory_Source = %s\n" , ret);
                isHTKLibSource = ( strcmp(ret,"HTKLib") == 0 );
            }
            useHModels=false;
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
            //return mSink->Read(ioData, iIndex);
            ioData = (float*)mSink->Read(iIndex);
            return ioData;
        }

        void SetSource(
            char* iFileName,
            IndexType iBeginFrame = -1, IndexType iEndFrame = -1
        )
        {
            assert(iFileName);
            TimeType b = iBeginFrame >= 0 ? mSink->TimeStamp(iBeginFrame) : -1;
            TimeType e = iEndFrame   >= 0 ? mSink->TimeStamp(iEndFrame)   : -1;
            //printf("Begin frame %ld -> time %lld\n", iBeginFrame, b);
            //printf("End   frame %ld -> time %lld\n", iEndFrame, e);
            mSource->Open(iFileName, b, e);
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
                Component<float>* p = mFactory.GetSpeakerIDSource();
                if (p)
                {
                    mSpeakerIDSink = new FrameSink<float>(p);
                    mSpeakerIDSink->Reset();
                }
                else
                    return "xxx";
            }
            assert(mSpeakerIDSink);
            const float* data = mSpeakerIDSink->Read(iIndex);
            if (data)
                return (char*)data;
            return "yyy";
        }

#ifdef HAVE_HTKLIB
        bool      isHTKLibSource;
        HModels  *HTKLIBModels;
        bool      useHModels;
#endif

    private:
        Tracter::ISource* mSource;
        FrameSink<float>* mSink;
        FrameSink<float>* mSpeakerIDSink;
        ASRFactory mFactory;
    };
}

#endif /* FRONTEND_H */
