//--------------------------------------------------------------------------------------
// File: XAudio2Enumerate.cpp
//
// Demonstrates enumerating audio devices and creating a XAudio2 mastering voice for them
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

#include <string>
#include <vector>

#include <wrl\client.h>

#if (_WIN32_WINNT >= 0x0602 /*_WIN32_WINNT_WIN8*/)
#include <xaudio2.h>
#pragma comment(lib,"xaudio2.lib")
#pragma comment(lib,"runtimeobject.lib")
#include <Windows.Devices.Enumeration.h>
#include <wrl.h>
#include <ppltasks.h>
#else
#include <C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Include\comdecl.h>
#include <C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Include\xaudio2.h>
#endif

using Microsoft::WRL::ComPtr;

//--------------------------------------------------------------------------------------
// Forward declaration
//--------------------------------------------------------------------------------------
struct AudioDevice
{
    std::wstring deviceId;
    std::wstring description;
};

HRESULT EnumerateAudio( _In_ IXAudio2* pXaudio2, _Inout_ std::vector<AudioDevice>& list );


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
    // Enumerate and display audio devices on the system
    //
    std::vector<AudioDevice> list;
    hr = EnumerateAudio( pXAudio2.Get(), list );
    if( FAILED( hr ) )
    {
        wprintf( L"Failed to enumerate audio devices: %#X\n", hr );
        CoUninitialize();
        return 0;
    }

    if ( hr == S_FALSE )
    {
        wprintf( L"No audio devices found\n");
        CoUninitialize();
        return 0;
    }

    UINT32 devcount = 0;
    UINT32 devindex = -1;
    for( auto it = list.cbegin(); it != list.cend(); ++it, ++devcount )
    {
        wprintf( L"\nDevice %u\n\tID = \"%s\"\n\tDescription = \"%s\"\n",
                 devcount,
                 it->deviceId.c_str(),
                 it->description.c_str() );

        // Simple selection criteria of just picking the first one
        if ( devindex == -1 )
        {
            devindex = devcount;
        }
    }

    wprintf( L"\n" );

    //
    // Create a mastering voice
    //
    IXAudio2MasteringVoice* pMasteringVoice = nullptr;

#if (_WIN32_WINNT >= 0x0602 /*_WIN32_WINNT_WIN8*/)
    if( FAILED( hr = pXAudio2->CreateMasteringVoice( &pMasteringVoice,
                                                     XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0,
                                                     list[ devindex ].deviceId.c_str() ) ) )
#else
    if( FAILED( hr = pXAudio2->CreateMasteringVoice( &pMasteringVoice,
                                                     XAUDIO2_DEFAULT_CHANNELS,
                                                     XAUDIO2_DEFAULT_SAMPLERATE, 0,
                                                     devindex ) ) )
#endif
    {
        wprintf( L"Failed creating mastering voice: %#X\n", hr );
        pXAudio2.Reset();
        CoUninitialize();
        return 0;
    }

    XAUDIO2_VOICE_DETAILS details;
    pMasteringVoice->GetVoiceDetails( &details );

    wprintf( L"Mastering voice created with %u input channels, %u sample rate\n", details.InputChannels, details.InputSampleRate );

    //
    // Cleanup XAudio2
    //
    // All XAudio2 interfaces are released when the engine is destroyed, but being tidy
    pMasteringVoice->DestroyVoice();

#if ( _WIN32_WINNT < 0x0602 /*_WIN32_WINNT_WIN8*/)
    if (mXAudioDLL)
        FreeLibrary(mXAudioDLL);
#endif

    pXAudio2.Reset();
    CoUninitialize();
}


//--------------------------------------------------------------------------------------
// Enumerate audio end-points
//--------------------------------------------------------------------------------------
HRESULT EnumerateAudio( _In_ IXAudio2* pXaudio2, _Inout_ std::vector<AudioDevice>& list )
{
#if (_WIN32_WINNT >= 0x0602 /*_WIN32_WINNT_WIN8*/)

    UNREFERENCED_PARAMETER( pXaudio2 );

#if defined(__cplusplus_winrt )

    // Enumerating with WinRT using C++/CX
    using namespace concurrency;
    using Windows::Devices::Enumeration::DeviceClass;
    using Windows::Devices::Enumeration::DeviceInformation;
    using Windows::Devices::Enumeration::DeviceInformationCollection;
 
    auto operation = DeviceInformation::FindAllAsync(DeviceClass::AudioRender);

    auto task = create_task( operation );

    task.then( [&list]( DeviceInformationCollection^ devices )
    {
        for( unsigned i=0; i < devices->Size; ++i )
        {
            using Windows::Devices::Enumeration::DeviceInformation;
 
            DeviceInformation^ d = devices->GetAt(i);

            AudioDevice device;
            device.deviceId = d->Id->Data();
            device.description = d->Name->Data();
            list.emplace_back( device );
        }
    });

    task.wait();
 
    if ( list.empty() )
        return S_FALSE;

#else

    // Enumerating with WinRT using WRL
    using namespace Microsoft::WRL;
    using namespace Microsoft::WRL::Wrappers;
    using namespace ABI::Windows::Foundation;
    using namespace ABI::Windows::Foundation::Collections;
    using namespace ABI::Windows::Devices::Enumeration;

    RoInitializeWrapper initialize(RO_INIT_MULTITHREADED);
    HRESULT hr = initialize;
    if ( FAILED(hr) )
        return hr;

    Microsoft::WRL::ComPtr<IDeviceInformationStatics> diFactory;
    hr = ABI::Windows::Foundation::GetActivationFactory( HStringReference(RuntimeClass_Windows_Devices_Enumeration_DeviceInformation).Get(), &diFactory );
    if ( FAILED(hr) )
        return hr;

    Event findCompleted( CreateEventEx( nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, WRITE_OWNER | EVENT_ALL_ACCESS ) );
    if ( !findCompleted.IsValid() )
        return HRESULT_FROM_WIN32( GetLastError() );

    auto callback = Callback<IAsyncOperationCompletedHandler<DeviceInformationCollection*>>(
        [&findCompleted,list]( IAsyncOperation<DeviceInformationCollection*>* aDevices, AsyncStatus status ) -> HRESULT
    {
        SetEvent( findCompleted.Get() );
        return S_OK;
    });

    ComPtr<IAsyncOperation<DeviceInformationCollection*>> operation;
    hr = diFactory->FindAllAsyncDeviceClass( DeviceClass_AudioRender, operation.GetAddressOf() );
    if ( FAILED(hr) )
        return hr;

    operation->put_Completed( callback.Get() );

    WaitForSingleObject( findCompleted.Get(), INFINITE );

    ComPtr<IVectorView<DeviceInformation*>> devices;
    operation->GetResults( devices.GetAddressOf() );

    unsigned int count = 0;
    hr = devices->get_Size( &count );
    if ( FAILED(hr) )
        return hr;

    if ( !count )
        return S_FALSE;

    for( unsigned int j = 0; j < count; ++j )
    {
        ComPtr<IDeviceInformation> deviceInfo;
        hr = devices->GetAt( j, deviceInfo.GetAddressOf() );
        if ( SUCCEEDED(hr) )
        {
            HString id;
            deviceInfo->get_Id( id.GetAddressOf() );

            HString name;
            deviceInfo->get_Name( name.GetAddressOf() );

            AudioDevice device;
            device.deviceId = id.GetRawBuffer( nullptr );
            device.description = name.GetRawBuffer( nullptr );
            list.emplace_back( device );
        }
    }

    return S_OK;

#endif 

#else // _WIN32_WINNT < _WIN32_WINNT_WIN8

    // Enumerating with XAudio 2.7
    UINT32 count = 0;
    HRESULT hr = pXaudio2->GetDeviceCount( &count );
    if ( FAILED(hr) )
        return hr;

    if ( !count )
        return S_FALSE;

    list.reserve( count );

    for( UINT32 j = 0; j < count; ++j )
    {
        XAUDIO2_DEVICE_DETAILS details;
        hr = pXaudio2->GetDeviceDetails( j, &details );
        if ( SUCCEEDED(hr) )
        {
            AudioDevice device;
            device.deviceId = details.DeviceID;
            device.description = details.DisplayName;
            list.emplace_back( device );
        }
    }

#endif

    return S_OK;
}
