//--------------------------------------------------------------------------------------
// File: audio.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "SDKmisc.h"
#include "WAVFileReader.h"
#include "audio.h"

using namespace DirectX;

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
AUDIO_STATE g_audioState;

//-----------------------------------------------------------------------------------------
// Initialize the audio by creating the XAudio2 device, mastering voice, etc.
//-----------------------------------------------------------------------------------------
HRESULT InitAudio()
{
    // Clear struct
    memset( &g_audioState, 0, sizeof( AUDIO_STATE ) );

    //
    // Initialize XAudio2
    //
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if( FAILED( hr ) )
        return hr;

#if ( _WIN32_WINNT < 0x0602 /*_WIN32_WINNT_WIN8*/)
    // Workaround for XAudio 2.7 known issue
#ifdef _DEBUG
    g_audioState.mXAudioDLL = LoadLibraryExW(L"XAudioD2_7.DLL", nullptr, 0x00000800 /* LOAD_LIBRARY_SEARCH_SYSTEM32 */);
#else
    g_audioState.mXAudioDLL = LoadLibraryExW(L"XAudio2_7.DLL", nullptr, 0x00000800 /* LOAD_LIBRARY_SEARCH_SYSTEM32 */);
#endif
    if (!g_audioState.mXAudioDLL)
        return HRESULT_FROM_WIN32( ERROR_NOT_FOUND );
#endif

    UINT32 flags = 0;
 #if (_WIN32_WINNT < 0x0602 /*_WIN32_WINNT_WIN8*/) && defined(_DEBUG)
    flags |= XAUDIO2_DEBUG_ENGINE;
 #endif
    hr = XAudio2Create( &g_audioState.pXAudio2, flags );
    if( FAILED( hr  ) )
        return hr;

#if (_WIN32_WINNT >= 0x0602 /*_WIN32_WINNT_WIN8*/) && defined(_DEBUG)
    // To see the trace output, you need to view ETW logs for this application:
    //    Go to Control Panel, Administrative Tools, Event Viewer.
    //    View->Show Analytic and Debug Logs.
    //    Applications and Services Logs / Microsoft / Windows / XAudio2. 
    //    Right click on Microsoft Windows XAudio2 debug logging, Properties, then Enable Logging, and hit OK 
    XAUDIO2_DEBUG_CONFIGURATION debug ={0};
    debug.TraceMask = XAUDIO2_LOG_ERRORS | XAUDIO2_LOG_WARNINGS;
    debug.BreakMask = XAUDIO2_LOG_ERRORS;
    g_audioState.pXAudio2->SetDebugConfiguration( &debug, 0 );
#endif

    //
    // Create a mastering voice
    //
    assert( g_audioState.pXAudio2 != 0 );
    if( FAILED( hr = g_audioState.pXAudio2->CreateMasteringVoice( &g_audioState.pMasteringVoice ) ) )
    {
        SAFE_RELEASE( g_audioState.pXAudio2 );
        return hr;
    }

    //
    // Done
    //
    g_audioState.bInitialized = true;

    return S_OK;
}


//-----------------------------------------------------------------------------
// Prepare a looping wave
//-----------------------------------------------------------------------------
HRESULT PrepareAudio( const LPWSTR wavname )
{
    if( !g_audioState.bInitialized )
        return E_FAIL;

    if( g_audioState.pSourceVoice )
    {
        g_audioState.pSourceVoice->Stop( 0 );
        g_audioState.pSourceVoice->DestroyVoice();
        g_audioState.pSourceVoice = 0;
    }

    SAFE_DELETE( g_audioState.pPipePre );
    SAFE_DELETE( g_audioState.pPipePost );

    //
    // Search for media
    //

    WCHAR strFilePath[ MAX_PATH ];
    WCHAR wavFilePath[ MAX_PATH ];

    wcscpy_s( wavFilePath, MAX_PATH, L"Media\\Wavs\\" );
    wcscat_s( wavFilePath, MAX_PATH, wavname );

    HRESULT hr;

    V_RETURN( DXUTFindDXSDKMediaFileCch( strFilePath, MAX_PATH, wavFilePath ) );

    //
    // Read in the wave file
    //
    const WAVEFORMATEX* pwfx;
    const uint8_t* sampleData;
    uint32_t waveSize;
    V_RETURN( LoadWAVAudioFromFile( strFilePath, g_audioState.waveData, &pwfx, &sampleData, &waveSize ) );

    //
    // Play the wave using a XAudio2SourceVoice
    //

    // Create the source voice
    assert( g_audioState.pXAudio2 != 0 );
    V_RETURN( g_audioState.pXAudio2->CreateSourceVoice( &g_audioState.pSourceVoice, pwfx, 0,
                                                        XAUDIO2_DEFAULT_FREQ_RATIO, nullptr, nullptr ) );

    // Create the custom APO instances
    CSimpleAPO* pSimpleAPO = nullptr;
    CSimpleAPO::CreateInstance( nullptr, 0, &pSimpleAPO );

    CMonitorAPO* pMonitorPre = nullptr;
    CMonitorAPO::CreateInstance( nullptr, 0, &pMonitorPre );

    CMonitorAPO* pMonitorPost = nullptr;
    CMonitorAPO::CreateInstance( nullptr, 0, &pMonitorPost );

    // Create the effect chain
    XAUDIO2_EFFECT_DESCRIPTOR apoDesc[3] = {0};
    apoDesc[0].InitialState = true;
    apoDesc[0].OutputChannels = 1;
    apoDesc[0].pEffect = static_cast<IXAPO*>(pMonitorPre);
    apoDesc[1].InitialState = true;
    apoDesc[1].OutputChannels = 1;
    apoDesc[1].pEffect = static_cast<IXAPO*>(pSimpleAPO);
    apoDesc[2].InitialState = true;
    apoDesc[2].OutputChannels = 1;
    apoDesc[2].pEffect = static_cast<IXAPO*>(pMonitorPost);

    XAUDIO2_EFFECT_CHAIN chain = {0};
    chain.EffectCount = sizeof(apoDesc) / sizeof(apoDesc[0]);
    chain.pEffectDescriptors = apoDesc;

    assert( g_audioState.pSourceVoice != 0 );
    V_RETURN( g_audioState.pSourceVoice->SetEffectChain( &chain ) );

    // Don't need to keep them now that XAudio2 has ownership
    pSimpleAPO->Release();
    pMonitorPre->Release();
    pMonitorPost->Release();

    // Submit the wave sample data using an XAUDIO2_BUFFER structure
    XAUDIO2_BUFFER buffer = {0};
    buffer.pAudioData = sampleData;
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    buffer.AudioBytes = waveSize;
    buffer.LoopCount = XAUDIO2_LOOP_INFINITE;

    V_RETURN( g_audioState.pSourceVoice->SubmitSourceBuffer( &buffer ) );

    V_RETURN( g_audioState.pSourceVoice->Start( 0 ) );

    // Set the initial effect params
    g_audioState.simpleParams.gain = 1.0f;
    V_RETURN( g_audioState.pSourceVoice->SetEffectParameters( 1, &g_audioState.simpleParams, sizeof( SimpleAPOParams ) ) );

    g_audioState.pPipePre = new MonitorAPOPipe;
    g_audioState.pPipePost = new MonitorAPOPipe;

    MonitorAPOParams mparams;
    mparams.pipe = g_audioState.pPipePre;
    V_RETURN( g_audioState.pSourceVoice->SetEffectParameters( 0, &mparams, sizeof(mparams) ) );

    mparams.pipe = g_audioState.pPipePost;
    V_RETURN( g_audioState.pSourceVoice->SetEffectParameters( 2, &mparams, sizeof(mparams) ) );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Set simple APO gain
//-----------------------------------------------------------------------------
VOID SetSimpleGain( float gain )
{
    if( !g_audioState.bInitialized )
        return;

    g_audioState.simpleParams.gain = gain;
    g_audioState.pSourceVoice->SetEffectParameters( 1, &g_audioState.simpleParams, sizeof( SimpleAPOParams ) );
}


//-----------------------------------------------------------------------------
// Pause audio playback
//-----------------------------------------------------------------------------
VOID PauseAudio( bool resume )
{
    if( !g_audioState.bInitialized )
        return;

    assert( g_audioState.pXAudio2 != 0 );

    if( resume )
        g_audioState.pXAudio2->StartEngine();
    else
        g_audioState.pXAudio2->StopEngine();
}



//-----------------------------------------------------------------------------
// Releases XAudio2
//-----------------------------------------------------------------------------
VOID CleanupAudio()
{
    if( !g_audioState.bInitialized )
        return;

    if( g_audioState.pSourceVoice )
    {
        g_audioState.pSourceVoice->DestroyVoice();
        g_audioState.pSourceVoice = nullptr;
    }

    if( g_audioState.pMasteringVoice )
    {
        g_audioState.pMasteringVoice->DestroyVoice();
        g_audioState.pMasteringVoice = nullptr;
    }

    if ( g_audioState.pXAudio2 )
        g_audioState.pXAudio2->StopEngine();

    SAFE_RELEASE( g_audioState.pXAudio2 );

    g_audioState.waveData.reset();

    SAFE_DELETE( g_audioState.pPipePre );
    SAFE_DELETE( g_audioState.pPipePost );

#if ( _WIN32_WINNT < 0x0602 /*_WIN32_WINNT_WIN8*/)
    if (g_audioState.mXAudioDLL)
    {
        FreeLibrary(g_audioState.mXAudioDLL);
        g_audioState.mXAudioDLL = nullptr;
    }
#endif

    CoUninitialize();

    g_audioState.bInitialized = false;
}
