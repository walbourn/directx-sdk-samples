//--------------------------------------------------------------------------------------
// File: XAudio2WaveBank.cpp
//
// Playing audio from a Wave Bank using XAudio2
//
// NOTE: Supports PCM and ADPCM data.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

#include <wrl\client.h>

#include "WaveBankReader.h"

#if (_WIN32_WINNT >= 0x0602 /*_WIN32_WINNT_WIN8*/)
#include <xaudio2.h>
#pragma comment(lib,"xaudio2.lib")
#else
#include <C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Include\comdecl.h>
#include <C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Include\xaudio2.h>
#endif

using namespace DirectX;
using Microsoft::WRL::ComPtr;

//#define USE_XWMA
//#define USE_ADPCM

//--------------------------------------------------------------------------------------
// Forward declaration
//--------------------------------------------------------------------------------------
HRESULT PlayWaveFromWaveBank( _In_ IXAudio2* pXaudio2, _Inout_ WaveBankReader& wb, _In_ uint32_t index );
HRESULT FindMediaFileCch( _Out_writes_(cchDest) WCHAR* strDestPath, _In_ int cchDest, _In_z_ LPCWSTR strFilename );


//--------------------------------------------------------------------------------------
// Entry point to the program
//--------------------------------------------------------------------------------------
int main()
{
    //
    // Initialize XAudio2
    //
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
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
    // Find our wave bank file
    //
    WCHAR wavebank[ MAX_PATH ];
#if defined(USE_XWMA)
#if (_WIN32_WINNT >= 0x0602 /*_WIN32_WINNT_WIN8*/) && (_WIN32_WINNT < 0x0A00 /*_WIN32_WINNT_WIN10*/ )
#error xWMA is not supported by XAudio 2.8 on Windows 8.x
#endif

    if( FAILED( hr = FindMediaFileCch( wavebank, MAX_PATH, L"Media\\Banks\\XWMAdroid.xwb" ) ) )
#elif defined(USE_ADPCM)
    if( FAILED( hr = FindMediaFileCch( wavebank, MAX_PATH, L"Media\\Banks\\ADPCMdroid.xwb" ) ) )
#else
    if( FAILED( hr = FindMediaFileCch( wavebank, MAX_PATH, L"Media\\Banks\\droid.xwb" ) ) )
#endif
    {
        wprintf( L"Failed to find media file (%#X)\n", hr );
        pXAudio2.Reset();
        CoUninitialize();
        return 0;
    }

    //
    // Extract wavebank data (entries, formats, offsets, and sizes)
    //
    WaveBankReader wb;

    if( FAILED( hr = wb.Open( wavebank ) ) )
    {
        wprintf( L"Failed to wavebank data (%#X)\n", hr );
        pXAudio2.Reset();
        CoUninitialize();
        return 0;
    }

    wprintf( L"Wavebank loaded with %u entries.\n", wb.Count() );

    if ( wb.IsStreamingBank() )
    {
        wprintf( L"This sample plays back in-memory wave banks.\nSee XAudio2AsyncStream for playing streaming wave banks" );
        pXAudio2.Reset();
        CoUninitialize();
        return 0;
    }

    //
    // At this point, the Wave Bank's metadata, format, entries, and names (if present) are available
    // The actual wave data for an in-memory buffer is potentially still loading asynchronously.
    // (note a streaming wave bank is prepared as soon as Open returns)
    //
    // We can either call WaitOnPrepare() to synchronously wait for this load, -or- we can
    // routinely check wb.IsPrepared() to see if it's ready yet...
    //
    wb.WaitOnPrepare();

    //
    // Play sounds from wave bank
    //
    for( uint32_t j = 0; j < wb.Count(); ++j )
    {
        WaveBankReader::Metadata metadata;
        if( FAILED( hr = wb.GetMetadata( j, metadata ) ) )
        {
            wprintf( L"Failed getting metadata for index %u: %#X\n", j, hr );
            pXAudio2.Reset();
            CoUninitialize();
            return 0;
        }

        if ( metadata.loopLength > 0 && metadata.loopLength != metadata.duration )
            wprintf( L"Playing entry %u (duration of %u samples; loop point %u,%u )...\n", j, metadata.duration, metadata.loopStart, metadata.loopLength );
        else
            wprintf( L"Playing entry %u (duration of %u samples)...\n", j, metadata.duration );

        if( FAILED( hr = PlayWaveFromWaveBank( pXAudio2.Get(), wb, j ) ) )
        {
            wprintf( L"Failed creating source voice for index %u: %#X\n", j, hr );
            pXAudio2.Reset();
            CoUninitialize();
            return 0;
        }
    }

    //
    // Cleanup XAudio2
    //

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
// Name: PlayWaveFromWaveBank
// Desc: Plays a wave and blocks until the wave finishes playing
//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT PlayWaveFromWaveBank( IXAudio2* pXaudio2, WaveBankReader& wb, uint32_t index )
{
    if ( index >= wb.Count() )
        return E_INVALIDARG;

    uint8_t waveFormat[64];
    auto pwfx = reinterpret_cast<WAVEFORMATEX*>( &waveFormat );

    HRESULT hr = wb.GetFormat( index, pwfx, 64 );
    if ( FAILED(hr) )
        return hr;

    const uint8_t* waveData = nullptr;
    uint32_t waveSize;

    hr = wb.GetWaveData( index, &waveData, waveSize );
    if ( FAILED(hr) )
        return hr;

    WaveBankReader::Metadata metadata;
    hr = wb.GetMetadata( index, metadata );
    if ( FAILED(hr) )
        return hr;

    //
    // Play the wave using a XAudio2SourceVoice
    //

    // Create the source voice
    IXAudio2SourceVoice* pSourceVoice;
    if( FAILED( hr = pXaudio2->CreateSourceVoice( &pSourceVoice, pwfx ) ) )
    {
        wprintf( L"Error %#X creating source voice\n", hr );
        return hr;
    }

    // Submit the wave sample data using an XAUDIO2_BUFFER structure
    XAUDIO2_BUFFER buffer = {0};
    buffer.pAudioData = waveData;
    buffer.Flags = XAUDIO2_END_OF_STREAM;  // tell the source voice not to expect any data after this buffer
    buffer.AudioBytes = waveSize;

    if ( metadata.loopLength > 0 && metadata.loopLength != metadata.duration )
    {
        buffer.LoopBegin = metadata.loopStart;
        buffer.LoopLength = metadata.loopLength;
        buffer.LoopCount = 1; // We'll just assume we play the loop twice
    }

    const uint32_t* seekTable;
    uint32_t seekTableCount;
    uint32_t tag;
    hr = wb.GetSeekTable( index, &seekTable, seekTableCount, tag );

    if ( seekTable )
    {
        if ( tag == WAVE_FORMAT_WMAUDIO2 || tag == WAVE_FORMAT_WMAUDIO3 )
        {
#if (_WIN32_WINNT < 0x0602 /*_WIN32_WINNT_WIN8*/) || (_WIN32_WINNT >= 0x0A00 /*_WIN32_WINNT_WIN10*/ )
            XAUDIO2_BUFFER_WMA xwmaBuffer = {0};
            xwmaBuffer.pDecodedPacketCumulativeBytes = seekTable;
            xwmaBuffer.PacketCount = seekTableCount;
            if( FAILED( hr = pSourceVoice->SubmitSourceBuffer( &buffer, &xwmaBuffer ) ) )
            {
                wprintf( L"Error %#X submitting source buffer (xWMA)\n", hr );
                pSourceVoice->DestroyVoice();
                return hr;
            }
#else
            wprintf( L"This platform does not support xWMA\n" );
            pSourceVoice->DestroyVoice();
            return hr;
#endif
        }
        else if ( tag == 0x166 /* WAVE_FORMAT_XMA2 */ )
        {
            wprintf( L"This platform does not support XMA2\n" );
            pSourceVoice->DestroyVoice();
            return hr;
        }
    }
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
