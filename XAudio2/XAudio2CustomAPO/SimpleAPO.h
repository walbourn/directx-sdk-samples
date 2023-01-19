//--------------------------------------------------------------------------------------
// SimpleAPO.h
//
// Example custom xAPO for XAudio2
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License (MIT).
//--------------------------------------------------------------------------------------
#pragma once

#include "SampleAPOBase.h"

struct SimpleAPOParams
{
    float gain;
};

class __declspec(uuid("5EB8D611-FF96-429d-8365-2DDF89A7C1CD")) CSimpleAPO
: public CSampleXAPOBase<CSimpleAPO, SimpleAPOParams>
{
public:
    CSimpleAPO();
    ~CSimpleAPO() override;

    void DoProcess( const SimpleAPOParams&, _Inout_updates_all_(cFrames * cChannels) FLOAT32* __restrict pData, UINT32 cFrames, UINT32 cChannels ) override;
};
