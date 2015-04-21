//--------------------------------------------------------------------------------------
// File: BasicHLSLFX11.cpp
//
// This sample shows a simple example of the Microsoft Direct3D 11's High-Level 
// Shader Language (HLSL) using the Effect interface. 
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

#include <d3dx11effect.h>

#define MAX_LIGHTS 3

#pragma warning( disable : 4100 )

using namespace DirectX;

//---------------------------------------------------518-----------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CDXUTDialogResourceManager          g_DialogResourceManager; // manager for shared resources of dialogs
CModelViewerCamera                  g_Camera;               // A model viewing camera
CDXUTDirectionWidget                g_LightControl[MAX_LIGHTS];
CD3DSettingsDlg                     g_D3DSettingsDlg;       // Device settings dialog
CDXUTDialog                         g_HUD;                  // manages the 3D   
CDXUTDialog                         g_SampleUI;             // dialog for sample specific controls
XMMATRIX                            g_mCenterMesh;
float                               g_fLightScale;
int                                 g_nNumActiveLights;
int                                 g_nActiveLight;
bool                                g_bShowHelp = false;    // If true, it renders the UI control text

// Direct3D11 resources
CDXUTTextHelper*                    g_pTxtHelper = nullptr;
CDXUTSDKMesh                        g_Mesh11;
ID3D11InputLayout*                  g_pVertexLayout = nullptr;

ID3DX11Effect*                      g_pEffect = nullptr;
ID3DX11EffectTechnique*             g_pTechRenderSceneWithTexture1Light = nullptr;
ID3DX11EffectTechnique*             g_pTechRenderSceneWithTexture2Light = nullptr;
ID3DX11EffectTechnique*             g_pTechRenderSceneWithTexture3Light = nullptr;
ID3DX11EffectShaderResourceVariable*g_ptxDiffuse = nullptr;
ID3DX11EffectVectorVariable*        g_pLightDir = nullptr;
ID3DX11EffectVectorVariable*        g_pLightDiffuse = nullptr;
ID3DX11EffectMatrixVariable*        g_pmWorldViewProjection = nullptr;
ID3DX11EffectMatrixVariable*        g_pmWorld = nullptr;
ID3DX11EffectScalarVariable*        g_pfTime = nullptr;
ID3DX11EffectVectorVariable*        g_pMaterialDiffuseColor = nullptr;
ID3DX11EffectVectorVariable*        g_pMaterialAmbientColor = nullptr;
ID3DX11EffectScalarVariable*        g_pnNumLights = nullptr;


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4
#define IDC_NUM_LIGHTS          6
#define IDC_NUM_LIGHTS_STATIC   7
#define IDC_ACTIVE_LIGHT        8
#define IDC_LIGHT_SCALE         9
#define IDC_LIGHT_SCALE_STATIC  10
#define IDC_TOGGLEWARP          11

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
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
    DXUTCreateWindow( L"BasicHLSLFX11" );
    DXUTCreateDevice( D3D_FEATURE_LEVEL_9_2, true, 800, 600 );
    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    for( int i = 0; i < MAX_LIGHTS; i++ )
    {
        g_LightControl[i].SetLightDirection( XMFLOAT3( sinf( XM_PI * 2 * i / MAX_LIGHTS - XM_PI / 6 ),
                                                       0, -cosf( XM_PI * 2 * i / MAX_LIGHTS - XM_PI / 6 ) ) );
    }

    g_nActiveLight = 0;
    g_nNumActiveLights = 1;
    g_fLightScale = 1.0f;

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

    WCHAR sz[100];
    iY += 24;
    swprintf_s( sz, 100, L"# Lights: %d", g_nNumActiveLights );
    g_SampleUI.AddStatic( IDC_NUM_LIGHTS_STATIC, sz, 35, iY += 24, 125, 22 );
    g_SampleUI.AddSlider( IDC_NUM_LIGHTS, 50, iY += 24, 100, 22, 1, MAX_LIGHTS, g_nNumActiveLights );

    iY += 24;
    swprintf_s( sz, 100, L"Light scale: %0.2f", g_fLightScale );
    g_SampleUI.AddStatic( IDC_LIGHT_SCALE_STATIC, sz, 35, iY += 24, 125, 22 );
    g_SampleUI.AddSlider( IDC_LIGHT_SCALE, 50, iY += 24, 100, 22, 0, 20, ( int )( g_fLightScale * 10.0f ) );

    iY += 24;
    g_SampleUI.AddButton( IDC_ACTIVE_LIGHT, L"Change active light (K)", 35, iY += 24, 125, 22, 'K' );
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
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 2, 0 );
    g_pTxtHelper->SetForegroundColor( Colors::Yellow );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );

    // Draw help
    if( g_bShowHelp )
    {
        UINT nBackBufferHeight = DXUTGetDXGIBackBufferSurfaceDesc()->Height;
        g_pTxtHelper->SetInsertionPos( 2, nBackBufferHeight - 15 * 6 );
        g_pTxtHelper->SetForegroundColor( Colors::Orange );
        g_pTxtHelper->DrawTextLine( L"Controls:" );

        g_pTxtHelper->SetInsertionPos( 20, nBackBufferHeight - 15 * 5 );
        g_pTxtHelper->DrawTextLine( L"Rotate model: Left mouse button\n"
                                    L"Rotate light: Right mouse button\n"
                                    L"Rotate camera: Middle mouse button\n"
                                    L"Zoom camera: Mouse wheel scroll\n" );

        g_pTxtHelper->SetInsertionPos( 250, nBackBufferHeight - 15 * 5 );
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

    g_LightControl[g_nActiveLight].HandleMessages( hWnd, uMsg, wParam, lParam );

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
            DXUTToggleFullScreen(); break;
        case IDC_TOGGLEREF:
            DXUTToggleREF(); break;
        case IDC_CHANGEDEVICE:
            g_D3DSettingsDlg.SetActive( !g_D3DSettingsDlg.IsActive() ); break;
        case IDC_TOGGLEWARP:
            DXUTToggleWARP(); break;
        case IDC_ACTIVE_LIGHT:
            if( !g_LightControl[g_nActiveLight].IsBeingDragged() )
            {
                g_nActiveLight++;
                g_nActiveLight %= g_nNumActiveLights;
            }
            break;

        case IDC_NUM_LIGHTS:
            if( !g_LightControl[g_nActiveLight].IsBeingDragged() )
            {
                WCHAR sz[100];
                swprintf_s( sz, 100, L"# Lights: %d", g_SampleUI.GetSlider( IDC_NUM_LIGHTS )->GetValue() );
                g_SampleUI.GetStatic( IDC_NUM_LIGHTS_STATIC )->SetText( sz );

                g_nNumActiveLights = g_SampleUI.GetSlider( IDC_NUM_LIGHTS )->GetValue();
                g_nActiveLight %= g_nNumActiveLights;
            }
            break;

        case IDC_LIGHT_SCALE:
            g_fLightScale = ( float )( g_SampleUI.GetSlider( IDC_LIGHT_SCALE )->GetValue() * 0.10f );

            WCHAR sz[100];
            swprintf_s( sz, 100, L"Light scale: %0.2f", g_fLightScale );
            g_SampleUI.GetStatic( IDC_LIGHT_SCALE_STATIC )->SetText( sz );
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
    HRESULT hr = S_OK;

    auto pd3dImmediateContext = DXUTGetD3D11DeviceContext();
    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );
    g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15 );

    g_SampleUI.GetStatic( IDC_NUM_LIGHTS_STATIC )->SetVisible( false );
    g_SampleUI.GetSlider( IDC_NUM_LIGHTS )->SetVisible( false );
    g_SampleUI.GetButton( IDC_ACTIVE_LIGHT )->SetVisible( false );

    XMFLOAT3 vCenter( 0.25767413f, -28.503521f, 111.00689f );
    FLOAT fObjectRadius = 378.15607f;

    g_mCenterMesh = XMMatrixTranslation( -vCenter.x, -vCenter.y, -vCenter.z );
    XMMATRIX m = XMMatrixRotationY( XM_PI );
    g_mCenterMesh *= m;
    m = XMMatrixRotationX( XM_PI / 2.0f );
    g_mCenterMesh *= m;

    g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15 );
    for( int i = 0; i < MAX_LIGHTS; i++ )
        g_LightControl[i].SetRadius( fObjectRadius );

    // Compile and create the effect.

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

    WCHAR szShaderPath[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( szShaderPath, MAX_PATH, L"BasicHLSLFX11.fx" ) );

    ID3DBlob* pErrorBlob = nullptr;
    hr = D3DX11CompileEffectFromFile( szShaderPath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, dwShaderFlags, 0, pd3dDevice, &g_pEffect, &pErrorBlob );

    if ( pErrorBlob )
    {
        OutputDebugStringA( reinterpret_cast<const char*>( pErrorBlob->GetBufferPointer() ) );
        pErrorBlob->Release();
    }

    if ( FAILED(hr) )
        return hr;

#else

    ID3DBlob* pEffectBuffer = nullptr;
    V_RETURN( DXUTCompileFromFile( L"BasicHLSLFX11.fx", nullptr, "none", "fx_5_0", dwShaderFlags, 0, &pEffectBuffer ) );
    hr = D3DX11CreateEffectFromMemory( pEffectBuffer->GetBufferPointer(), pEffectBuffer->GetBufferSize(), 0, pd3dDevice, &g_pEffect );
    SAFE_RELEASE( pEffectBuffer );
    if ( FAILED(hr) )
        return hr;

#endif

    // Obtain technique objects
    g_pTechRenderSceneWithTexture1Light = g_pEffect->GetTechniqueByName( "RenderSceneWithTexture1Light" );
    g_pTechRenderSceneWithTexture2Light = g_pEffect->GetTechniqueByName( "RenderSceneWithTexture2Light" );
    g_pTechRenderSceneWithTexture3Light = g_pEffect->GetTechniqueByName( "RenderSceneWithTexture3Light" );

    // Obtain variables
    g_ptxDiffuse = g_pEffect->GetVariableByName( "g_MeshTexture" )->AsShaderResource();
    g_pLightDir = g_pEffect->GetVariableByName( "g_LightDir" )->AsVector();
    g_pLightDiffuse = g_pEffect->GetVariableByName( "g_LightDiffuse" )->AsVector();
    g_pmWorldViewProjection = g_pEffect->GetVariableByName( "g_mWorldViewProjection" )->AsMatrix();
    g_pmWorld = g_pEffect->GetVariableByName( "g_mWorld" )->AsMatrix();
    g_pfTime = g_pEffect->GetVariableByName( "g_fTime" )->AsScalar();
    g_pMaterialAmbientColor = g_pEffect->GetVariableByName( "g_MaterialAmbientColor" )->AsVector();
    g_pMaterialDiffuseColor = g_pEffect->GetVariableByName( "g_MaterialDiffuseColor" )->AsVector();
    g_pnNumLights = g_pEffect->GetVariableByName( "g_nNumLights" )->AsScalar();

    // Create our vertex input layout
    const D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    D3DX11_PASS_DESC PassDesc;
    V_RETURN( g_pTechRenderSceneWithTexture1Light->GetPassByIndex( 0 )->GetDesc( &PassDesc ) );
    V_RETURN( pd3dDevice->CreateInputLayout( layout, 3, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pVertexLayout ) );

    // Load the mesh
    V_RETURN( g_Mesh11.Create( pd3dDevice, L"tiny\\tiny.sdkmesh" ) );

    // Set effect variables as needed
    XMFLOAT4 colorMtrlDiffuse( 1.0f, 1.0f, 1.0f, 1.0f );
    XMFLOAT4 colorMtrlAmbient( 0.35f, 0.35f, 0.35f, 0 );
    V_RETURN( g_pMaterialAmbientColor->SetFloatVector( ( float* )&colorMtrlAmbient ) );
    V_RETURN( g_pMaterialDiffuseColor->SetFloatVector( ( float* )&colorMtrlDiffuse ) );

    // Setup the camera's view parameters
    static const XMVECTORF32 s_vecEye = { 0.0f, 0.0f, -15.0f, 0.0f };
    g_Camera.SetViewParams( s_vecEye, g_XMZero );
    g_Camera.SetRadius( fObjectRadius * 3.0f, fObjectRadius * 0.5f, fObjectRadius * 10.0f );

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
    g_Camera.SetButtonMasks( MOUSE_LEFT_BUTTON, MOUSE_WHEEL, MOUSE_MIDDLE_BUTTON );

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

    // Get the projection & view matrix from the camera class
    XMMATRIX mWorld = g_mCenterMesh * g_Camera.GetWorldMatrix();
    XMMATRIX mProj = g_Camera.GetProjMatrix();
    XMMATRIX mView = g_Camera.GetViewMatrix();

    XMMATRIX mWorldViewProjection = mWorld * mView * mProj;
    
    // Render the light arrow so the user can visually see the light dir
    XMFLOAT3 vLightDir[MAX_LIGHTS];
    XMFLOAT4 vLightDiffuse[MAX_LIGHTS];
    for( int i = 0; i < g_nNumActiveLights; i++ )
    {
        XMVECTOR arrowColor = ( i == g_nActiveLight ) ? Colors::Yellow : Colors::White;
        V( g_LightControl[i].OnRender( arrowColor, mView, mProj, g_Camera.GetEyePt() ) );
        XMStoreFloat3( &vLightDir[i], g_LightControl[i].GetLightDirection() );
        vLightDiffuse[i].x = vLightDiffuse[i].y = vLightDiffuse[i].z = vLightDiffuse[i].w = g_fLightScale;
    }

    V( g_pLightDir->SetRawValue( vLightDir, 0, sizeof( XMFLOAT3 ) * MAX_LIGHTS ) );
    V( g_pLightDiffuse->SetFloatVectorArray( ( float* )vLightDiffuse, 0, MAX_LIGHTS ) );

    XMFLOAT4X4 m;
    XMStoreFloat4x4( &m, mWorldViewProjection );
    V( g_pmWorldViewProjection->SetMatrix( ( float* )&m ) );

    XMStoreFloat4x4( &m, mWorld );
    V( g_pmWorld->SetMatrix( ( float* )&m) );

    V( g_pfTime->SetFloat( ( float )fTime ) );
    V( g_pnNumLights->SetInt( g_nNumActiveLights ) );

    // Render the scene with this technique as defined in the .fx file
    ID3DX11EffectTechnique* pRenderTechnique;
    switch( g_nNumActiveLights )
    {
        case 1:
            pRenderTechnique = g_pTechRenderSceneWithTexture1Light;
            break;
        case 2:
            pRenderTechnique = g_pTechRenderSceneWithTexture2Light;
            break;
        case 3:
            pRenderTechnique = g_pTechRenderSceneWithTexture3Light;
            break;
        default:
            pRenderTechnique = g_pTechRenderSceneWithTexture1Light;
            break;
    }

    //Get the mesh
    //IA setup
    pd3dImmediateContext->IASetInputLayout( g_pVertexLayout );
    UINT Strides[1];
    UINT Offsets[1];
    ID3D11Buffer* pVB[1];
    pVB[0] = g_Mesh11.GetVB11( 0, 0 );
    Strides[0] = ( UINT )g_Mesh11.GetVertexStride( 0, 0 );
    Offsets[0] = 0;
    pd3dImmediateContext->IASetVertexBuffers( 0, 1, pVB, Strides, Offsets );
    pd3dImmediateContext->IASetIndexBuffer( g_Mesh11.GetIB11( 0 ), g_Mesh11.GetIBFormat11( 0 ), 0 );

    //Render
    D3DX11_TECHNIQUE_DESC techDesc;
    V( pRenderTechnique->GetDesc( &techDesc ) );

    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        for( UINT subset = 0; subset < g_Mesh11.GetNumSubsets( 0 ); ++subset )
        {
            // Get the subset
            auto pSubset = g_Mesh11.GetSubset( 0, subset );

            auto  PrimType = CDXUTSDKMesh::GetPrimitiveType11( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
            pd3dImmediateContext->IASetPrimitiveTopology( PrimType );

            auto pDiffuseRV = g_Mesh11.GetMaterial( pSubset->MaterialID )->pDiffuseRV11;
            g_ptxDiffuse->SetResource( pDiffuseRV );

            pRenderTechnique->GetPassByIndex( p )->Apply( 0, pd3dImmediateContext );
            pd3dImmediateContext->DrawIndexed( ( UINT )pSubset->IndexCount, 0, ( UINT )pSubset->VertexStart );
        }
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

    SAFE_RELEASE( g_pEffect );
    
    SAFE_RELEASE( g_pVertexLayout );
    g_Mesh11.Destroy();

}
