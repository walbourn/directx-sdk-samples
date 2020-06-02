//--------------------------------------------------------------------------------------
// File: utils.h
//
// Helper utilities for the Compute Shader Accelerated BC6H BC7 Encoder/Decoder
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#ifndef __UTILS_H
#define __UTILS_H

#pragma once

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=nullptr; } }
#endif

#ifndef V_GOTO
#define V_GOTO( x ) { hr = (x); if ( FAILED(hr) ) {goto quit;} }
#endif

#ifndef V_RETURN
#define V_RETURN( x )    { hr = (x); if( FAILED(hr) ) { return hr; } }
#endif

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
                                          ID3D11DeviceContext** ppImmediateContext );

//--------------------------------------------------------------------------------------
// Create the D3D device and device context
//--------------------------------------------------------------------------------------
HRESULT CreateDevice( ID3D11Device** ppDeviceOut, ID3D11DeviceContext** ppContextOut, BOOL bForceRef = FALSE );

//--------------------------------------------------------------------------------------
// Helper function which makes CS invocation more convenient
//--------------------------------------------------------------------------------------
void RunComputeShader( ID3D11DeviceContext* pd3dImmediateContext,
                       ID3D11ComputeShader* pComputeShader,
                       ID3D11ShaderResourceView** pShaderResourceViews, 
					   UINT uNumSRVs,
                       ID3D11Buffer* pCBCS, 
                       ID3D11UnorderedAccessView* pUnorderedAccessView,
                       UINT X, UINT Y, UINT Z );


//--------------------------------------------------------------------------------------
// Loads a texture from file
//-------------------------------------------------------------------------------------- 
HRESULT LoadTextureFromFile( ID3D11Device* pd3dDevice, LPCTSTR lpFileName, DXGI_FORMAT fmtLoadAs, BOOL bNoMips,
    DirectX::TEX_FILTER_FLAGS dwFilter, ID3D11Texture2D** ppTextureOut );

//--------------------------------------------------------------------------------------
// Create a CPU accessible buffer and download the content of a GPU buffer into it
//-------------------------------------------------------------------------------------- 
ID3D11Buffer* CreateAndCopyToCPUBuf( ID3D11Device* pDevice, ID3D11DeviceContext* pd3dImmediateContext, ID3D11Buffer* pBuffer );

//-------------------------------------------------------------------------------------- 
bool FileExists( const WCHAR* pszFilename );

#endif