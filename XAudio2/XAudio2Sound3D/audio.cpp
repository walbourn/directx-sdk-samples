//--------------------------------------------------------------------------------------
// File: audio.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTsettingsdlg.h"
#include "SDKmisc.h"
#include "WAVFileReader.h"
#include "audio.h"

using namespace DirectX;

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
AUDIO_STATE g_audioState;

// Specify sound cone to add directionality to listener for artistic effect:
// Emitters behind the listener are defined here to be more attenuated,
// have a lower LPF cutoff frequency,
// yet have a slightly higher reverb send level.
static const X3DAUDIO_CONE Listener_DirectionalCone = { X3DAUDIO_PI*5.0f/6.0f, X3DAUDIO_PI*11.0f/6.0f, 1.0f, 0.75f, 0.0f, 0.25f, 0.708f, 1.0f };

// Specify LFE level distance curve such that it rolls off much sooner than
// all non-LFE channels, making use of the subwoofer more dramatic.
static const X3DAUDIO_DISTANCE_CURVE_POINT Emitter_LFE_CurvePoints[3] = { 0.0f, 1.0f, 0.25f, 0.0f, 1.0f, 0.0f };
static const X3DAUDIO_DISTANCE_CURVE       Emitter_LFE_Curve          = { (X3DAUDIO_DISTANCE_CURVE_POINT*)&Emitter_LFE_CurvePoints[0], 3 };

// Specify reverb send level distance curve such that reverb send increases
// slightly with distance before rolling off to silence.
// With the direct channels being increasingly attenuated with distance,
// this has the effect of increasing the reverb-to-direct sound ratio,
// reinforcing the perception of distance.
static const X3DAUDIO_DISTANCE_CURVE_POINT Emitter_Reverb_CurvePoints[3] = { 0.0f, 0.5f, 0.75f, 1.0f, 1.0f, 0.0f };
static const X3DAUDIO_DISTANCE_CURVE       Emitter_Reverb_Curve          = { (X3DAUDIO_DISTANCE_CURVE_POINT*)&Emitter_Reverb_CurvePoints[0], 3 };

// Must match order of g_PRESET_NAMES
XAUDIO2FX_REVERB_I3DL2_PARAMETERS g_PRESET_PARAMS[ NUM_PRESETS ] =
{
    XAUDIO2FX_I3DL2_PRESET_FOREST,
    XAUDIO2FX_I3DL2_PRESET_DEFAULT,
    XAUDIO2FX_I3DL2_PRESET_GENERIC,
    XAUDIO2FX_I3DL2_PRESET_PADDEDCELL,
    XAUDIO2FX_I3DL2_PRESET_ROOM,
    XAUDIO2FX_I3DL2_PRESET_BATHROOM,
    XAUDIO2FX_I3DL2_PRESET_LIVINGROOM,
    XAUDIO2FX_I3DL2_PRESET_STONEROOM,
    XAUDIO2FX_I3DL2_PRESET_AUDITORIUM,
    XAUDIO2FX_I3DL2_PRESET_CONCERTHALL,
    XAUDIO2FX_I3DL2_PRESET_CAVE,
    XAUDIO2FX_I3DL2_PRESET_ARENA,
    XAUDIO2FX_I3DL2_PRESET_HANGAR,
    XAUDIO2FX_I3DL2_PRESET_CARPETEDHALLWAY,
    XAUDIO2FX_I3DL2_PRESET_HALLWAY,
    XAUDIO2FX_I3DL2_PRESET_STONECORRIDOR,
    XAUDIO2FX_I3DL2_PRESET_ALLEY,
    XAUDIO2FX_I3DL2_PRESET_CITY,
    XAUDIO2FX_I3DL2_PRESET_MOUNTAINS,
    XAUDIO2FX_I3DL2_PRESET_QUARRY,
    XAUDIO2FX_I3DL2_PRESET_PLAIN,
    XAUDIO2FX_I3DL2_PRESET_PARKINGLOT,
    XAUDIO2FX_I3DL2_PRESET_SEWERPIPE,
    XAUDIO2FX_I3DL2_PRESET_UNDERWATER,
    XAUDIO2FX_I3DL2_PRESET_SMALLROOM,
    XAUDIO2FX_I3DL2_PRESET_MEDIUMROOM,
    XAUDIO2FX_I3DL2_PRESET_LARGEROOM,
    XAUDIO2FX_I3DL2_PRESET_MEDIUMHALL,
    XAUDIO2FX_I3DL2_PRESET_LARGEHALL,
    XAUDIO2FX_I3DL2_PRESET_PLATE,
};


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
    HRESULT hr = CoInitializeEx( nullptr, COINIT_MULTITHREADED );
    if (FAILED(hr))
        return hr;

#if ( _WIN32_WINNT < 0x0602 /*_WIN32_WINNT_WIN8*/)
    // Workaround for XAudio 2.7 known issue
#ifdef _DEBUG
    g_audioState.mXAudioDLL = LoadLibraryExW(L"XAudioD2_7.DLL", nullptr, 0x00000800 /* LOAD_LIBRARY_SEARCH_SYSTEM32 */);
#else
    g_audioState.mXAudioDLL = LoadLibraryExW(L"XAudio2_7.DLL", nullptr, 0x00000800 /* LOAD_LIBRARY_SEARCH_SYSTEM32 */);
#endif
    if (!g_audioState.mXAudioDLL)
        return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
#endif

    UINT32 flags = 0;
 #if (_WIN32_WINNT < 0x0602 /*_WIN32_WINNT_WIN8*/) && defined(_DEBUG)
    flags |= XAUDIO2_DEBUG_ENGINE;
 #endif
    hr = XAudio2Create( &g_audioState.pXAudio2, flags );
    if( FAILED( hr ) )
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
    if( FAILED( hr = g_audioState.pXAudio2->CreateMasteringVoice( &g_audioState.pMasteringVoice ) ) )
    {
        SAFE_RELEASE( g_audioState.pXAudio2 );
        return hr;
    }

    // Check device details to make sure it's within our sample supported parameters
    DWORD dwChannelMask;
    UINT32 nSampleRate;

#if (_WIN32_WINNT >= 0x0602 /*_WIN32_WINNT_WIN8*/)

    XAUDIO2_VOICE_DETAILS details;
    g_audioState.pMasteringVoice->GetVoiceDetails( &details );

    if( details.InputChannels > OUTPUTCHANNELS )
    {
        SAFE_RELEASE( g_audioState.pXAudio2 );
        return E_FAIL;
    }

    if ( FAILED( hr = g_audioState.pMasteringVoice->GetChannelMask( &dwChannelMask ) ) )
    {
        SAFE_RELEASE( g_audioState.pXAudio2 );
        return E_FAIL;
    }

    nSampleRate = details.InputSampleRate;
    g_audioState.nChannels = details.InputChannels;
    g_audioState.dwChannelMask  = dwChannelMask;

#else

    XAUDIO2_DEVICE_DETAILS details;
    if( FAILED( hr = g_audioState.pXAudio2->GetDeviceDetails( 0, &details ) ) )
    {
        SAFE_RELEASE( g_audioState.pXAudio2 );
        return hr;
    }

    if( details.OutputFormat.Format.nChannels > OUTPUTCHANNELS )
    {
        SAFE_RELEASE( g_audioState.pXAudio2 );
        return E_FAIL;
    }

    nSampleRate = details.OutputFormat.Format.nSamplesPerSec;
    dwChannelMask = g_audioState.dwChannelMask = details.OutputFormat.dwChannelMask;
    g_audioState.nChannels = details.OutputFormat.Format.nChannels;

#endif

    //
    // Create reverb effect
    //
    UINT32 rflags = 0;
 #if (_WIN32_WINNT < 0x0602 /*_WIN32_WINNT_WIN8*/) && defined(_DEBUG)
    rflags |= XAUDIO2FX_DEBUG;
 #endif
    if( FAILED( hr = XAudio2CreateReverb( &g_audioState.pReverbEffect, rflags ) ) )
    {
        SAFE_RELEASE( g_audioState.pXAudio2 );
        return hr;
    }

    //
    // Create a submix voice
    //

    // Performance tip: you need not run global FX with the sample number
    // of channels as the final mix.  For example, this sample runs
    // the reverb in mono mode, thus reducing CPU overhead.
    XAUDIO2_EFFECT_DESCRIPTOR effects[] = { { g_audioState.pReverbEffect, TRUE, 1 } };
    XAUDIO2_EFFECT_CHAIN effectChain = { 1, effects };

    if( FAILED( hr = g_audioState.pXAudio2->CreateSubmixVoice( &g_audioState.pSubmixVoice, 1,
                                                               nSampleRate, 0, 0,
                                                               nullptr, &effectChain ) ) )
    {
        SAFE_RELEASE( g_audioState.pXAudio2 );
        SAFE_RELEASE( g_audioState.pReverbEffect );
        return hr;
    }

    // Set default FX params
    XAUDIO2FX_REVERB_PARAMETERS native;
    ReverbConvertI3DL2ToNative( &g_PRESET_PARAMS[0], &native );
    g_audioState.pSubmixVoice->SetEffectParameters( 0, &native, sizeof( native ) );

    //
    // Initialize X3DAudio
    //  Speaker geometry configuration on the final mix, specifies assignment of channels
    //  to speaker positions, defined as per WAVEFORMATEXTENSIBLE.dwChannelMask
    //
    //  SpeedOfSound - speed of sound in user-defined world units/second, used
    //  only for doppler calculations, it must be >= FLT_MIN
    //
    const float SPEEDOFSOUND = X3DAUDIO_SPEED_OF_SOUND;

    X3DAudioInitialize( dwChannelMask, SPEEDOFSOUND, g_audioState.x3DInstance );

    g_audioState.vListenerPos.x =
    g_audioState.vListenerPos.y = 
    g_audioState.vListenerPos.z =
    g_audioState.vEmitterPos.x = 
    g_audioState.vEmitterPos.y = 0.f;

    g_audioState.vEmitterPos.z = float( ZMAX );

    g_audioState.fListenerAngle = 0;
    g_audioState.fUseListenerCone = TRUE;
    g_audioState.fUseInnerRadius = TRUE;
    g_audioState.fUseRedirectToLFE = ((dwChannelMask & SPEAKER_LOW_FREQUENCY) != 0);

    //
    // Setup 3D audio structs
    //
    g_audioState.listener.Position.x = g_audioState.vListenerPos.x;
    g_audioState.listener.Position.y = g_audioState.vListenerPos.y;
    g_audioState.listener.Position.z = g_audioState.vListenerPos.z;

    g_audioState.listener.OrientFront.x =
    g_audioState.listener.OrientFront.y = 
    g_audioState.listener.OrientTop.x = 
    g_audioState.listener.OrientTop.z = 0.f;

    g_audioState.listener.OrientFront.z = 
    g_audioState.listener.OrientTop.y = 1.f;

    g_audioState.listener.pCone = (X3DAUDIO_CONE*)&Listener_DirectionalCone;

    g_audioState.emitter.pCone = &g_audioState.emitterCone;
    g_audioState.emitter.pCone->InnerAngle = 0.0f;
    // Setting the inner cone angles to X3DAUDIO_2PI and
    // outer cone other than 0 causes
    // the emitter to act like a point emitter using the
    // INNER cone settings only.
    g_audioState.emitter.pCone->OuterAngle = 0.0f;
    // Setting the outer cone angles to zero causes
    // the emitter to act like a point emitter using the
    // OUTER cone settings only.
    g_audioState.emitter.pCone->InnerVolume = 0.0f;
    g_audioState.emitter.pCone->OuterVolume = 1.0f;
    g_audioState.emitter.pCone->InnerLPF = 0.0f;
    g_audioState.emitter.pCone->OuterLPF = 1.0f;
    g_audioState.emitter.pCone->InnerReverb = 0.0f;
    g_audioState.emitter.pCone->OuterReverb = 1.0f;

    g_audioState.emitter.Position.x = g_audioState.vEmitterPos.x;
    g_audioState.emitter.Position.y = g_audioState.vEmitterPos.y;
    g_audioState.emitter.Position.z = g_audioState.vEmitterPos.z;

    g_audioState.emitter.OrientFront.x =
    g_audioState.emitter.OrientFront.y = 
    g_audioState.emitter.OrientTop.x = 
    g_audioState.emitter.OrientTop.z = 0.f;

    g_audioState.emitter.OrientFront.z = 
    g_audioState.emitter.OrientTop.y = 1.f;

    g_audioState.emitter.ChannelCount = INPUTCHANNELS;
    g_audioState.emitter.ChannelRadius = 1.0f;
    g_audioState.emitter.pChannelAzimuths = g_audioState.emitterAzimuths;

    // Use of Inner radius allows for smoother transitions as
    // a sound travels directly through, above, or below the listener.
    // It also may be used to give elevation cues.
    g_audioState.emitter.InnerRadius = 2.0f;
    g_audioState.emitter.InnerRadiusAngle = X3DAUDIO_PI/4.0f;;

    g_audioState.emitter.pVolumeCurve = (X3DAUDIO_DISTANCE_CURVE*)&X3DAudioDefault_LinearCurve;
    g_audioState.emitter.pLFECurve    = (X3DAUDIO_DISTANCE_CURVE*)&Emitter_LFE_Curve;
    g_audioState.emitter.pLPFDirectCurve = nullptr; // use default curve
    g_audioState.emitter.pLPFReverbCurve = nullptr; // use default curve
    g_audioState.emitter.pReverbCurve    = (X3DAUDIO_DISTANCE_CURVE*)&Emitter_Reverb_Curve;
    g_audioState.emitter.CurveDistanceScaler = 14.0f;
    g_audioState.emitter.DopplerScaler = 1.0f;

    g_audioState.dspSettings.SrcChannelCount = INPUTCHANNELS;
    g_audioState.dspSettings.DstChannelCount = g_audioState.nChannels;
    g_audioState.dspSettings.pMatrixCoefficients = g_audioState.matrixCoefficients;

    //
    // Done
    //
    g_audioState.bInitialized = true;

    return S_OK;
}


//-----------------------------------------------------------------------------
// Prepare a looping wave
//-----------------------------------------------------------------------------
HRESULT PrepareAudio( _In_z_ const LPWSTR wavname )
{
    if( !g_audioState.bInitialized )
        return E_FAIL;

    if( g_audioState.pSourceVoice )
    {
        g_audioState.pSourceVoice->Stop( 0 );
        g_audioState.pSourceVoice->DestroyVoice();
        g_audioState.pSourceVoice = 0;
    }

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
    // Play the wave using a source voice that sends to both the submix and mastering voices
    //
    XAUDIO2_SEND_DESCRIPTOR sendDescriptors[2];
    sendDescriptors[0].Flags = XAUDIO2_SEND_USEFILTER; // LPF direct-path
    sendDescriptors[0].pOutputVoice = g_audioState.pMasteringVoice;
    sendDescriptors[1].Flags = XAUDIO2_SEND_USEFILTER; // LPF reverb-path -- omit for better performance at the cost of less realistic occlusion
    sendDescriptors[1].pOutputVoice = g_audioState.pSubmixVoice;
    const XAUDIO2_VOICE_SENDS sendList = { 2, sendDescriptors };

    // create the source voice
    V_RETURN( g_audioState.pXAudio2->CreateSourceVoice( &g_audioState.pSourceVoice, pwfx, 0,
                                                        2.0f, nullptr, &sendList ) );

    // Submit the wave sample data using an XAUDIO2_BUFFER structure
    XAUDIO2_BUFFER buffer = {0};
    buffer.pAudioData = sampleData;
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    buffer.AudioBytes = waveSize;
    buffer.LoopCount = XAUDIO2_LOOP_INFINITE;

    V_RETURN( g_audioState.pSourceVoice->SubmitSourceBuffer( &buffer ) );

    V_RETURN( g_audioState.pSourceVoice->Start( 0 ) );

    g_audioState.nFrameToApply3DAudio = 0;

    return S_OK;
}


//-----------------------------------------------------------------------------
// Perform per-frame update of audio
//-----------------------------------------------------------------------------
HRESULT UpdateAudio( float fElapsedTime )
{
    if( !g_audioState.bInitialized )
        return S_FALSE;

    if( g_audioState.nFrameToApply3DAudio == 0 )
    {
        // Calculate listener orientation in x-z plane
        if( g_audioState.vListenerPos.x != g_audioState.listener.Position.x
            || g_audioState.vListenerPos.z != g_audioState.listener.Position.z )
        {
            XMVECTOR v1 = XMLoadFloat3( &g_audioState.vListenerPos );
            XMVECTOR v2 = XMVectorSet( g_audioState.listener.Position.x, g_audioState.listener.Position.y, g_audioState.listener.Position.z, 0.f  );

            XMVECTOR vDelta = v1 - v2;

            g_audioState.fListenerAngle = float( atan2( XMVectorGetX( vDelta ), XMVectorGetZ( vDelta ) ) );

            vDelta = XMVectorSetY( vDelta, 0.f );
            vDelta = XMVector3Normalize( vDelta );

            XMFLOAT3 tmp;
            XMStoreFloat3( &tmp, vDelta );

            g_audioState.listener.OrientFront.x = tmp.x;
            g_audioState.listener.OrientFront.y = 0.f;
            g_audioState.listener.OrientFront.z = tmp.z;
        }

        if (g_audioState.fUseListenerCone)
        {
            g_audioState.listener.pCone = (X3DAUDIO_CONE*)&Listener_DirectionalCone;
        }
        else
        {
            g_audioState.listener.pCone = nullptr;
        }
        if (g_audioState.fUseInnerRadius)
        {
            g_audioState.emitter.InnerRadius = 2.0f;
            g_audioState.emitter.InnerRadiusAngle = X3DAUDIO_PI/4.0f;
        }
        else
        {
            g_audioState.emitter.InnerRadius = 0.0f;
            g_audioState.emitter.InnerRadiusAngle = 0.0f;
        }

        if( fElapsedTime > 0 )
        {
            XMVECTOR v1 = XMLoadFloat3( &g_audioState.vListenerPos );
            XMVECTOR v2 = XMVectorSet( g_audioState.listener.Position.x, g_audioState.listener.Position.y, g_audioState.listener.Position.z, 0 );

            XMVECTOR lVelocity = ( v1 - v2 ) / fElapsedTime;
            g_audioState.listener.Position.x = g_audioState.vListenerPos.x;
            g_audioState.listener.Position.y = g_audioState.vListenerPos.y;
            g_audioState.listener.Position.z = g_audioState.vListenerPos.z;

            XMFLOAT3 tmp;
            XMStoreFloat3( &tmp, lVelocity );
            g_audioState.listener.Velocity.x = tmp.x;
            g_audioState.listener.Velocity.y = tmp.y;
            g_audioState.listener.Velocity.z = tmp.z;

            v1 = XMLoadFloat3( &g_audioState.vEmitterPos );
            v2 = XMVectorSet( g_audioState.emitter.Position.x, g_audioState.emitter.Position.y, g_audioState.emitter.Position.z, 0.f );

            XMVECTOR eVelocity = ( v1 - v2 ) / fElapsedTime;
            g_audioState.emitter.Position.x = g_audioState.vEmitterPos.x;
            g_audioState.emitter.Position.y = g_audioState.vEmitterPos.y;
            g_audioState.emitter.Position.z = g_audioState.vEmitterPos.z;

            XMStoreFloat3( &tmp, eVelocity );
            g_audioState.emitter.Velocity.x = tmp.x;
            g_audioState.emitter.Velocity.y = tmp.y;
            g_audioState.emitter.Velocity.z = tmp.z;
        }

        DWORD dwCalcFlags = X3DAUDIO_CALCULATE_MATRIX | X3DAUDIO_CALCULATE_DOPPLER
            | X3DAUDIO_CALCULATE_LPF_DIRECT | X3DAUDIO_CALCULATE_LPF_REVERB
            | X3DAUDIO_CALCULATE_REVERB;
        if (g_audioState.fUseRedirectToLFE)
        {
            // On devices with an LFE channel, allow the mono source data
            // to be routed to the LFE destination channel.
            dwCalcFlags |= X3DAUDIO_CALCULATE_REDIRECT_TO_LFE;
        }

        X3DAudioCalculate( g_audioState.x3DInstance, &g_audioState.listener, &g_audioState.emitter, dwCalcFlags,
                           &g_audioState.dspSettings );

        IXAudio2SourceVoice* voice = g_audioState.pSourceVoice;
        if( voice )
        {
            // Apply X3DAudio generated DSP settings to XAudio2
            voice->SetFrequencyRatio( g_audioState.dspSettings.DopplerFactor );
            voice->SetOutputMatrix( g_audioState.pMasteringVoice, INPUTCHANNELS, g_audioState.nChannels,
                                    g_audioState.matrixCoefficients );

            voice->SetOutputMatrix(g_audioState.pSubmixVoice, 1, 1, &g_audioState.dspSettings.ReverbLevel);

            XAUDIO2_FILTER_PARAMETERS FilterParametersDirect = { LowPassFilter, 2.0f * sinf(X3DAUDIO_PI/6.0f * g_audioState.dspSettings.LPFDirectCoefficient), 1.0f }; // see XAudio2CutoffFrequencyToRadians() in XAudio2.h for more information on the formula used here
            voice->SetOutputFilterParameters(g_audioState.pMasteringVoice, &FilterParametersDirect);
            XAUDIO2_FILTER_PARAMETERS FilterParametersReverb = { LowPassFilter, 2.0f * sinf(X3DAUDIO_PI/6.0f * g_audioState.dspSettings.LPFReverbCoefficient), 1.0f }; // see XAudio2CutoffFrequencyToRadians() in XAudio2.h for more information on the formula used here
            voice->SetOutputFilterParameters(g_audioState.pSubmixVoice, &FilterParametersReverb);
        }
    }

    g_audioState.nFrameToApply3DAudio++;
    g_audioState.nFrameToApply3DAudio &= 1;

    return S_OK;
}


//-----------------------------------------------------------------------------
// Set reverb effect
//-----------------------------------------------------------------------------
HRESULT SetReverb( int nReverb )
{
    if( !g_audioState.bInitialized )
        return S_FALSE;

    if( nReverb < 0 || nReverb >= NUM_PRESETS )
        return E_FAIL;

    if( g_audioState.pSubmixVoice )
    {
        XAUDIO2FX_REVERB_PARAMETERS native;
        ReverbConvertI3DL2ToNative( &g_PRESET_PARAMS[ nReverb ], &native );
        g_audioState.pSubmixVoice->SetEffectParameters( 0, &native, sizeof( native ) );
    }

    return S_OK;
}


//-----------------------------------------------------------------------------
// Pause audio playback
//-----------------------------------------------------------------------------
VOID PauseAudio( bool resume )
{
    if( !g_audioState.bInitialized )
        return;

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

    if( g_audioState.pSubmixVoice )
    {
        g_audioState.pSubmixVoice->DestroyVoice();
        g_audioState.pSubmixVoice = nullptr;
    }

    if( g_audioState.pMasteringVoice )
    {
        g_audioState.pMasteringVoice->DestroyVoice();
        g_audioState.pMasteringVoice = nullptr;
    }

    g_audioState.pXAudio2->StopEngine();
    SAFE_RELEASE( g_audioState.pXAudio2 );
    SAFE_RELEASE( g_audioState.pReverbEffect );

    g_audioState.waveData.reset();

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
