//--------------------------------------------------------------------------------------
// File: audio.h
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "XAudio2Versions.h"
#ifndef USING_XAUDIO2_7_DIRECTX
#include <xaudio2fx.h>
#else
#include <C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Include\xaudio2fx.h>
#endif

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
    IXAudio2* pXAudio2;
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
HRESULT PrepareAudio( const LPWSTR wavname );
VOID SetSimpleGain( float gain );
VOID PauseAudio( bool resume );
VOID CleanupAudio();
