//--------------------------------------------------------------------------------------
// MonitorAPO.h
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

#ifndef MONITOR_APO_PIPE_LEN
#define MONITOR_APO_PIPE_LEN 14
#endif

#include <DXUTLockFreePipe.h>
typedef DXUTLockFreePipe<MONITOR_APO_PIPE_LEN> MonitorAPOPipe;

struct MonitorAPOParams
{
    MonitorAPOPipe *pipe;
};

class __declspec( uuid("{A4945B8A-EB14-4c96-8067-DF726B528091}")) 
CMonitorAPO
: public CSampleXAPOBase<CMonitorAPO, MonitorAPOParams>
{
public:
    CMonitorAPO();
    ~CMonitorAPO();

    void DoProcess( const MonitorAPOParams&, _Inout_updates_all_(cFrames * cChannels) FLOAT32* __restrict pData, UINT32 cFrames, UINT32 cChannels ) override;
};

#pragma warning(pop)