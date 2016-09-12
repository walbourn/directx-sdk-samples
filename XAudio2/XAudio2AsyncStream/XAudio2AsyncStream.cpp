//--------------------------------------------------------------------------------------
// File: XAudio2AsyncStream.cpp
//
// Streaming from a Wave Bank using XAudio2 and asynchronous I/O
//
// NOTE: Currently ignores loop regions in the Wave Bank, and only works for PCM data
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <assert.h>

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

//--------------------------------------------------------------------------------------
#define STREAMING_BUFFER_SIZE 65536
#define MAX_BUFFER_COUNT 3

static_assert( (STREAMING_BUFFER_SIZE % 2048) == 0, "Streaming size must be 2K aligned to use for async I/O" );

//--------------------------------------------------------------------------------------
// Callback structure
//--------------------------------------------------------------------------------------
struct StreamingVoiceContext : public IXAudio2VoiceCallback
{
    STDMETHOD_( void, OnVoiceProcessingPassStart )( UINT32 ) override
    {
    }
    STDMETHOD_( void, OnVoiceProcessingPassEnd )() override
    {
    }
    STDMETHOD_( void, OnStreamEnd )() override
    {
    }
    STDMETHOD_( void, OnBufferStart )( void* ) override
    {
    }
    STDMETHOD_( void, OnBufferEnd )( void* ) override
    {
        SetEvent( hBufferEndEvent );
    }
    STDMETHOD_( void, OnLoopEnd )( void* ) override
    {
    }
    STDMETHOD_( void, OnVoiceError )( void*, HRESULT ) override
    {
    }

    HANDLE hBufferEndEvent;

    StreamingVoiceContext() :
#if (_WIN32_WINNT >= _WIN32_WINNT_VISTA)
        hBufferEndEvent( CreateEventEx( nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE ) )
#else
        hBufferEndEvent( CreateEvent( nullptr, FALSE, FALSE, nullptr ) )
#endif
    {
    }
    virtual ~StreamingVoiceContext()
    {
        CloseHandle( hBufferEndEvent );
    }
};


//--------------------------------------------------------------------------------------
// Forward declaration
//--------------------------------------------------------------------------------------
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

    if( FAILED( hr = FindMediaFileCch( wavebank, MAX_PATH, L"Media\\Banks\\wavebank.xwb" ) ) )
    {
        wprintf( L"Failed to find media file (%#X)\n", hr );
        pXAudio2.Reset();
        CoUninitialize();
        return 0;
    }

    //
    // Extract wavebank data (entries, formats, offsets, and sizes)
    //
    // Note we are using Wave Banks to get sector-aligned streaming data to allow us to use
    // async unbuffered I/O. Raw .WAV files do not meet these requirements.
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

    if ( !wb.IsStreamingBank() )
    {
        wprintf( L"This sample plays back streaming wave banks.\nSee XAudio2WaveBank for playing in-memory wave banks" );
        pXAudio2.Reset();
        CoUninitialize();
        return 0;
    }

    wprintf( L"Press <ESC> to exit.\n" );

    //
    // Repeated loop through all the wavebank entries
    //
    bool exit = false;

    while( !exit )
    {
        for( DWORD i = 0; i < wb.Count(); ++i )
        {
            wprintf( L"Now playing wave entry %u", i );

            //
            // Get the info we need to play back this wave (need enough space for PCM, ADPCM, and xWMA formats)
            //
            char formatBuff[ 64 ]; 
            WAVEFORMATEX *wfx = reinterpret_cast<WAVEFORMATEX*>(&formatBuff);

            if( FAILED( hr = wb.GetFormat( i, wfx, 64 ) ) )
            {
                wprintf( L"\nCouldn't get wave format for entry %u: error 0x%x\n", i, hr );
                exit = true;
                break;
            }

            WaveBankReader::Metadata metadata;
            if ( FAILED( hr = wb.GetMetadata( i, metadata ) ) )
            {
                wprintf( L"\nCouldn't get metadta for entry %u: error 0x%x\n", i, hr );
                exit = true;
                break;
            }

            //
            // Create an XAudio2 voice to stream this wave
            //
            StreamingVoiceContext voiceContext;

            IXAudio2SourceVoice* pSourceVoice;
            if( FAILED( hr = pXAudio2->CreateSourceVoice( &pSourceVoice, wfx, 0, 1.0f, &voiceContext ) ) )
            {
                wprintf( L"\nError %#X creating source voice\n", hr );
                exit = true;
                break;
            }
            pSourceVoice->Start( 0, 0 );

            //
            // Create an overlapped structure and buffers to handle the async I/O
            //
            OVERLAPPED ovlCurrentRequest = {0};
#if (_WIN32_WINNT >= _WIN32_WINNT_VISTA)
            ovlCurrentRequest.hEvent = CreateEventEx( nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, EVENT_MODIFY_STATE | SYNCHRONIZE );
#else
            ovlCurrentRequest.hEvent = CreateEvent( nullptr, TRUE, FALSE, nullptr );
#endif

            if ( (STREAMING_BUFFER_SIZE % wfx->nBlockAlign) != 0 )
            {
                //
                // non-PCM data will fail here. ADPCM requires a more complicated streaming mechanism to deal with submission in audio frames that do
                // not necessarily align to the 2K async boundary.
                //
                wprintf( L"\nStreaming buffer size (%u) is not aligned with sample block requirements (%u)\n", STREAMING_BUFFER_SIZE, wfx->nBlockAlign );
                exit = true;
                break;
            }

            std::unique_ptr<uint8_t[]> buffers[MAX_BUFFER_COUNT];
            for( size_t j=0; j < MAX_BUFFER_COUNT; ++j )
            {
                buffers[j].reset( new uint8_t[ STREAMING_BUFFER_SIZE ] );
            }
            DWORD currentDiskReadBuffer = 0;
            DWORD currentPosition = 0;

            //
            // This sample code shows the simplest way to manage asynchronous
            // streaming. There are three different processes involved. One is the management
            // process, which is what we're writing here. The other two processes are
            // essentially hardware operations: disk reads from the I/O system, and
            // audio processing from XAudio2. Disk reads and audio playback both happen
            // without much intervention from our application, so our job is just to make
            // sure that the data being read off the disk makes it over to the audio
            // processor in time to be played back.
            //
            // There are two events that can happen in this system. The disk I/O system can
            // signal that data is ready, and the audio system can signal that it's done
            // playing back data. We can handle either or both of these events either synchronously
            // (via polling) or asynchronously (via callbacks or by waiting on an event
            // object).
            //
            HANDLE async = wb.GetAsyncHandle();

            while( currentPosition < metadata.lengthBytes )
            {
                wprintf( L"." );

                if( GetAsyncKeyState( VK_ESCAPE ) )
                {
                    exit = true;

                    while( GetAsyncKeyState( VK_ESCAPE ) )
                        Sleep( 10 );

                    break;
                }

                //
                // Issue a request.
                //
                // Note: although the file read will be done asynchronously, it is possible for the
                // call to ReadFile to block for longer than you might think. If the I/O system needs
                // to read the file allocation table in order to satisfy the read, it will do that
                // BEFORE returning from ReadFile. That means that this call could potentially
                // block for several milliseconds! In order to get "true" async I/O you should put
                // this entire loop on a separate thread.
                //
                // Second note: async requests have to be a multiple of the disk sector size. Rather than
                // handle this conditionally, make all reads the same size but remember how many
                // bytes we actually want and only submit that many to the voice.
                //
                DWORD cbValid = min( STREAMING_BUFFER_SIZE, metadata.lengthBytes - currentPosition );
                ovlCurrentRequest.Offset = metadata.offsetBytes + currentPosition;

                bool wait = false;
                if( !ReadFile( async, buffers[ currentDiskReadBuffer ].get(), STREAMING_BUFFER_SIZE, nullptr, &ovlCurrentRequest ) )
                {
                    DWORD error = GetLastError();
                    if ( error != ERROR_IO_PENDING )
                    {
                        wprintf( L"\nCouldn't start async read: error %#X\n", HRESULT_FROM_WIN32( error ) );
                        exit = true;
                        break;
                    }
                    wait = true;
                }

                currentPosition += cbValid;

                //
                // At this point the read is progressing in the background and we are free to do
                // other processing while we wait for it to finish. For the purposes of this sample,
                // however, we'll just go to sleep until the read is done.
                //
                if ( wait )
                    WaitForSingleObject( ovlCurrentRequest.hEvent, INFINITE );

                DWORD cb;
#if (_WIN32_WINNT >= 0x0602 /*_WIN32_WINNT_WIN8*/)
                BOOL result = GetOverlappedResultEx( async, &ovlCurrentRequest, &cb, 0, FALSE );
#else
                BOOL result = GetOverlappedResult( async, &ovlCurrentRequest, &cb, FALSE );
#endif
                if( !result )
                {
                    wprintf( L"\nFailed waiting for async read: error %#X\n", HRESULT_FROM_WIN32( GetLastError() ) );
                    exit = true;
                    break;
                }

                //
                // Now that the event has been signaled, we know we have audio available. The next
                // question is whether our XAudio2 source voice has played enough data for us to give
                // it another buffer full of audio. We'd like to keep no more than MAX_BUFFER_COUNT - 1
                // buffers on the queue, so that one buffer is always free for disk I/O.
                //
                XAUDIO2_VOICE_STATE state;
                for( ;; )
                {
                    pSourceVoice->GetState( &state );
                    if( state.BuffersQueued < MAX_BUFFER_COUNT - 1 )
                        break;

                    WaitForSingleObject( voiceContext.hBufferEndEvent, INFINITE );
                }

                //
                // At this point we have a buffer full of audio and enough room to submit it, so
                // let's submit it and get another read request going.
                //
                XAUDIO2_BUFFER buf = {0};
                buf.AudioBytes = cbValid;
                buf.pAudioData = buffers[currentDiskReadBuffer].get();
                if( currentPosition >= metadata.lengthBytes )
                    buf.Flags = XAUDIO2_END_OF_STREAM;

                pSourceVoice->SubmitSourceBuffer( &buf );

                currentDiskReadBuffer++;
                currentDiskReadBuffer %= MAX_BUFFER_COUNT;
            }

            if( !exit )
            {
                wprintf( L"done streaming.." );

                XAUDIO2_VOICE_STATE state;
                for(; ; )
                {
                    pSourceVoice->GetState( &state );
                    if( !state.BuffersQueued )
                        break;

                    wprintf( L"." );
                    WaitForSingleObject( voiceContext.hBufferEndEvent, INFINITE );
                }
            }

            //
            // Clean up
            //
            pSourceVoice->Stop( 0 );
            pSourceVoice->DestroyVoice();

            CloseHandle( ovlCurrentRequest.hEvent );

            wprintf( L"stopped\n" );

            if( exit )
                break;

            Sleep( 500 );
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
