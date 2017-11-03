//--------------------------------------------------------------------------------------
// File: SimpleBezier11.cpp
//
// This sample shows an simple implementation of the DirectX 11 Hardware Tessellator
// for rendering a Bezier Patch.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "SDKmisc.h"
#include "resource.h"
#include "MobiusStrip.h"

#pragma warning( disable : 4100 )

using namespace DirectX;

const DWORD MIN_DIVS = 4;
const DWORD MAX_DIVS = 16; // Min and Max divisions of the patch per side for the slider control

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CDXUTDialogResourceManager          g_DialogResourceManager; // manager for shared resources of dialogs
CModelViewerCamera                  g_Camera;                // A model viewing camera
CD3DSettingsDlg                     g_D3DSettingsDlg;        // Device settings dialog
CDXUTDialog                         g_HUD;                   // manages the 3D   
CDXUTDialog                         g_SampleUI;              // dialog for sample specific controls

// Resources
CDXUTTextHelper*                    g_pTxtHelper = nullptr;

ID3D11InputLayout*                  g_pPatchLayout = nullptr;

ID3D11VertexShader*                 g_pVertexShader = nullptr;
ID3D11HullShader*                   g_pHullShaderInteger = nullptr;
ID3D11HullShader*                   g_pHullShaderFracEven = nullptr;
ID3D11HullShader*                   g_pHullShaderFracOdd = nullptr;
ID3D11DomainShader*                 g_pDomainShader = nullptr;
ID3D11PixelShader*                  g_pPixelShader = nullptr;
ID3D11PixelShader*                  g_pSolidColorPS = nullptr;

ID3D11Buffer*   g_pControlPointVB;                           // Control points for mesh

struct CB_PER_FRAME_CONSTANTS
{
    XMFLOAT4X4  mViewProjection;
    XMFLOAT3    vCameraPosWorld;
    float       fTessellationFactor;
};

ID3D11Buffer*                       g_pcbPerFrame = nullptr;
UINT                                g_iBindPerFrame = 0;

ID3D11RasterizerState*              g_pRasterizerStateSolid = nullptr;
ID3D11RasterizerState*              g_pRasterizerStateWireframe = nullptr;

// Control variables
float                               g_fSubdivs = 8;          // Startup subdivisions per side
bool                                g_bDrawWires = false;    // Draw the mesh with wireframe overlay

enum E_PARTITION_MODE
{
   PARTITION_INTEGER,
   PARTITION_FRACTIONAL_EVEN,
   PARTITION_FRACTIONAL_ODD
};

E_PARTITION_MODE                    g_iPartitionMode = PARTITION_INTEGER;

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN      1
#define IDC_TOGGLEREF             3
#define IDC_CHANGEDEVICE          4

#define IDC_PATCH_SUBDIVS         5
#define IDC_PATCH_SUBDIVS_STATIC  6
#define IDC_TOGGLE_LINES          7
#define IDC_PARTITION_MODE        8
#define IDC_PARTITION_INTEGER     9
#define IDC_PARTITION_FRAC_EVEN   10
#define IDC_PARTITION_FRAC_ODD    11

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );

bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
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
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // DXUT will create and use the best device
    // that is available on the system depending on which D3D callbacks are set below

    // Set DXUT callbacks
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackFrameMove( OnFrameMove );

    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );

    InitApp();
    DXUTInit( true, true ); // Parse the command line, show msgboxes on error, and an extra cmd line param to force REF for now
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"SimpleBezier11" );
    DXUTCreateDevice( D3D_FEATURE_LEVEL_11_0,  true, 800, 600 );
    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    // Initialize dialogs
    g_D3DSettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 20;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 0, iY, 170, 22 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 0, iY += 26, 170, 22, VK_F3 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 0, iY += 26, 170, 22, VK_F2 );

    g_SampleUI.SetCallback( OnGUIEvent ); iY = 10;

    WCHAR sz[100];
    iY += 24;
    swprintf_s( sz, L"Patch Divisions: %2.1f", g_fSubdivs );
    g_SampleUI.AddStatic( IDC_PATCH_SUBDIVS_STATIC, sz, 10, iY += 26, 150, 22 );
    g_SampleUI.AddSlider( IDC_PATCH_SUBDIVS, 10, iY += 24, 150, 22, 10 * MIN_DIVS, 10 * MAX_DIVS, (int)(g_fSubdivs * 10) );

    iY += 24;
    g_SampleUI.AddCheckBox( IDC_TOGGLE_LINES, L"Toggle Wires", 20, iY += 26, 150, 22, g_bDrawWires );

    iY += 24;
    g_SampleUI.AddRadioButton( IDC_PARTITION_INTEGER, IDC_PARTITION_MODE, L"Integer", 20, iY += 26, 170, 22 );
    g_SampleUI.AddRadioButton( IDC_PARTITION_FRAC_EVEN, IDC_PARTITION_MODE, L"Fractional Even", 20, iY += 26, 170, 22 );
    g_SampleUI.AddRadioButton( IDC_PARTITION_FRAC_ODD, IDC_PARTITION_MODE, L"Fractional Odd", 20, iY += 26, 170, 22 );
    g_SampleUI.GetRadioButton( IDC_PARTITION_INTEGER )->SetChecked( true );

    // Setup the camera's view parameters
    static const XMVECTORF32 s_vecEye = { 1.0f, 1.5f, -3.5f, 0.f };
    static const XMVECTORF32 s_vecAt = { 0.0f, 0.0f, 0.0f, 0.f };
    g_Camera.SetViewParams( s_vecEye, s_vecAt );

}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene
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
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 2, 0 );
    g_pTxtHelper->SetForegroundColor( Colors::Yellow );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );

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

    // Pass all remaining windows messages to camera so it can respond to user input
    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
}


//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    switch( nControlID )
    {
            // Standard DXUT controls
        case IDC_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen(); break;
        case IDC_TOGGLEREF:
            DXUTToggleREF(); break;
        case IDC_CHANGEDEVICE:
            g_D3DSettingsDlg.SetActive( !g_D3DSettingsDlg.IsActive() ); break;

            // Custom app controls
        case IDC_PATCH_SUBDIVS:
        {
            g_fSubdivs = g_SampleUI.GetSlider( IDC_PATCH_SUBDIVS )->GetValue() / 10.0f;

            WCHAR sz[100];
            swprintf_s( sz, L"Patch Divisions: %2.1f", g_fSubdivs );
            g_SampleUI.GetStatic( IDC_PATCH_SUBDIVS_STATIC )->SetText( sz );
        }
            break;
        case IDC_TOGGLE_LINES:
            g_bDrawWires = g_SampleUI.GetCheckBox( IDC_TOGGLE_LINES )->GetChecked();
            break;
        case IDC_PARTITION_INTEGER:
            g_iPartitionMode = PARTITION_INTEGER;
            break;
        case IDC_PARTITION_FRAC_EVEN:
            g_iPartitionMode = PARTITION_FRACTIONAL_EVEN;
            break;
        case IDC_PARTITION_FRAC_ODD:
            g_iPartitionMode = PARTITION_FRACTIONAL_ODD;
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

    // Compile shaders
    ID3DBlob* pBlobVS = nullptr;
    ID3DBlob* pBlobHSInt = nullptr;
    ID3DBlob* pBlobHSFracEven = nullptr;
    ID3DBlob* pBlobHSFracOdd = nullptr;
    ID3DBlob* pBlobDS = nullptr;
    ID3DBlob* pBlobPS = nullptr;
    ID3DBlob* pBlobPSSolid = nullptr;

    // This macro is used to compile the hull shader with different partition modes
    // Please see the partitioning mode attribute for the hull shader for more information
    D3D_SHADER_MACRO integerPartitioning[] = { { "BEZIER_HS_PARTITION", "\"integer\"" }, { 0 } };
    D3D_SHADER_MACRO fracEvenPartitioning[] = { { "BEZIER_HS_PARTITION", "\"fractional_even\"" }, { 0 } };
    D3D_SHADER_MACRO fracOddPartitioning[] = { { "BEZIER_HS_PARTITION", "\"fractional_odd\"" }, { 0 } };

    V_RETURN( DXUTCompileFromFile( L"SimpleBezier11.hlsl", nullptr, "BezierVS", "vs_5_0",
                                   D3DCOMPILE_ENABLE_STRICTNESS, 0, &pBlobVS ) );
    V_RETURN( DXUTCompileFromFile( L"SimpleBezier11.hlsl", integerPartitioning, "BezierHS", "hs_5_0",
                                   D3DCOMPILE_ENABLE_STRICTNESS, 0, &pBlobHSInt ) );
    V_RETURN( DXUTCompileFromFile( L"SimpleBezier11.hlsl", fracEvenPartitioning, "BezierHS", "hs_5_0",
                                   D3DCOMPILE_ENABLE_STRICTNESS, 0, &pBlobHSFracEven ) );
    V_RETURN( DXUTCompileFromFile( L"SimpleBezier11.hlsl", fracOddPartitioning, "BezierHS", "hs_5_0",
                                   D3DCOMPILE_ENABLE_STRICTNESS, 0, &pBlobHSFracOdd ) );
    V_RETURN( DXUTCompileFromFile( L"SimpleBezier11.hlsl", nullptr, "BezierDS", "ds_5_0",
                                   D3DCOMPILE_ENABLE_STRICTNESS, 0, &pBlobDS ) );
    V_RETURN( DXUTCompileFromFile( L"SimpleBezier11.hlsl", nullptr, "BezierPS", "ps_5_0",
                                   D3DCOMPILE_ENABLE_STRICTNESS, 0, &pBlobPS ) );
    V_RETURN( DXUTCompileFromFile( L"SimpleBezier11.hlsl", nullptr, "SolidColorPS", "ps_5_0",
                                   D3DCOMPILE_ENABLE_STRICTNESS, 0, &pBlobPSSolid ) );

    // Create shaders
    V_RETURN( pd3dDevice->CreateVertexShader( pBlobVS->GetBufferPointer(), pBlobVS->GetBufferSize(), nullptr, &g_pVertexShader ) );
    DXUT_SetDebugName( g_pVertexShader, "BezierVS" );

    V_RETURN( pd3dDevice->CreateHullShader( pBlobHSInt->GetBufferPointer(), pBlobHSInt->GetBufferSize(), nullptr, &g_pHullShaderInteger ) );
    DXUT_SetDebugName( g_pHullShaderInteger, "BezierHS int" );

    V_RETURN( pd3dDevice->CreateHullShader( pBlobHSFracEven->GetBufferPointer(), pBlobHSFracEven->GetBufferSize(), nullptr, &g_pHullShaderFracEven ) );
    DXUT_SetDebugName( g_pHullShaderFracEven, "BezierHS frac even" );

    V_RETURN( pd3dDevice->CreateHullShader( pBlobHSFracOdd->GetBufferPointer(), pBlobHSFracOdd->GetBufferSize(), nullptr, &g_pHullShaderFracOdd ) );
    DXUT_SetDebugName( g_pHullShaderFracOdd, "BezierHS frac odd" );

    V_RETURN( pd3dDevice->CreateDomainShader( pBlobDS->GetBufferPointer(), pBlobDS->GetBufferSize(), nullptr, &g_pDomainShader ) );
    DXUT_SetDebugName( g_pDomainShader, "BezierDS" );

    V_RETURN( pd3dDevice->CreatePixelShader( pBlobPS->GetBufferPointer(), pBlobPS->GetBufferSize(), nullptr, &g_pPixelShader ) );
    DXUT_SetDebugName( g_pPixelShader, "BezierPS" );

    V_RETURN( pd3dDevice->CreatePixelShader( pBlobPSSolid->GetBufferPointer(), pBlobPSSolid->GetBufferSize(), nullptr, &g_pSolidColorPS ) );
    DXUT_SetDebugName( g_pSolidColorPS, "SolidColorPS" );

    // Create our vertex input layout - this matches the BEZIER_CONTROL_POINT structure
    const D3D11_INPUT_ELEMENT_DESC patchlayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    V_RETURN( pd3dDevice->CreateInputLayout( patchlayout, ARRAYSIZE( patchlayout ), pBlobVS->GetBufferPointer(),
                                             pBlobVS->GetBufferSize(), &g_pPatchLayout ) );
    DXUT_SetDebugName( g_pPatchLayout, "Primary" );

    SAFE_RELEASE( pBlobVS );
    SAFE_RELEASE( pBlobHSInt );
    SAFE_RELEASE( pBlobHSFracEven );
    SAFE_RELEASE( pBlobHSFracOdd );
    SAFE_RELEASE( pBlobDS );
    SAFE_RELEASE( pBlobPS );
    SAFE_RELEASE( pBlobPSSolid );

    // Create constant buffers
    D3D11_BUFFER_DESC Desc;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Desc.MiscFlags = 0;

    Desc.ByteWidth = sizeof( CB_PER_FRAME_CONSTANTS );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, nullptr, &g_pcbPerFrame ) );
    DXUT_SetDebugName( g_pcbPerFrame, "CB_PER_FRAME_CONSTANTS" );

    // Create solid and wireframe rasterizer state objects
    D3D11_RASTERIZER_DESC RasterDesc;
    ZeroMemory( &RasterDesc, sizeof(D3D11_RASTERIZER_DESC) );
    RasterDesc.FillMode = D3D11_FILL_SOLID;
    RasterDesc.CullMode = D3D11_CULL_NONE;
    RasterDesc.DepthClipEnable = TRUE;
    V_RETURN( pd3dDevice->CreateRasterizerState( &RasterDesc, &g_pRasterizerStateSolid ) );
    DXUT_SetDebugName( g_pRasterizerStateSolid, "Solid" );

    RasterDesc.FillMode = D3D11_FILL_WIREFRAME;
    V_RETURN( pd3dDevice->CreateRasterizerState( &RasterDesc, &g_pRasterizerStateWireframe ) );
    DXUT_SetDebugName( g_pRasterizerStateWireframe, "Wireframe" );

    D3D11_BUFFER_DESC vbDesc;
    ZeroMemory( &vbDesc, sizeof(D3D11_BUFFER_DESC) );
    vbDesc.ByteWidth = sizeof(BEZIER_CONTROL_POINT) * ARRAYSIZE(g_MobiusStrip);
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbInitData;
    ZeroMemory( &vbInitData, sizeof(vbInitData) );
    vbInitData.pSysMem = g_MobiusStrip;
    V_RETURN( pd3dDevice->CreateBuffer( &vbDesc, &vbInitData, &g_pControlPointVB ) );
    DXUT_SetDebugName( g_pControlPointVB, "Control Points" );

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
    g_Camera.SetProjParams( XM_PI / 4, fAspectRatio, 0.1f, 20.0f );
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
    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    // WVP
    XMMATRIX mProj = g_Camera.GetProjMatrix();
    XMMATRIX mView = g_Camera.GetViewMatrix();

    XMMATRIX mViewProjection = mView * mProj;

    // Update per-frame variables
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    pd3dImmediateContext->Map( g_pcbPerFrame, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
    auto pData = reinterpret_cast<CB_PER_FRAME_CONSTANTS*>( MappedResource.pData );
    XMStoreFloat4x4( &pData->mViewProjection, XMMatrixTranspose( mViewProjection ) );
    XMStoreFloat3( &pData->vCameraPosWorld, g_Camera.GetEyePt() );
    pData->fTessellationFactor = (float)g_fSubdivs;

    pd3dImmediateContext->Unmap( g_pcbPerFrame, 0 );

    // Clear the render target and depth stencil
    auto pRTV = DXUTGetD3D11RenderTargetView();
    pd3dImmediateContext->ClearRenderTargetView( pRTV, Colors::Black );
    auto pDSV = DXUTGetD3D11DepthStencilView();
    pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );

    // Set state for solid rendering
    pd3dImmediateContext->RSSetState( g_pRasterizerStateSolid );

    // Render the meshes
    // Bind all of the CBs
    pd3dImmediateContext->VSSetConstantBuffers( g_iBindPerFrame, 1, &g_pcbPerFrame );
    pd3dImmediateContext->HSSetConstantBuffers( g_iBindPerFrame, 1, &g_pcbPerFrame );
    pd3dImmediateContext->DSSetConstantBuffers( g_iBindPerFrame, 1, &g_pcbPerFrame );
    pd3dImmediateContext->PSSetConstantBuffers( g_iBindPerFrame, 1, &g_pcbPerFrame );

    // Set the shaders
    pd3dImmediateContext->VSSetShader( g_pVertexShader, nullptr, 0 );

    // For this sample, choose either the "integer", "fractional_even",
    // or "fractional_odd" hull shader
    if (g_iPartitionMode == PARTITION_INTEGER)
        pd3dImmediateContext->HSSetShader( g_pHullShaderInteger, nullptr, 0 );
    else if (g_iPartitionMode == PARTITION_FRACTIONAL_EVEN)
        pd3dImmediateContext->HSSetShader( g_pHullShaderFracEven, nullptr, 0 );
    else if (g_iPartitionMode == PARTITION_FRACTIONAL_ODD)
        pd3dImmediateContext->HSSetShader( g_pHullShaderFracOdd, nullptr, 0 );

    pd3dImmediateContext->DSSetShader( g_pDomainShader, nullptr, 0 );
    pd3dImmediateContext->GSSetShader( nullptr, nullptr, 0 );
    pd3dImmediateContext->PSSetShader( g_pPixelShader, nullptr, 0 );

    // Optionally draw the wireframe
    if( g_bDrawWires )
    {
        pd3dImmediateContext->PSSetShader( g_pSolidColorPS, nullptr, 0 );
        pd3dImmediateContext->RSSetState( g_pRasterizerStateWireframe ); 
    }

    // Set the input assembler
    // This sample uses patches with 16 control points each
    // Although the Mobius strip only needs to use a vertex buffer,
    // you can use an index buffer as well by calling IASetIndexBuffer().
    pd3dImmediateContext->IASetInputLayout( g_pPatchLayout );
    UINT Stride = sizeof( BEZIER_CONTROL_POINT );
    UINT Offset = 0;
    pd3dImmediateContext->IASetVertexBuffers( 0, 1, &g_pControlPointVB, &Stride, &Offset );
    pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST );

    // Draw the mesh
    pd3dImmediateContext->Draw( ARRAYSIZE(g_MobiusStrip), 0 );

    pd3dImmediateContext->RSSetState( g_pRasterizerStateSolid );

    // Render the HUD
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
    SAFE_RELEASE( g_pPatchLayout );
    SAFE_RELEASE( g_pcbPerFrame );

    SAFE_RELEASE( g_pVertexShader );
    SAFE_RELEASE( g_pHullShaderInteger );
    SAFE_RELEASE( g_pHullShaderFracEven );
    SAFE_RELEASE( g_pHullShaderFracOdd );
    SAFE_RELEASE( g_pDomainShader );
    SAFE_RELEASE( g_pPixelShader );
    SAFE_RELEASE( g_pSolidColorPS );

    SAFE_RELEASE( g_pRasterizerStateSolid );
    SAFE_RELEASE( g_pRasterizerStateWireframe );

    SAFE_RELEASE( g_pControlPointVB );
}
