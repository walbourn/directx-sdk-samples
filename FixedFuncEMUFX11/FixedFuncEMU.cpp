//--------------------------------------------------------------------------------------
// File: FixedFuncEMU.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License (MIT).
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTgui.h"
#include "DXUTsettingsdlg.h"
#include "DXUTcamera.h"
#include "SDKmisc.h"
#include "SDKmesh.h"
#include "resource.h"

#include <d3dx11effect.h>

#pragma warning( disable : 4100 )

using namespace DirectX;

#define FOGMODE_NONE    0
#define FOGMODE_LINEAR  1
#define FOGMODE_EXP     2
#define FOGMODE_EXP2    3
#define DEG2RAD( a ) ( a * XM_PI / 180.f )
#define MAX_BALLS 10

struct SCENE_VERTEX
{
    XMFLOAT3 pos;
    XMFLOAT3 norm;
    XMFLOAT2 tex;
};

struct SCENE_LIGHT
{
    XMFLOAT4 Position;
    XMFLOAT4 Diffuse;
    XMFLOAT4 Specular;
    XMFLOAT4 Ambient;
    XMFLOAT4 Atten;
};

struct BALL
{
    double dStartTime;
    XMFLOAT4X4 mWorld;
    XMFLOAT3 velStart;
};


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CModelViewerCamera                  g_Camera;               // A model viewing camera
CDXUTDialogResourceManager          g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg                     g_D3DSettingsDlg;       // Device settings dialog
CDXUTDialog                         g_HUD;                  // manages the 3D UI
CDXUTDialog                         g_SampleUI;             // dialog for sample specific controls
XMMATRIX                            g_mBlackHole;
XMMATRIX                            g_mLightView;
XMMATRIX                            g_mLightProj;
SCENE_LIGHT g_lights[8];
XMFLOAT4 g_clipplanes[3];
BALL g_balls[ MAX_BALLS ];
double                              g_fLaunchInterval = 0.3f;
float                               g_fRotateSpeed = 70.0f;

CDXUTTextHelper*                    g_pTxtHelper = nullptr;

ID3DX11Effect*                      g_pEffect = nullptr;
ID3D11InputLayout*                  g_pVertexLayout = nullptr;
ID3D11Buffer*                       g_pScreenQuadVB = nullptr;

ID3D11ShaderResourceView*           g_pScreenTexRV = nullptr;
ID3D11ShaderResourceView*           g_pProjectedTexRV = nullptr;

CDXUTSDKMesh                        g_ballMesh;
CDXUTSDKMesh                        g_roomMesh;
CDXUTSDKMesh                        g_holeMesh;

ID3DX11EffectTechnique*              g_pRenderSceneGouraudTech = nullptr;
ID3DX11EffectTechnique*              g_pRenderSceneFlatTech = nullptr;
ID3DX11EffectTechnique*              g_pRenderScenePointTech = nullptr;
ID3DX11EffectTechnique*              g_pRenderScreenSpaceAlphaTestTech = nullptr;

ID3DX11EffectMatrixVariable*         g_pmWorld = nullptr;
ID3DX11EffectMatrixVariable*         g_pmView = nullptr;
ID3DX11EffectMatrixVariable*         g_pmProj = nullptr;
ID3DX11EffectMatrixVariable*         g_pmInvProj = nullptr;
ID3DX11EffectMatrixVariable*         g_pmLightViewProj = nullptr;
ID3DX11EffectShaderResourceVariable* g_pDiffuseTex = nullptr;
ID3DX11EffectShaderResourceVariable* g_pProjectedTex = nullptr;
ID3DX11EffectVariable*               g_pSceneLights = nullptr;
ID3DX11EffectVectorVariable*         g_pClipPlanes = nullptr;
ID3DX11EffectScalarVariable*         g_pViewportHeight = nullptr;
ID3DX11EffectScalarVariable*         g_pViewportWidth = nullptr;
ID3DX11EffectScalarVariable*         g_pNearPlane = nullptr;
ID3DX11EffectScalarVariable*         g_pPointSize = nullptr;
ID3DX11EffectScalarVariable*         g_pEnableLighting = nullptr;
ID3DX11EffectScalarVariable*         g_pEnableClipping = nullptr;
ID3DX11EffectScalarVariable*         g_pFogMode = nullptr;
ID3DX11EffectScalarVariable*         g_pFogStart = nullptr;
ID3DX11EffectScalarVariable*         g_pFogEnd = nullptr;
ID3DX11EffectScalarVariable*         g_pFogDensity = nullptr;
ID3DX11EffectVectorVariable*         g_pFogColor = nullptr;


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4
#define IDC_TOGGLEWARP          5

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
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

void RenderMesh( CDXUTSDKMesh& mesh, ID3D11DeviceContext* pd3dImmediateContext, ID3DX11EffectTechnique* pTechnique,
                 ID3DX11EffectShaderResourceVariable* ptxDiffuse );


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

    // DXUT will create and use the best device (D3D11) 
    // that is available on the system depending on which D3D callbacks are set below

    // Set DXUT callbacks
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( KeyboardProc );
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
    DXUTCreateWindow( L"FixedFuncEMU" );
    DXUTCreateDevice( D3D_FEATURE_LEVEL_10_0, true, 800, 600 );
    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    g_D3DSettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 0, iY, 170, 23 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 0, iY += 26, 170, 23, VK_F2 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 0, iY += 26, 170, 23, VK_F3 );
    g_HUD.AddButton( IDC_TOGGLEWARP, L"Toggle WARP (F4)", 0, iY += 26, 170, 23, VK_F4 );

    g_HUD.SetCallback( OnGUIEvent ); iY = 10;
}


//--------------------------------------------------------------------------------------
// Called right before creating a device, allowing the app to modify the device settings as needed
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
    g_Camera.FrameMove( fElapsedTime );

    float fBlackHoleRads = ( float )fTime * DEG2RAD( g_fRotateSpeed );
    g_mBlackHole = XMMatrixRotationY( fBlackHoleRads );

    // Rotate the clip planes to align with the black holes
    g_clipplanes[0] = XMFLOAT4( 0, 1.0f, 0, -0.8f );

    static const XMVECTORF32 s_v3Plane1 = { 0.707f, 0.707f, 0.f, 0.f };
    static const XMVECTORF32 s_v3Plane2 = { -0.707f, 0.707f, 0.f, 0.f };
    XMStoreFloat4( &g_clipplanes[1], XMVector3TransformNormal( s_v3Plane1, g_mBlackHole ) );
    XMStoreFloat4( &g_clipplanes[2], XMVector3TransformNormal( s_v3Plane2, g_mBlackHole ) );
    g_clipplanes[1].w = 0.70f;
    g_clipplanes[2].w = 0.70f;
    g_pClipPlanes->SetFloatVectorArray( ( float* )g_clipplanes, 0, 3 );

    XMFLOAT3 ballLaunch( 2.1f, 8.1f, 0 );
    XMFLOAT3 ballStart( 0,0.45f,0 );
    XMFLOAT3 ballGravity( 0,-9.8f, 0 );
    XMFLOAT3 ballNow;

    float fBall_Life = 3.05f / ballLaunch.x;

    // Move existing balls
    for( int i = 0; i < MAX_BALLS; i++ )
    {
        float T = ( float )( fTime - g_balls[i].dStartTime );
        if( T < fBall_Life + 0.5f ) // Live 1/2 second longer to fully show off clipping
        {
            // Use the equation X = Xo + VoT + 1/2AT^2
            ballNow.x = ballStart.x + g_balls[i].velStart.x * T + 0.5f * ballGravity.x * T * T;
            ballNow.y = ballStart.y + g_balls[i].velStart.y * T + 0.5f * ballGravity.y * T * T;
            ballNow.z = ballStart.z + g_balls[i].velStart.z * T + 0.5f * ballGravity.z * T * T;

            // Create a world matrix
            XMStoreFloat4x4( &g_balls[i].mWorld, XMMatrixTranslation( ballNow.x, ballNow.y, ballNow.z ) );
        }
        else
        {
            g_balls[i].dStartTime = -1.0;
        }
    }

    // Launch a ball if it's time
    XMMATRIX wLaunchMatrix;
    bool bFound = false;
    static double dLastLaunch = -g_fLaunchInterval - 1;
    if( ( fTime - dLastLaunch ) > g_fLaunchInterval )
    {
        for( int i = 0; i < MAX_BALLS && !bFound; i++ )
        {
            if( g_balls[i].dStartTime < 0.0 )
            {
                // Found a free ball
                g_balls[i].dStartTime = fTime;
                wLaunchMatrix = XMMatrixRotationY( ( i % 2 ) * DEG2RAD(180.0f) + fBlackHoleRads +
                                     DEG2RAD( fBall_Life*g_fRotateSpeed ) );
                XMStoreFloat3( &g_balls[i].velStart, XMVector3TransformNormal( XMLoadFloat3( &ballLaunch ), wLaunchMatrix ) );
                XMStoreFloat4x4( &g_balls[i].mWorld, XMMatrixTranslation( ballStart.x, ballStart.y, ballStart.z ) );
                bFound = true;
            }
        }
        dLastLaunch = fTime;
    }

    // Rotate the cookie matrix
    XMMATRIX mLightRot = XMMatrixRotationY( DEG2RAD( 50.0f ) * ( float )fTime );
    static const XMVECTORF32 s_vLightEye = { 0.f, 5.65f, 0.f, 0.f };
    static const XMVECTORF32 s_vLightAt = { 0.f, 0.f, 0.f, 0.f };
    static const XMVECTORF32 s_vUp = { 0.f, 0.f, 1.f, 0.f };
    g_mLightView = XMMatrixLookAtLH( s_vLightEye, s_vLightAt, s_vUp );
    g_mLightView = mLightRot * g_mLightView;
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
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
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
        case IDC_TOGGLEWARP:
            DXUTToggleWARP(); break;
        case IDC_CHANGEDEVICE:
            g_D3DSettingsDlg.SetActive( !g_D3DSettingsDlg.IsActive() ); break;
    }
}


//--------------------------------------------------------------------------------------
// Reject any devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// Create any resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext )
{
    HRESULT hr;
    auto pd3dImmediateContext = DXUTGetD3D11DeviceContext();
    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );
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
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"FixedFuncEMU.fx" ) );
    V_RETURN( D3DX11CompileEffectFromFile( str, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, dwShaderFlags, 0, pd3dDevice, &g_pEffect, nullptr) );

#else

    ID3DBlob* pEffectBuffer = nullptr;
    V_RETURN( DXUTCompileFromFile( L"FixedFuncEMU.fx", nullptr, "none", "fx_5_0", dwShaderFlags, 0, &pEffectBuffer ) );
    hr = D3DX11CreateEffectFromMemory( pEffectBuffer->GetBufferPointer(), pEffectBuffer->GetBufferSize(), 0, pd3dDevice, &g_pEffect );
    SAFE_RELEASE( pEffectBuffer );
    if ( FAILED(hr) )
        return hr;

#endif

    // Obtain the technique handles
    g_pRenderSceneGouraudTech = g_pEffect->GetTechniqueByName( "RenderSceneGouraud" );
    g_pRenderSceneFlatTech = g_pEffect->GetTechniqueByName( "RenderSceneFlat" );
    g_pRenderScenePointTech = g_pEffect->GetTechniqueByName( "RenderScenePoint" );
    g_pRenderScreenSpaceAlphaTestTech = g_pEffect->GetTechniqueByName( "RenderScreenSpaceAlphaTest" );

    // Obtain the parameter handles
    g_pmWorld = g_pEffect->GetVariableByName( "g_mWorld" )->AsMatrix();
    g_pmView = g_pEffect->GetVariableByName( "g_mView" )->AsMatrix();
    g_pmProj = g_pEffect->GetVariableByName( "g_mProj" )->AsMatrix();
    g_pmInvProj = g_pEffect->GetVariableByName( "g_mInvProj" )->AsMatrix();
    g_pmLightViewProj = g_pEffect->GetVariableByName( "g_mLightViewProj" )->AsMatrix();
    g_pDiffuseTex = g_pEffect->GetVariableByName( "g_txDiffuse" )->AsShaderResource();
    g_pProjectedTex = g_pEffect->GetVariableByName( "g_txProjected" )->AsShaderResource();
    g_pSceneLights = g_pEffect->GetVariableByName( "g_lights" );
    g_pClipPlanes = g_pEffect->GetVariableByName( "g_clipplanes" )->AsVector();
    g_pViewportHeight = g_pEffect->GetVariableByName( "g_viewportHeight" )->AsScalar();
    g_pViewportWidth = g_pEffect->GetVariableByName( "g_viewportWidth" )->AsScalar();
    g_pNearPlane = g_pEffect->GetVariableByName( "g_nearPlane" )->AsScalar();
    g_pPointSize = g_pEffect->GetVariableByName( "g_pointSize" )->AsScalar();
    g_pEnableLighting = g_pEffect->GetVariableByName( "g_bEnableLighting" )->AsScalar();
    g_pEnableClipping = g_pEffect->GetVariableByName( "g_bEnableClipping" )->AsScalar();
    g_pFogMode = g_pEffect->GetVariableByName( "g_fogMode" )->AsScalar();
    g_pFogStart = g_pEffect->GetVariableByName( "g_fogStart" )->AsScalar();
    g_pFogEnd = g_pEffect->GetVariableByName( "g_fogEnd" )->AsScalar();
    g_pFogDensity = g_pEffect->GetVariableByName( "g_fogDensity" )->AsScalar();
    g_pFogColor = g_pEffect->GetVariableByName( "g_fogColor" )->AsVector();

    //set constant variables
    g_pPointSize->SetFloat( 3.0f );
    g_pFogMode->SetInt( FOGMODE_LINEAR );
    g_pFogStart->SetFloat( 12.0f );
    g_pFogEnd->SetFloat( 22.0f );
    g_pFogDensity->SetFloat( 0.05f );
    XMFLOAT4 v4FogColor( 0.7f,1.0f,1.0f,1 );
    g_pFogColor->SetFloatVector( ( float* )&v4FogColor );

    // Create our vertex input layout
    const D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXTURE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    D3DX11_PASS_DESC PassDesc;
    V_RETURN( g_pRenderSceneGouraudTech->GetPassByIndex( 0 )->GetDesc( &PassDesc ) );
    V_RETURN( pd3dDevice->CreateInputLayout( layout, 3, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pVertexLayout ) );

    // Load the meshes
    V_RETURN( g_ballMesh.Create( pd3dDevice, L"misc\\ball.sdkmesh" ) );
    V_RETURN( g_roomMesh.Create( pd3dDevice, L"BlackHoleRoom\\BlackHoleRoom.sdkmesh" ) );
    V_RETURN( g_holeMesh.Create( pd3dDevice, L"BlackHoleRoom\\BlackHole.sdkmesh" ) );
    g_mBlackHole = XMMatrixIdentity();

    // Initialize the balls
    for( int i = 0; i < MAX_BALLS; i++ )
        g_balls[i].dStartTime = -1.0;

    // Setup the Lights
    ZeroMemory( g_lights, 8 * sizeof( SCENE_LIGHT ) );
    int iLight = 0;
    for( int y = 0; y < 3; y++ )
    {
        for( int x = 0; x < 3; x++ )
        {
            if( x != 1 || y != 1 )
            {
                g_lights[iLight].Position = XMFLOAT4( 3.0f * ( -1.0f + x ), 5.65f, 5.0f * ( -1.0f + y ), 1 );
                if( 0 == iLight % 2 )
                {
                    g_lights[iLight].Diffuse = XMFLOAT4( 0.20f, 0.20f, 0.20f, 1.0f );
                    g_lights[iLight].Specular = XMFLOAT4( 0.5f, 0.5f, 0.5f, 1.0f );
                    g_lights[iLight].Ambient = XMFLOAT4( 0.03f, 0.03f, 0.03f, 0.0f );
                }
                else
                {
                    g_lights[iLight].Diffuse = XMFLOAT4( 0.0f, 0.15f, 0.20f, 1.0f );
                    g_lights[iLight].Specular = XMFLOAT4( 0.15f, 0.25f, 0.3f, 1.0f );
                    g_lights[iLight].Ambient = XMFLOAT4( 0.00f, 0.02f, 0.03f, 0.0f );
                }
                g_lights[iLight].Atten.x = 1.0f;

                iLight ++;
            }
        }
    }
    g_pSceneLights->SetRawValue( g_lights, 0, 8 * sizeof( SCENE_LIGHT ) );

    g_mLightProj = XMMatrixPerspectiveFovLH( DEG2RAD(90.0f), 1.0f, 0.1f, 100.0f );

    // Create the screenspace quad VB
    // This gets initialized in OnD3D11SwapChainResized
    D3D11_BUFFER_DESC vbdesc =
    {
        4 * sizeof( SCENE_VERTEX ),
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_VERTEX_BUFFER,
        0,
        0
    };
    V_RETURN( pd3dDevice->CreateBuffer( &vbdesc, nullptr, &g_pScreenQuadVB ) );

    // Load the HUD and Cookie Textures
    V_RETURN( DXUTCreateShaderResourceViewFromFile( pd3dDevice, L"misc\\hud.dds", &g_pScreenTexRV ) );
    V_RETURN( DXUTCreateShaderResourceViewFromFile( pd3dDevice, L"misc\\cookie.dds", &g_pProjectedTexRV ) );
    g_pProjectedTex->SetResource( g_pProjectedTexRV );

    // Setup the camera's view parameters
    static const XMVECTORF32 s_vecEye = { 0.0f, 2.3f, -8.5f, 0.f };
    static const XMVECTORF32 s_vecAt = { 0.0f, 2.0f, 0.0f, 0.f };
    g_Camera.SetViewParams( s_vecEye, s_vecAt );
    g_Camera.SetRadius( 9.0f, 1.0f, 15.0f );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr = S_OK;

    V_RETURN( g_DialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    float fWidth = static_cast< float >( pBackBufferSurfaceDesc->Width );
    float fHeight = static_cast< float >( pBackBufferSurfaceDesc->Height );

    // Set the viewport width/height
    g_pViewportWidth->SetFloat( fWidth );
    g_pViewportHeight->SetFloat( fHeight );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( XM_PI / 4, fAspectRatio, 0.1f, 100.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( MOUSE_LEFT_BUTTON, MOUSE_WHEEL, MOUSE_MIDDLE_BUTTON );

    g_pNearPlane->SetFloat( 0.1f );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 170, 300 );

    // Update our Screen-space quad
    SCENE_VERTEX aVerts[] =
    {
        { XMFLOAT3( 0, 0, 0.5 ), XMFLOAT3( 0, 0, 0 ), XMFLOAT2( 0, 0 ) },
        { XMFLOAT3( fWidth, 0, 0.5 ), XMFLOAT3( 0, 0, 0 ), XMFLOAT2( 1, 0 ) },
        { XMFLOAT3( 0, fHeight, 0.5 ), XMFLOAT3( 0, 0, 0 ), XMFLOAT2( 0, 1 ) },
        { XMFLOAT3( fWidth, fHeight, 0.5 ), XMFLOAT3( 0, 0, 0 ), XMFLOAT2( 1, 1 ) },
    };

    auto pd3dImmediateContext = DXUTGetD3D11DeviceContext();
    pd3dImmediateContext->UpdateSubresource( g_pScreenQuadVB, 0, nullptr, aVerts, 0, 0 );

    return hr;
}


//--------------------------------------------------------------------------------------
// Render mesh with FX
//--------------------------------------------------------------------------------------
void RenderMesh( CDXUTSDKMesh& mesh, ID3D11DeviceContext* pd3dImmediateContext, ID3DX11EffectTechnique* pTechnique,
                 ID3DX11EffectShaderResourceVariable* ptxDiffuse )
{
    //Get the mesh
    //IA setup
    pd3dImmediateContext->IASetInputLayout( g_pVertexLayout );
    UINT Strides[1];
    UINT Offsets[1];
    ID3D11Buffer* pVB[1];
    pVB[0] = mesh.GetVB11( 0, 0 );
    Strides[0] = ( UINT )mesh.GetVertexStride( 0, 0 );
    Offsets[0] = 0;
    pd3dImmediateContext->IASetVertexBuffers( 0, 1, pVB, Strides, Offsets );
    pd3dImmediateContext->IASetIndexBuffer( mesh.GetIB11( 0 ), mesh.GetIBFormat11( 0 ), 0 );

    //Render
    D3DX11_TECHNIQUE_DESC techDesc;
    HRESULT hr;
    V( pTechnique->GetDesc( &techDesc ) );
    
    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        for( UINT subset = 0; subset < mesh.GetNumSubsets( 0 ); ++subset )
        {
            // Get the subset
            auto pSubset = mesh.GetSubset( 0, subset );

            auto PrimType = CDXUTSDKMesh::GetPrimitiveType11( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
            pd3dImmediateContext->IASetPrimitiveTopology( PrimType );

            auto pDiffuseRV = mesh.GetMaterial( pSubset->MaterialID )->pDiffuseRV11;
            ptxDiffuse->SetResource( pDiffuseRV );

            pTechnique->GetPassByIndex( p )->Apply( 0, pd3dImmediateContext );
            pd3dImmediateContext->DrawIndexed( ( UINT )pSubset->IndexCount, 0, ( UINT )pSubset->VertexStart );
        }
    }
}


//--------------------------------------------------------------------------------------
// Render a screen quad
//--------------------------------------------------------------------------------------
void RenderScreenQuad( ID3D11DeviceContext* pd3dImmediateContext, ID3DX11EffectTechnique* pTechnique )
{
    UINT uStride = sizeof( SCENE_VERTEX );
    UINT offsets = 0;
    ID3D11Buffer* pBuffers[1] = { g_pScreenQuadVB };
    pd3dImmediateContext->IASetVertexBuffers( 0, 1, pBuffers, &uStride, &offsets );
    pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

    D3DX11_TECHNIQUE_DESC techDesc;
    HRESULT hr;
    V( pTechnique->GetDesc( &techDesc ) );
    g_pDiffuseTex->SetResource( g_pScreenTexRV );
    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        pTechnique->GetPassByIndex( p )->Apply( 0, pd3dImmediateContext );
        pd3dImmediateContext->Draw( 4, 0 );
    }
}


//--------------------------------------------------------------------------------------
// Render the scene using the device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                  float fElapsedTime, void* pUserContext )
{
    auto pRTV = DXUTGetD3D11RenderTargetView();
    pd3dImmediateContext->ClearRenderTargetView( pRTV, Colors::MidnightBlue );
    auto pDSV = DXUTGetD3D11DepthStencilView();
    pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );

    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    pd3dImmediateContext->IASetInputLayout( g_pVertexLayout );

    // Setup the view matrices
    XMMATRIX mLightViewProj = g_mLightView * g_mLightProj;
    g_pmLightViewProj->SetMatrix( ( float* )&mLightViewProj );

    XMMATRIX mWorld = XMMatrixIdentity();
    XMMATRIX mProj = g_Camera.GetProjMatrix();
    XMMATRIX mView = g_Camera.GetViewMatrix();

    XMFLOAT4X4 tmp4x4;
    XMStoreFloat4x4( &tmp4x4, mWorld );
    g_pmWorld->SetMatrix( reinterpret_cast<float*>( &tmp4x4 ) );

    XMStoreFloat4x4( &tmp4x4, mView );
    g_pmView->SetMatrix( reinterpret_cast<float*>( &tmp4x4 ) );

    XMStoreFloat4x4( &tmp4x4, mProj );
    g_pmProj->SetMatrix( reinterpret_cast<float*>( &tmp4x4 ) );

    XMMATRIX mInvProj = XMMatrixInverse( nullptr, mProj );
    XMStoreFloat4x4( &tmp4x4, mInvProj );
    g_pmInvProj->SetMatrix( reinterpret_cast<float*>( &tmp4x4 ) );

    // Render the room and the blackholes
    g_pEnableClipping->SetBool( false );
    g_pEnableLighting->SetBool( false );
    RenderMesh( g_roomMesh, pd3dImmediateContext, g_pRenderSceneGouraudTech, g_pDiffuseTex );
    g_pmWorld->SetMatrix( ( float* )&g_mBlackHole );
    RenderMesh( g_holeMesh, pd3dImmediateContext, g_pRenderSceneGouraudTech, g_pDiffuseTex );

    // Render the balls
    g_pEnableClipping->SetBool( true );
    g_pEnableLighting->SetBool( true );
    for( int i = 0; i < MAX_BALLS; i++ )
    {
        if( g_balls[i].dStartTime > -1.0 )
        {
            g_pmWorld->SetMatrix( ( float* )&g_balls[i].mWorld );

            if( i % 3 == 0 )
                RenderMesh( g_ballMesh, pd3dImmediateContext, g_pRenderSceneGouraudTech, g_pDiffuseTex );
            else if( i % 3 == 1 )
                RenderMesh( g_ballMesh, pd3dImmediateContext, g_pRenderSceneFlatTech, g_pDiffuseTex );
            else
                RenderMesh( g_ballMesh, pd3dImmediateContext, g_pRenderScenePointTech, g_pDiffuseTex );
        }
    }

    g_pEnableClipping->SetBool( false );
    g_pEnableLighting->SetBool( false );
    RenderScreenQuad( pd3dImmediateContext, g_pRenderScreenSpaceAlphaTestTech );

    // Render the HUD
    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
    RenderText();
    g_HUD.OnRender( fElapsedTime );
    g_SampleUI.OnRender( fElapsedTime );
    DXUT_EndPerfEvent();
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
// Release resources created in OnD3D11ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11ReleasingSwapChain();
}


//--------------------------------------------------------------------------------------
// Release resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11DestroyDevice();
    g_D3DSettingsDlg.OnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE( g_pTxtHelper );
    SAFE_RELEASE( g_pEffect );
    SAFE_RELEASE( g_pVertexLayout );
    SAFE_RELEASE( g_pScreenTexRV );
    SAFE_RELEASE( g_pProjectedTexRV );
    SAFE_RELEASE( g_pScreenQuadVB );
    g_ballMesh.Destroy();
    g_roomMesh.Destroy();
    g_holeMesh.Destroy();
}



