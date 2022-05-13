//--------------------------------------------------------------------------------------
// File: audio.h
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License (MIT).
//--------------------------------------------------------------------------------------

#include "XAudio2Versions.h"

#include <wrl/client.h>

#include "SimpleAPO.h"
#include "MonitorAPO.h"

//-----------------------------------------------------------------------------
// Global defines
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Struct to hold audio game state
//-----------------------------------------------------------------------------
struct AUDIO_STATE
{
    bool bInitialized;

    // XAudio2
#ifdef USING_XAUDIO2_7_DIRECTX
    HMODULE mXAudioDLL;
#endif
    Microsoft::WRL::ComPtr<IXAudio2> pXAudio2;
    IXAudio2MasteringVoice* pMasteringVoice;
    IXAudio2SourceVoice* pSourceVoice;
    std::unique_ptr<uint8_t[]> waveData;

    // APOs
    SimpleAPOParams simpleParams;
    MonitorAPOPipe *pPipePre;
    MonitorAPOPipe *pPipePost;
};


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
extern AUDIO_STATE  g_audioState;


//--------------------------------------------------------------------------------------
// External functions
//--------------------------------------------------------------------------------------
HRESULT InitAudio();
HRESULT PrepareAudio( const LPCWSTR wavname );
VOID SetSimpleGain( float gain );
VOID PauseAudio( bool resume );
VOID CleanupAudio();
