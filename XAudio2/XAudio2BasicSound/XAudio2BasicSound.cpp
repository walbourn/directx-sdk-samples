//--------------------------------------------------------------------------------------
// File: XAudio2BasicSound.cpp
//
// Simple playback of a .WAV file using XAudio2
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

#include <wrl\client.h>

#include "WAVFileReader.h"

#if (_WIN32_WINNT >= 0x0602 /*_WIN32_WINNT_WIN8*/)
#include <xaudio2.h>
#pragma comment(lib,"xaudio2.lib")
#else
#include <C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Include\comdecl.h>
#include <C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Include\xaudio2.h>
#endif

using Microsoft::WRL::ComPtr;;

//--------------------------------------------------------------------------------------
// Forward declaration
//--------------------------------------------------------------------------------------
HRESULT PlayWave( _In_ IXAudio2* pXaudio2, _In_z_ LPCWSTR szFilename );
HRESULT FindMediaFileCch( _Out_writes_(cchDest) WCHAR* strDestPath, _In_ int cchDest, _In_z_ LPCWSTR strFilename );


//--------------------------------------------------------------------------------------
// Entry point to the program
//--------------------------------------------------------------------------------------
int main()
{
    //
    // Initialize XAudio2
    //
    HRESULT hr = CoInitializeEx( nullptr, COINIT_MULTITHREADED );
    if (FAILED(hr))
    {
        wprintf(L"Failed to init COM: %#X\n", hr);
        return 0;
    }

#if ( _WIN32_WINNT < 0x0602 /*_WIN32_WINNT_WIN8*/)
    // Workaround for XAudio 2.7 known issue
#ifdef _DEBUG
    HMODULE mXAudioDLL = LoadLibraryExW(L"XAudioD2_7.DLL", nullptr, 0x00000800 /* LOAD_LIBRARY_SEARCH_SYSTEM32 */);
#else
    HMODULE mXAudioDLL = LoadLibraryExW(L"XAudio2_7.DLL", nullptr, 0x00000800 /* LOAD_LIBRARY_SEARCH_SYSTEM32 */);
#endif
    if (!mXAudioDLL)
    {
        wprintf(L"Failed to find XAudio 2.7 DLL");
        CoUninitialize();
        return 0;
    }
#endif

    UINT32 flags = 0;
#if (_WIN32_WINNT < 0x0602 /*_WIN32_WINNT_WIN8*/) && defined(_DEBUG)
    flags |= XAUDIO2_DEBUG_ENGINE;
#endif
    ComPtr<IXAudio2> pXAudio2;
    hr = XAudio2Create( pXAudio2.GetAddressOf(), flags );
    if( FAILED( hr ) )
    {
        wprintf( L"Failed to init XAudio2 engine: %#X\n", hr );
        CoUninitialize();
        return 0;
    }

#if (_WIN32_WINNT >= 0x0602 /*_WIN32_WINNT_WIN8*/) && defined(_DEBUG)
    // To see the trace output, you need to view ETW logs for this application:
    //    Go to Control Panel, Administrative Tools, Event Viewer.
    //    View->Show Analytic and Debug Logs.
    //    Applications and Services Logs / Microsoft / Windows / XAudio2. 
    //    Right click on Microsoft Windows XAudio2 debug logging, Properties, then Enable Logging, and hit OK 
    XAUDIO2_DEBUG_CONFIGURATION debug ={0};
    debug.TraceMask = XAUDIO2_LOG_ERRORS | XAUDIO2_LOG_WARNINGS;
    debug.BreakMask = XAUDIO2_LOG_ERRORS;
    pXAudio2->SetDebugConfiguration( &debug, 0 );
#endif

    //
    // Create a mastering voice
    //
    IXAudio2MasteringVoice* pMasteringVoice = nullptr;

    if( FAILED( hr = pXAudio2->CreateMasteringVoice( &pMasteringVoice ) ) )
    {
        wprintf( L"Failed creating mastering voice: %#X\n", hr );
        pXAudio2.Reset();
        CoUninitialize();
        return 0;
    }

    //
    // Play a PCM wave file
    //
    wprintf( L"Playing mono WAV PCM file..." );
    if( FAILED( hr = PlayWave( pXAudio2.Get(), L"Media\\Wavs\\MusicMono.wav" ) ) )
    {
        wprintf( L"Failed creating source voice: %#X\n", hr );
        pXAudio2.Reset();
        CoUninitialize();
        return 0;
    }

    //
    // Play an ADPCM wave file
    //
    wprintf( L"\nPlaying mono WAV ADPCM file (loops twice)..." );
    if( FAILED( hr = PlayWave( pXAudio2.Get(), L"Media\\Wavs\\MusicMono_adpcm.wav" ) ) )
    {
        wprintf( L"Failed creating source voice: %#X\n", hr );
        pXAudio2.Reset();
        CoUninitialize();
        return 0;
    }

    //
    // Play a 5.1 PCM wave extensible file
    //
    wprintf( L"\nPlaying 5.1 WAV PCM file..." );
    if( FAILED( hr = PlayWave( pXAudio2.Get(), L"Media\\Wavs\\MusicSurround.wav" ) ) )
    {
        wprintf( L"Failed creating source voice: %#X\n", hr );
        pXAudio2.Reset();
        CoUninitialize();
        return 0;
    }

#if (_WIN32_WINNT < 0x0602 /*_WIN32_WINNT_WIN8*/) || (_WIN32_WINNT >= 0x0A00 /*_WIN32_WINNT_WIN10*/ )

    //
    // Play a mono xWMA wave file
    //

    wprintf( L"\nPlaying mono xWMA file..." );
    if( FAILED( hr = PlayWave( pXAudio2.Get(), L"Media\\Wavs\\MusicMono_xwma.wav" ) ) )
    {
        wprintf( L"Failed creating source voice: %#X\n", hr );
        pXAudio2.Reset();
        CoUninitialize();
        return 0;
    }

    //
    // Play a 5.1 xWMA wave file
    //

    wprintf( L"\nPlaying 5.1 xWMA file..." );
    if( FAILED( hr = PlayWave( pXAudio2.Get(), L"Media\\Wavs\\MusicSurround_xwma.wav" ) ) )
    {
        wprintf( L"Failed creating source voice: %#X\n", hr );
        pXAudio2.Reset();
        CoUninitialize();
        return 0;
    }

#endif

    //
    // Cleanup XAudio2
    //
    wprintf( L"\nFinished playing\n" );

    // All XAudio2 interfaces are released when the engine is destroyed, but being tidy
    pMasteringVoice->DestroyVoice();

    pXAudio2.Reset();

#if ( _WIN32_WINNT < 0x0602 /*_WIN32_WINNT_WIN8*/)
    if (mXAudioDLL)
        FreeLibrary(mXAudioDLL);
#endif

    CoUninitialize();
}


//--------------------------------------------------------------------------------------
// Name: PlayWave
// Desc: Plays a wave and blocks until the wave finishes playing
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT PlayWave( IXAudio2* pXaudio2, LPCWSTR szFilename )
{
    //
    // Locate the wave file
    //
    WCHAR strFilePath[MAX_PATH];
    HRESULT hr = FindMediaFileCch( strFilePath, MAX_PATH, szFilename );
    if( FAILED( hr ) )
    {
        wprintf( L"Failed to find media file: %s\n", szFilename );
        return hr;
    }

    //
    // Read in the wave file
    //
    std::unique_ptr<uint8_t[]> waveFile;
    DirectX::WAVData waveData;
    if ( FAILED( hr = DirectX::LoadWAVAudioFromFileEx( strFilePath, waveFile, waveData ) ) )
    {
        wprintf( L"Failed reading WAV file: %#X (%s)\n", hr, strFilePath );
        return hr;
    }

    //
    // Play the wave using a XAudio2SourceVoice
    //

    // Create the source voice
    IXAudio2SourceVoice* pSourceVoice;
    if( FAILED( hr = pXaudio2->CreateSourceVoice( &pSourceVoice, waveData.wfx ) ) )
    {
        wprintf( L"Error %#X creating source voice\n", hr );
        return hr;
    }

    // Submit the wave sample data using an XAUDIO2_BUFFER structure
    XAUDIO2_BUFFER buffer = {0};
    buffer.pAudioData = waveData.startAudio;
    buffer.Flags = XAUDIO2_END_OF_STREAM;  // tell the source voice not to expect any data after this buffer
    buffer.AudioBytes = waveData.audioBytes;

    if ( waveData.loopLength > 0 )
    {
        buffer.LoopBegin = waveData.loopStart;
        buffer.LoopLength = waveData.loopLength;
        buffer.LoopCount = 1; // We'll just assume we play the loop twice
    }

#if (_WIN32_WINNT < 0x0602 /*_WIN32_WINNT_WIN8*/) || (_WIN32_WINNT >= 0x0A00 /*_WIN32_WINNT_WIN10*/ )
    if ( waveData.seek )
    {
        XAUDIO2_BUFFER_WMA xwmaBuffer = {0};
        xwmaBuffer.pDecodedPacketCumulativeBytes = waveData.seek;
        xwmaBuffer.PacketCount = waveData.seekCount;
        if( FAILED( hr = pSourceVoice->SubmitSourceBuffer( &buffer, &xwmaBuffer ) ) )
        {
            wprintf( L"Error %#X submitting source buffer (xWMA)\n", hr );
            pSourceVoice->DestroyVoice();
            return hr;
        }
    }
#else
    if ( waveData.seek )
    {
        wprintf( L"This platform does not support xWMA or XMA2\n" );
        pSourceVoice->DestroyVoice();
        return hr;
    }
#endif
    else if( FAILED( hr = pSourceVoice->SubmitSourceBuffer( &buffer ) ) )
    {
        wprintf( L"Error %#X submitting source buffer\n", hr );
        pSourceVoice->DestroyVoice();
        return hr;
    }

    hr = pSourceVoice->Start( 0 );

    // Let the sound play
    BOOL isRunning = TRUE;
    while( SUCCEEDED( hr ) && isRunning )
    {
        XAUDIO2_VOICE_STATE state;
        pSourceVoice->GetState( &state );
        isRunning = ( state.BuffersQueued > 0 ) != 0;

        // Wait till the escape key is pressed
        if( GetAsyncKeyState( VK_ESCAPE ) )
            break;

        Sleep( 10 );
    }

    // Wait till the escape key is released
    while( GetAsyncKeyState( VK_ESCAPE ) )
        Sleep( 10 );

    pSourceVoice->DestroyVoice();

    return hr;
}


//--------------------------------------------------------------------------------------
// Helper function to try to find the location of a media file
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT FindMediaFileCch( WCHAR* strDestPath, int cchDest, LPCWSTR strFilename )
{
    bool bFound = false;

    if( !strFilename || strFilename[0] == 0 || !strDestPath || cchDest < 10 )
        return E_INVALIDARG;

    // Get the exe name, and exe path
    WCHAR strExePath[MAX_PATH] = {0};
    WCHAR strExeName[MAX_PATH] = {0};
    WCHAR* strLastSlash = nullptr;
    GetModuleFileName( nullptr, strExePath, MAX_PATH );
    strExePath[MAX_PATH - 1] = 0;
    strLastSlash = wcsrchr( strExePath, TEXT( '\\' ) );
    if( strLastSlash )
    {
        wcscpy_s( strExeName, MAX_PATH, &strLastSlash[1] );

        // Chop the exe name from the exe path
        *strLastSlash = 0;

        // Chop the .exe from the exe name
        strLastSlash = wcsrchr( strExeName, TEXT( '.' ) );
        if( strLastSlash )
            *strLastSlash = 0;
    }

    wcscpy_s( strDestPath, cchDest, strFilename );
    if( GetFileAttributes( strDestPath ) != 0xFFFFFFFF )
        return S_OK;

    // Search all parent directories starting at .\ and using strFilename as the leaf name
    WCHAR strLeafName[MAX_PATH] = {0};
    wcscpy_s( strLeafName, MAX_PATH, strFilename );

    WCHAR strFullPath[MAX_PATH] = {0};
    WCHAR strFullFileName[MAX_PATH] = {0};
    WCHAR strSearch[MAX_PATH] = {0};
    WCHAR* strFilePart = nullptr;

    GetFullPathName( L".", MAX_PATH, strFullPath, &strFilePart );
    if( !strFilePart )
        return E_FAIL;

    while( strFilePart && *strFilePart != '\0' )
    {
        swprintf_s( strFullFileName, MAX_PATH, L"%s\\%s", strFullPath, strLeafName );
        if( GetFileAttributes( strFullFileName ) != 0xFFFFFFFF )
        {
            wcscpy_s( strDestPath, cchDest, strFullFileName );
            bFound = true;
            break;
        }

        swprintf_s( strFullFileName, MAX_PATH, L"%s\\%s\\%s", strFullPath, strExeName, strLeafName );
        if( GetFileAttributes( strFullFileName ) != 0xFFFFFFFF )
        {
            wcscpy_s( strDestPath, cchDest, strFullFileName );
            bFound = true;
            break;
        }

        swprintf_s( strSearch, MAX_PATH, L"%s\\..", strFullPath );
        GetFullPathName( strSearch, MAX_PATH, strFullPath, &strFilePart );
    }
    if( bFound )
        return S_OK;

    // On failure, return the file as the path but also return an error code
    wcscpy_s( strDestPath, cchDest, strFilename );

    return HRESULT_FROM_WIN32( ERROR_FILE_NOT_FOUND );
}
