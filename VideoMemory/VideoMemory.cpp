//----------------------------------------------------------------------------
// File: VideoMemory.cpp
//
// Copyright (c) Microsoft Corp. All rights reserved.
//-----------------------------------------------------------------------------
#define INITGUID
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <dxgi.h>
#include <d3d9.h>

//#define FORCE_USE_D3D9

//-----------------------------------------------------------------------------
// Defines, and constants
//-----------------------------------------------------------------------------
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=nullptr; } }
#endif

HRESULT GetVideoMemoryViaDxDiag( HMONITOR hMonitor, DWORD* pdwDisplayMemory );
HRESULT GetVideoMemoryViaDirectDraw( HMONITOR hMonitor, DWORD* pdwAvailableVidMem );
HRESULT GetVideoMemoryViaWMI( HMONITOR hMonitor, DWORD* pdwAdapterRam );
HRESULT GetVideoMemoryViaD3D9( HMONITOR hMonitor, UINT* pdwAvailableTextureMem );

HRESULT GetHMonitorFromD3D9Device( IDirect3DDevice9* pd3dDevice, HMONITOR hMonitor );


//-----------------------------------------------------------------------------
void EnumerateUsingDXGI( IDXGIFactory* pDXGIFactory )
{
    assert( pDXGIFactory != 0 );

    for( UINT index = 0; ; ++index )
    {
        IDXGIAdapter* pAdapter = nullptr;
        HRESULT hr = pDXGIFactory->EnumAdapters( index, &pAdapter );
        if( FAILED( hr ) ) // DXGIERR_NOT_FOUND is expected when the end of the list is hit
            break;

        DXGI_ADAPTER_DESC desc;
        memset( &desc, 0, sizeof( DXGI_ADAPTER_DESC ) );
        if( SUCCEEDED( pAdapter->GetDesc( &desc ) ) )
        {
            wprintf( L"\nDXGI Adapter: %u\nDescription: %s\n", index, desc.Description );

            for( UINT iOutput = 0; ; ++iOutput )
            {
                IDXGIOutput* pOutput = nullptr;
                hr = pAdapter->EnumOutputs( iOutput, &pOutput );
                if( FAILED( hr ) ) // DXGIERR_NOT_FOUND is expected when the end of the list is hit
                    break;

                DXGI_OUTPUT_DESC outputDesc;
                memset( &outputDesc, 0, sizeof( DXGI_OUTPUT_DESC ) );
                if( SUCCEEDED( pOutput->GetDesc( &outputDesc ) ) )
                {
                    wprintf( L"hMonitor: 0x%0.8Ix\n", ( DWORD_PTR )outputDesc.Monitor );
                    wprintf( L"hMonitor Device Name: %s\n", outputDesc.DeviceName );
                }

                SAFE_RELEASE( pOutput );
            }

            wprintf(
                L"\tGetVideoMemoryViaDXGI\n\t\tDedicatedVideoMemory: %Iu MB (%Iu)\n\t\tDedicatedSystemMemory: %Iu MB (%Iu)\n\t\tSharedSystemMemory: %Iu MB (%Iu)\n",
                desc.DedicatedVideoMemory / 1024 / 1024, desc.DedicatedVideoMemory,
                desc.DedicatedSystemMemory / 1024 / 1024, desc.DedicatedSystemMemory,
                desc.SharedSystemMemory / 1024 / 1024, desc.SharedSystemMemory );
        }

        SAFE_RELEASE( pAdapter );
    }
}


//-----------------------------------------------------------------------------
void EnumerateUsingD3D9( IDirect3D9* pD3D9 )
{
    assert( pD3D9 != 0 );

    UINT dwAdapterCount = pD3D9->GetAdapterCount();
    for( UINT iAdapter = 0; iAdapter < dwAdapterCount; iAdapter++ )
    {
        D3DADAPTER_IDENTIFIER9 id;
        ZeroMemory( &id, sizeof( D3DADAPTER_IDENTIFIER9 ) );
        pD3D9->GetAdapterIdentifier( iAdapter, 0, &id );
        wprintf( L"\nD3D9 Adapter: %u\nDriver: %S\nDescription: %S\n", iAdapter, id.Driver, id.Description );

        HMONITOR hMonitor = pD3D9->GetAdapterMonitor( iAdapter );
        wprintf( L"hMonitor: 0x%0.8Ix\n", ( DWORD_PTR )hMonitor );

        MONITORINFOEX mi;
        mi.cbSize = sizeof( MONITORINFOEX );
        GetMonitorInfo( hMonitor, &mi );
        wprintf( L"hMonitor Device Name: %s\n", mi.szDevice );

        DWORD dwAvailableVidMem;
        if( SUCCEEDED( GetVideoMemoryViaDirectDraw( hMonitor, &dwAvailableVidMem ) ) )
            wprintf( L"\tGetVideoMemoryViaDirectDraw\n\t\tdwAvailableVidMem: %u MB (%u)\n",
                        dwAvailableVidMem / 1024 / 1024, dwAvailableVidMem );
        else
            wprintf( L"\tGetVideoMemoryViaDirectDraw\n\t\tn/a\n" );

#if _WIN32_WINNT >= 0x600
        DWORD dwDisplayMemory;
        if( SUCCEEDED( GetVideoMemoryViaDxDiag( hMonitor, &dwDisplayMemory ) ) )
            wprintf( L"\tGetVideoMemoryViaDxDiag\n\t\tdwDisplayMemory: %u MB (%u)\n", dwDisplayMemory / 1024 /
                        1024, dwDisplayMemory );
        else
            wprintf( L"\tGetVideoMemoryViaDxDiag\n\t\tn/a\n" );

        // dxdiag is not present in the Windows 7.1 SDK used by "v110_xp". Could get it from the legacy DirectX SDK if
        // needed for Windows XP
#endif

        DWORD dwAdapterRAM;
        if( SUCCEEDED( GetVideoMemoryViaWMI( hMonitor, &dwAdapterRAM ) ) )
            wprintf( L"\tGetVideoMemoryViaWMI\n\t\tdwAdapterRAM: %u MB (%u)\n", dwAdapterRAM / 1024 / 1024,
                        dwAdapterRAM );
        else
            wprintf( L"\tGetVideoMemoryViaWMI\n\t\tn/a\n" );

        UINT dwAvailableTextureMem;
        if( SUCCEEDED( GetVideoMemoryViaD3D9( hMonitor, &dwAvailableTextureMem ) ) )
            wprintf( L"\tGetVideoMemoryViaD3D9\n\t\tdwAvailableTextureMem: %u MB (%u)\n", dwAvailableTextureMem /
                        1024 / 1024, dwAvailableTextureMem );
        else
            wprintf( L"\tGetVideoMemoryViaD3D9\n\t\tn/a\n" );
    }
}


//-----------------------------------------------------------------------------
int main()
{

// DXGI is only available on Windows Vista or later. This method returns the 
// amount of dedicated video memory, the amount of dedicated system memory, 
// and the amount of shared system memory. DXGI is more reflective of the true 
// system configuration than the other 4 methods. 

#ifndef FORCE_USE_D3D9
    HINSTANCE hDXGI = LoadLibrary( L"dxgi.dll" );
    if( hDXGI )
    {
        typedef HRESULT ( WINAPI* LPCREATEDXGIFACTORY )( REFIID, void** );

        LPCREATEDXGIFACTORY pCreateDXGIFactory = nullptr;
        IDXGIFactory* pDXGIFactory = nullptr;

        // We prefer the use of DXGI 1.1
        pCreateDXGIFactory = ( LPCREATEDXGIFACTORY )GetProcAddress( hDXGI, "CreateDXGIFactory1" );

        if ( !pCreateDXGIFactory )
        {
            pCreateDXGIFactory = ( LPCREATEDXGIFACTORY )GetProcAddress( hDXGI, "CreateDXGIFactory" );

            if ( !pCreateDXGIFactory )
            {
                FreeLibrary( hDXGI );
                wprintf( L"ERROR: dxgi.dll missing entry-point\n" );
                return -1;
            }
        }
        
        HRESULT hr = pCreateDXGIFactory( __uuidof( IDXGIFactory ), ( LPVOID* )&pDXGIFactory );

        if ( SUCCEEDED(hr) )
        {
            EnumerateUsingDXGI( pDXGIFactory );

            SAFE_RELEASE( pDXGIFactory );

            return 0;
        }

        FreeLibrary( hDXGI );
    }
    else
#endif
    {
        // This sample loops over all d3d9 adapters and outputs info about them
        IDirect3D9* pD3D9 = nullptr;
        pD3D9 = Direct3DCreate9( D3D_SDK_VERSION );
        if ( pD3D9 )
        {
            EnumerateUsingD3D9( pD3D9 );

            SAFE_RELEASE( pD3D9 );

            return 0;
        }
    }

    wprintf( L"ERROR: Failed to create required object\n" );
    return -1;
}



