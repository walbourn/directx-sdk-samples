//--------------------------------------------------------------------------------------
// SimpleAPO.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "SimpleAPO.h"



//--------------------------------------------------------------------------------------
// Name: CSimpleAPO::CSimpleAPO
// Desc: Constructor
//--------------------------------------------------------------------------------------
CSimpleAPO::CSimpleAPO()
: CSampleXAPOBase<CSimpleAPO, SimpleAPOParams>()
{
}

//--------------------------------------------------------------------------------------
// Name: CSimpleAPO::~CSimpleAPO
// Desc: Destructor
//--------------------------------------------------------------------------------------
CSimpleAPO::~CSimpleAPO()
{
}


//--------------------------------------------------------------------------------------
// Name: CSimpleAPO::DoProcess
// Desc: Process each sample by multiplying it with the gain parameter
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
void CSimpleAPO::DoProcess( const SimpleAPOParams& params, FLOAT32* __restrict pData, UINT32 cFrames, UINT32 cChannels )
{
    //
    // This simple sample shows how to write an audio effect in straight C++.
    // For better performance, use vector operations (VMX or SSE).
    //
    float gain = params.gain;
    for( UINT32 i = 0; i < cFrames * cChannels; ++i )
    {
        pData[i] *= gain;
    }
}

