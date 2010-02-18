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
#include <SpeakerIDSocketSource.h>

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

    class FrontEnd : public Tracter::Object
    {
    public:
        FrontEnd(int iInputVecSize, FrontEndFormat iFormat)
        {
            mObjectName = "FrontEnd";

            /* The feature acquisition chain */
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
            printf("iInputVecSize %d FrameSize %d\n", iInputVecSize, mSink->Frame().size);
            assert(iInputVecSize == mSink->Frame().size);

            /* The speaker ID chain */
            mSpeakerIDSource = 0;
            mSpeakerIDSink = 0;
            const char* sidHost = GetEnv("SpeakerIDHost", (char*)0);
            if (sidHost)
            {
                mSpeakerIDSource = new SpeakerIDSocketSource();
                mSpeakerIDSink = new FrameSink<float>(mSpeakerIDSource);
                mSpeakerIDSource->Open(sidHost);
                mSpeakerIDSink->Reset();
            }
        }

        virtual ~FrontEnd() throw ()
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
                return "xxx";
            assert(mSpeakerIDSink);
            const float* data = mSpeakerIDSink->Read(iIndex);
            if (data)
                return (char*)data;
            return "yyy";
        }

    private:
        ASRFactory mFactory;
        ISource* mSource;
        FrameSink<float>* mSink;
        SpeakerIDSocketSource* mSpeakerIDSource;
        FrameSink<float>* mSpeakerIDSink;
    };
}

#endif /* FRONTEND_H */
