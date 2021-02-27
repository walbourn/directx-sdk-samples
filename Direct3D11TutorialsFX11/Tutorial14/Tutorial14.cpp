//--------------------------------------------------------------------------------------
// File: Tutorial14.cpp
//
// Render State Management
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License (MIT).
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "SDKmisc.h"
#include "SDKmesh.h"
#include "DDSTextureLoader.h"

#include <d3dx11effect.h>

#pragma warning( disable : 4100 )

using namespace DirectX;

const WCHAR*                         g_szQuadTechniques[] =
{
    L"RenderQuadSolid",
    L"RenderQuadSrcAlphaAdd",
    L"RenderQuadSrcAlphaSub",
    L"RenderQuadSrcColorAdd",
    L"RenderQuadSrcColorSub"
};
#define MAX_QUAD_TECHNIQUES 5

const WCHAR*                         g_szDepthStencilModes[] =
{
    L"DepthOff/StencilOff",
    L"DepthLess/StencilOff",
    L"DepthGreater/StencilOff",

    L"DepthOff/StencilIncOnFail",
    L"DepthLess/StencilIncOnFail",
    L"DepthGreater/StencilIncOnFail",

    L"DepthOff/StencilIncOnPass",
    L"DepthLess/StencilIncOnPass",
    L"DepthGreater/StencilIncOnPass",
};
#define MAX_DEPTH_STENCIL_MODES 9

const WCHAR*                         g_szRasterizerModes[] =
{
    L"CullOff/FillSolid",
    L"CullFront/FillSolid",
    L"CullBack/FillSolid",

    L"CullOff/FillWire",
    L"CullFront/FillWire",
    L"CullBack/FillWire",
};
#define MAX_RASTERIZER_MODES 6

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
CModelViewerCamera                  g_Camera;               // A model viewing camera
CDXUTDialogResourceManager          g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg                     g_SettingsDlg;          // Device settings dialog
CDXUTTextHelper*                    g_pTxtHelper = nullptr;
CDXUTDialog                         g_HUD;                  // manages the 3D UI
CDXUTDialog                         g_SampleUI;             // dialog for sample specific controls

XMMATRIX                            g_World;
float                               g_fModelPuffiness = 0.0f;
bool                                g_bSpinning = true;
ID3DX11Effect*                      g_pEffect = nullptr;
ID3D11InputLayout*                  g_pSceneLayout = nullptr;
ID3D11InputLayout*                  g_pQuadLayout = nullptr;
ID3D11Buffer*                       g_pScreenQuadVB = nullptr;
CDXUTSDKMesh                        g_Mesh;
ID3D11ShaderResourceView*           g_pScreenRV[2] = { nullptr, nullptr };

UINT                                g_eSceneDepthStencilMode = 0;
ID3D11DepthStencilState*            g_pDepthStencilStates[MAX_DEPTH_STENCIL_MODES]; // Depth Stencil states for non-FX 
// depth stencil state managment

UINT                                g_eSceneRasterizerMode = 0;
ID3D11RasterizerState*              g_pRasterStates[MAX_RASTERIZER_MODES];  // Rasterizer states for non-FX 
// rasterizer state management

UINT                                g_eQuadRenderMode = 0;
ID3DX11EffectTechnique*             g_pTechniqueQuad[MAX_QUAD_TECHNIQUES]; // Quad Techniques from the FX file for 
// FX based alpha blend state management

ID3DX11EffectTechnique*             g_pTechniqueScene = nullptr;             // FX technique for rendering the scene
ID3DX11EffectTechnique*             g_pTechniqueRenderWithStencil = nullptr; // FX technique for rendering using FX based depth
// stencil state management

ID3DX11EffectShaderResourceVariable* g_ptxDiffuseVariable = nullptr;
ID3DX11EffectMatrixVariable*        g_pWorldVariable = nullptr;
ID3DX11EffectMatrixVariable*        g_pViewVariable = nullptr;
ID3DX11EffectMatrixVariable*        g_pProjectionVariable = nullptr;
ID3DX11EffectScalarVariable*        g_pPuffiness = nullptr;

//--------------------------------------------------------------------------------------
// Struct describing our screen-space vertex
//--------------------------------------------------------------------------------------
struct SCREEN_VERTEX
{
    XMFLOAT4 pos;
    XMFLOAT2 tex;
};

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_STATIC                  -1
#define IDC_TOGGLEFULLSCREEN        1
#define IDC_TOGGLEREF               2
#define IDC_CHANGEDEVICE            3
#define IDC_TOGGLEWARP              4
#define IDC_TOGGLESPIN              5
#define IDC_QUADRENDER_MODE         6
#define IDC_SCENEDEPTHSTENCIL_MODE  7
#define IDC_SCENERASTERIZER_MODE    8


//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
void RenderText();
void InitApp();
void LoadQuadTechniques();
void LoadDepthStencilStates( ID3D11Device* pd3dDevice );
void LoadRasterizerStates( ID3D11Device* pd3dDevice );

//--------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    pDeviceSettings->d3d11.AutoDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    return true;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext )
{
    HRESULT hr = S_OK;

    auto pd3dImmediateContext = DXUTGetD3D11DeviceContext();
    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    V_RETURN( g_SettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );
    g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15 );

    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;

    // Disable optimizations to further improve shader debugging
    dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

#if D3D_COMPILER_VERSION >= 46

    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Tutorial14.fx" ) );

    V_RETURN( D3DX11CompileEffectFromFile( str, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, dwShaderFlags, 0, pd3dDevice, &g_pEffect, nullptr) );

#else

    ID3DBlob* pEffectBuffer = nullptr;
    V_RETURN( DXUTCompileFromFile( L"Tutorial14.fx", nullptr, "none", "fx_5_0", dwShaderFlags, 0, &pEffectBuffer ) );
    hr = D3DX11CreateEffectFromMemory( pEffectBuffer->GetBufferPointer(), pEffectBuffer->GetBufferSize(), 0, pd3dDevice, &g_pEffect );
    SAFE_RELEASE( pEffectBuffer );
    if ( FAILED(hr) )
        return hr;
    
#endif

    // Obtain the technique handles
    g_pTechniqueScene = g_pEffect->GetTechniqueByName( "RenderScene" );
    g_pTechniqueRenderWithStencil = g_pEffect->GetTechniqueByName( "RenderWithStencil" );
    LoadQuadTechniques();
    LoadDepthStencilStates( pd3dDevice );
    LoadRasterizerStates( pd3dDevice );

    // Obtain the variables
    g_ptxDiffuseVariable = g_pEffect->GetVariableByName( "g_txDiffuse" )->AsShaderResource();
    g_pWorldVariable = g_pEffect->GetVariableByName( "World" )->AsMatrix();
    g_pViewVariable = g_pEffect->GetVariableByName( "View" )->AsMatrix();
    g_pProjectionVariable = g_pEffect->GetVariableByName( "Projection" )->AsMatrix();
    g_pPuffiness = g_pEffect->GetVariableByName( "Puffiness" )->AsScalar();

    // Set Puffiness
    g_pPuffiness->SetFloat( g_fModelPuffiness );

    // Define the input layout
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT numElements = ARRAYSIZE( layout );

    // Create the input layout
    D3DX11_PASS_DESC PassDesc;
    V_RETURN( g_pTechniqueScene->GetPassByIndex( 0 )->GetDesc( &PassDesc ) );
    V_RETURN( pd3dDevice->CreateInputLayout( layout, numElements, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pSceneLayout ) );

    // Load the mesh
    V_RETURN( g_Mesh.Create( pd3dDevice, L"Tiny\\tiny.sdkmesh" ) );

    // Initialize the world matrices
    g_World = XMMatrixIdentity();

    // Create a screen quad
    const D3D11_INPUT_ELEMENT_DESC quadlayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    V_RETURN( g_pTechniqueQuad[0]->GetPassByIndex( 0 )->GetDesc( &PassDesc ) );
    V_RETURN( pd3dDevice->CreateInputLayout( quadlayout, 2, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pQuadLayout ) );

    SCREEN_VERTEX svQuad[4];
    float fSize = 1.0f;
    svQuad[0].pos = XMFLOAT4( -fSize, fSize, 0.0f, 1.0f );
    svQuad[0].tex = XMFLOAT2( 0.0f, 0.0f );
    svQuad[1].pos = XMFLOAT4( fSize, fSize, 0.0f, 1.0f );
    svQuad[1].tex = XMFLOAT2( 1.0f, 0.0f );
    svQuad[2].pos = XMFLOAT4( -fSize, -fSize, 0.0f, 1.0f );
    svQuad[2].tex = XMFLOAT2( 0.0f, 1.0f );
    svQuad[3].pos = XMFLOAT4( fSize, -fSize, 0.0f, 1.0f );
    svQuad[3].tex = XMFLOAT2( 1.0f, 1.0f );

    D3D11_BUFFER_DESC vbdesc =
    {
        4 * sizeof( SCREEN_VERTEX ),
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_VERTEX_BUFFER,
        0,
        0
    };

    D3D11_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = svQuad;
    InitData.SysMemPitch = 0;
    InitData.SysMemSlicePitch = 0;
    V_RETURN( pd3dDevice->CreateBuffer( &vbdesc, &InitData, &g_pScreenQuadVB ) );

    // Load the texture for the screen quad
    WCHAR* szScreenTextures[] =
    {
        L"misc\\MarbleClouds.dds",
        L"misc\\NormTest.dds"
    };

    for( int i = 0; i < 2; i++ )
    {
        V_RETURN( DXUTCreateShaderResourceViewFromFile( pd3dDevice, szScreenTextures[i], &g_pScreenRV[i] ) );
    }

    // Setup the camera's view parameters
    static const XMVECTORF32 s_Eye = { 0.0f, 3.0f, -800.0f, 0.f };
    static const XMVECTORF32 s_At = { 0.0f, 1.0f, 0.0f, 0.f };
    g_Camera.SetViewParams( s_Eye, s_At );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_SettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( XM_PI / 4, fAspectRatio, 0.1f, 5000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( MOUSE_LEFT_BUTTON, MOUSE_WHEEL, MOUSE_MIDDLE_BUTTON );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 270, pBackBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 170, 300 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );

    if( g_bSpinning )
    {
        g_World = XMMatrixRotationY( 60.0f * XMConvertToRadians((float)fTime) );
    }
    else
    {
        g_World = XMMatrixRotationY( XMConvertToRadians( 180.f ) );
    }

    XMMATRIX mRot = XMMatrixRotationX( XMConvertToRadians( -90.0f ) );
    g_World = mRot * g_World;
}


//--------------------------------------------------------------------------------------
// Render the help and statistics text
//--------------------------------------------------------------------------------------
void RenderText()
{
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 2, 0 );
    g_pTxtHelper->SetForegroundColor( Colors::Yellow );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );
    g_pTxtHelper->End();
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext,
                                  double fTime, float fElapsedTime, void* pUserContext )
{
    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.OnRender( fElapsedTime );
        return;
    }

    //
    // Clear the back buffer
    //
    auto pRTV = DXUTGetD3D11RenderTargetView();
    pd3dImmediateContext->ClearRenderTargetView( pRTV, Colors::MidnightBlue );

    //
    // Clear the depth stencil
    //
    auto pDSV = DXUTGetD3D11DepthStencilView();
    pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0, 0 );

    XMMATRIX mView = g_Camera.GetViewMatrix();
    XMMATRIX mProj = g_Camera.GetProjMatrix();
    XMMATRIX mWorldViewProjection = g_World * mView * mProj;

    //
    // Update variables that change once per frame
    //
    g_pProjectionVariable->SetMatrix( ( float* )&mProj );
    g_pViewVariable->SetMatrix( ( float* )&mView );
    g_pWorldVariable->SetMatrix( ( float* )&g_World );

    //
    // Update the Cull Mode (non-FX method)
    //
    pd3dImmediateContext->RSSetState( g_pRasterStates[ g_eSceneRasterizerMode ] );

    //
    // Update the Depth Stencil States (non-FX method)
    //
    pd3dImmediateContext->OMSetDepthStencilState( g_pDepthStencilStates[ g_eSceneDepthStencilMode ], 0 );

    //
    // Render the mesh
    //
    pd3dImmediateContext->IASetInputLayout( g_pSceneLayout );

    UINT Strides[1];
    UINT Offsets[1];
    ID3D11Buffer* pVB[1];
    pVB[0] = g_Mesh.GetVB11( 0, 0 );
    Strides[0] = ( UINT )g_Mesh.GetVertexStride( 0, 0 );
    Offsets[0] = 0;
    pd3dImmediateContext->IASetVertexBuffers( 0, 1, pVB, Strides, Offsets );
    pd3dImmediateContext->IASetIndexBuffer( g_Mesh.GetIB11( 0 ), g_Mesh.GetIBFormat11( 0 ), 0 );

    D3DX11_TECHNIQUE_DESC techDesc;
    HRESULT hr;
    V( g_pTechniqueScene->GetDesc( &techDesc ) );

    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        for( UINT subset = 0; subset < g_Mesh.GetNumSubsets( 0 ); ++subset )
        {
            auto pSubset = g_Mesh.GetSubset( 0, subset );

            auto PrimType = g_Mesh.GetPrimitiveType11( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
            pd3dImmediateContext->IASetPrimitiveTopology( PrimType );

            auto pDiffuseRV = g_Mesh.GetMaterial( pSubset->MaterialID )->pDiffuseRV11;
            g_ptxDiffuseVariable->SetResource( pDiffuseRV );

            g_pTechniqueScene->GetPassByIndex( p )->Apply( 0, pd3dImmediateContext );
            pd3dImmediateContext->DrawIndexed( ( UINT )pSubset->IndexCount, 0, ( UINT )pSubset->VertexStart );
        }
    }

    //
    // Reset the world transform
    //
    XMMATRIX mWorld = XMMatrixScaling( 150.0f, 150.0f, 1.0f );
    g_pWorldVariable->SetMatrix( ( const float* )&mWorld );

    //
    // Render the screen space quad
    //
    auto pTech = g_pTechniqueQuad[ g_eQuadRenderMode ];
    g_ptxDiffuseVariable->SetResource( g_pScreenRV[0] );
    UINT strides = sizeof( SCREEN_VERTEX );
    UINT offsets = 0;
    ID3D11Buffer* pBuffers[1] = { g_pScreenQuadVB };

    pd3dImmediateContext->IASetInputLayout( g_pQuadLayout );
    pd3dImmediateContext->IASetVertexBuffers( 0, 1, pBuffers, &strides, &offsets );
    pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

    V( pTech->GetDesc( &techDesc ) );

    for( UINT uiPass = 0; uiPass < techDesc.Passes; uiPass++ )
    {
        pTech->GetPassByIndex( uiPass )->Apply( 0, pd3dImmediateContext );

        pd3dImmediateContext->Draw( 4, 0 );
    }

    //
    // Render the screen space quad again, but this time with a different texture
    //  and only render where the stencil buffer is != 0
    //  Look at the FX file for the state settings
    //
    pTech = g_pTechniqueRenderWithStencil;
    g_ptxDiffuseVariable->SetResource( g_pScreenRV[1] );
    pd3dImmediateContext->IASetInputLayout( g_pQuadLayout );
    pd3dImmediateContext->IASetVertexBuffers( 0, 1, pBuffers, &strides, &offsets );
    pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

    V( pTech->GetDesc( &techDesc ) );
    for( UINT uiPass = 0; uiPass < techDesc.Passes; uiPass++ )
    {
        pTech->GetPassByIndex( uiPass )->Apply( 0, pd3dImmediateContext );
        pd3dImmediateContext->Draw( 4, 0 );
    }

    //
    // Reset our Cull Mode (non-FX method)
    //
    pd3dImmediateContext->RSSetState( g_pRasterStates[ 0 ] );

    //
    // Reset the Depth Stencil State
    //
    pd3dImmediateContext->OMSetDepthStencilState( g_pDepthStencilStates[ 1 ], 0 );

    //
    // Render the UI
    //
    g_HUD.OnRender( fElapsedTime );
    g_SampleUI.OnRender( fElapsedTime );
    RenderText();
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11ReleasingSwapChain();
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11DestroyDevice();
    g_SettingsDlg.OnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE( g_pTxtHelper );

    g_Mesh.Destroy();

    SAFE_RELEASE( g_pSceneLayout );
    SAFE_RELEASE( g_pQuadLayout );
    SAFE_RELEASE( g_pEffect );
    SAFE_RELEASE( g_pScreenQuadVB );

    for( int i = 0; i < 2; i++ )
    {
        SAFE_RELEASE( g_pScreenRV[i] );
    }

    for( UINT i = 0; i < MAX_DEPTH_STENCIL_MODES; i++ )
        SAFE_RELEASE( g_pDepthStencilStates[i] );

    for( UINT i = 0; i < MAX_RASTERIZER_MODES; i++ )
        SAFE_RELEASE( g_pRasterStates[i] );
}


//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                          bool* pbNoFurtherProcessing, void* pUserContext )
{
    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass messages to settings dialog if its active
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;
    *pbNoFurtherProcessing = g_SampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass all remaining windows messages to camera so it can respond to user input
    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
}


//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
    if( bKeyDown )
    {
        switch( nChar )
        {
            case VK_F1: // Change as needed                
                break;
        }
    }
}


//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    switch( nControlID )
    {
        case IDC_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen();
            break;
        case IDC_TOGGLEREF:
            DXUTToggleREF();
            break;
        case IDC_CHANGEDEVICE:
            g_SettingsDlg.SetActive( !g_SettingsDlg.IsActive() );
            break;
        case IDC_TOGGLEWARP:
            DXUTToggleWARP();
            break;

        case IDC_TOGGLESPIN:
        {
            g_bSpinning = g_SampleUI.GetCheckBox( IDC_TOGGLESPIN )->GetChecked();
            break;
        }

        case IDC_QUADRENDER_MODE:
        {
            auto pComboBox = reinterpret_cast<CDXUTComboBox*>( pControl );
            g_eQuadRenderMode = ( UINT )PtrToInt( pComboBox->GetSelectedData() );
            break;
        }

        case IDC_SCENEDEPTHSTENCIL_MODE:
        {
            auto pComboBox = reinterpret_cast<CDXUTComboBox*>( pControl );
            g_eSceneDepthStencilMode = ( UINT )PtrToInt( pComboBox->GetSelectedData() );
            break;
        }

        case IDC_SCENERASTERIZER_MODE:
        {
            auto pComboBox = reinterpret_cast<CDXUTComboBox*>( pControl );
            g_eSceneRasterizerMode = ( UINT )PtrToInt( pComboBox->GetSelectedData() );
            break;
        }
    }
}


//--------------------------------------------------------------------------------------
// Call if device was removed.  Return true to find a new device, false to quit
//--------------------------------------------------------------------------------------
bool CALLBACK OnDeviceRemoved( void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// Initialize everything and go into a render loop
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow )
{
    // Enable run-time memory check for debug builds.
#ifdef _DEBUG
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // DXUT will create and use the best device
    // that is available on the system depending on which D3D callbacks are set below

    // Set general DXUT callbacks
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackDeviceRemoved( OnDeviceRemoved );

    // Set the D3D11 DXUT callbacks. Remove these sets if the app doesn't need to support D3D11
    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );

    // Perform any application-level initialization here

    DXUTInit( true, true, nullptr ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen

    InitApp();
    DXUTCreateWindow( L"Tutorial14" );

    // Only require 10-level hardware or later
    DXUTCreateDevice( D3D_FEATURE_LEVEL_10_0, true, 800, 600 );
    DXUTMainLoop(); // Enter into the DXUT render loop

    // Perform any application-level cleanup here

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    g_fModelPuffiness = 0.0f;
    g_bSpinning = true;

    g_SettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 0, iY, 170, 22 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 0, iY += 26, 170, 22, VK_F2 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 0, iY += 26, 170, 22, VK_F3 );
    g_HUD.AddButton( IDC_TOGGLEWARP, L"Toggle WARP (F4)", 0, iY += 26, 170, 22, VK_F4 );

    g_SampleUI.SetCallback( OnGUIEvent );

    CDXUTComboBox* pComboBox = nullptr;

    iY = 0;
    g_SampleUI.AddStatic( IDC_STATIC, L"(Q)uad Render Mode", 0, iY, 200, 25 );
    iY += 25;
    HRESULT hr = g_SampleUI.AddComboBox( IDC_QUADRENDER_MODE, 0, iY, 270, 24, 'Q', false, &pComboBox );
    if( SUCCEEDED(hr) && pComboBox )
        pComboBox->SetDropHeight( 150 );

    iY += 40;
    g_SampleUI.AddStatic( IDC_STATIC, L"Scene (R)asterizer Mode", 0, iY, 200, 25 );
    iY += 25;
    hr = g_SampleUI.AddComboBox( IDC_SCENERASTERIZER_MODE, 0, iY, 270, 24, 'R', false, &pComboBox );
    if ( SUCCEEDED(hr) && pComboBox)
        pComboBox->SetDropHeight( 150 );

    iY += 40;
    g_SampleUI.AddStatic( IDC_STATIC, L"Scene Depth/(S)tencil Mode", 0, iY, 200, 25 );
    iY += 25;
    hr = g_SampleUI.AddComboBox( IDC_SCENEDEPTHSTENCIL_MODE, 0, iY, 270, 24, 'S', false, &pComboBox );
    if ( SUCCEEDED(hr) && pComboBox)
        pComboBox->SetDropHeight( 150 );

    iY += 24;
    g_SampleUI.AddCheckBox( IDC_TOGGLESPIN, L"Toggle Spinning", 0, iY += 26, 170, 22, g_bSpinning );
}


//--------------------------------------------------------------------------------------
// LoadQuadTechniques
// Load the techniques for rendering the quad from the FX file.  The techniques in the
// FX file contain the alpha blending state setup.
//--------------------------------------------------------------------------------------
void LoadQuadTechniques()
{
    for( UINT i = 0; i < MAX_QUAD_TECHNIQUES; i++ )
    {
        char mbstr[MAX_PATH];
        if ( !WideCharToMultiByte( CP_ACP, 0, g_szQuadTechniques[i], -1, mbstr, MAX_PATH, 0, 0 ) )
            continue;

        g_pTechniqueQuad[i] = g_pEffect->GetTechniqueByName( mbstr );

        g_SampleUI.GetComboBox( IDC_QUADRENDER_MODE )->AddItem( g_szQuadTechniques[i], ( void* )( UINT64 )i );

    }
}


//--------------------------------------------------------------------------------------
// LoadDepthStencilStates
// Create a set of depth stencil states for non-FX state managment.  These states
// will later be set using OMSetDepthStencilState in OnD3D11FrameRender.
//--------------------------------------------------------------------------------------
void LoadDepthStencilStates( ID3D11Device* pd3dDevice )
{
    static const BOOL bDepthEnable[ MAX_DEPTH_STENCIL_MODES ] =
    {
        FALSE,
        TRUE,
        TRUE,
        FALSE,
        TRUE,
        TRUE,
        FALSE,
        TRUE,
        TRUE
    };

    static const BOOL bStencilEnable[ MAX_DEPTH_STENCIL_MODES ] =
    {
        FALSE,
        FALSE,
        FALSE,
        TRUE,
        TRUE,
        TRUE,
        TRUE,
        TRUE,
        TRUE
    };

    static const D3D11_COMPARISON_FUNC compFunc[ MAX_DEPTH_STENCIL_MODES ] =
    {
        D3D11_COMPARISON_LESS,
        D3D11_COMPARISON_LESS,
        D3D11_COMPARISON_GREATER,
        D3D11_COMPARISON_LESS,
        D3D11_COMPARISON_LESS,
        D3D11_COMPARISON_GREATER,
        D3D11_COMPARISON_LESS,
        D3D11_COMPARISON_LESS,
        D3D11_COMPARISON_GREATER,
    };

    static const D3D11_STENCIL_OP FailOp[ MAX_DEPTH_STENCIL_MODES ] =
    {
        D3D11_STENCIL_OP_KEEP,
        D3D11_STENCIL_OP_KEEP,
        D3D11_STENCIL_OP_KEEP,

        D3D11_STENCIL_OP_INCR,
        D3D11_STENCIL_OP_INCR,
        D3D11_STENCIL_OP_INCR,

        D3D11_STENCIL_OP_KEEP,
        D3D11_STENCIL_OP_KEEP,
        D3D11_STENCIL_OP_KEEP,
    };

    static const D3D11_STENCIL_OP PassOp[ MAX_DEPTH_STENCIL_MODES ] =
    {
        D3D11_STENCIL_OP_KEEP,
        D3D11_STENCIL_OP_KEEP,
        D3D11_STENCIL_OP_KEEP,

        D3D11_STENCIL_OP_KEEP,
        D3D11_STENCIL_OP_KEEP,
        D3D11_STENCIL_OP_KEEP,

        D3D11_STENCIL_OP_INCR,
        D3D11_STENCIL_OP_INCR,
        D3D11_STENCIL_OP_INCR,
    };

    for( UINT i = 0; i < MAX_DEPTH_STENCIL_MODES; i++ )
    {
        D3D11_DEPTH_STENCIL_DESC dsDesc;
        dsDesc.DepthEnable = bDepthEnable[i];
        dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        dsDesc.DepthFunc = compFunc[i];

        // Stencil test parameters
        dsDesc.StencilEnable = bStencilEnable[i];
        dsDesc.StencilReadMask = 0xFF;
        dsDesc.StencilWriteMask = 0xFF;

        // Stencil operations if pixel is front-facing
        dsDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        dsDesc.FrontFace.StencilDepthFailOp = FailOp[i];
        dsDesc.FrontFace.StencilPassOp = PassOp[i];
        dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

        // Stencil operations if pixel is back-facing
        dsDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        dsDesc.BackFace.StencilDepthFailOp = FailOp[i];
        dsDesc.BackFace.StencilPassOp = PassOp[i];
        dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

        // Create depth stencil state
        pd3dDevice->CreateDepthStencilState( &dsDesc, &g_pDepthStencilStates[i] );

        g_SampleUI.GetComboBox( IDC_SCENEDEPTHSTENCIL_MODE )->AddItem( g_szDepthStencilModes[i],
                                                                       ( void* )( UINT64 )i );
    }
}


//--------------------------------------------------------------------------------------
// LoadRasterizerStates
// Create a set of rasterizer states for non-FX state managment.  These states
// will later be set using RSSetState in OnD3D11FrameRender.
//--------------------------------------------------------------------------------------
void LoadRasterizerStates( ID3D11Device* pd3dDevice )
{
    static const D3D11_FILL_MODE fill[ MAX_RASTERIZER_MODES ] =
    {
        D3D11_FILL_SOLID,
        D3D11_FILL_SOLID,
        D3D11_FILL_SOLID,
        D3D11_FILL_WIREFRAME,
        D3D11_FILL_WIREFRAME,
        D3D11_FILL_WIREFRAME
    };
    static const D3D11_CULL_MODE cull[ MAX_RASTERIZER_MODES ] =
    {
        D3D11_CULL_NONE,
        D3D11_CULL_FRONT,
        D3D11_CULL_BACK,
        D3D11_CULL_NONE,
        D3D11_CULL_FRONT,
        D3D11_CULL_BACK
    };

    for( UINT i = 0; i < MAX_RASTERIZER_MODES; i++ )
    {
        D3D11_RASTERIZER_DESC rasterizerState;
        rasterizerState.FillMode = fill[i];
        rasterizerState.CullMode = cull[i];
        rasterizerState.FrontCounterClockwise = false;
        rasterizerState.DepthBias = false;
        rasterizerState.DepthBiasClamp = 0;
        rasterizerState.SlopeScaledDepthBias = 0;
        rasterizerState.DepthClipEnable = true;
        rasterizerState.ScissorEnable = false;
        rasterizerState.MultisampleEnable = false;
        rasterizerState.AntialiasedLineEnable = false;
        pd3dDevice->CreateRasterizerState( &rasterizerState, &g_pRasterStates[i] );

        g_SampleUI.GetComboBox( IDC_SCENERASTERIZER_MODE )->AddItem( g_szRasterizerModes[i], ( void* )( UINT64 )i );
    }
}
