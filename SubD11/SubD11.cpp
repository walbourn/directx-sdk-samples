//--------------------------------------------------------------------------------------
// File: SubD11.cpp
//
// This sample shows an implementation of Charles Loop's and Scott Schaefer's Approximate
// Catmull-Clark subdvision paper (http://research.microsoft.com/~cloop/msrtr-2007-44.pdf)
//
// Special thanks to Charles Loop and Peter-Pike Sloan for implementation details.
// Special thanks to Bay Raitt for the monsterfrog and bigguy models.
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
#include "SubDMesh.h"

#include <algorithm>

#pragma warning( disable : 4100 )

using namespace DirectX;

#define MAX_BUMP 3000                           // Maximum bump amount * 1000 (for UI slider)
#define MAX_DIVS 32                             // Maximum divisions of a patch per side (about 2048 triangles)

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CDXUTDialogResourceManager          g_DialogResourceManager; // manager for shared resources of dialogs
CModelViewerCamera                  g_Camera;                   // A model viewing camera
CDXUTDirectionWidget                g_LightControl;
CD3DSettingsDlg                     g_D3DSettingsDlg;           // Device settings dialog
CDXUTDialog                         g_HUD;                      // manages the 3D   
CDXUTDialog                         g_SampleUI;                 // dialog for sample specific controls

// Resources
CDXUTTextHelper*                    g_pTxtHelper = nullptr;

ID3D11InputLayout*					g_pPatchLayout = nullptr;
ID3D11InputLayout*					g_pMeshLayout = nullptr;

ID3D11VertexShader*					g_pPatchSkinningVS = nullptr;
ID3D11VertexShader*					g_pMeshSkinningVS = nullptr;
ID3D11HullShader*					g_pSubDToBezierHS = nullptr;
ID3D11HullShader*					g_pSubDToBezierHS4444 = nullptr;
ID3D11DomainShader*					g_pBezierEvalDS = nullptr;
ID3D11PixelShader*					g_pSmoothPS = nullptr;
ID3D11PixelShader*					g_pSolidColorPS = nullptr;

struct CB_TANGENT_STENCIL_CONSTANTS
{
    float TanM[MAX_VALENCE][64][4];	// Tangent patch stencils precomputed by the application
    float fCi[16][4];				// Valence coefficients precomputed by the application
};

#define MAX_BONE_MATRICES 80
struct CB_PER_MESH_CONSTANTS
{
    XMFLOAT4X4 mConstBoneWorld[MAX_BONE_MATRICES];
};

struct CB_PER_FRAME_CONSTANTS
{
    XMFLOAT4X4 mViewProjection;
    XMFLOAT3 vCameraPosWorld;
    float fTessellationFactor;
    float fDisplacementHeight;
    XMFLOAT3 vSolidColor;
};

ID3D11RasterizerState*              g_pRasterizerStateSolid = nullptr;
ID3D11RasterizerState*              g_pRasterizerStateWireframe = nullptr;
ID3D11SamplerState*                 g_pSamplerStateHeightMap = nullptr;
ID3D11SamplerState*                 g_pSamplerStateNormalMap = nullptr;

ID3D11Buffer*						g_pcbTangentStencilConstants = nullptr;
ID3D11Buffer*						g_pcbPerMesh = nullptr;
ID3D11Buffer*						g_pcbPerFrame = nullptr;

UINT								g_iBindTangentStencilConstants = 0;
UINT								g_iBindPerMesh = 1;
UINT								g_iBindPerFrame = 2;
UINT								g_iBindValencePrefixBuffer = 0;

// Control variables
INT                                 g_iSubdivs = 2;                         // Startup subdivisions per side
bool                                g_bDrawWires = true;                    // Draw the mesh with wireframe overlay
bool                                g_bUseMaterials = true;                // Render the object with surface materials
FLOAT                               g_fDisplacementHeight = 0.0f;           // The height amount for displacement mapping
bool                                g_bDrawHUD = true;
UINT                                g_iMSAASampleCount = 1;
bool                                g_bCloseupCamera = false;

// Movie capture mode
bool    g_bMovieMode = false;
FLOAT   g_fMovieFrameTime = 1.0f / 24.0f;
FLOAT   g_fMovieStartTime = 0.0f;
FLOAT   g_fMovieEndTime = 0.0f;
INT     g_iMovieFrameCount = 0;
INT     g_iMovieStartFrame = 0;
INT     g_iMovieFrameStride = 1;
INT     g_iMovieCurrentFrame = 0;

FLOAT   g_fFieldOfView = 65.0f;

const WCHAR* g_strDefaultMeshFileName = L"SubD10\\sebastian.sdkmesh";
CHAR g_strCameraName[MAX_PATH] = "Char_animCameras_combo_camera1";

WCHAR g_strSelectedMeshFileName[MAX_PATH] = L"";
WCHAR g_strMoviePath[MAX_PATH] = L".\\";


CSubDMesh g_SubDMesh;

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN      1
#define IDC_TOGGLEREF             3
#define IDC_CHANGEDEVICE          4

#define IDC_PATCH_SUBDIVS         5
#define IDC_PATCH_SUBDIVS_STATIC  6
#define IDC_BUMP_HEIGHT	          7
#define IDC_BUMP_HEIGHT_STATIC    8
#define IDC_TOGGLE_LINES          9
#define IDC_TOGGLE_MATERIALS      10

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
UINT SetPatchConstants( ID3D11Device* pd3dDevice, UINT iSubdivs );
void UpdateSubDPatches( ID3D11Device* pd3dDevice, CSubDMesh* pMesh );
void ConvertFromSubDToBezier( ID3D11Device* pd3dDevice, CSubDMesh* pMesh );
HRESULT CreateConstantBuffers( ID3D11Device* pd3dDevice );
void FillTables( ID3D11DeviceContext* pd3dDeviceContext );
void ParseCommandLine( const WCHAR* strCmdLine );

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

    ParseCommandLine( lpCmdLine );

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
    
    DXUTInit( true, true, nullptr); // Parse the command line, show msgboxes on error, and an extra cmd line param to force REF for now
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"SubD11" );
    DXUTCreateDevice( D3D_FEATURE_LEVEL_11_0,  true, 800, 600 );

    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Parse the command line, searching for filenames and other options
//--------------------------------------------------------------------------------------
void ParseCommandLine( const WCHAR* strCmdLine )
{
    WCHAR strCmdLineCopy[512] = {0};
    wcscpy_s( strCmdLineCopy, strCmdLine );
    _wcslwr_s( strCmdLineCopy );

    INT iArgCount = 0;
    LPWSTR* strCommands = CommandLineToArgvW( strCmdLineCopy, &iArgCount );
    for( INT i = 0; i < iArgCount; ++i )
    {
        const WCHAR* strCommand = strCommands[i];
        if( strCommand[0] == L'-' || strCommand[0] == L'/' )
        {
            ++strCommand;
            if( wcsstr( strCommand, L"materials" ) == strCommand )
            {
                g_bDrawWires = false;
                g_bUseMaterials = true;
            }
            else if( wcsstr( strCommand, L"nohud" ) == strCommand )
            {
                g_bDrawHUD = false;
            }
            else if( wcsstr( strCommand, L"msaa" ) == strCommand )
            {
                strCommand = wcschr( strCommand, L':' );
                if( strCommand )
                {
                    g_iMSAASampleCount = _wtoi( strCommand + 1 );
                    g_iMSAASampleCount = std::min<UINT>( 16, std::max<UINT>( 1, g_iMSAASampleCount ) );
                }
            }
            else if( wcsstr( strCommand, L"moviepath" ) == strCommand )
            {
                strCommand = wcschr( strCommand, L':' );
                if( strCommand )
                {
                    const WCHAR* strPath = strCommand + 1;
                    wcscpy_s( g_strMoviePath, strPath );
                }
            }
            else if( wcsstr( strCommand, L"movie" ) == strCommand )
            {
                g_bMovieMode = true;
            }
            else if( wcsstr( strCommand, L"stride" ) == strCommand )
            {
                strCommand = wcschr( strCommand, L':' );
                if( strCommand )
                {
                    g_iMovieFrameStride = _wtoi( strCommand + 1 );
                    g_bMovieMode = true;
                }
            }
            else if( wcsstr( strCommand, L"startframe" ) == strCommand )
            {
                strCommand = wcschr( strCommand, L':' );
                if( strCommand )
                {
                    g_iMovieStartFrame = _wtoi( strCommand + 1 );
                    g_bMovieMode = true;
                }
            }
            else if( wcsstr( strCommand, L"starttime" ) == strCommand )
            {
                strCommand = wcschr( strCommand, L':' );
                if( strCommand )
                {
                    g_fMovieStartTime = (FLOAT)_wtof( strCommand + 1 );
                    g_bMovieMode = true;
                }
            }
            else if( wcsstr( strCommand, L"endtime" ) == strCommand )
            {
                strCommand = wcschr( strCommand, L':' );
                if( strCommand )
                {
                    g_fMovieEndTime = (FLOAT)_wtof( strCommand + 1 );
                    g_bMovieMode = true;
                }
            }
            else if( wcsstr( strCommand, L"fov" ) == strCommand )
            {
                strCommand = wcschr( strCommand, L':' );
                if( strCommand )
                {
                    g_fFieldOfView = (FLOAT)_wtof( strCommand + 1 );
                }
            }
            else if( wcsstr( strCommand, L"cameraname" ) == strCommand )
            {
                strCommand = wcschr( strCommand, L':' );
                if( strCommand )
                {
                    const WCHAR* strName = strCommand + 1;
                    WideCharToMultiByte( CP_ACP, 0, strName, (int)wcslen( strName ), g_strCameraName, ARRAYSIZE(g_strCameraName), nullptr, nullptr );
                }
            }
            else if( wcsstr( strCommand, L"framerate" ) == strCommand )
            {
                strCommand = wcschr( strCommand, L':' );
                if( strCommand )
                {
                    INT iFrameRate = _wtoi( strCommand + 1 );
                    iFrameRate = std::max( 1, iFrameRate );
                    g_fMovieFrameTime = 1.0f / (FLOAT)iFrameRate;
                    g_bMovieMode = true;
                }
            }
            else if( wcsstr( strCommand, L"subdiv" ) == strCommand )
            {
                strCommand = wcschr( strCommand, L':' );
                if( strCommand )
                {
                    g_iSubdivs = _wtoi( strCommand + 1 );
                    g_iSubdivs = std::min( 15, std::max( 1, g_iSubdivs ) );
                }
            }
        }

        if( strCommand && wcsstr( strCommand, L".sdkmesh" ) )
        {
            wcscpy_s( g_strSelectedMeshFileName, strCommand );
        }
    }

    LocalFree ( strCommands );
}

//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    g_LightControl.SetLightDirection( XMFLOAT3( 0, 0, -1 ) );

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
    swprintf_s( sz, L"Patch Divisions: %d", g_iSubdivs );
    g_SampleUI.AddStatic( IDC_PATCH_SUBDIVS_STATIC, sz, 20, iY += 26, 150, 22 );
    g_SampleUI.AddSlider( IDC_PATCH_SUBDIVS, 50, iY += 24, 100, 22, 1, MAX_DIVS - 1, g_iSubdivs );

    swprintf_s( sz, L"BumpHeight: %.4f", g_fDisplacementHeight );
    g_SampleUI.AddStatic( IDC_BUMP_HEIGHT_STATIC, sz, 20, iY += 26, 150, 22 );
    g_SampleUI.AddSlider( IDC_BUMP_HEIGHT, 50, iY += 24, 100, 22, 0, MAX_BUMP, ( int )( 1000.0f * g_fDisplacementHeight ) );

    iY += 24;
    g_SampleUI.AddCheckBox( IDC_TOGGLE_LINES, L"Toggle Wires", 20, iY += 26, 150, 22, g_bDrawWires );
    g_SampleUI.AddCheckBox( IDC_TOGGLE_MATERIALS, L"Toggle Materials", 20, iY += 26, 150, 22, g_bUseMaterials );
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    pDeviceSettings->d3d11.sd.SampleDesc.Count = g_iMSAASampleCount;

    return true;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );

    XMMATRIX mWorld = XMMatrixIdentity();

    static UINT FrameCount = 0;
    static DOUBLE LastTime = 0.0f;
    if( g_bMovieMode )
    {
        DOUBLE fFrameElapsed = ( fTime - LastTime );
        CHAR strUpdate[100];
        sprintf_s( strUpdate, "Render frame %u, movie frame %d/%d: %0.3lf msec, %0.3lf time\n", FrameCount, g_iMovieCurrentFrame, g_iMovieFrameCount, fFrameElapsed * 1000.0, fTime );
        SetWindowTextA( DXUTGetHWND(), strUpdate );
    }
    LastTime = fTime;
    ++FrameCount;

    FLOAT fAnimationTime = (FLOAT)fTime;
    if( g_bMovieMode )
    {
        fAnimationTime = g_fMovieStartTime + ( (FLOAT)g_iMovieCurrentFrame * g_fMovieFrameTime );
    }
    g_SubDMesh.Update( mWorld, fAnimationTime );
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
            g_iSubdivs = g_SampleUI.GetSlider( IDC_PATCH_SUBDIVS )->GetValue();

            WCHAR sz[100];
            swprintf_s( sz, L"Patch Divisions: %d", g_iSubdivs );
            g_SampleUI.GetStatic( IDC_PATCH_SUBDIVS_STATIC )->SetText( sz );
        }
        break;
    case IDC_BUMP_HEIGHT:
        {
            g_fDisplacementHeight = ( float )g_SampleUI.GetSlider( IDC_BUMP_HEIGHT )->GetValue() / 1000.0f;

            WCHAR sz[100];
            swprintf_s( sz, L"BumpHeight: %.4f", g_fDisplacementHeight );
            g_SampleUI.GetStatic( IDC_BUMP_HEIGHT_STATIC )->SetText( sz );
        }
        break;
    case IDC_TOGGLE_LINES:
        {
            g_bDrawWires = g_SampleUI.GetCheckBox( IDC_TOGGLE_LINES )->GetChecked();
        }
        break;
    case IDC_TOGGLE_MATERIALS:
        {
            g_bUseMaterials = g_SampleUI.GetCheckBox( IDC_TOGGLE_MATERIALS )->GetChecked();
        }
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
HRESULT CreateConstantBuffers( ID3D11Device* pd3dDevice )
{
    HRESULT hr = S_OK;

    // Setup constant buffers
    D3D11_BUFFER_DESC Desc;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Desc.MiscFlags = 0;

    Desc.ByteWidth = sizeof( CB_TANGENT_STENCIL_CONSTANTS );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, nullptr, &g_pcbTangentStencilConstants ) );
    DXUT_SetDebugName( g_pcbTangentStencilConstants, "CB_TANGENT_STENCIL_CONSTANTS" );

    Desc.ByteWidth = sizeof( CB_PER_MESH_CONSTANTS );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, nullptr, &g_pcbPerMesh ) );
    DXUT_SetDebugName( g_pcbPerMesh, "CB_PER_MESH_CONSTANTS" );

    Desc.ByteWidth = sizeof( CB_PER_FRAME_CONSTANTS );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, nullptr, &g_pcbPerFrame ) );
    DXUT_SetDebugName( g_pcbPerFrame, "CB_PER_FRAME_CONSTANTS" );

    return hr;
}

//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependent on the back buffer
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
    ID3DBlob* pBlobPatchVS = nullptr;
    ID3DBlob* pBlobMeshVS = nullptr;
    ID3DBlob* pBlobHS = nullptr;
    ID3DBlob* pBlobHS4444 = nullptr;
    ID3DBlob* pBlobDS = nullptr;
    ID3DBlob* pBlobPS = nullptr;
    ID3DBlob* pBlobPSSolid = nullptr;

    V_RETURN( DXUTCompileFromFile( L"SubD11.hlsl", nullptr, "PatchSkinningVS","vs_5_0",
                                   D3DCOMPILE_ENABLE_STRICTNESS, 0, &pBlobPatchVS ) );
    V_RETURN( DXUTCompileFromFile( L"SubD11.hlsl", nullptr, "MeshSkinningVS", "vs_5_0",
                                   D3DCOMPILE_ENABLE_STRICTNESS, 0, &pBlobMeshVS ) );
    V_RETURN( DXUTCompileFromFile( L"SubD11.hlsl", nullptr, "SubDToBezierHS", "hs_5_0",
                                   D3DCOMPILE_ENABLE_STRICTNESS, 0, &pBlobHS ) );
    V_RETURN( DXUTCompileFromFile( L"SubD11.hlsl", nullptr, "SubDToBezierHS4444", "hs_5_0",
                                   D3DCOMPILE_ENABLE_STRICTNESS, 0, &pBlobHS4444 ) );
    V_RETURN( DXUTCompileFromFile( L"SubD11.hlsl", nullptr, "BezierEvalDS",   "ds_5_0",
                                   D3DCOMPILE_ENABLE_STRICTNESS, 0, &pBlobDS ) );
    V_RETURN( DXUTCompileFromFile( L"SubD11.hlsl", nullptr, "SmoothPS",	   "ps_5_0",
                                   D3DCOMPILE_ENABLE_STRICTNESS, 0, &pBlobPS ) );
    V_RETURN( DXUTCompileFromFile( L"SubD11.hlsl", nullptr, "SolidColorPS",   "ps_5_0",
                                   D3DCOMPILE_ENABLE_STRICTNESS, 0, &pBlobPSSolid ) );

    // Create shaders
    V_RETURN( pd3dDevice->CreateVertexShader( pBlobPatchVS->GetBufferPointer(), pBlobPatchVS->GetBufferSize(), nullptr, &g_pPatchSkinningVS ) );
    DXUT_SetDebugName( g_pPatchSkinningVS, "PatchSkinningVS" );

    V_RETURN( pd3dDevice->CreateVertexShader( pBlobMeshVS->GetBufferPointer(), pBlobMeshVS->GetBufferSize(), nullptr, &g_pMeshSkinningVS ) );
    DXUT_SetDebugName( g_pMeshSkinningVS, "MeshSkinningVS" );

    V_RETURN( pd3dDevice->CreateHullShader( pBlobHS->GetBufferPointer(), pBlobHS->GetBufferSize(), nullptr, &g_pSubDToBezierHS ) );
    DXUT_SetDebugName( g_pSubDToBezierHS, "SubDToBezierHS" );

    V_RETURN( pd3dDevice->CreateHullShader( pBlobHS4444->GetBufferPointer(), pBlobHS4444->GetBufferSize(), nullptr, &g_pSubDToBezierHS4444 ) );
    DXUT_SetDebugName( g_pSubDToBezierHS4444, "SubDToBezierHS4444" );

    V_RETURN( pd3dDevice->CreateDomainShader( pBlobDS->GetBufferPointer(), pBlobDS->GetBufferSize(), nullptr, &g_pBezierEvalDS ) );
    DXUT_SetDebugName( g_pBezierEvalDS, "BezierEvalDS" );

    V_RETURN( pd3dDevice->CreatePixelShader( pBlobPS->GetBufferPointer(), pBlobPS->GetBufferSize(), nullptr, &g_pSmoothPS ) );
    DXUT_SetDebugName( g_pSmoothPS, "SmoothPS" );

    V_RETURN( pd3dDevice->CreatePixelShader( pBlobPSSolid->GetBufferPointer(), pBlobPSSolid->GetBufferSize(), nullptr, &g_pSolidColorPS ) );
    DXUT_SetDebugName( g_pSolidColorPS, "SolidColorPS" );
    
    // Create our vertex input layout - this matches the SUBD_CONTROL_POINT structure
    const D3D11_INPUT_ELEMENT_DESC patchlayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "WEIGHTS",  0, DXGI_FORMAT_R8G8B8A8_UNORM,  0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BONES",    0, DXGI_FORMAT_R8G8B8A8_UINT,   0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    V_RETURN( pd3dDevice->CreateInputLayout( patchlayout, ARRAYSIZE( patchlayout ), pBlobPatchVS->GetBufferPointer(),
                                             pBlobPatchVS->GetBufferSize(), &g_pPatchLayout ) );
    DXUT_SetDebugName( g_pPatchLayout, "Patch" );

    V_RETURN( pd3dDevice->CreateInputLayout( patchlayout, ARRAYSIZE( patchlayout ), pBlobMeshVS->GetBufferPointer(),
                                             pBlobMeshVS->GetBufferSize(), &g_pMeshLayout ) );
    DXUT_SetDebugName( g_pMeshLayout, "Mesh" );

    SAFE_RELEASE( pBlobPatchVS );
    SAFE_RELEASE( pBlobMeshVS );
    SAFE_RELEASE( pBlobHS );
    SAFE_RELEASE( pBlobHS4444 );
    SAFE_RELEASE( pBlobDS );
    SAFE_RELEASE( pBlobPS );
    SAFE_RELEASE( pBlobPSSolid );

    // Create constant buffers
    V_RETURN( CreateConstantBuffers( pd3dDevice ) );

    // Fill our helper/temporary tables
    FillTables( pd3dImmediateContext );
    
    // Load mesh
    WCHAR strMeshFileName[MAX_PATH];
    WCHAR strAnimFileName[MAX_PATH];
    const WCHAR* strSelectedMeshFileName = g_strDefaultMeshFileName;
    if( wcslen( g_strSelectedMeshFileName ) > 0 )
    {
        strSelectedMeshFileName = g_strSelectedMeshFileName;
    }
    wcscpy_s( strMeshFileName, strSelectedMeshFileName );
    wcscpy_s( strAnimFileName, strSelectedMeshFileName );
    wcscat_s( strAnimFileName, L"_anim" );
    V_RETURN( g_SubDMesh.LoadSubDFromSDKMesh( pd3dDevice, strMeshFileName, strAnimFileName, g_strCameraName ) );

    // Set up movie capture mode
    if( g_bMovieMode )
    {
        FLOAT fAnimDuration = g_SubDMesh.GetAnimationDuration();
        if( fAnimDuration <= 0.0f )
        {
            g_bMovieMode = false;
        }
        else
        {
            if( g_fMovieEndTime <= 0.0f )
            {
                g_fMovieEndTime = fAnimDuration;
            }
            g_fMovieEndTime = std::min( g_fMovieEndTime, fAnimDuration );
            g_fMovieStartTime = std::min( g_fMovieStartTime, g_fMovieEndTime );
            FLOAT fDuration = g_fMovieEndTime - g_fMovieStartTime;
            g_iMovieFrameCount = (INT)( fDuration / g_fMovieFrameTime );
            g_iMovieCurrentFrame = g_iMovieStartFrame;
        }
    }

    // Setup the camera's view parameters
    XMVECTOR vCenter, vExtents;
    g_SubDMesh.GetBounds( &vCenter, &vExtents );
    XMVECTOR vEye;
    if( g_bCloseupCamera )
    {
        FLOAT fRadius = XMVectorGetX( XMVector3Length( vExtents ) );
        XMVECTOR vAdjust = XMVectorSet( 0.f, fRadius * 0.63f, 0.f, 0.f );
        vCenter += vAdjust;
        vAdjust = XMVectorSet( fRadius * 0.3f, 0.f, -fRadius * 0.3f, 0.f );
        vEye = vCenter + vAdjust;
    }
    else
    {
        FLOAT fRadius = XMVectorGetX( XMVector3Length( vExtents ) );
        const FLOAT fTheta = XM_PI * 0.125f;
        FLOAT fDistance = fRadius / tanf( fTheta );
        XMVECTOR vAdjust = XMVectorSet( 0.f, 0.f, fDistance, 0.f );
        vEye = vCenter - vAdjust;
    }
    g_Camera.SetViewParams( vEye, vCenter );

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

    // Create sampler state for heightmap and normal map
    D3D11_SAMPLER_DESC SSDesc;
    ZeroMemory( &SSDesc, sizeof( D3D11_SAMPLER_DESC ) );
    SSDesc.Filter = D3D11_FILTER_ANISOTROPIC;
    SSDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    SSDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    SSDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    SSDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    SSDesc.MaxAnisotropy = 16;
    SSDesc.MinLOD = 0;
    SSDesc.MaxLOD = D3D11_FLOAT32_MAX;
    V_RETURN( pd3dDevice->CreateSamplerState( &SSDesc, &g_pSamplerStateNormalMap ) );
    DXUT_SetDebugName( g_pSamplerStateNormalMap, "NormalMap" );

    SSDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    V_RETURN( pd3dDevice->CreateSamplerState( &SSDesc, &g_pSamplerStateHeightMap ) );
    DXUT_SetDebugName( g_pSamplerStateHeightMap, "HeightMap" );

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
    FLOAT fFOV = g_fFieldOfView * ( XM_PI / 180.0f );
    g_Camera.SetProjParams( fFOV * 0.5f, fAspectRatio, 0.1f, 4000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( MOUSE_MIDDLE_BUTTON, MOUSE_WHEEL, MOUSE_LEFT_BUTTON );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 170, 300 );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Use the gpu to convert from subds to cubic bezier patches using stream out
//--------------------------------------------------------------------------------------
void RenderSubDMesh( ID3D11DeviceContext* pd3dDeviceContext, CSubDMesh* pMesh, ID3D11PixelShader* pPixelShader )
{
    // Bind all of the CBs
    pd3dDeviceContext->HSSetConstantBuffers( g_iBindTangentStencilConstants, 1, &g_pcbTangentStencilConstants );
    pd3dDeviceContext->HSSetConstantBuffers( g_iBindPerFrame, 1, &g_pcbPerFrame );
    pd3dDeviceContext->VSSetConstantBuffers( g_iBindPerFrame, 1, &g_pcbPerFrame );
    pd3dDeviceContext->DSSetConstantBuffers( g_iBindPerFrame, 1, &g_pcbPerFrame );
    pd3dDeviceContext->PSSetConstantBuffers( g_iBindPerFrame, 1, &g_pcbPerFrame );

    // Set the shaders
    pd3dDeviceContext->VSSetShader( g_pPatchSkinningVS, nullptr, 0 );
    pd3dDeviceContext->HSSetShader( g_pSubDToBezierHS, nullptr, 0 );
    pd3dDeviceContext->DSSetShader( g_pBezierEvalDS, nullptr, 0 );
    pd3dDeviceContext->GSSetShader( nullptr, nullptr, 0 );
    pd3dDeviceContext->PSSetShader( pPixelShader, nullptr, 0 );

    // Set the heightmap sampler state
    pd3dDeviceContext->DSSetSamplers( 0, 1, &g_pSamplerStateHeightMap );
    pd3dDeviceContext->PSSetSamplers( 0, 1, &g_pSamplerStateNormalMap );

    // Set the input layout
    pd3dDeviceContext->IASetInputLayout( g_pPatchLayout );

    static bool s_bEnableAnimation = true;

    int PieceCount = pMesh->GetNumPatchPieces();

    // For better performance, the rendering of subd patches has been split into two passes

    // The first pass only renders regular patches (aka valence 4444), with a specialized hull shader which
    // only deals with regular patches
    XMMATRIX id = XMMatrixIdentity();

    pd3dDeviceContext->HSSetShader( g_pSubDToBezierHS4444, nullptr, 0 );
    for( int i = 0; i < PieceCount; ++i )
    {
        // Per frame cb update
        D3D11_MAPPED_SUBRESOURCE MappedResource;
        pd3dDeviceContext->Map( g_pcbPerMesh, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
        auto pData = reinterpret_cast<CB_PER_MESH_CONSTANTS*>( MappedResource.pData );

        int MeshIndex = pMesh->GetPatchMeshIndex( i );
        int NumTransforms = pMesh->GetNumInfluences( MeshIndex );
        assert( NumTransforms <= MAX_BONE_MATRICES );
        for( int j = 0; j < NumTransforms; ++j )
        {
            if( !s_bEnableAnimation )
            {
                XMStoreFloat4x4( &pData->mConstBoneWorld[j], id );
            }
            else
            {
                XMStoreFloat4x4( &pData->mConstBoneWorld[j], XMMatrixTranspose( pMesh->GetInfluenceMatrix( MeshIndex, j ) ) );
            }
        }
        if( NumTransforms == 0 )
        {
            XMMATRIX matTransform = pMesh->GetPatchPieceTransform( i );
            XMStoreFloat4x4( &pData->mConstBoneWorld[0], XMMatrixTranspose( matTransform ) );
        }
        pd3dDeviceContext->Unmap( g_pcbPerMesh, 0 );
        pd3dDeviceContext->VSSetConstantBuffers( g_iBindPerMesh, 1, &g_pcbPerMesh );

        pMesh->RenderPatchPiece_OnlyRegular( pd3dDeviceContext, i );
    }

    // The second pass renders the rest of the patches, with the general hull shader
    pd3dDeviceContext->HSSetShader( g_pSubDToBezierHS, nullptr, 0 );
    for( int i = 0; i < PieceCount; ++i )
    {
        // Per frame cb update
        D3D11_MAPPED_SUBRESOURCE MappedResource;
        pd3dDeviceContext->Map( g_pcbPerMesh, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
        auto pData = reinterpret_cast<CB_PER_MESH_CONSTANTS*>( MappedResource.pData );

        int MeshIndex = pMesh->GetPatchMeshIndex( i );
        int NumTransforms = pMesh->GetNumInfluences( MeshIndex );
        assert( NumTransforms <= MAX_BONE_MATRICES );
        for( int j = 0; j < NumTransforms; ++j )
        {
            if( !s_bEnableAnimation )
            {
                XMStoreFloat4x4( &pData->mConstBoneWorld[j], id );
            }
            else
            {
                XMStoreFloat4x4( &pData->mConstBoneWorld[j], XMMatrixTranspose( pMesh->GetInfluenceMatrix( MeshIndex, j ) ) );
            }
        }
        if( NumTransforms == 0 )
        {
            XMMATRIX matTransform = pMesh->GetPatchPieceTransform( i );
            XMStoreFloat4x4( &pData->mConstBoneWorld[0], XMMatrixTranspose( matTransform ) );
        }
        pd3dDeviceContext->Unmap( g_pcbPerMesh, 0 );
        pd3dDeviceContext->VSSetConstantBuffers( g_iBindPerMesh, 1, &g_pcbPerMesh );

        pMesh->RenderPatchPiece_OnlyExtraordinary( pd3dDeviceContext, i );
    }

    pd3dDeviceContext->VSSetShader( g_pMeshSkinningVS, nullptr, 0 );
    pd3dDeviceContext->HSSetShader( nullptr, nullptr, 0 );
    pd3dDeviceContext->DSSetShader( nullptr, nullptr, 0 );
    pd3dDeviceContext->GSSetShader( nullptr, nullptr, 0 );
    pd3dDeviceContext->PSSetShader( pPixelShader, nullptr, 0 );
    pd3dDeviceContext->IASetInputLayout( g_pMeshLayout );

    // Then finally renders the poly portion of the mesh
    PieceCount = pMesh->GetNumPolyMeshPieces();
    for( INT i = 0; i < PieceCount; ++i )
    {
        // Per frame cb update
        D3D11_MAPPED_SUBRESOURCE MappedResource;
        pd3dDeviceContext->Map( g_pcbPerMesh, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
        auto pData = reinterpret_cast<CB_PER_MESH_CONSTANTS*>( MappedResource.pData );

        int MeshIndex = pMesh->GetPolyMeshIndex( i );
        int NumTransforms = pMesh->GetNumInfluences( MeshIndex );
        assert( NumTransforms <= MAX_BONE_MATRICES );
        for( int j = 0; j < NumTransforms; ++j )
        {
            if( !s_bEnableAnimation )
            {
                XMStoreFloat4x4( &pData->mConstBoneWorld[j], id );
            }
            else
            {
                XMStoreFloat4x4( &pData->mConstBoneWorld[j], XMMatrixTranspose( pMesh->GetInfluenceMatrix( MeshIndex, j ) ) );
            }
        }
        if( NumTransforms == 0 )
        {
            XMMATRIX matTransform=  pMesh->GetPolyMeshPieceTransform( i );
            XMStoreFloat4x4( &pData->mConstBoneWorld[0], XMMatrixTranspose( matTransform ) );
        }
        pd3dDeviceContext->Unmap( g_pcbPerMesh, 0 );
        pd3dDeviceContext->VSSetConstantBuffers( g_iBindPerMesh, 1, &g_pcbPerMesh );

        pMesh->RenderPolyMeshPiece( pd3dDeviceContext, i );
    }
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
    XMVECTOR vCameraPosWorld = g_Camera.GetEyePt();

    g_SubDMesh.GetCameraViewMatrix( &mView, &vCameraPosWorld );

    XMMATRIX mViewProjection = mView * mProj;

    // Update per-frame variables
    {
        D3D11_MAPPED_SUBRESOURCE MappedResource;
        pd3dImmediateContext->Map( g_pcbPerFrame, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
        auto pData = reinterpret_cast<CB_PER_FRAME_CONSTANTS*>( MappedResource.pData );
        XMStoreFloat4x4( &pData->mViewProjection, XMMatrixTranspose( mViewProjection ) );
        XMStoreFloat3( &pData->vCameraPosWorld, vCameraPosWorld );
        pData->fTessellationFactor = (float)g_iSubdivs;
        pData->fDisplacementHeight = g_fDisplacementHeight;
        pData->vSolidColor = XMFLOAT3( 0.3f, 0.3f, 0.3f );
        pd3dImmediateContext->Unmap( g_pcbPerFrame, 0 );
    }
    
    // Clear the render target and depth stencil
    auto pRTV = DXUTGetD3D11RenderTargetView();
    pd3dImmediateContext->ClearRenderTargetView( pRTV, Colors::Black );
    auto pDSV = DXUTGetD3D11DepthStencilView();
    pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );

    // Set state for solid rendering
    pd3dImmediateContext->RSSetState( g_pRasterizerStateSolid );

    // Render the meshes
    if( g_bUseMaterials )
    {
        // Render with materials
        RenderSubDMesh( pd3dImmediateContext, &g_SubDMesh, g_pSmoothPS );
    }
    else
    {
        // Render without materials
        RenderSubDMesh( pd3dImmediateContext, &g_SubDMesh, g_pSolidColorPS );
    }

    // Optionally draw overlay wireframe
    if( g_bDrawWires )
    {
        D3D11_MAPPED_SUBRESOURCE MappedResource;
        pd3dImmediateContext->Map( g_pcbPerFrame, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
        auto pData = reinterpret_cast<CB_PER_FRAME_CONSTANTS*>( MappedResource.pData );
        XMStoreFloat4x4( &pData->mViewProjection, XMMatrixTranspose( mViewProjection ) );
        XMStoreFloat3( &pData->vCameraPosWorld, vCameraPosWorld );
        pData->fTessellationFactor = (float)g_iSubdivs;
        pData->fDisplacementHeight = g_fDisplacementHeight;
        pData->vSolidColor = XMFLOAT3( 0, 1, 0 );
        pd3dImmediateContext->Unmap( g_pcbPerFrame, 0 );

        pd3dImmediateContext->RSSetState( g_pRasterizerStateWireframe );
        // Render the meshes
        RenderSubDMesh( pd3dImmediateContext, &g_SubDMesh, g_pSolidColorPS );
        pd3dImmediateContext->RSSetState( g_pRasterizerStateSolid );
    }

    if( g_bDrawHUD )
    {
        // Render the HUD
        DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
        g_HUD.OnRender( fElapsedTime );
        g_SampleUI.OnRender( fElapsedTime );
        RenderText();
        DXUT_EndPerfEvent();
    }

    if( g_bMovieMode )
    {
        WCHAR strCapName[MAX_PATH];
        swprintf_s( strCapName, L"%s\\Movie%04d.bmp", g_strMoviePath, g_iMovieCurrentFrame );
        DXUTSnapD3D11Screenshot( strCapName, false );

        g_iMovieCurrentFrame += g_iMovieFrameStride;
        if( g_iMovieCurrentFrame > g_iMovieFrameCount )
        {
            DXUTShutdown();
        }
    }
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
    SAFE_RELEASE( g_pMeshLayout );
    SAFE_RELEASE( g_pcbTangentStencilConstants );
    SAFE_RELEASE( g_pcbPerMesh );
    SAFE_RELEASE( g_pcbPerFrame );

    SAFE_RELEASE( g_pPatchSkinningVS );
    SAFE_RELEASE( g_pMeshSkinningVS );
    SAFE_RELEASE( g_pSubDToBezierHS );
    SAFE_RELEASE( g_pSubDToBezierHS4444 );
    SAFE_RELEASE( g_pBezierEvalDS );
    SAFE_RELEASE( g_pSmoothPS );
    SAFE_RELEASE( g_pSolidColorPS );

    SAFE_RELEASE( g_pRasterizerStateSolid );
    SAFE_RELEASE( g_pRasterizerStateWireframe );
    SAFE_RELEASE( g_pSamplerStateHeightMap );
    SAFE_RELEASE( g_pSamplerStateNormalMap );

    g_SubDMesh.Destroy();
}

//--------------------------------------------------------------------------------------
// Fill the TanM and Ci precalculated tables.  This function precalculates part of the
// stencils (weights) used when calculating UV patches.  We precalculate a lot of the
// values here and just pass them in as shader constants.
//--------------------------------------------------------------------------------------
void FillTables( ID3D11DeviceContext* pd3dDeviceContext )
{
    // Per frame cb update
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    pd3dDeviceContext->Map( g_pcbTangentStencilConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
    auto pData = reinterpret_cast<CB_TANGENT_STENCIL_CONSTANTS*>( MappedResource.pData );
    
    for( UINT v = 0; v < MAX_VALENCE; v++ )
    {
        int a = 0;
        float CosfPIV = cosf( XM_PI / v );
        float VSqrtTerm = ( v * sqrtf( 4.0f + CosfPIV * CosfPIV ) );

        for( UINT i = 0; i < 32; i++ )
        {
            pData->TanM[v][i * 2 + a][0] = ( ( 1.0f / v ) + CosfPIV / VSqrtTerm ) * cosf( ( 2 * XM_PI * i ) / ( float )v );
        }
        a = 1;
        for( UINT i = 0; i < 32; i++ )
        {
            pData->TanM[v][i * 2 + a][0] = ( 1.0f / VSqrtTerm ) * cosf( ( 2 * XM_PI * i + XM_PI ) / ( float )v );
        }
    }

    for( UINT v = 0; v < MAX_VALENCE; v++ )
    {
        pData->fCi[v][0] = cosf( ( 2.0f * XM_PI ) / ( v + 3.0f ) );
    }

    // Constants
    pd3dDeviceContext->Unmap( g_pcbTangentStencilConstants, 0 );
}
