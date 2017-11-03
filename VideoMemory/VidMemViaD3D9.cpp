//----------------------------------------------------------------------------
// File: VidMemViaD3D9.cpp
//
// This method queries D3D9 for the amount of available texture memory. On 
// Windows Vista, this number is typically the dedicated video memory plus 
// the shared system memory minus the amount of memory in use by textures 
// and render targets. 
//
// Copyright (c) Microsoft Corp. All rights reserved.
//-----------------------------------------------------------------------------
#define INITGUID
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <d3d9.h>


//-----------------------------------------------------------------------------
// Defines, and constants
//-----------------------------------------------------------------------------
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=nullptr; } }
#endif


HRESULT GetVideoMemoryViaD3D9( HMONITOR hMonitor, UINT* pdwAvailableTextureMem )
{
    HRESULT hr;
    bool bGotMemory = false;
    *pdwAvailableTextureMem = 0;

    IDirect3D9* pD3D9 = nullptr;
    pD3D9 = Direct3DCreate9( D3D_SDK_VERSION );
    if( pD3D9 )
    {
        UINT dwAdapterCount = pD3D9->GetAdapterCount();
        for( UINT iAdapter = 0; iAdapter < dwAdapterCount; iAdapter++ )
        {
            IDirect3DDevice9* pd3dDevice = nullptr;

            HMONITOR hAdapterMonitor = pD3D9->GetAdapterMonitor( iAdapter );
            if( hMonitor != hAdapterMonitor )
                continue;

            HWND hWnd = GetDesktopWindow();

            D3DPRESENT_PARAMETERS pp;
            ZeroMemory( &pp, sizeof( D3DPRESENT_PARAMETERS ) );
            pp.BackBufferWidth = 800;
            pp.BackBufferHeight = 600;
            pp.BackBufferFormat = D3DFMT_R5G6B5;
            pp.BackBufferCount = 1;
            pp.MultiSampleType = D3DMULTISAMPLE_NONE;
            pp.MultiSampleQuality = 0;
            pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
            pp.hDeviceWindow = hWnd;
            pp.Windowed = TRUE;

            pp.EnableAutoDepthStencil = FALSE;
            pp.Flags = 0;
            pp.FullScreen_RefreshRateInHz = 0;
            pp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

            hr = pD3D9->CreateDevice( iAdapter, D3DDEVTYPE_HAL, hWnd,
                                      D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &pd3dDevice );
            if( SUCCEEDED( hr ) )
            {
                *pdwAvailableTextureMem = pd3dDevice->GetAvailableTextureMem();
                bGotMemory = true;
                SAFE_RELEASE( pd3dDevice );
            }
        }

        SAFE_RELEASE( pD3D9 );
    }

    if( bGotMemory )
        return S_OK;
    else
        return E_FAIL;
}


HRESULT GetHMonitorFromD3D9Device( IDirect3DDevice9* pd3dDevice, HMONITOR hMonitor )
{
    D3DDEVICE_CREATION_PARAMETERS cp;
    hMonitor = nullptr;
    bool bFound = false;
    if( SUCCEEDED( pd3dDevice->GetCreationParameters( &cp ) ) )
    {
        IDirect3D9* pD3D = nullptr;
        if( SUCCEEDED( pd3dDevice->GetDirect3D( &pD3D ) ) )
        {
            hMonitor = pD3D->GetAdapterMonitor( cp.AdapterOrdinal );
            bFound = true;
        }
        pD3D->Release();
    }

    if( bFound )
        return S_OK;
    else
        return E_FAIL;
}
