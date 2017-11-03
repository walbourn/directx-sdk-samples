//--------------------------------------------------------------------------------------
// MonitorAPO.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "MonitorAPO.h"


//--------------------------------------------------------------------------------------
// Name: CMonitorAPO::CMonitorAPO
// Desc: Constructor
//--------------------------------------------------------------------------------------
CMonitorAPO::CMonitorAPO()
: CSampleXAPOBase<CMonitorAPO, MonitorAPOParams>()
{
}

//--------------------------------------------------------------------------------------
// Name: CMonitorAPO::~CMonitorAPO
// Desc: Destructor
//--------------------------------------------------------------------------------------
CMonitorAPO::~CMonitorAPO()
{
}


//--------------------------------------------------------------------------------------
// Name: CMonitorAPO::DoProcess
// Desc: Process by copying off a portion of the samples to another thread via a LF pipe
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
void CMonitorAPO::DoProcess( const MonitorAPOParams& params, FLOAT32* __restrict pData, UINT32 cFrames, UINT32 cChannels )
{
    if( cFrames )
    {
        MonitorAPOPipe* pipe = params.pipe;
        if( pipe )
            pipe->Write( pData, cFrames * cChannels * (WaveFormat().wBitsPerSample >> 3) );
    }
}