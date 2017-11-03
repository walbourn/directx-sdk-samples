//----------------------------------------------------------------------------
// File: VidMemViaWMI.cpp
//
// This method queries the Windows Management Instrumentation (WMI) interfaces 
// to determine the amount of video memory. On a discrete video card, this is 
// often close to the amount of dedicated video memory and usually does not take 
// into account the amount of shared system memory. 
//
// Copyright (c) Microsoft Corp. All rights reserved.
//-----------------------------------------------------------------------------
#define INITGUID
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <oleauto.h>
#include <initguid.h>
#include <wbemidl.h>
#define PRINTF_DEBUGGING


//-----------------------------------------------------------------------------
// Defines, and constants
//-----------------------------------------------------------------------------
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=nullptr; } }
#endif

HRESULT GetDeviceIDFromHMonitor( HMONITOR hm, WCHAR* strDeviceID, int cchDeviceID ); // from vidmemviaddraw.cpp
typedef BOOL ( WINAPI* PfnCoSetProxyBlanket )( IUnknown* pProxy, DWORD dwAuthnSvc, DWORD dwAuthzSvc,
                                               OLECHAR* pServerPrincName, DWORD dwAuthnLevel, DWORD dwImpLevel,
                                               RPC_AUTH_IDENTITY_HANDLE pAuthInfo, DWORD dwCapabilities );

HRESULT GetVideoMemoryViaWMI( HMONITOR hMonitor, DWORD* pdwAdapterRam )
{
    WCHAR strInputDeviceID[512];
    GetDeviceIDFromHMonitor( hMonitor, strInputDeviceID, 512 );

    HRESULT hr;
    bool bGotMemory = false;
    HRESULT hrCoInitialize = S_OK;
    IWbemLocator* pIWbemLocator = nullptr;
    IWbemServices* pIWbemServices = nullptr;
    BSTR pNamespace = nullptr;

    *pdwAdapterRam = 0;
    hrCoInitialize = CoInitialize( 0 );

    hr = CoCreateInstance( CLSID_WbemLocator,
                           nullptr,
                           CLSCTX_INPROC_SERVER,
                           IID_IWbemLocator,
                           ( LPVOID* )&pIWbemLocator );
#ifdef PRINTF_DEBUGGING
    if( FAILED( hr ) ) wprintf( L"WMI: CoCreateInstance failed: 0x%0.8x\n", hr );
#endif

    if( SUCCEEDED( hr ) && pIWbemLocator )
    {
        // Using the locator, connect to WMI in the given namespace.
        pNamespace = SysAllocString( L"\\\\.\\root\\cimv2" );

        hr = pIWbemLocator->ConnectServer( pNamespace, nullptr, nullptr, 0L,
                                           0L, nullptr, nullptr, &pIWbemServices );
#ifdef PRINTF_DEBUGGING
        if( FAILED( hr ) ) wprintf( L"WMI: pIWbemLocator->ConnectServer failed: 0x%0.8x\n", hr );
#endif
        if( SUCCEEDED( hr ) && pIWbemServices != 0 )
        {
            HINSTANCE hinstOle32 = nullptr;

            hinstOle32 = LoadLibraryW( L"ole32.dll" );
            if( hinstOle32 )
            {
                PfnCoSetProxyBlanket pfnCoSetProxyBlanket = nullptr;

                pfnCoSetProxyBlanket = ( PfnCoSetProxyBlanket )GetProcAddress( hinstOle32, "CoSetProxyBlanket" );
                if( pfnCoSetProxyBlanket != 0 )
                {
                    // Switch security level to IMPERSONATE. 
                    pfnCoSetProxyBlanket( pIWbemServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                                          RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, 0 );
                }

                FreeLibrary( hinstOle32 );
            }

            IEnumWbemClassObject* pEnumVideoControllers = nullptr;
            BSTR pClassName = nullptr;

            pClassName = SysAllocString( L"Win32_VideoController" );

            hr = pIWbemServices->CreateInstanceEnum( pClassName, 0,
                                                     nullptr, &pEnumVideoControllers );
#ifdef PRINTF_DEBUGGING
            if( FAILED( hr ) ) wprintf( L"WMI: pIWbemServices->CreateInstanceEnum failed: 0x%0.8x\n", hr );
#endif

            if( SUCCEEDED( hr ) && pEnumVideoControllers )
            {
                IWbemClassObject* pVideoControllers[10] = {0};
                DWORD uReturned = 0;
                BSTR pPropName = nullptr;

                // Get the first one in the list
                pEnumVideoControllers->Reset();
                hr = pEnumVideoControllers->Next( 5000,             // timeout in 5 seconds
                                                  10,                  // return the first 10
                                                  pVideoControllers,
                                                  &uReturned );
#ifdef PRINTF_DEBUGGING
                if( FAILED( hr ) ) wprintf( L"WMI: pEnumVideoControllers->Next failed: 0x%0.8x\n", hr );
                if( uReturned == 0 ) wprintf( L"WMI: pEnumVideoControllers uReturned == 0\n" );
#endif

                VARIANT var;
                if( SUCCEEDED( hr ) )
                {
                    bool bFound = false;
                    for( UINT iController = 0; iController < uReturned; iController++ )
                    {
                        if ( !pVideoControllers[iController] )
                            continue;

                        pPropName = SysAllocString( L"PNPDeviceID" );
                        hr = pVideoControllers[iController]->Get( pPropName, 0L, &var, nullptr, nullptr );
#ifdef PRINTF_DEBUGGING
                        if( FAILED( hr ) )
                            wprintf( L"WMI: pVideoControllers[iController]->Get PNPDeviceID failed: 0x%0.8x\n", hr );
#endif
                        if( SUCCEEDED( hr ) )
                        {
                            if( wcsstr( var.bstrVal, strInputDeviceID ) != 0 )
                                bFound = true;
                        }
                        VariantClear( &var );
                        if( pPropName ) SysFreeString( pPropName );

                        if( bFound )
                        {
                            pPropName = SysAllocString( L"AdapterRAM" );
                            hr = pVideoControllers[iController]->Get( pPropName, 0L, &var, nullptr, nullptr );
#ifdef PRINTF_DEBUGGING
                            if( FAILED( hr ) )
                                wprintf( L"WMI: pVideoControllers[iController]->Get AdapterRAM failed: 0x%0.8x\n",
                                         hr );
#endif
                            if( SUCCEEDED( hr ) )
                            {
                                bGotMemory = true;
                                *pdwAdapterRam = var.ulVal;
                            }
                            VariantClear( &var );
                            if( pPropName ) SysFreeString( pPropName );
                            break;
                        }
                        SAFE_RELEASE( pVideoControllers[iController] );
                    }
                }
            }

            if( pClassName )
                SysFreeString( pClassName );
            SAFE_RELEASE( pEnumVideoControllers );
        }

        if( pNamespace )
            SysFreeString( pNamespace );
        SAFE_RELEASE( pIWbemServices );
    }

    SAFE_RELEASE( pIWbemLocator );

    if( SUCCEEDED( hrCoInitialize ) )
        CoUninitialize();

    if( bGotMemory )
        return S_OK;
    else
        return E_FAIL;
}

