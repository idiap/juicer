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

enum SourceFormat
{
    SOURCE_HTK,
    SOURCE_LNA8,
    SOURCE_LNA16,
    SOURCE_FACTORY
};

class FrontEnd
{
public:
    FrontEnd(int iInputVecSize, SourceFormat iFormat)
    {
        Plugin<float>* source;
        switch (iFormat)
        {
        case SOURCE_HTK:
        {
            HTKSource* tmp = new HTKSource();
            source = tmp;
            mSource = tmp;
            break;
        }
        case SOURCE_LNA8:
        {
            LNASource* tmp = new LNASource();
            source = tmp;
            mSource = tmp;
            break;
        }
        case SOURCE_FACTORY:
        {
            Tracter::ASRFactory factory;
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
    Source* mSource;
    ArraySink<float>* mSink;
};

#endif /* FRONTEND_H */
