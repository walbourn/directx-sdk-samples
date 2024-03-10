//--------------------------------------------------------------------------------------
// MonitorAPO.h
//
// Example custom xAPO for XAudio2
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License (MIT).
//--------------------------------------------------------------------------------------
#pragma once

#include "SampleAPOBase.h"

#ifndef MONITOR_APO_PIPE_LEN
#define MONITOR_APO_PIPE_LEN 14
#endif

#include <DXUTLockFreePipe.h>
typedef DXUTLockFreePipe<MONITOR_APO_PIPE_LEN> MonitorAPOPipe;

struct MonitorAPOParams
{
    MonitorAPOPipe *pipe;
};

class __declspec(uuid("A4945B8A-EB14-4c96-8067-DF726B528091")) CMonitorAPO
: public CSampleXAPOBase<CMonitorAPO, MonitorAPOParams>
{
public:
    CMonitorAPO();
    ~CMonitorAPO() override;

    void DoProcess( const MonitorAPOParams&, _Inout_updates_all_(cFrames * cChannels) FLOAT32* __restrict pData, UINT32 cFrames, UINT32 cChannels ) override;
};
