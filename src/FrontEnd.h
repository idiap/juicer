/*
 * Copyright 2007 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */
#ifndef FRONTEND_H
#define FRONTEND_H

#include "HTKSource.h"
#include "LNASource.h"
#include "ArraySink.h"

enum SourceFormat
{
    SOURCE_HTK,
    SOURCE_LNA8,
    SOURCE_LNA16
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
        default:
            assert(0);
        }
        mSink = new ArraySink<float>(source);
    }

    ~FrontEnd()
    {
        delete mSink;
    }

    bool GetArray(float*& ioData, int iIndex)
    {
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
