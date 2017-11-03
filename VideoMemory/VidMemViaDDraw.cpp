//----------------------------------------------------------------------------
// File: VidMemViaDDraw.cpp
//
// This method queries the DirectDraw 7 interfaces for the amount of available 
// video memory. On a discrete video card, this is often close to the amount 
// of dedicated video memory and usually does not take into account the amount 
// of shared system memory. This number can end up smaller than expected
// on systems with large memories and/or large VRAM video cards due to
// 32-bit overflow. DXGI doesn't suffer from these issues.
//
// Copyright (c) Microsoft Corp. All rights reserved.
//-----------------------------------------------------------------------------
#define INITGUID
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <ddraw.h>


//-----------------------------------------------------------------------------
// Defines, and constants
//-----------------------------------------------------------------------------
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=nullptr; } }
#endif

typedef HRESULT ( WINAPI* LPDIRECTDRAWCREATE )( GUID FAR *lpGUID, LPDIRECTDRAW FAR *lplpDD, IUnknown FAR *pUnkOuter );
BOOL CALLBACK MonitorEnumProc( HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData );

struct DDRAW_MATCH
{
    GUID guid;
    HMONITOR hMonitor;
    CHAR strDriverName[512];
    bool bFound;
};


//-----------------------------------------------------------------------------
BOOL WINAPI DDEnumCallbackEx( GUID FAR* lpGUID, LPSTR lpDriverDescription, LPSTR lpDriverName, LPVOID lpContext, HMONITOR hm )
 {
    UNREFERENCED_PARAMETER( lpDriverDescription );

    DDRAW_MATCH* pDDMatch = ( DDRAW_MATCH* ) lpContext;
    if( pDDMatch->hMonitor == hm )
    {
        pDDMatch->bFound = true;
        strcpy_s( pDDMatch->strDriverName, 512, lpDriverName );
        memcpy( &pDDMatch->guid, lpGUID, sizeof( GUID ) );
    }
    return TRUE;
}


//-----------------------------------------------------------------------------
HRESULT GetVideoMemoryViaDirectDraw( HMONITOR hMonitor, DWORD* pdwAvailableVidMem )
{
    LPDIRECTDRAW pDDraw = nullptr;
    LPDIRECTDRAWENUMERATEEXA pDirectDrawEnumerateEx = nullptr;
    HRESULT hr;
    bool bGotMemory = false;
    *pdwAvailableVidMem = 0;

    HINSTANCE hInstDDraw;
    LPDIRECTDRAWCREATE pDDCreate = nullptr;

    hInstDDraw = LoadLibrary( L"ddraw.dll" );
    if( hInstDDraw )
    {
        DDRAW_MATCH match;
        ZeroMemory( &match, sizeof( DDRAW_MATCH ) );
        match.hMonitor = hMonitor;

        pDirectDrawEnumerateEx = ( LPDIRECTDRAWENUMERATEEXA )GetProcAddress( hInstDDraw, "DirectDrawEnumerateExA" );

        if( pDirectDrawEnumerateEx )
        {
            hr = pDirectDrawEnumerateEx( DDEnumCallbackEx, ( VOID* )&match, DDENUM_ATTACHEDSECONDARYDEVICES );
        }

        pDDCreate = ( LPDIRECTDRAWCREATE )GetProcAddress( hInstDDraw, "DirectDrawCreate" );
        if( pDDCreate )
        {
            pDDCreate( &match.guid, &pDDraw, nullptr );

            LPDIRECTDRAW7 pDDraw7;
            if( SUCCEEDED( pDDraw->QueryInterface( IID_IDirectDraw7, ( VOID** )&pDDraw7 ) ) )
            {
                DDSCAPS2 ddscaps;
                ZeroMemory( &ddscaps, sizeof( DDSCAPS2 ) );
                ddscaps.dwCaps = DDSCAPS_VIDEOMEMORY | DDSCAPS_LOCALVIDMEM;
                hr = pDDraw7->GetAvailableVidMem( &ddscaps, pdwAvailableVidMem, nullptr );
                if( SUCCEEDED( hr ) )
                    bGotMemory = true;
                pDDraw7->Release();
            }
        }
        FreeLibrary( hInstDDraw );
    }


    if( bGotMemory )
        return S_OK;
    else
        return E_FAIL;
}




HRESULT GetDeviceIDFromHMonitor( HMONITOR hm, WCHAR* strDeviceID, int cchDeviceID )
{

    HINSTANCE hInstDDraw;

    hInstDDraw = LoadLibrary( L"ddraw.dll" );
    if( hInstDDraw )
    {
        DDRAW_MATCH match;
        ZeroMemory( &match, sizeof( DDRAW_MATCH ) );
        match.hMonitor = hm;

        LPDIRECTDRAWENUMERATEEXA pDirectDrawEnumerateEx = nullptr;
        pDirectDrawEnumerateEx = ( LPDIRECTDRAWENUMERATEEXA )GetProcAddress( hInstDDraw, "DirectDrawEnumerateExA" );

        if( pDirectDrawEnumerateEx )
            pDirectDrawEnumerateEx( DDEnumCallbackEx, ( VOID* )&match, DDENUM_ATTACHEDSECONDARYDEVICES );

        if( match.bFound )
        {
            LONG iDevice = 0;
            DISPLAY_DEVICEA dispdev;

            ZeroMemory( &dispdev, sizeof( dispdev ) );
            dispdev.cb = sizeof( dispdev );

            while( EnumDisplayDevicesA( nullptr, iDevice, ( DISPLAY_DEVICEA* )&dispdev, 0 ) )
            {
                // Skip devices that are monitors that echo another display
                if( dispdev.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER )
                {
                    iDevice++;
                    continue;
                }

                // Skip devices that aren't attached since they cause problems
                if( ( dispdev.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP ) == 0 )
                {
                    iDevice++;
                    continue;
                }

                if( _stricmp( match.strDriverName, dispdev.DeviceName ) == 0 )
                {
                    MultiByteToWideChar( CP_ACP, 0, dispdev.DeviceID, -1, strDeviceID, cchDeviceID );
                    return S_OK;
                }

                iDevice++;
            }
        }

        FreeLibrary( hInstDDraw );
    }

    return E_FAIL;
}
