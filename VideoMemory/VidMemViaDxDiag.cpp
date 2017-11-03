//----------------------------------------------------------------------------
// File: VidMemViaDXDiag.cpp
//
// DxDiag internally uses both DirectDraw 7 and WMI and returns the rounded WMI 
// value if WMI is available. Otherwise, it returns a rounded DirectDraw 7 value. 
//
// Copyright (c) Microsoft Corp. All rights reserved.
//-----------------------------------------------------------------------------
#define INITGUID
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <initguid.h>

// dxdiag is not present in the Windows 7.1 SDK used by "v110_xp". Could get it from the legacy DirectX SDK if
// needed for Windows XP
#include <dxdiag.h>

//-----------------------------------------------------------------------------
// Defines, and constants
//-----------------------------------------------------------------------------
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=nullptr; } }
#endif

HRESULT GetDeviceIDFromHMonitor( HMONITOR hm, WCHAR* strDeviceID, int cchDeviceID ); // from vidmemviaddraw.cpp


HRESULT GetVideoMemoryViaDxDiag( HMONITOR hMonitor, DWORD* pdwDisplayMemory )
{
    WCHAR strInputDeviceID[512];
    GetDeviceIDFromHMonitor( hMonitor, strInputDeviceID, 512 );

    HRESULT hr;
    HRESULT hrCoInitialize = S_OK;
    bool bGotMemory = false;
    IDxDiagProvider* pDxDiagProvider = nullptr;
    IDxDiagContainer* pDxDiagRoot = nullptr;
    IDxDiagContainer* pDevices = nullptr;
    IDxDiagContainer* pDevice = nullptr;
    WCHAR wszChildName[256];
    WCHAR wszPropValue[256];
    DWORD dwDeviceCount;
    VARIANT var;

    *pdwDisplayMemory = 0;
    hrCoInitialize = CoInitialize( 0 );
    VariantInit( &var );

    // CoCreate a IDxDiagProvider*
    hr = CoCreateInstance( CLSID_DxDiagProvider,
                           nullptr,
                           CLSCTX_INPROC_SERVER,
                           IID_IDxDiagProvider,
                           ( LPVOID* )&pDxDiagProvider );
    if( SUCCEEDED( hr ) ) // if FAILED(hr) then it is likely DirectX 9 is not installed
    {
        DXDIAG_INIT_PARAMS dxDiagInitParam;
        ZeroMemory( &dxDiagInitParam, sizeof( DXDIAG_INIT_PARAMS ) );
        dxDiagInitParam.dwSize = sizeof( DXDIAG_INIT_PARAMS );
        dxDiagInitParam.dwDxDiagHeaderVersion = DXDIAG_DX9_SDK_VERSION;
        dxDiagInitParam.bAllowWHQLChecks = FALSE;
        dxDiagInitParam.pReserved = nullptr;
        pDxDiagProvider->Initialize( &dxDiagInitParam );

        hr = pDxDiagProvider->GetRootContainer( &pDxDiagRoot );
        if( SUCCEEDED( hr ) )
        {
            hr = pDxDiagRoot->GetChildContainer( L"DxDiag_DisplayDevices", &pDevices );
            if( SUCCEEDED( hr ) )
            {
                hr = pDevices->GetNumberOfChildContainers( &dwDeviceCount );
                if( SUCCEEDED( hr ) )
                {
                    for( DWORD iDevice = 0; iDevice < dwDeviceCount; iDevice++ )
                    {
                        bool bFound = false;
                        hr = pDevices->EnumChildContainerNames( iDevice, wszChildName, 256 );
                        if( SUCCEEDED( hr ) )
                        {
                            hr = pDevices->GetChildContainer( wszChildName, &pDevice );
                            if( SUCCEEDED( hr ) )
                            {
                                hr = pDevice->GetProp( L"szKeyDeviceID", &var );
                                if( SUCCEEDED( hr ) )
                                {
                                    if( var.vt == VT_BSTR )
                                    {
                                        if( wcsstr( var.bstrVal, strInputDeviceID ) != 0 )
                                            bFound = true;
                                    }
                                    VariantClear( &var );
                                }

                                if( bFound )
                                {
                                    hr = pDevice->GetProp( L"szDisplayMemoryEnglish", &var );
                                    if( SUCCEEDED( hr ) )
                                    {
                                        if( var.vt == VT_BSTR )
                                        {
                                            bGotMemory = true;
                                            wcscpy_s( wszPropValue, 256, var.bstrVal );

                                            // wszPropValue should be something like "256.0 MB" so _wtoi will convert it correctly
                                            *pdwDisplayMemory = _wtoi( wszPropValue );

                                            // Convert from MB to bytes
                                            *pdwDisplayMemory *= 1024 * 1024;
                                        }
                                        VariantClear( &var );
                                    }
                                }
                                SAFE_RELEASE( pDevice );
                            }
                        }
                    }
                }
                SAFE_RELEASE( pDevices );
            }
            SAFE_RELEASE( pDxDiagRoot );
        }
        SAFE_RELEASE( pDxDiagProvider );
    }

    if( SUCCEEDED( hrCoInitialize ) )
        CoUninitialize();

    if( bGotMemory )
        return S_OK;
    else
        return E_FAIL;
}

