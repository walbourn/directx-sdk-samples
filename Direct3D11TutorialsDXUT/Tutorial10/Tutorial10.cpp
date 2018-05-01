//--------------------------------------------------------------------------------------
// File: Tutorial10.cpp
//
// Advanced DXUT
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "SDKmisc.h"
#include "SDKmesh.h"

#pragma warning( disable : 4100 )

using namespace DirectX;

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct CBNeverChanges
{
    XMFLOAT4 vLightDir;
};

struct CBChangesEveryFrame
{
    XMFLOAT4X4 mWorldViewProj;
    XMFLOAT4X4 mWorld;
    XMFLOAT4   vMisc;
};


//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
CModelViewerCamera          g_Camera;               // A model viewing camera
CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg             g_SettingsDlg;          // Device settings dialog
CDXUTTextHelper*            g_pTxtHelper = nullptr;
CDXUTDialog                 g_HUD;                  // dialog for standard controls
CDXUTDialog                 g_SampleUI;             // dialog for sample specific controls

// Direct3D 11 resources
ID3D11VertexShader*         g_pVertexShader = nullptr;
ID3D11PixelShader*          g_pPixelShader = nullptr;
ID3D11InputLayout*          g_pVertexLayout = nullptr;
CDXUTSDKMesh                g_Mesh;
ID3D11Buffer*               g_pCBNeverChanges = nullptr;
ID3D11Buffer*               g_pCBChangesEveryFrame = nullptr;
ID3D11SamplerState*         g_pSamplerLinear = nullptr;
XMMATRIX                    g_World;

static const XMVECTORF32 s_LightDir = { -0.577f, 0.577f, -0.577f, 0.f };

float                       g_fModelPuffiness = 0.0f;
bool                        g_bSpinning = true;

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           2
#define IDC_CHANGEDEVICE        3
#define IDC_TOGGLEWARP          4
#define IDC_TOGGLESPIN          5
#define IDC_PUFF_SCALE          6
#define IDC_PUFF_STATIC         7


//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
void RenderText();
void InitApp();


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

    // Compile the vertex shader
    ID3DBlob* pVSBlob = nullptr;
    V_RETURN( DXUTCompileFromFile( L"Tutorial10.fx", nullptr, "VS", "vs_4_0", dwShaderFlags, 0, &pVSBlob ) );

    // Create the vertex shader
    hr = pd3dDevice->CreateVertexShader( pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &g_pVertexShader );
    if( FAILED( hr ) )
    {    
        SAFE_RELEASE( pVSBlob );
        return hr;
    }

    // Define the input layout
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT numElements = ARRAYSIZE( layout );

    // Create the input layout
    hr = pd3dDevice->CreateInputLayout( layout, numElements, pVSBlob->GetBufferPointer(),
                                        pVSBlob->GetBufferSize(), &g_pVertexLayout );
    SAFE_RELEASE( pVSBlob );
    if( FAILED( hr ) )
        return hr;

    // Set the input layout
    pd3dImmediateContext->IASetInputLayout( g_pVertexLayout );

    // Compile the pixel shader
    ID3DBlob* pPSBlob  = nullptr;
    V_RETURN( DXUTCompileFromFile( L"Tutorial10.fx", nullptr, "PS", "ps_4_0", dwShaderFlags, 0, &pPSBlob  ) );

    // Create the pixel shader
    hr = pd3dDevice->CreatePixelShader( pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pPixelShader );
    SAFE_RELEASE( pPSBlob );
    if( FAILED( hr ) )
        return hr;

    // Load the mesh
    V_RETURN( g_Mesh.Create( pd3dDevice, L"Tiny\\tiny.sdkmesh" ) );

    // Create the constant buffers
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(CBChangesEveryFrame);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    V_RETURN( pd3dDevice->CreateBuffer( &bd, nullptr, &g_pCBChangesEveryFrame ) );

    bd.ByteWidth = sizeof(CBNeverChanges);
    V_RETURN( pd3dDevice->CreateBuffer( &bd, nullptr, &g_pCBNeverChanges ) );

    D3D11_MAPPED_SUBRESOURCE MappedResource;
    V( pd3dImmediateContext->Map( g_pCBNeverChanges, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    auto pCB = reinterpret_cast<CBNeverChanges*>( MappedResource.pData );
    XMStoreFloat3( reinterpret_cast<XMFLOAT3*>( &pCB->vLightDir ), s_LightDir );
    pCB->vLightDir.w = 1.f;
    pd3dImmediateContext->Unmap( g_pCBNeverChanges , 0 );

    // Create the sample state
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    V_RETURN( pd3dDevice->CreateSamplerState( &sampDesc, &g_pSamplerLinear ) );

    // Initialize the world matrices
    g_World = XMMatrixIdentity();

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
    g_Camera.SetProjParams( XM_PI / 4, fAspectRatio, 0.1f, 1000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( MOUSE_LEFT_BUTTON, MOUSE_WHEEL, MOUSE_MIDDLE_BUTTON );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 170, 300 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Rotate cube around the origin
    

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
// Render the help and statistics text.
//--------------------------------------------------------------------------------------
void RenderText()
{
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 5, 5 );
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
    pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );

    XMMATRIX mView = g_Camera.GetViewMatrix();
    XMMATRIX mProj = g_Camera.GetProjMatrix();
    XMMATRIX mWorldViewProjection = g_World * mView * mProj;

    // Update constant buffer that changes once per frame
    HRESULT hr;
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    V( pd3dImmediateContext->Map( g_pCBChangesEveryFrame , 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    auto pCB = reinterpret_cast<CBChangesEveryFrame*>( MappedResource.pData );
    XMStoreFloat4x4( &pCB->mWorldViewProj, XMMatrixTranspose( mWorldViewProjection ) );
    XMStoreFloat4x4( &pCB->mWorld, XMMatrixTranspose( g_World ) );
    pCB->vMisc.x = g_fModelPuffiness;
    pd3dImmediateContext->Unmap( g_pCBChangesEveryFrame , 0 );

    //
    // Set the Vertex Layout
    //
    pd3dImmediateContext->IASetInputLayout( g_pVertexLayout );

    //
    // Render the mesh
    //
    UINT Strides[1];
    UINT Offsets[1];
    ID3D11Buffer* pVB[1];
    pVB[0] = g_Mesh.GetVB11( 0, 0 );
    Strides[0] = ( UINT )g_Mesh.GetVertexStride( 0, 0 );
    Offsets[0] = 0;
    pd3dImmediateContext->IASetVertexBuffers( 0, 1, pVB, Strides, Offsets );
    pd3dImmediateContext->IASetIndexBuffer( g_Mesh.GetIB11( 0 ), g_Mesh.GetIBFormat11( 0 ), 0 );

    pd3dImmediateContext->VSSetShader( g_pVertexShader, nullptr, 0 );
    pd3dImmediateContext->VSSetConstantBuffers( 0, 1, &g_pCBNeverChanges );
    pd3dImmediateContext->VSSetConstantBuffers( 1, 1, &g_pCBChangesEveryFrame );

    pd3dImmediateContext->PSSetShader( g_pPixelShader, nullptr, 0 );
    pd3dImmediateContext->PSSetConstantBuffers( 1, 1, &g_pCBChangesEveryFrame );
    pd3dImmediateContext->PSSetSamplers( 0, 1, &g_pSamplerLinear );

    for( UINT subset = 0; subset < g_Mesh.GetNumSubsets( 0 ); ++subset )
    {
        auto pSubset = g_Mesh.GetSubset( 0, subset );

        auto PrimType = g_Mesh.GetPrimitiveType11( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
        pd3dImmediateContext->IASetPrimitiveTopology( PrimType );

        // Ignores most of the material information in them mesh to use only a simple shader
        auto pDiffuseRV = g_Mesh.GetMaterial( pSubset->MaterialID )->pDiffuseRV11;
        pd3dImmediateContext->PSSetShaderResources( 0, 1, &pDiffuseRV );

        pd3dImmediateContext->DrawIndexed( ( UINT )pSubset->IndexCount, 0, ( UINT )pSubset->VertexStart );
    }

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

    SAFE_RELEASE( g_pVertexLayout );
    SAFE_RELEASE( g_pVertexShader );
    SAFE_RELEASE( g_pPixelShader );
    SAFE_RELEASE( g_pCBNeverChanges );
    SAFE_RELEASE( g_pCBChangesEveryFrame );
    SAFE_RELEASE( g_pSamplerLinear );
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
        case IDC_TOGGLEWARP:
            DXUTToggleWARP();
            break;
        case IDC_CHANGEDEVICE:
            g_SettingsDlg.SetActive( !g_SettingsDlg.IsActive() );
            break;

        case IDC_TOGGLESPIN:
        {
            g_bSpinning = g_SampleUI.GetCheckBox( IDC_TOGGLESPIN )->GetChecked();
            break;
        }

        case IDC_PUFF_SCALE:
        {
            WCHAR sz[100];
            g_fModelPuffiness = ( float )( g_SampleUI.GetSlider( IDC_PUFF_SCALE )->GetValue() * 0.01f );
            swprintf_s( sz, 100, L"Puffiness: %0.2f", g_fModelPuffiness );
            g_SampleUI.GetStatic( IDC_PUFF_STATIC )->SetText( sz );
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
    DXUTCreateWindow( L"Tutorial10" );

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

    g_SampleUI.SetCallback( OnGUIEvent ); iY = 10;

    WCHAR sz[100];
    iY += 24;
    swprintf_s( sz, 100, L"Puffiness: %0.2f", g_fModelPuffiness );
    g_SampleUI.AddStatic( IDC_PUFF_STATIC, sz, 0 , iY += 26, 170, 22 );
    g_SampleUI.AddSlider( IDC_PUFF_SCALE, 50, iY += 26, 100, 22, 0, 2000, ( int )( g_fModelPuffiness * 100.0f ) );

    iY += 24;
    g_SampleUI.AddCheckBox( IDC_TOGGLESPIN, L"Toggle Spinning", 0, iY += 26, 170, 22, g_bSpinning );
}
