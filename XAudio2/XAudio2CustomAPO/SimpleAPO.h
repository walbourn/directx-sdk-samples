//--------------------------------------------------------------------------------------
// SimpleAPO.h
//
// Example custom xAPO for XAudio2
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "SampleAPOBase.h"

#pragma warning(push)
#pragma warning(disable : 4481)
// VS 2010 considers 'override' to be a extension, but it's part of C++11 as of VS 2012

struct SimpleAPOParams
{
    float gain;
};

class __declspec( uuid("{5EB8D611-FF96-429d-8365-2DDF89A7C1CD}")) 
CSimpleAPO 
: public CSampleXAPOBase<CSimpleAPO, SimpleAPOParams>
{
public:
    CSimpleAPO();
    ~CSimpleAPO();

    void DoProcess( const SimpleAPOParams&, _Inout_updates_all_(cFrames * cChannels) FLOAT32* __restrict pData, UINT32 cFrames, UINT32 cChannels ) override;
};

#pragma warning(pop)