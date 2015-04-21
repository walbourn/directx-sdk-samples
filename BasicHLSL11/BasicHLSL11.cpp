//--------------------------------------------------------------------------------------
// File: BasicHLSL11.cpp
//
// This sample shows a simple example of the Microsoft High-Level Shader Language (HLSL)
// with Direct3D 11 using the DUXT framework.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "SDKmisc.h"
#include "SDKMesh.h"
#include "resource.h"

#pragma warning( disable : 4100 )

using namespace DirectX;

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
CModelViewerCamera          g_Camera;               // A model viewing camera
CDXUTDirectionWidget        g_LightControl;
CD3DSettingsDlg             g_D3DSettingsDlg;       // Device settings dialog
CDXUTDialog                 g_HUD;                  // manages the 3D   
CDXUTDialog                 g_SampleUI;             // dialog for sample specific controls
XMMATRIX                    g_mCenterMesh;
float                       g_fLightScale;
int                         g_nNumActiveLights;
int                         g_nActiveLight;
bool                        g_bShowHelp = false;    // If true, it renders the UI control text

CDXUTTextHelper*            g_pTxtHelper = nullptr;

CDXUTSDKMesh                g_Mesh11;

ID3D11InputLayout*          g_pVertexLayout11 = nullptr;
ID3D11VertexShader*         g_pVertexShader = nullptr;
ID3D11PixelShader*          g_pPixelShader = nullptr;
ID3D11SamplerState*         g_pSamLinear = nullptr;

struct CB_VS_PER_OBJECT
{
    XMFLOAT4X4 m_WorldViewProj;
    XMFLOAT4X4 m_World;
};
UINT                        g_iCBVSPerObjectBind = 0;

struct CB_PS_PER_OBJECT
{
    XMFLOAT4 m_vObjectColor;
};
UINT                        g_iCBPSPerObjectBind = 0;

struct CB_PS_PER_FRAME
{
    XMFLOAT4 m_vLightDirAmbient;
};
UINT                        g_iCBPSPerFrameBind = 1;

ID3D11Buffer*               g_pcbVSPerObject = nullptr;
ID3D11Buffer*               g_pcbPSPerObject = nullptr;
ID3D11Buffer*               g_pcbPSPerFrame = nullptr;

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           2
#define IDC_CHANGEDEVICE        3
#define IDC_TOGGLEWARP          4

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );

bool CALLBACK IsD3D11DeviceAcceptable(const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext );
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D11DestroyDevice( void* pUserContext );
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                  float fElapsedTime, void* pUserContext );

void InitApp();
void RenderText();


//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow )
{
    // Enable run-time memory check for debug builds.
#ifdef _DEBUG
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // DXUT will create and use the best device
    // that is available on the system depending on which D3D callbacks are set below

    // Set DXUT callbacks
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackFrameMove( OnFrameMove );

    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );

    InitApp();
    DXUTInit( true, true, nullptr ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"BasicHLSL11" );
    DXUTCreateDevice (D3D_FEATURE_LEVEL_9_2, true, 800, 600 );
    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    static const XMVECTORF32 s_vLightDir = { -1.f, 1, -1.f, 0.f };
    XMVECTOR vLightDir = XMVector3Normalize( s_vLightDir );
    g_LightControl.SetLightDirection( vLightDir );

    // Initialize dialogs
    g_D3DSettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 0, iY, 170, 23 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 0, iY += 26, 170, 23, VK_F2 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 0, iY += 26, 170, 23, VK_F3 );
    g_HUD.AddButton( IDC_TOGGLEWARP, L"Toggle WARP (F4)", 0, iY += 26, 170, 23, VK_F4 );

    g_SampleUI.SetCallback( OnGUIEvent ); iY = 10;
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );
}


//--------------------------------------------------------------------------------------
// Render the help and statistics text
//--------------------------------------------------------------------------------------
void RenderText()
{
    UINT nBackBufferHeight = DXUTGetDXGIBackBufferSurfaceDesc()->Height;

    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 2, 0 );
    g_pTxtHelper->SetForegroundColor( Colors::Yellow );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );

    // Draw help
    if( g_bShowHelp )
    {
        g_pTxtHelper->SetInsertionPos( 2, nBackBufferHeight - 20 * 6 );
        g_pTxtHelper->SetForegroundColor( Colors::Orange );
        g_pTxtHelper->DrawTextLine( L"Controls:" );

        g_pTxtHelper->SetInsertionPos( 20, nBackBufferHeight - 20 * 5 );
        g_pTxtHelper->DrawTextLine( L"Rotate model: Left mouse button\n"
                                    L"Rotate light: Right mouse button\n"
                                    L"Rotate camera: Middle mouse button\n"
                                    L"Zoom camera: Mouse wheel scroll\n" );

        g_pTxtHelper->SetInsertionPos( 550, nBackBufferHeight - 20 * 5 );
        g_pTxtHelper->DrawTextLine( L"Hide help: F1\n"
                                    L"Quit: ESC\n" );
    }
    else
    {
        g_pTxtHelper->SetForegroundColor( Colors::White );
        g_pTxtHelper->DrawTextLine( L"Press F1 for help" );
    }

    g_pTxtHelper->End();
}


//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext )
{
    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass messages to settings dialog if its active
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;
    *pbNoFurtherProcessing = g_SampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    g_LightControl.HandleMessages( hWnd, uMsg, wParam, lParam );

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
            case VK_F1:
                g_bShowHelp = !g_bShowHelp; break;
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
            g_D3DSettingsDlg.SetActive( !g_D3DSettingsDlg.IsActive() );
            break;
    }

}


//--------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext )
{
    HRESULT hr;

    auto pd3dImmediateContext = DXUTGetD3D11DeviceContext();
    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );
    g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15 );

    XMFLOAT3 vCenter( 0.25767413f, -28.503521f, 111.00689f );
    FLOAT fObjectRadius = 378.15607f;

    g_mCenterMesh = XMMatrixTranslation( -vCenter.x, -vCenter.y, -vCenter.z );
    XMMATRIX m = XMMatrixRotationY( XM_PI );
    g_mCenterMesh *= m;
    m = XMMatrixRotationX( XM_PI / 2.0f );
    g_mCenterMesh *= m;

    // Compile the shaders using the lowest possible profile for broadest feature level support

    // Best practice would be to compile these offline as part of the build, but it's more convienent for experimentation to compile at runtime.
    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    // Disable optimizations to further improve shader debugging
    dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3DBlob* pVertexShaderBuffer = nullptr;
    V_RETURN( DXUTCompileFromFile( L"BasicHLSL11_VS.hlsl", nullptr, "VSMain", "vs_4_0_level_9_1", dwShaderFlags, 0, &pVertexShaderBuffer ) );

    ID3DBlob* pPixelShaderBuffer = nullptr;
    V_RETURN( DXUTCompileFromFile( L"BasicHLSL11_PS.hlsl", nullptr, "PSMain", "ps_4_0_level_9_1", dwShaderFlags, 0, &pPixelShaderBuffer ) );

    // Create the shaders
    V_RETURN( pd3dDevice->CreateVertexShader( pVertexShaderBuffer->GetBufferPointer(),
                                              pVertexShaderBuffer->GetBufferSize(), nullptr, &g_pVertexShader ) );
    DXUT_SetDebugName( g_pVertexShader, "VSMain" );
    V_RETURN( pd3dDevice->CreatePixelShader( pPixelShaderBuffer->GetBufferPointer(),
                                             pPixelShaderBuffer->GetBufferSize(), nullptr, &g_pPixelShader ) );
    DXUT_SetDebugName( g_pPixelShader, "PSMain" );

    // Create our vertex input layout
    const D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    V_RETURN( pd3dDevice->CreateInputLayout( layout, ARRAYSIZE( layout ), pVertexShaderBuffer->GetBufferPointer(),
                                             pVertexShaderBuffer->GetBufferSize(), &g_pVertexLayout11 ) );
    DXUT_SetDebugName( g_pVertexLayout11, "Primary" );

    SAFE_RELEASE( pVertexShaderBuffer );
    SAFE_RELEASE( pPixelShaderBuffer );

    // Load the mesh
    V_RETURN( g_Mesh11.Create( pd3dDevice, L"tiny\\tiny.sdkmesh" ) );

    // Create a sampler state
    D3D11_SAMPLER_DESC SamDesc;
    SamDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SamDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.MipLODBias = 0.0f;
    SamDesc.MaxAnisotropy = 1;
    SamDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    SamDesc.BorderColor[0] = SamDesc.BorderColor[1] = SamDesc.BorderColor[2] = SamDesc.BorderColor[3] = 0;
    SamDesc.MinLOD = 0;
    SamDesc.MaxLOD = D3D11_FLOAT32_MAX;
    V_RETURN( pd3dDevice->CreateSamplerState( &SamDesc, &g_pSamLinear ) );
    DXUT_SetDebugName( g_pSamLinear, "Primary" );

    // Setup constant buffers
    D3D11_BUFFER_DESC Desc;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Desc.MiscFlags = 0;

    Desc.ByteWidth = sizeof( CB_VS_PER_OBJECT );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, nullptr, &g_pcbVSPerObject ) );
    DXUT_SetDebugName( g_pcbVSPerObject, "CB_VS_PER_OBJECT" );

    Desc.ByteWidth = sizeof( CB_PS_PER_OBJECT );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, nullptr, &g_pcbPSPerObject ) );
    DXUT_SetDebugName( g_pcbPSPerObject, "CB_PS_PER_OBJECT" );

    Desc.ByteWidth = sizeof( CB_PS_PER_FRAME );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, nullptr, &g_pcbPSPerFrame ) );
    DXUT_SetDebugName( g_pcbPSPerFrame, "CB_PS_PER_FRAME" );

    // Setup the camera's view parameters
    static const XMVECTORF32 s_vecEye = { 0.0f, 0.0f, -100.0f, 0.0f };
    g_Camera.SetViewParams( s_vecEye, g_XMZero );
    g_Camera.SetRadius( fObjectRadius * 3.0f, fObjectRadius * 0.5f, fObjectRadius * 10.0f );

    g_HUD.GetButton( IDC_TOGGLEWARP )->SetEnabled( true );

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
    V_RETURN( g_D3DSettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( XM_PI / 4, fAspectRatio, 2.0f, 4000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( MOUSE_MIDDLE_BUTTON, MOUSE_WHEEL, MOUSE_LEFT_BUTTON );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 170, 300 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                  float fElapsedTime, void* pUserContext )
{
    HRESULT hr;

    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    // Clear the render target and depth stencil
    auto pRTV = DXUTGetD3D11RenderTargetView();
    pd3dImmediateContext->ClearRenderTargetView( pRTV, Colors::MidnightBlue );
    auto pDSV = DXUTGetD3D11DepthStencilView();
    pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );

    // Get the light direction
    XMVECTOR vLightDir = g_LightControl.GetLightDirection();

    // Per frame cb update
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    V( pd3dImmediateContext->Map( g_pcbPSPerFrame, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    auto pPerFrame = reinterpret_cast<CB_PS_PER_FRAME*>( MappedResource.pData );
    float fAmbient = 0.1f;
    XMStoreFloat3( reinterpret_cast<XMFLOAT3*>( &pPerFrame->m_vLightDirAmbient ), vLightDir );
    pPerFrame->m_vLightDirAmbient.w = fAmbient;
    pd3dImmediateContext->Unmap( g_pcbPSPerFrame, 0 );

    pd3dImmediateContext->PSSetConstantBuffers( g_iCBPSPerFrameBind, 1, &g_pcbPSPerFrame );

    //Get the mesh
    //IA setup
    pd3dImmediateContext->IASetInputLayout( g_pVertexLayout11 );
    UINT Strides[1];
    UINT Offsets[1];
    ID3D11Buffer* pVB[1];
    pVB[0] = g_Mesh11.GetVB11( 0, 0 );
    Strides[0] = ( UINT )g_Mesh11.GetVertexStride( 0, 0 );
    Offsets[0] = 0;
    pd3dImmediateContext->IASetVertexBuffers( 0, 1, pVB, Strides, Offsets );
    pd3dImmediateContext->IASetIndexBuffer( g_Mesh11.GetIB11( 0 ), g_Mesh11.GetIBFormat11( 0 ), 0 );

    // Set the shaders
    pd3dImmediateContext->VSSetShader( g_pVertexShader, nullptr, 0 );
    pd3dImmediateContext->PSSetShader( g_pPixelShader, nullptr, 0 );
    
    // Get the projection & view matrix from the camera class
    XMMATRIX mProj = g_Camera.GetProjMatrix();
    XMMATRIX mView = g_Camera.GetViewMatrix();

    // Set the per object constant data
    XMMATRIX mWorld = g_mCenterMesh * g_Camera.GetWorldMatrix();
    XMMATRIX mWorldViewProjection = mWorld * mView * mProj;
        
    // VS Per object
    V( pd3dImmediateContext->Map( g_pcbVSPerObject, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    auto pVSPerObject = reinterpret_cast<CB_VS_PER_OBJECT*>( MappedResource.pData );
    XMMATRIX mt = XMMatrixTranspose( mWorldViewProjection );
    XMStoreFloat4x4( &pVSPerObject->m_WorldViewProj, mt );
    mt = XMMatrixTranspose( mWorld );
    XMStoreFloat4x4( &pVSPerObject->m_World, mt );
    pd3dImmediateContext->Unmap( g_pcbVSPerObject, 0 );

    pd3dImmediateContext->VSSetConstantBuffers( g_iCBVSPerObjectBind, 1, &g_pcbVSPerObject );

    // PS Per object
    V( pd3dImmediateContext->Map( g_pcbPSPerObject, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    auto pPSPerObject = reinterpret_cast<CB_PS_PER_OBJECT*>( MappedResource.pData );
    XMStoreFloat4( &pPSPerObject->m_vObjectColor, Colors::White );
    pd3dImmediateContext->Unmap( g_pcbPSPerObject, 0 );

    pd3dImmediateContext->PSSetConstantBuffers( g_iCBPSPerObjectBind, 1, &g_pcbPSPerObject );

    //Render
    pd3dImmediateContext->PSSetSamplers( 0, 1, &g_pSamLinear );

    for( UINT subset = 0; subset < g_Mesh11.GetNumSubsets( 0 ); ++subset )
    {
        // Get the subset
        auto pSubset = g_Mesh11.GetSubset( 0, subset );

        auto PrimType = CDXUTSDKMesh::GetPrimitiveType11( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
        pd3dImmediateContext->IASetPrimitiveTopology( PrimType );

        // Ignores most of the material information in them mesh to use only a simple shader
        auto pDiffuseRV = g_Mesh11.GetMaterial( pSubset->MaterialID )->pDiffuseRV11;
        pd3dImmediateContext->PSSetShaderResources( 0, 1, &pDiffuseRV );

        pd3dImmediateContext->DrawIndexed( ( UINT )pSubset->IndexCount, 0, ( UINT )pSubset->VertexStart );
    }

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
    g_HUD.OnRender( fElapsedTime );
    g_SampleUI.OnRender( fElapsedTime );
    RenderText();
    DXUT_EndPerfEvent();
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
    g_D3DSettingsDlg.OnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE( g_pTxtHelper );

    g_Mesh11.Destroy();
                
    SAFE_RELEASE( g_pVertexLayout11 );
    SAFE_RELEASE( g_pVertexShader );
    SAFE_RELEASE( g_pPixelShader );
    SAFE_RELEASE( g_pSamLinear );

    SAFE_RELEASE( g_pcbVSPerObject );
    SAFE_RELEASE( g_pcbPSPerObject );
    SAFE_RELEASE( g_pcbPSPerFrame );
}



