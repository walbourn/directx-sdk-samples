//--------------------------------------------------------------------------------------
// File: utils.cpp
//
// Helper utilities for the Compute Shader Accelerated BC6H BC7 Encoder
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <stdio.h>
#include <assert.h>
#include <d3d11.h>
#include "DirectXTex.h"
#include <shlobj.h>
#include <string>
#include <vector>
#include "utils.h"

using namespace DirectX;

#if defined(_DEBUG) || defined(PROFILE)
#pragma comment(lib,"dxguid.lib")
#endif

inline static bool ispow2( _In_ size_t x )
{
    return ((x != 0) && !(x & (x - 1)));
}

//--------------------------------------------------------------------------------------
// This is equivalent to D3D11CreateDevice, except it dynamically loads d3d11.dll,
// this gives us a graceful way to message the user on systems with no d3d11 installed
//--------------------------------------------------------------------------------------
HRESULT WINAPI Dynamic_D3D11CreateDevice( IDXGIAdapter* pAdapter,
                                          D3D_DRIVER_TYPE DriverType,
                                          HMODULE Software,
                                          UINT32 Flags,
                                          D3D_FEATURE_LEVEL* pFeatureLevels,
                                          UINT FeatureLevels,
                                          UINT32 SDKVersion,
                                          ID3D11Device** ppDevice,
                                          D3D_FEATURE_LEVEL* pFeatureLevel,
                                          ID3D11DeviceContext** ppImmediateContext )
{
    typedef HRESULT (WINAPI * LPD3D11CREATEDEVICE)( IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT32, D3D_FEATURE_LEVEL*, UINT, UINT32, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext** );
    static LPD3D11CREATEDEVICE  s_DynamicD3D11CreateDevice = nullptr;
    
    if ( !s_DynamicD3D11CreateDevice )
    {            
        HMODULE hModD3D11 = LoadLibrary( L"d3d11.dll" );

        if ( !hModD3D11 )
        {
            // Ensure this "D3D11 absent" message is shown only once. As sometimes, the app would like to try
            // to create device multiple times
            static bool bMessageAlreadyShwon = false;
            
            if ( !bMessageAlreadyShwon )
            {
                OSVERSIONINFOEX osv;
                memset( &osv, 0, sizeof(osv) );
                osv.dwOSVersionInfoSize = sizeof(osv);
                #pragma warning(suppress:4996)
                GetVersionEx( (LPOSVERSIONINFO)&osv );

                if ( ( osv.dwMajorVersion > 6 )
                    || ( osv.dwMajorVersion == 6 && osv.dwMinorVersion >= 1 ) 
                    || ( osv.dwMajorVersion == 6 && osv.dwMinorVersion == 0 && osv.dwBuildNumber > 6002 ) )
                {

                    MessageBox( 0, L"Direct3D 11 components were not found.", L"Error", MB_ICONEXCLAMATION );
                    // This should not happen, but is here for completeness as the system could be
                    // corrupted or some future OS version could pull D3D11.DLL for some reason
                }
                else if ( osv.dwMajorVersion == 6 && osv.dwMinorVersion == 0 && osv.dwBuildNumber == 6002 )
                {

                    MessageBox( 0, L"Direct3D 11 components were not found, but are available for"\
                        L" this version of Windows.\n"\
                        L"For details see Microsoft Knowledge Base Article #971644\n"\
                        L"http://support.microsoft.com/default.aspx/kb/971644/", L"Error", MB_ICONEXCLAMATION );

                }
                else if ( osv.dwMajorVersion == 6 && osv.dwMinorVersion == 0 )
                {
                    MessageBox( 0, L"Direct3D 11 components were not found. Please install the latest Service Pack.\n"\
                        L"For details see Microsoft Knowledge Base Article #935791\n"\
                        L" http://support.microsoft.com/default.aspx/kb/935791", L"Error", MB_ICONEXCLAMATION );

                }
                else
                {
                    MessageBox( 0, L"Direct3D 11 is not supported on this OS.", L"Error", MB_ICONEXCLAMATION );
                }

                bMessageAlreadyShwon = true;
            }            

            return E_FAIL;
        }

        s_DynamicD3D11CreateDevice = ( LPD3D11CREATEDEVICE )GetProcAddress( hModD3D11, "D3D11CreateDevice" );           
    }

    return s_DynamicD3D11CreateDevice( pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels,
                                       SDKVersion, ppDevice, pFeatureLevel, ppImmediateContext );
}

//--------------------------------------------------------------------------------------
// Create the D3D device and device context
//--------------------------------------------------------------------------------------
HRESULT CreateDevice( ID3D11Device** ppDeviceOut, ID3D11DeviceContext** ppContextOut, BOOL bForceRef /*= FALSE*/ )
{
    *ppDeviceOut = nullptr;
    *ppContextOut = nullptr;
    
    HRESULT hr = S_OK;

    UINT uCreationFlags = D3D11_CREATE_DEVICE_SINGLETHREADED;
#if defined(_DEBUG)
    uCreationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL flOut = D3D_FEATURE_LEVEL_9_1;
    
    if ( !bForceRef )
    {
        // IMPORTANT: this following device creation call caps the to-be-created device feature level to FL10,
        // when copy/paste this code snippet to your title, it is generally recommended to leave the fifth parameter to be nullptr,
        // which will try to create greatest feature level available.
        //
        // This sample only uses CS4x shaders, so it can run on 10.0 hardware.
        // We use FL 11.0 when available to support the maximum texutre size of 16k by 16k.
        
        D3D_FEATURE_LEVEL flIn[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
        
        hr = Dynamic_D3D11CreateDevice( nullptr,                      // Use default graphics card
                                         D3D_DRIVER_TYPE_HARDWARE,    // Try to create a hardware accelerated device
                                         nullptr,                     // Do not use external software rasterizer module
                                         uCreationFlags,              // Device creation flags
                                         flIn,                        // For the purpose of this sample, we cap the FL to 10 to reduce test burden, 
                                                                      // In practice, it is generally recommended to leave this parameter nullptr (and next parameter 0),
                                                                      // which will try to get greatest feature level available
                                         _countof(flIn),              // # of elements in the previous array
                                         D3D11_SDK_VERSION,           // SDK version
                                         ppDeviceOut,                 // Device out
                                         &flOut,                      // Actual feature level created
                                         ppContextOut );              // Context out                
    }
    
    if ( bForceRef || FAILED(hr) )
    {
        // Either because of failure on creating a hardware device or we are forced to create a ref device, we create a ref device here

        SAFE_RELEASE( *ppDeviceOut );
        SAFE_RELEASE( *ppContextOut );
        
        hr = Dynamic_D3D11CreateDevice( nullptr,                      // Use default graphics card
                                         D3D_DRIVER_TYPE_REFERENCE,   // Try to create a hardware accelerated device
                                         nullptr,                     // Do not use external software rasterizer module
                                         uCreationFlags,              // Device creation flags
                                         nullptr,                     // Try to get greatest feature level available
                                         0,                           // # of elements in the previous array
                                         D3D11_SDK_VERSION,           // SDK version
                                         ppDeviceOut,                 // Device out
                                         &flOut,                      // Actual feature level created
                                         ppContextOut );              // Context out
        if ( FAILED(hr) )
        {
            printf( "Reference rasterizer device create failure\n" );
            return hr;
        }
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Helper function which makes CS invocation more convenient
//--------------------------------------------------------------------------------------
void RunComputeShader( ID3D11DeviceContext* pd3dImmediateContext,
                       ID3D11ComputeShader* pComputeShader,
                       ID3D11ShaderResourceView** pShaderResourceViews, 
					   UINT uNumSRVs,
                       ID3D11Buffer* pCBCS, 
                       ID3D11UnorderedAccessView* pUnorderedAccessView,
                       UINT X, UINT Y, UINT Z )
{    
    pd3dImmediateContext->CSSetShader( pComputeShader, nullptr, 0 );
    pd3dImmediateContext->CSSetShaderResources( 0, uNumSRVs, pShaderResourceViews );
    pd3dImmediateContext->CSSetUnorderedAccessViews( 0, 1, &pUnorderedAccessView, nullptr );
    pd3dImmediateContext->CSSetConstantBuffers( 0, 1, &pCBCS );

    pd3dImmediateContext->Dispatch( X, Y, Z );

    ID3D11UnorderedAccessView* ppUAViewNULL[1] = { nullptr };
    pd3dImmediateContext->CSSetUnorderedAccessViews( 0, 1, ppUAViewNULL, nullptr );
    ID3D11ShaderResourceView* ppSRVNULL[3] = { nullptr, nullptr, nullptr };
    pd3dImmediateContext->CSSetShaderResources( 0, 3, ppSRVNULL );
    ID3D11Buffer* ppBufferNULL[1] = { nullptr };
    pd3dImmediateContext->CSSetConstantBuffers( 0, 1, ppBufferNULL );
}

//--------------------------------------------------------------------------------------
// Loads a texture from file
// This function also generates mip levels as necessary using the specified filter
//-------------------------------------------------------------------------------------- 
HRESULT LoadTextureFromFile( ID3D11Device* pd3dDevice, LPCTSTR lpFileName, DXGI_FORMAT fmtLoadAs, BOOL bNoMips, DWORD dwFilter, ID3D11Texture2D** ppTextureOut )
{
    HRESULT hr = S_OK;
    
    WCHAR ext[_MAX_EXT];
    WCHAR fname[_MAX_FNAME];
    _wsplitpath_s( lpFileName, nullptr, 0, nullptr, 0, fname, _MAX_FNAME, ext, _MAX_EXT );

    TexMetadata info;
    ScratchImage* image = new ScratchImage;
    if ( !image )
    {
        return E_OUTOFMEMORY;
    }

    if ( _wcsicmp( ext, L".dds" ) == 0 )
    {
        hr = LoadFromDDSFile( lpFileName, DDS_FLAGS_NONE, &info, *image );
        if ( FAILED(hr) )
        {
            delete image;
            return hr;
        }
    }
    else if ( _wcsicmp( ext, L".tga" ) == 0 )
    {
        hr = LoadFromTGAFile( lpFileName, &info, *image );
        if ( FAILED(hr) )
        {
            delete image;
            return hr;
        }
    }
    else
    {
        hr = LoadFromWICFile( lpFileName, dwFilter, &info, *image );
        if ( FAILED(hr) )
        {            
            delete image;
            return hr;
        }
    }

    // Texture 3D is not currently supported
    if ( info.dimension == TEX_DIMENSION_TEXTURE3D )
    {
        delete image;
        return E_FAIL;
    }

    // We want to encode uncompressed files
    if ( IsCompressed( info.format ) )
    {
        delete image;
        return E_FAIL;
    }

    // If input is sRGB, we want to maintain it instead of incorrectly converting it
    if ( IsSRGB(info.format) && fmtLoadAs == DXGI_FORMAT_R8G8B8A8_UNORM )
    {
        fmtLoadAs = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    }

    // Convert to fmtLoadAs, so that our encoder can accept it
    if ( info.format != fmtLoadAs )
    {
        ScratchImage* timage = new ScratchImage;
        if ( !timage )
        {
            delete image;
            return E_OUTOFMEMORY;
        }

        hr = Convert( image->GetImages(), image->GetImageCount(), image->GetMetadata(), fmtLoadAs, dwFilter /*| dwSRGB*/, 0.5f, *timage );
        if ( FAILED(hr) )
        {
            delete timage;
            delete image;
            return hr;
        }

        const TexMetadata& tinfo = timage->GetMetadata();

        assert( tinfo.format == fmtLoadAs );
        info.format = tinfo.format;

        assert( info.width == tinfo.width );
        assert( info.height == tinfo.height );
        assert( info.depth == tinfo.depth );
        assert( info.arraySize == tinfo.arraySize );
        assert( info.mipLevels == tinfo.mipLevels );
        assert( info.miscFlags == tinfo.miscFlags );
        assert( info.dimension == tinfo.dimension );

        delete image;
        image = timage;
    }

    // If the number of mip levels from the input texture is 1 and the user didn't disable mip, we generate the full mip chain
    // If the number of mip levels from the input texture is greater than 1, we use that mip chain directly and don't re-generate
    if ( ( info.mipLevels <= 1 && !bNoMips )
         && ispow2(info.width) && ispow2(info.height) )
    {
        ScratchImage* timage = new ScratchImage;
        if ( !timage )
        {
            delete image;
            return E_OUTOFMEMORY;
        }
        
        hr = GenerateMipMaps( image->GetImages(), image->GetImageCount(), image->GetMetadata(), dwFilter /*| dwFilterOpts*/, 0, *timage );        
        if ( FAILED(hr) )
        {
            delete timage;
            delete image;
            return hr;
        }

        const TexMetadata& tinfo = timage->GetMetadata();
        info.mipLevels = tinfo.mipLevels;

        assert( info.width == tinfo.width );
        assert( info.height == tinfo.height );
        assert( info.depth == tinfo.depth );
        assert( info.arraySize == tinfo.arraySize );
        assert( info.mipLevels == tinfo.mipLevels );
        assert( info.miscFlags == tinfo.miscFlags );
        assert( info.dimension == tinfo.dimension );

        delete image;
        image = timage;
    }
    
    if ( !bNoMips )
    {
        // The user didn't disable mip, then use the full input resource, 
        // which contains mip levels either directly read from the file, or generated from above
        hr = CreateTexture( pd3dDevice, image->GetImages(), image->GetImageCount(), image->GetMetadata(), (ID3D11Resource**)ppTextureOut );
        if ( FAILED(hr) )
        {
            delete image;
            return hr;
        }
    } 
    else
    {
        // The user explicitly disabled mip, then we only use mip 0 from all faces of the input resource
        std::vector<Image> images;
        for ( size_t item = 0; item < info.arraySize; ++item )
            images.push_back( *(image->GetImage( 0, item, 0 )) );
        info.mipLevels = 1;

        hr = CreateTexture( pd3dDevice, &images[0], images.size(), info, (ID3D11Resource**)ppTextureOut );
        if ( FAILED(hr) )
        {
            delete image;
            return hr;
        }
    }   

    return hr;
}

//--------------------------------------------------------------------------------------
// Create a CPU accessible buffer and download the content of a GPU buffer into it
//-------------------------------------------------------------------------------------- 
ID3D11Buffer* CreateAndCopyToCPUBuf( ID3D11Device* pDevice, ID3D11DeviceContext* pd3dImmediateContext, ID3D11Buffer* pBuffer )
{
    ID3D11Buffer* cpubuf = nullptr;

    D3D11_BUFFER_DESC desc = {};
    pBuffer->GetDesc( &desc );
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;
    if ( SUCCEEDED(pDevice->CreateBuffer(&desc, nullptr, &cpubuf)) )
    {
#if defined(_DEBUG) || defined(PROFILE)
        cpubuf->SetPrivateData( WKPDID_D3DDebugObjectName, sizeof( "CPU" ) - 1, "CPU" );
#endif
        pd3dImmediateContext->CopyResource( cpubuf, pBuffer );
    }

    return cpubuf;
}

//-------------------------------------------------------------------------------------- 
bool FileExists( const WCHAR* pszFilename )
{
    FILE *f = nullptr;
    if ( !_wfopen_s( &f, pszFilename, L"rb" ) )
    {
        if ( f )
            fclose(f);

        return true;
    }

    return false;
}
