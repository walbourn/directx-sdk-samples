//--------------------------------------------------------------------------------------
// File: Instancing.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTgui.h"
#include "DXUTsettingsdlg.h"
#include "DXUTcamera.h"
#include "SDKmisc.h"
#include "SDKmesh.h"
#include "DXUTRes.h"
#include "resource.h"
#include "DDSTextureLoader.h"

#include <d3dx11effect.h>

#pragma warning( disable : 4100 )

using namespace DirectX;

//--------------------------------------------------------------------------------------
// Defines
//--------------------------------------------------------------------------------------

struct QUAD_VERTEX
{
    XMFLOAT3 pos;
    XMFLOAT2 tex;
};

struct QUAD_MESH
{
    ID3D11Buffer* pVB;
    ID3D11Buffer* pIB;
    DWORD dwNumVerts;
    DWORD dwNumIndices;
    UINT uStride;
    ID3D11Texture2D** ppTexture;
    ID3D11ShaderResourceView** ppSRV;
};

#pragma pack(push, 1)
struct INSTANCEDATA
{
    XMFLOAT4X4 mMatWorld;
    float fOcc;
};
#pragma pack(pop)


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CModelViewerCamera                  g_Camera;               // A model viewing camera
CDXUTDialogResourceManager          g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg                     g_D3DSettingsDlg;       // Device settings dialog
CDXUTDialog                         g_HUD;                  // manages the 3D UI
CDXUTDialog                         g_SampleUI;             // dialog for sample specific controls
CDXUTTextHelper*                    g_pTxtHelper = nullptr;

// Textures for various texture arrays
LPCTSTR g_szLeafTextureNames[] =
{
    L"trees\\leaf_v3_green_tex.dds",
    L"trees\\leaf_v3_olive_tex.dds",
    L"trees\\leaf_v3_dark_tex.dds",
};

LPCTSTR g_szTreeLeafInstanceNames[] =
{
    L"data\\leaves5.mtx",
};

LPCTSTR g_szGrassTextureNames[] =
{
    L"IslandScene\\grass_v1_basic_tex.dds",
    L"IslandScene\\grass_v2_light_tex.dds",
    L"IslandScene\\grass_v3_dark_tex.dds",
    L"IslandScene\\dry_flowers_v1_tex.dds",
    L"IslandScene\\grass_guide_v3_tex.dds",
};

// List of tree matrices for instancing.  Trees and islands will be placed randomly throughout the scene.
#define MAX_TREE_INSTANCES 50
XMMATRIX g_treeInstanceMatrices[MAX_TREE_INSTANCES] =
{
    //center the very first tree
    XMMATRIX( 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 ),
};

// Direct3D 11 resources
ID3DX11Effect*                      g_pEffect = nullptr;
ID3D11RasterizerState*              g_pRasterState = nullptr;

ID3D11InputLayout*                  g_pInstVertexLayout = nullptr;
ID3D11InputLayout*                  g_pSkyVertexLayout = nullptr;
ID3D11InputLayout*                  g_pLeafVertexLayout = nullptr;

CDXUTSDKMesh                        g_MeshSkybox;
CDXUTSDKMesh                        g_MeshIsland;
CDXUTSDKMesh                        g_MeshIslandTop;
CDXUTSDKMesh                        g_MeshTree;
ID3D11Buffer*                       g_pLeafInstanceData = nullptr;
DWORD                               g_dwNumLeaves;
QUAD_MESH                           g_MeshLeaf;

ID3D11Texture2D*                    g_pGrassTexture;
ID3D11ShaderResourceView*           g_pGrassTexRV;

ID3D11Texture1D*                    g_pRandomTexture = nullptr;
ID3D11ShaderResourceView*           g_pRandomTexRV = nullptr;
ID3D11Texture2D*                    g_pLeafTexture = nullptr;
ID3D11ShaderResourceView*           g_pLeafTexRV = nullptr;
ID3D11Buffer*                       g_pTreeInstanceData = nullptr;
XMMATRIX*                           g_pTreeInstanceList = nullptr;
int                                 g_iNumTreeInstances;
int                                 g_iNumTreesToDraw = 15;//MAX_TREE_INSTANCES;
int                                 g_iGrassCoverage = 15;
float                               g_fGrassMessiness = 30.0f;
bool                                g_bAnimateCamera;   // Whether the camera movement is on

// Effect handles
ID3DX11EffectMatrixVariable*         g_pmWorldViewProj = nullptr;
ID3DX11EffectMatrixVariable*         g_pmWorldView = nullptr;
ID3DX11EffectShaderResourceVariable* g_pDiffuseTex = nullptr;
ID3DX11EffectShaderResourceVariable* g_pRandomTex = nullptr;
ID3DX11EffectShaderResourceVariable* g_pTextureArray = nullptr;
ID3DX11EffectMatrixVariable*         g_pmTreeMatrices = nullptr;
ID3DX11EffectScalarVariable*         g_piNumTrees = nullptr;
ID3DX11EffectScalarVariable*         g_pGrassWidth = nullptr;
ID3DX11EffectScalarVariable*         g_pGrassHeight = nullptr;
ID3DX11EffectScalarVariable*         g_piGrassCoverage = nullptr;
ID3DX11EffectScalarVariable*         g_pGrassMessiness = nullptr;

ID3DX11EffectTechnique*              g_pRenderInstancedVertLighting = nullptr;
ID3DX11EffectTechnique*              g_pRenderSkybox = nullptr;
ID3DX11EffectTechnique*              g_pRenderQuad = nullptr;
ID3DX11EffectTechnique*              g_pRenderGrass = nullptr;


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_STATIC                 -1
#define IDC_TOGGLEFULLSCREEN        1
#define IDC_TOGGLEREF               2
#define IDC_CHANGEDEVICE            3
#define IDC_NUMTREES_STATIC         4
#define IDC_NUMTREES                5
#define IDC_GRASSCOVERAGE_STATIC    6
#define IDC_GRASSCOVERAGE           7
#define IDC_GRASSMESSINESS_STATIC   8
#define IDC_GRASSMESSINESS          9
#define IDC_TOGGLEWARP             11

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

void UpdateLeafVertexLayout( ID3D11Device* pd3dDevice );
HRESULT RenderSceneGeometry( ID3D11DeviceContext* pd3dImmediateContext, CXMMATRIX mView, CXMMATRIX mProj );
HRESULT RenderInstancedQuads( ID3D11DeviceContext* pd3dImmediateContext, CXMMATRIX mView, CXMMATRIX mProj );
HRESULT CreateQuadMesh( ID3D11Device* pd3dDevice, QUAD_MESH* pMesh, float fWidth, float fHeight );
HRESULT LoadInstanceData( ID3D11Device* pd3dDevice, ID3D11Buffer** ppInstanceData, DWORD* pdwNumLeaves, LPCTSTR szFileName );
HRESULT LoadTreeInstanceData( ID3D11Device* pd3dDevice, ID3D11Buffer** ppInstanceData, DWORD dwNumTrees );
HRESULT LoadTextureArray( ID3D11Device* pd3dDevice, LPCTSTR* szTextureNames, int iNumTextures, ID3D11Texture2D** ppTex2D, ID3D11ShaderResourceView** ppSRV );
HRESULT CreateRandomTexture( ID3D11Device* pd3dDevice );
HRESULT CreateRandomTreeMatrices();


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
    DXUTCreateWindow( L"Instancing" );
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
    g_SampleUI.SetCallback( OnGUIEvent );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 0, iY, 170, 23 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 0, iY += 26, 170, 23, VK_F2 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 0, iY += 26, 170, 23, VK_F3 );
    g_HUD.AddButton( IDC_TOGGLEWARP, L"Toggle WARP (F4)", 0, iY += 26, 170, 23, VK_F4 );

    iY += 50;
    WCHAR str[MAX_PATH];
    swprintf_s( str, MAX_PATH, L"Trees: %d", g_iNumTreesToDraw );
    g_HUD.AddStatic( IDC_NUMTREES_STATIC, str, 25, iY += 24, 135, 22 );
    g_HUD.AddSlider( IDC_NUMTREES, 35, iY += 24, 135, 22, 0, 20, g_iNumTreesToDraw );

    swprintf_s( str, MAX_PATH, L"Grass Coverage: %d", g_iGrassCoverage );
    g_HUD.AddStatic( IDC_GRASSCOVERAGE_STATIC, str, 25, iY += 24, 135, 22 );
    g_HUD.AddSlider( IDC_GRASSCOVERAGE, 35, iY += 24, 135, 22, 0, 50, g_iGrassCoverage );

    swprintf_s( str, MAX_PATH, L"Grass Messiness: %f", g_fGrassMessiness );
    g_HUD.AddStatic( IDC_GRASSMESSINESS_STATIC, str, 20, iY += 24, 140, 22 );
    g_HUD.AddSlider( IDC_GRASSMESSINESS, 35, iY += 24, 135, 22, 0, 2000, ( int )( g_fGrassMessiness * 25 ) );
}


//--------------------------------------------------------------------------------------
// Called right before creating a device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    // We are using BGRA format textures
    pDeviceSettings->d3d11.CreateFlags |= D3D11_CREATE_DEVICE_BGRA_SUPPORT;
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
    WCHAR str[MAX_PATH] = {0};

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
        case IDC_NUMTREES:
        {
            g_iNumTreesToDraw = g_HUD.GetSlider( IDC_NUMTREES )->GetValue();

            swprintf_s( str, MAX_PATH, L"Trees: %d", g_iNumTreesToDraw );
            g_HUD.GetStatic( IDC_NUMTREES_STATIC )->SetText( str );

            UpdateLeafVertexLayout( DXUTGetD3D11Device() );
            break;
        }
        case IDC_GRASSCOVERAGE:
        {
            g_iGrassCoverage = g_HUD.GetSlider( IDC_GRASSCOVERAGE )->GetValue();

            swprintf_s( str, MAX_PATH, L"Grass Coverage: %d", g_iGrassCoverage );
            g_HUD.GetStatic( IDC_GRASSCOVERAGE_STATIC )->SetText( str );
            break;
        }
        case IDC_GRASSMESSINESS:
        {
            g_fGrassMessiness = g_HUD.GetSlider( IDC_GRASSMESSINESS )->GetValue() / ( float )25.0;
            g_pGrassMessiness->SetFloat( g_fGrassMessiness );

            swprintf_s( str, MAX_PATH, L"Grass Messiness: %f", g_fGrassMessiness );
            g_HUD.GetStatic( IDC_GRASSMESSINESS_STATIC )->SetText( str );
            break;
        }
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

    // Set us up for multisampling
    D3D11_RASTERIZER_DESC CurrentRasterizerState;
    CurrentRasterizerState.FillMode = D3D11_FILL_SOLID;
    CurrentRasterizerState.CullMode = D3D11_CULL_FRONT;
    CurrentRasterizerState.FrontCounterClockwise = true;
    CurrentRasterizerState.DepthBias = false;
    CurrentRasterizerState.DepthBiasClamp = 0;
    CurrentRasterizerState.SlopeScaledDepthBias = 0;
    CurrentRasterizerState.DepthClipEnable = true;
    CurrentRasterizerState.ScissorEnable = false;
    CurrentRasterizerState.MultisampleEnable = true;
    CurrentRasterizerState.AntialiasedLineEnable = false;
    V_RETURN( pd3dDevice->CreateRasterizerState( &CurrentRasterizerState, &g_pRasterState ) );
    pd3dImmediateContext->RSSetState( g_pRasterState );

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
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Instancing.fx" ) );

    V_RETURN( D3DX11CompileEffectFromFile( str, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, dwShaderFlags, 0, pd3dDevice, &g_pEffect, nullptr) );

#else

    ID3DBlob* pEffectBuffer = nullptr;
    V_RETURN( DXUTCompileFromFile( L"Instancing.fx", nullptr, "none", "fx_5_0", dwShaderFlags, 0, &pEffectBuffer ) );
    hr = D3DX11CreateEffectFromMemory( pEffectBuffer->GetBufferPointer(), pEffectBuffer->GetBufferSize(), 0, pd3dDevice, &g_pEffect );
    SAFE_RELEASE( pEffectBuffer );
    if ( FAILED(hr) )
        return hr;
    
#endif

    // Obtain the technique handles
    g_pRenderInstancedVertLighting = g_pEffect->GetTechniqueByName( "RenderInstancedVertLighting" );
    g_pRenderSkybox = g_pEffect->GetTechniqueByName( "RenderSkybox" );
    g_pRenderQuad = g_pEffect->GetTechniqueByName( "RenderQuad" );
    g_pRenderGrass = g_pEffect->GetTechniqueByName( "RenderGrass" );

    // Obtain the parameter handles
    g_pmWorldViewProj = g_pEffect->GetVariableByName( "g_mWorldViewProj" )->AsMatrix();
    g_pmWorldView = g_pEffect->GetVariableByName( "g_mWorldView" )->AsMatrix();
    g_pDiffuseTex = g_pEffect->GetVariableByName( "g_txDiffuse" )->AsShaderResource();
    g_pTextureArray = g_pEffect->GetVariableByName( "g_tx2dArray" )->AsShaderResource();
    g_pRandomTex = g_pEffect->GetVariableByName( "g_txRandom" )->AsShaderResource();
    g_pmTreeMatrices = g_pEffect->GetVariableByName( "g_mTreeMatrices" )->AsMatrix();
    g_piNumTrees = g_pEffect->GetVariableByName( "g_iNumTrees" )->AsScalar();
    g_pGrassWidth = g_pEffect->GetVariableByName( "g_GrassWidth" )->AsScalar();
    g_pGrassHeight = g_pEffect->GetVariableByName( "g_GrassHeight" )->AsScalar();
    g_piGrassCoverage = g_pEffect->GetVariableByName( "g_iGrassCoverage" )->AsScalar();
    g_pGrassMessiness = g_pEffect->GetVariableByName( "g_GrassMessiness" )->AsScalar();

    g_pGrassMessiness->SetFloat( g_fGrassMessiness );

    // Define our instanced vertex data layout
    const D3D11_INPUT_ELEMENT_DESC instlayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXTURE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "mTransform", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "mTransform", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "mTransform", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "mTransform", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
    };
    int iNumElements = sizeof( instlayout ) / sizeof( D3D11_INPUT_ELEMENT_DESC );

    D3DX11_PASS_DESC PassDesc;
    auto pPass = g_pRenderInstancedVertLighting->GetPassByIndex( 0 );
    V_RETURN( pPass->GetDesc( &PassDesc ) );

    V_RETURN( pd3dDevice->CreateInputLayout( instlayout, iNumElements, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pInstVertexLayout ) );

    g_iNumTreeInstances = MAX_TREE_INSTANCES;
    g_pTreeInstanceList = g_treeInstanceMatrices;

    // We use a special function to update our leaf layout to make sure that we can modify it to match the number of trees we're drawing
    UpdateLeafVertexLayout( pd3dDevice );

    // Define our scene vertex layout
    const D3D11_INPUT_ELEMENT_DESC scenelayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXTURE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    iNumElements = sizeof( scenelayout ) / sizeof( D3D11_INPUT_ELEMENT_DESC );

    pPass = g_pRenderSkybox->GetPassByIndex( 0 );
    V_RETURN( pPass->GetDesc( &PassDesc ) );
    V_RETURN( pd3dDevice->CreateInputLayout( scenelayout, iNumElements, PassDesc.pIAInputSignature,
                                             PassDesc.IAInputSignatureSize, &g_pSkyVertexLayout ) );

    // Load meshes
    V_RETURN( g_MeshSkybox.Create( pd3dDevice, L"CloudBox\\skysphere.sdkmesh" ) );
    V_RETURN( g_MeshIsland.Create( pd3dDevice, L"IslandScene\\island.sdkmesh" ) );
    V_RETURN( g_MeshIslandTop.Create( pd3dDevice, L"IslandScene\\islandtop_opt.sdkmesh" ) );
    V_RETURN( g_MeshTree.Create( pd3dDevice, L"trees\\tree.sdkmesh" ) );

    // Create some random tree matrices
    CreateRandomTreeMatrices();

    // Load our leaf instance data
    V_RETURN( LoadInstanceData( pd3dDevice, &g_pLeafInstanceData, &g_dwNumLeaves, g_szTreeLeafInstanceNames[0] ) );

    // Get the list of tree instances
    V_RETURN( LoadTreeInstanceData( pd3dDevice, &g_pTreeInstanceData, g_iNumTreeInstances ) );

    // Create a leaf mesh consisting of 4 verts to be instanced all over the trees
    V_RETURN( CreateQuadMesh( pd3dDevice, &g_MeshLeaf, 80.0f, 80.0f ) );

    // Load the leaf array textures we'll be using
    V_RETURN( LoadTextureArray( pd3dDevice, g_szLeafTextureNames, sizeof( g_szLeafTextureNames ) / sizeof
                                ( g_szLeafTextureNames[0] ),
                                &g_pLeafTexture, &g_pLeafTexRV ) );

    // Load the grass texture arrays
    V_RETURN( LoadTextureArray( pd3dDevice, g_szGrassTextureNames, sizeof( g_szGrassTextureNames ) / sizeof
                                ( g_szGrassTextureNames[0] ),
                                &g_pGrassTexture, &g_pGrassTexRV ) );

    // Create the random texture that fuels our random vector generator in the effect
    V_RETURN( CreateRandomTexture( pd3dDevice ) );

    // Setup the camera's view parameters
    static const XMVECTORF32 s_vEyeStart = { 100.f, 400.f, 2000.f, 0.f };
    static const XMVECTORF32 s_vAtStart = { 0.f, 0.f, -2000.f, 0.f };
    g_Camera.SetViewParams( s_vEyeStart, s_vAtStart );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr = S_OK;

    V_RETURN( g_DialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( 53.4f * ( XM_PI / 180.0f ), fAspectRatio, 20.0f, 30000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( 0, MOUSE_WHEEL, MOUSE_RIGHT_BUTTON | MOUSE_LEFT_BUTTON );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 170, 300 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT RenderSceneGeometry( ID3D11DeviceContext* pd3dImmediateContext, CXMMATRIX mView, CXMMATRIX mProj )
{
    // Set IA parameters
    pd3dImmediateContext->IASetInputLayout( g_pSkyVertexLayout );

    // Render the skybox
    XMMATRIX mViewSkybox = mView;
    static const XMVECTORU32 ctrl = { XM_SELECT_0, XM_SELECT_0, XM_SELECT_0, XM_SELECT_1 };
    mViewSkybox.r[ 3 ] = XMVectorSelect( g_XMZero, mViewSkybox.r[ 3 ], ctrl );
    XMMATRIX mWorldViewProj = XMMatrixMultiply( mViewSkybox, mProj );
    g_pmWorldViewProj->SetMatrix( ( float* )&mWorldViewProj );
    g_pmWorldView->SetMatrix( ( float* )&mViewSkybox );

    pd3dImmediateContext->IASetInputLayout( g_pSkyVertexLayout );

    ID3D11Buffer* pVB[2];
    UINT Strides[2] = {0,0};
    UINT Offsets[2] = {0,0};
    pVB[0] = g_MeshSkybox.GetVB11( 0, 0 );
    pVB[1] = nullptr;
    Strides[0] = ( UINT )g_MeshSkybox.GetVertexStride( 0, 0 );
    pd3dImmediateContext->IASetVertexBuffers( 0, 2, pVB, Strides, Offsets );
    pd3dImmediateContext->IASetIndexBuffer( g_MeshSkybox.GetIB11( 0 ), g_MeshSkybox.GetIBFormat11( 0 ), 0 );

    //Render
    D3DX11_TECHNIQUE_DESC techDesc;
    HRESULT hr;
    V_RETURN( g_pRenderSkybox->GetDesc(&techDesc) );
    
    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        for( UINT subset = 0; subset < g_MeshSkybox.GetNumSubsets( 0 ); ++subset )
        {
            // Get the subset
            auto pSubset = g_MeshSkybox.GetSubset( 0, subset );

            auto PrimType = CDXUTSDKMesh::GetPrimitiveType11( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
            pd3dImmediateContext->IASetPrimitiveTopology( PrimType );

            auto pDiffuseRV = g_MeshSkybox.GetMaterial( pSubset->MaterialID )->pDiffuseRV11;
            g_pDiffuseTex->SetResource( pDiffuseRV );

            g_pRenderSkybox->GetPassByIndex( p )->Apply( 0, pd3dImmediateContext );
            pd3dImmediateContext->DrawIndexed( ( UINT )pSubset->IndexCount, 0, ( UINT )pSubset->VertexStart );
        }
    }

    // set us up for instanced rendering
    pd3dImmediateContext->IASetInputLayout( g_pInstVertexLayout );
    mWorldViewProj = XMMatrixMultiply( mView, mProj );
    g_pmWorldViewProj->SetMatrix( ( float* )&mWorldViewProj );
    g_pmWorldView->SetMatrix( ( float* )&mView );

    ID3DX11EffectTechnique* pTechnique = g_pRenderInstancedVertLighting;

    // Render the island instanced
    pVB[0] = g_MeshIsland.GetVB11(0, 0);
    pVB[1] = g_pTreeInstanceData;
    Strides[0] = ( UINT )g_MeshIsland.GetVertexStride( 0, 0 );
    Strides[1] = sizeof( XMMATRIX );

    pd3dImmediateContext->IASetVertexBuffers( 0, 2, pVB, Strides, Offsets );
    pd3dImmediateContext->IASetIndexBuffer( g_MeshIsland.GetIB11( 0 ), g_MeshIsland.GetIBFormat11( 0 ), 0 );

    V_RETURN( pTechnique->GetDesc( &techDesc ) );

    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        for( UINT subset = 0; subset < g_MeshIsland.GetNumSubsets( 0 ); ++subset )
        {
            auto pSubset = g_MeshIsland.GetSubset( 0, subset );

            auto PrimType = g_MeshIsland.GetPrimitiveType11( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
            pd3dImmediateContext->IASetPrimitiveTopology( PrimType );

            auto pMat = g_MeshIsland.GetMaterial( pSubset->MaterialID );
            if( pMat )
                g_pDiffuseTex->SetResource( pMat->pDiffuseRV11 );

            pTechnique->GetPassByIndex( p )->Apply( 0, pd3dImmediateContext );
            pd3dImmediateContext->DrawIndexedInstanced( ( UINT )pSubset->IndexCount, g_iNumTreesToDraw,
                                              0, ( UINT )pSubset->VertexStart, 0 );
        }
    }

    // Render the tree instanced
    pVB[0] = g_MeshTree.GetVB11( 0, 0 );
    pVB[1] = g_pTreeInstanceData;
    Strides[0] = ( UINT )g_MeshTree.GetVertexStride( 0, 0 );
    Strides[1] = sizeof( XMMATRIX );

    pd3dImmediateContext->IASetVertexBuffers( 0, 2, pVB, Strides, Offsets );
    pd3dImmediateContext->IASetIndexBuffer( g_MeshTree.GetIB11( 0 ), g_MeshTree.GetIBFormat11( 0 ), 0 );

    V_RETURN( pTechnique->GetDesc( &techDesc ) );
    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        for( UINT subset = 0; subset < g_MeshTree.GetNumSubsets( 0 ); ++subset )
        {
            auto pSubset = g_MeshTree.GetSubset( 0, subset );

            auto PrimType = g_MeshTree.GetPrimitiveType11( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
            pd3dImmediateContext->IASetPrimitiveTopology( PrimType );

            auto pMat = g_MeshTree.GetMaterial( pSubset->MaterialID );
            if( pMat )
                g_pDiffuseTex->SetResource( pMat->pDiffuseRV11 );

            pTechnique->GetPassByIndex( p )->Apply( 0, pd3dImmediateContext );
            pd3dImmediateContext->DrawIndexedInstanced( ( UINT )pSubset->IndexCount, g_iNumTreesToDraw,
                                              0, ( UINT )pSubset->VertexStart, 0 );
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT RenderInstancedQuads( ID3D11DeviceContext* pd3dImmediateContext, CXMMATRIX mView, CXMMATRIX mProj )
{
    // Init Input Assembler states
    UINT strides[2] = { g_MeshLeaf.uStride, sizeof( INSTANCEDATA ) };
    UINT offsets[2] = { 0, 0 };

    // Draw leaves for all trees
    pd3dImmediateContext->IASetInputLayout( g_pLeafVertexLayout );

    g_pmTreeMatrices->SetMatrixArray( ( float* )g_pTreeInstanceList, 0, g_iNumTreesToDraw );
    g_piNumTrees->SetInt( g_iNumTreesToDraw );
    XMMATRIX mWorldViewProj = XMMatrixMultiply( mView, mProj );
    g_pmWorldViewProj->SetMatrix( ( float* )&mWorldViewProj );
    g_pmWorldView->SetMatrix( ( float* )&mView );

    ID3D11Buffer* pBuffers[2] =
    {
        g_MeshLeaf.pVB, g_pLeafInstanceData
    };
    pd3dImmediateContext->IASetVertexBuffers( 0, 2, pBuffers, strides, offsets );
    pd3dImmediateContext->IASetIndexBuffer( g_MeshLeaf.pIB, DXGI_FORMAT_R16_UINT, 0 );
    pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

    D3DX11_TECHNIQUE_DESC techDesc;
    HRESULT hr;
    V_RETURN( g_pRenderQuad->GetDesc( &techDesc ) );
    g_pTextureArray->SetResource( g_pLeafTexRV );
    for( UINT iPass = 0; iPass < techDesc.Passes; iPass++ )
    {
        g_pRenderQuad->GetPassByIndex( iPass )->Apply( 0, pd3dImmediateContext );
        pd3dImmediateContext->DrawIndexedInstanced( g_MeshLeaf.dwNumIndices, g_dwNumLeaves * g_iNumTreesToDraw, 0, 0, 0 );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
void RenderGrass( ID3D11DeviceContext* pd3dImmediateContext, CXMMATRIX mView, CXMMATRIX mProj )
{
    // Draw Grass
    XMMATRIX mIdentity = XMMatrixIdentity();
    XMMATRIX mWorldViewProj = XMMatrixMultiply( mView, mProj );
    g_pmWorldViewProj->SetMatrix( ( float* )&mWorldViewProj );
    g_pmWorldView->SetMatrix( ( float* )&mView );
    g_pRandomTex->SetResource( g_pRandomTexRV );

    int iGrassCoverage = g_iGrassCoverage;

    g_pGrassWidth->SetFloat( 50.0f );
    g_pGrassHeight->SetFloat( 50.0f );
    g_pTextureArray->SetResource( g_pGrassTexRV );
    g_piGrassCoverage->SetInt( iGrassCoverage );

    // set us up for instanced rendering
    pd3dImmediateContext->IASetInputLayout( g_pInstVertexLayout );
    mWorldViewProj = XMMatrixMultiply( mView, mProj );
    g_pmWorldViewProj->SetMatrix( ( float* )&mWorldViewProj );
    g_pmWorldView->SetMatrix( ( float* )&mView );

    ID3DX11EffectTechnique* pTechnique = g_pRenderGrass;
    ID3D11Buffer* pVB[2];
    UINT Strides[2];
    UINT Offsets[2] = {0,0};

    // Render the island tops instanced
    pVB[0] = g_MeshIslandTop.GetVB11( 0, 0 );
    pVB[1] = g_pTreeInstanceData;
    Strides[0] = ( UINT )g_MeshIslandTop.GetVertexStride( 0, 0 );
    Strides[1] = sizeof( XMMATRIX );

    pd3dImmediateContext->IASetVertexBuffers( 0, 2, pVB, Strides, Offsets );
    pd3dImmediateContext->IASetIndexBuffer( g_MeshIslandTop.GetIB11( 0 ), g_MeshIslandTop.GetIBFormat11( 0 ), 0 );

    D3DX11_TECHNIQUE_DESC techDesc;
    HRESULT hr;
    V( pTechnique->GetDesc( &techDesc ) );
    SDKMESH_SUBSET* pSubset = nullptr;
    SDKMESH_MATERIAL* pMat = nullptr;
    D3D11_PRIMITIVE_TOPOLOGY PrimType;

    for( UINT p = 0; p < techDesc.Passes; ++p )
    {
        for( UINT subset = 0; subset < g_MeshIslandTop.GetNumSubsets( 0 ); ++subset )
        {
            pSubset = g_MeshIslandTop.GetSubset( 0, subset );

            PrimType = g_MeshIslandTop.GetPrimitiveType11( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
            pd3dImmediateContext->IASetPrimitiveTopology( PrimType );

            pMat = g_MeshIslandTop.GetMaterial( pSubset->MaterialID );
            if( pMat )
                g_pDiffuseTex->SetResource( pMat->pDiffuseRV11 );

            pTechnique->GetPassByIndex( p )->Apply( 0, pd3dImmediateContext );
            pd3dImmediateContext->DrawIndexedInstanced( ( UINT )pSubset->IndexCount, g_iNumTreesToDraw,
                                              0, ( UINT )pSubset->VertexStart, 0 );
        }
    }
}


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

    auto pRTV = DXUTGetD3D11RenderTargetView();
    pd3dImmediateContext->ClearRenderTargetView( pRTV, Colors::Black );
    auto pDSV = DXUTGetD3D11DepthStencilView();
    pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );

    XMMATRIX mProj = g_Camera.GetProjMatrix();
    XMMATRIX mView = g_Camera.GetViewMatrix();

    RenderSceneGeometry( pd3dImmediateContext, mView, mProj ); // Render the scene with texture    
    RenderInstancedQuads( pd3dImmediateContext, mView, mProj ); // Render the instanced leaves for each tree
    RenderGrass( pd3dImmediateContext, mView, mProj ); // Render grass

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
    g_HUD.OnRender( fElapsedTime );
    g_SampleUI.OnRender( fElapsedTime );
    RenderText();
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
// Update the vertex layout for the number of leaves based upon the number
// of trees we're drawing.  This is to help us efficiently render leaves using
// instancing across the tree mesh and the leaf points on the trees.
//--------------------------------------------------------------------------------------
void UpdateLeafVertexLayout( ID3D11Device* pd3dDevice )
{
    SAFE_RELEASE( g_pLeafVertexLayout );

    // Define our leaf vertex data layout
    static const D3D11_INPUT_ELEMENT_DESC s_leaflayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXTURE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "mTransform", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 0 },
        { "mTransform", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 0 },
        { "mTransform", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 0 },
        { "mTransform", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 0 },
        { "fOcc",       0, DXGI_FORMAT_R32_FLOAT,          1, 64, D3D11_INPUT_PER_INSTANCE_DATA, 0 },
    };

    D3D11_INPUT_ELEMENT_DESC layout[ _countof(s_leaflayout) ];
    memcpy_s( layout, sizeof(layout), s_leaflayout, sizeof(s_leaflayout) );
    layout[2].InstanceDataStepRate =
    layout[3].InstanceDataStepRate =
    layout[4].InstanceDataStepRate =
    layout[5].InstanceDataStepRate =
    layout[6].InstanceDataStepRate = g_iNumTreesToDraw;

    D3DX11_PASS_DESC PassDesc;
    HRESULT hr;
    V( g_pRenderQuad->GetPassByIndex( 0 )->GetDesc( &PassDesc ) );
    pd3dDevice->CreateInputLayout( layout, _countof(s_leaflayout), PassDesc.pIAInputSignature,
                                   PassDesc.IAInputSignatureSize, &g_pLeafVertexLayout );
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
    SAFE_DELETE( g_pTxtHelper );
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_RELEASE( g_pEffect );
    SAFE_RELEASE( g_pInstVertexLayout );
    SAFE_RELEASE( g_pSkyVertexLayout );
    SAFE_RELEASE( g_pLeafVertexLayout );

    SAFE_RELEASE( g_pLeafInstanceData );
    SAFE_RELEASE( g_pLeafTexture );
    SAFE_RELEASE( g_pLeafTexRV );
    SAFE_RELEASE( g_pTreeInstanceData );

    SAFE_RELEASE( g_pGrassTexture );
    SAFE_RELEASE( g_pGrassTexRV );
    SAFE_RELEASE( g_pRandomTexture );
    SAFE_RELEASE( g_pRandomTexRV );

    SAFE_RELEASE( g_MeshLeaf.pVB );
    SAFE_RELEASE( g_MeshLeaf.pIB );

    g_MeshSkybox.Destroy();
    g_MeshIsland.Destroy();
    g_MeshIslandTop.Destroy();
    g_MeshTree.Destroy();

    SAFE_RELEASE( g_pRasterState );
}


//--------------------------------------------------------------------------------------
HRESULT CreateQuadMesh( ID3D11Device* pd3dDevice, QUAD_MESH* pMesh, float fWidth, float fHeight )
{
    HRESULT hr = S_OK;

    pMesh->dwNumVerts = 4;
    pMesh->dwNumIndices = 12;
    pMesh->uStride = sizeof( QUAD_VERTEX );

    /********************************
      leaves are quads anchored on the branch
      
      |---------|
      |D       C|
      |         |
      |         |
      |A       B|
      |---------|
      O<-----branch
      
     ********************************/
    fWidth /= 2.0f;
    QUAD_VERTEX quadVertices[] =
    {
        { XMFLOAT3( -fWidth, 0.0f, 0.0f ), XMFLOAT2( 0, 1 ) },
        { XMFLOAT3( fWidth, 0.0f, 0.0f ), XMFLOAT2( 1, 1 ) },
        { XMFLOAT3( fWidth, fHeight, 0.0f ), XMFLOAT2( 1, 0 ) },
        { XMFLOAT3( -fWidth, fHeight, 0.0f ), XMFLOAT2( 0, 0 ) },
    };

    // Create a resource with the input vertices
    // Create it with usage default, because it will never change
    D3D11_BUFFER_DESC bufferDesc =
    {
        pMesh->dwNumVerts * sizeof( QUAD_VERTEX ),
        D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0
    };
    D3D11_SUBRESOURCE_DATA vbInitData = {};
    vbInitData.pSysMem = quadVertices;
    V_RETURN( pd3dDevice->CreateBuffer( &bufferDesc, &vbInitData, &pMesh->pVB ) );

    // Create the index buffer
    bufferDesc.ByteWidth = pMesh->dwNumIndices * sizeof( WORD );
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bufferDesc.CPUAccessFlags = 0;

    WORD wIndices[] =
    {
        0,1,2,
        0,2,3,
        0,2,1,
        0,3,2,
    };
    D3D11_SUBRESOURCE_DATA ibInitData = {};
    ibInitData.pSysMem = wIndices;

    V_RETURN( pd3dDevice->CreateBuffer( &bufferDesc, &ibInitData, &pMesh->pIB ) );

    return hr;
}


//--------------------------------------------------------------------------------------
HRESULT LoadInstanceData( ID3D11Device* pd3dDevice, ID3D11Buffer** ppInstanceData, DWORD* pdwNumLeaves, LPCTSTR szFileName )
{
    HRESULT hr = S_OK;

    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, szFileName ) );

    *pdwNumLeaves = 0;
    HANDLE hFile = CreateFile( str, FILE_READ_DATA, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr );
    if( INVALID_HANDLE_VALUE == hFile )
        return E_FAIL;

    DWORD dwBytesRead = 0;
    if( !ReadFile( hFile, pdwNumLeaves, sizeof( DWORD ), &dwBytesRead, nullptr ) )
    {
        CloseHandle( hFile );
        return E_FAIL;
    }

    // Make sure we have some leaves
    if( ( *pdwNumLeaves ) == 0 )
    {
        CloseHandle( hFile );
        return E_FAIL;
    }

    // Create a resource with the input matrices
    // We're creating this buffer as dynamic because in a game, the instance data could be dynamic... aka
    // we could have moving leaves.
    D3D11_BUFFER_DESC bufferDesc =
    {
        ( *pdwNumLeaves ) * sizeof( INSTANCEDATA ),
        D3D11_USAGE_DYNAMIC,
        D3D11_BIND_VERTEX_BUFFER,
        D3D11_CPU_ACCESS_WRITE,
        0
    };

    V_RETURN( pd3dDevice->CreateBuffer( &bufferDesc, nullptr, ppInstanceData ) );

    INSTANCEDATA* pInstances = nullptr;

    D3D11_MAPPED_SUBRESOURCE mr;

    auto pd3dImmediateContext = DXUTGetD3D11DeviceContext();

    if( FAILED( pd3dImmediateContext->Map(*ppInstanceData, 0, D3D11_MAP_WRITE_DISCARD, 0, &mr) ) )
    {
        CloseHandle( hFile );
        return E_FAIL;
    }

    pInstances = (INSTANCEDATA*)mr.pData;

    if( !ReadFile( hFile, pInstances, sizeof( INSTANCEDATA ) * ( *pdwNumLeaves ), &dwBytesRead, nullptr ) )
    {
        CloseHandle( hFile );
        pd3dImmediateContext->Unmap(*ppInstanceData, 0);
        return E_FAIL;
    }

    pd3dImmediateContext->Unmap(*ppInstanceData, 0);

    CloseHandle( hFile );

    return hr;
}


//--------------------------------------------------------------------------------------
HRESULT LoadTreeInstanceData( ID3D11Device* pd3dDevice, ID3D11Buffer** ppInstanceData, DWORD dwNumTrees )
{
    HRESULT hr = S_OK;

    // Create a resource with the input matrices
    // We're creating this buffer as dynamic because in a game, the instance data could be dynamic... aka
    // we could have moving trees.
    D3D11_BUFFER_DESC bufferDesc =
    {
        dwNumTrees * sizeof( XMMATRIX ),
        D3D11_USAGE_DYNAMIC,
        D3D11_BIND_VERTEX_BUFFER,
        D3D11_CPU_ACCESS_WRITE,
        0
    };

    V_RETURN( pd3dDevice->CreateBuffer( &bufferDesc, nullptr, ppInstanceData ) );

    XMMATRIX* pMatrices = nullptr;

    D3D11_MAPPED_SUBRESOURCE mr;

    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();

    V_RETURN( pd3dImmediateContext->Map(*ppInstanceData, 0, D3D11_MAP_WRITE_DISCARD, 0, &mr) );

    pMatrices = (XMMATRIX*)mr.pData;

    memcpy( pMatrices, g_pTreeInstanceList, dwNumTrees * sizeof( XMMATRIX ) );

    pd3dImmediateContext->Unmap(*ppInstanceData, 0);

    return hr;
}



//--------------------------------------------------------------------------------------
// LoadTextureArray loads a texture array and associated view from a series
// of textures on disk.
//--------------------------------------------------------------------------------------
HRESULT LoadTextureArray( ID3D11Device* pd3dDevice, LPCTSTR* szTextureNames, int iNumTextures,
                          ID3D11Texture2D** ppTex2D, ID3D11ShaderResourceView** ppSRV )
{
    HRESULT hr = S_OK;
    D3D11_TEXTURE2D_DESC desc = {};

    WCHAR str[MAX_PATH] = {};
    for( int i = 0; i < iNumTextures; i++ )
    {
        V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, szTextureNames[i] ) );

        ID3D11Resource* pRes = nullptr;
        V_RETURN( CreateDDSTextureFromFileEx( pd3dDevice, str, 0, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ, 0, true, &pRes, nullptr, nullptr ) );
        if( pRes )
        {
            ID3D11Texture2D* pTemp;
            V_RETURN( pRes->QueryInterface( __uuidof( ID3D11Texture2D ), ( LPVOID* )&pTemp ) );
            pTemp->GetDesc( &desc );

            if( desc.MipLevels > 4 )
                desc.MipLevels -= 4;
            if( !( *ppTex2D ) )
            {
                desc.Usage = D3D11_USAGE_DEFAULT;
                desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                desc.CPUAccessFlags = 0;
                desc.ArraySize = iNumTextures;
                V_RETURN( pd3dDevice->CreateTexture2D( &desc, nullptr, ppTex2D ) );
            }

            ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();

            for( UINT iMip = 0; iMip < desc.MipLevels; iMip++ )
            {

                D3D11_MAPPED_SUBRESOURCE mr;

                pd3dImmediateContext->Map(pTemp, iMip, D3D11_MAP_READ, 0, &mr);

                if(mr.pData)
                {
                    pd3dImmediateContext->UpdateSubresource( ( *ppTex2D ),
                                                   D3D11CalcSubresource( iMip, i, desc.MipLevels ),
                                                   nullptr,
                                                   mr.pData,
                                                   mr.RowPitch,
                                                   0 );
                }

                pd3dImmediateContext->Unmap( pTemp, iMip );
            }

            SAFE_RELEASE( pRes );
            SAFE_RELEASE( pTemp );
        }
        else
        {
            return E_FAIL;
        }
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
    SRVDesc.Format = MAKE_SRGB( desc.Format );
    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    SRVDesc.Texture2DArray.MipLevels = desc.MipLevels;
    SRVDesc.Texture2DArray.ArraySize = iNumTextures;
    V_RETURN( pd3dDevice->CreateShaderResourceView( *ppTex2D, &SRVDesc, ppSRV ) );

    return hr;
}


//--------------------------------------------------------------------------------------
// This helper function creates a 1D texture full of random vectors.  The shader uses
// the current time value to index into this texture to get a random vector.
//--------------------------------------------------------------------------------------
HRESULT CreateRandomTexture( ID3D11Device* pd3dDevice )
{
    HRESULT hr = S_OK;

    int iNumRandValues = 1024;
    srand( 0 );
    //create the data
    D3D11_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = new float[iNumRandValues * 4];
    if( !InitData.pSysMem )
        return E_OUTOFMEMORY;
    InitData.SysMemPitch = iNumRandValues * 4 * sizeof( float );
    InitData.SysMemSlicePitch = iNumRandValues * 4 * sizeof( float );
    for( int i = 0; i < iNumRandValues * 4; i++ )
    {
        ( ( float* )InitData.pSysMem )[i] = float( ( rand() % 10000 ) - 5000 );
    }

    // Create the texture
    D3D11_TEXTURE1D_DESC dstex;
    dstex.Width = iNumRandValues;
    dstex.MipLevels = 1;
    dstex.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    dstex.Usage = D3D11_USAGE_DEFAULT;
    dstex.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    dstex.CPUAccessFlags = 0;
    dstex.MiscFlags = 0;
    dstex.ArraySize = 1;
    V_RETURN( pd3dDevice->CreateTexture1D( &dstex, &InitData, &g_pRandomTexture ) );
    SAFE_DELETE_ARRAY( InitData.pSysMem );

    // Create the resource view
    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
    SRVDesc.Format = dstex.Format;
    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
    SRVDesc.Texture1D.MipLevels = dstex.MipLevels;
    V_RETURN( pd3dDevice->CreateShaderResourceView( g_pRandomTexture, &SRVDesc, &g_pRandomTexRV ) );

    return hr;
}

//--------------------------------------------------------------------------------------
float RPercent()
{
    float ret = ( float )( ( rand() % 20000 ) - 10000 );
    return ret / 10000.0f;
}

//--------------------------------------------------------------------------------------
// This helper function creates a bunch of random tree matrices to show off
// instancing the same tree and island multiple times.
//--------------------------------------------------------------------------------------
HRESULT CreateRandomTreeMatrices()
{
    srand( 100 );	//use the same random seed every time so we can verify output
    float fScale = 100.0f;

    for( int i = 1; i < MAX_TREE_INSTANCES; i++ )
    {
        //find a random position
        XMFLOAT3 pos;
        pos.x = RPercent() * 140.0f;
        pos.y = RPercent() * 20.0f - 10.0f;
        pos.z = 15.0f + fabs( RPercent() ) * 200.0f;

        pos.x *= -1;
        pos.z *= -1;

        pos.x *= fScale; pos.y *= fScale; pos.z *= fScale;

        float fRot = RPercent() * XM_PI;

        XMMATRIX mRot = XMMatrixRotationY( fRot );
        XMMATRIX mTrans = XMMatrixTranslation( pos.x, pos.y, pos.z );

        g_treeInstanceMatrices[i] = mRot * mTrans;
    }
    return S_OK;
}
