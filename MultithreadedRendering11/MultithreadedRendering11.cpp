//--------------------------------------------------------------------------------------
// File: MultithreadedRendering11.cpp
//
// This sample shows an example of using Direct3D 11's Deferred Contexts for
// multi-threaded rendering.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "resource.h"
#include "SDKmisc.h"
#include "SDKMesh.h"

#include <process.h>

#include <algorithm>

#include "MultiDeviceContextDXUTMesh.h"

#pragma warning( disable : 4100 )

using namespace DirectX;

// #defines for compile-time Debugging switches:
//#define ADJUSTABLE_LIGHT          // The 0th light is adjustable with the mouse (right mouse button down)
#define RENDER_SCENE_LIGHT_POV      // F4 toggles between the usual camera and the 0th light's point-of-view
//#define UNCOMPRESSED_VERTEX_DATA  // The sdkmesh file contained uncompressed vertex data

// The five render path options, in a radio button group at the right of the screen
enum DEVICECONTEXT_TYPE
{
    DEVICECONTEXT_IMMEDIATE,                // Traditional rendering, one thread, immediate device context
    DEVICECONTEXT_ST_DEFERRED_PER_SCENE,    // One thread, multiple deferred device contexts, one per scene 
    DEVICECONTEXT_MT_DEFERRED_PER_SCENE,    // Multiple threads, one per scene, each with one deferred device context
    DEVICECONTEXT_ST_DEFERRED_PER_CHUNK,    // One thread, multiple deferred device contexts, one per physical processor 
    DEVICECONTEXT_MT_DEFERRED_PER_CHUNK,    // Multiple threads, one per physical processor, each with one deferred device context
};

// By convention, the first n lights are shadow casting, and the rest just illuminate.
const int   g_iNumLights = 4;
const int   g_iNumShadows = 1;
const int   g_iNumMirrors = 4;

// The vertex for a corner of the mirror quad.  Only the Position is used.  
// The others are so we can use the same vertex shader as the main scene
struct MirrorVertex
{
    XMFLOAT3 Position;
    XMFLOAT3 Normal;
    XMFLOAT2 Texcoord;
    XMFLOAT3 Tangent;
};
typedef MirrorVertex MirrorRect[4];

//--------------------------------------------------------------------------------------
// Job queue structures
//--------------------------------------------------------------------------------------

// Everything necessary for scene setup which depends on
// which scene we're drawing (shadow/mirror/direct), but
// doesn't change per scene.
//
// These can be passed to per-chunk worker threads by
// reference.
struct SceneParamsStatic
{
    ID3D11DepthStencilState*    m_pDepthStencilState;
    UINT8                       m_iStencilRef;

    ID3D11RasterizerState*      m_pRasterizerState;

    XMFLOAT4                    m_vTintColor;
    XMFLOAT4                    m_vMirrorPlane;

    // If m_pDepthStencilView is non-nullptr then these 
    // are for a shadow map.  Otherwise, use the DXUT
    // defaults.
    ID3D11DepthStencilView*     m_pDepthStencilView;
    D3D11_VIEWPORT*             m_pViewport;
};

// Everything necessary for scene setup which depends on
// which scene we're drawing (shadow/mirror/direct), and
// also changes per scene.
//
// To be safe, we pass these to per-chunk worker threads 
// by value.  This would become necessary if we were to 
// start letting thread communication lag by more than
// one scene --- e.g. the main thread starts on scene 2
// while the worker threads are still operating on scene 1.
struct SceneParamsDynamic
{
    XMFLOAT4X4 m_mViewProj;
};

// The different types of job in the per-chunk work queues
enum WorkQueueEntryType
{
    WORK_QUEUE_ENTRY_TYPE_SETUP, 
    WORK_QUEUE_ENTRY_TYPE_CHUNK, 
    WORK_QUEUE_ENTRY_TYPE_FINALIZE, 

    WORK_QUEUE_ENTRY_TYPE_COUNT
};

// The contents of the work queues depend on the job type...
struct WorkQueueEntryBase
{
    WorkQueueEntryType          m_iType;
};

// Work item params for scene setup
struct WorkQueueEntrySetup : public WorkQueueEntryBase
{
    const SceneParamsStatic*    m_pSceneParamsStatic;
    SceneParamsDynamic          m_SceneParamsDynamic;
};

// Work item params for chunk render
struct WorkQueueEntryChunk : public WorkQueueEntryBase
{
    int                         m_iMesh;
};

// Work item params for scene finalize
struct WorkQueueEntryFinalize : public WorkQueueEntryBase
{
};

// The work item queue for each per-chunk worker thread
const int                   g_iSceneQueueSizeInBytes = 16 * 1024;
typedef BYTE                ChunkQueue[g_iSceneQueueSizeInBytes];

//--------------------------------------------------------------------------------------
// Constant buffers
//--------------------------------------------------------------------------------------
struct CB_VS_PER_OBJECT
{
    XMFLOAT4X4 m_mWorld;
};
UINT                        g_iCBVSPerObjectBind = 0;

struct CB_VS_PER_SCENE
{
    XMFLOAT4X4 m_mViewProj;
};
UINT                        g_iCBVSPerSceneBind = 1;

struct CB_PS_PER_OBJECT
{
    XMFLOAT4 m_vObjectColor;
};
UINT                        g_iCBPSPerObjectBind = 0;

struct CB_PS_PER_LIGHT
{
    struct LightDataStruct
    {
        XMFLOAT4X4  m_mLightViewProj;
        XMFLOAT4 m_vLightPos;
        XMFLOAT4 m_vLightDir;
        XMFLOAT4 m_vLightColor;
        XMFLOAT4 m_vFalloffs;    // x = dist end, y = dist range, z = cos angle end, w = cos range
    } m_LightData[g_iNumLights];
};
UINT                        g_iCBPSPerLightBind = 1;

struct CB_PS_PER_SCENE
{
    XMFLOAT4 m_vMirrorPlane;
    XMFLOAT4 m_vAmbientColor;
    XMFLOAT4 m_vTintColor;
};
UINT                        g_iCBPSPerSceneBind = 2;

ID3D11Buffer*               g_pcbVSPerObject = nullptr;
ID3D11Buffer*               g_pcbVSPerScene = nullptr;
ID3D11Buffer*               g_pcbPSPerObject = nullptr;
ID3D11Buffer*               g_pcbPSPerLight = nullptr;
ID3D11Buffer*               g_pcbPSPerScene = nullptr;

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN                    1
#define IDC_TOGGLEREF                           3
#define IDC_CHANGEDEVICE                        4
#define IDC_TOGGLEWIRE                          5
#define IDC_DEVICECONTEXT_GROUP                 6
#define IDC_DEVICECONTEXT_IMMEDIATE             7
#define IDC_DEVICECONTEXT_ST_DEFERRED_PER_SCENE 8
#define IDC_DEVICECONTEXT_MT_DEFERRED_PER_SCENE 9
#define IDC_DEVICECONTEXT_ST_DEFERRED_PER_CHUNK 10
#define IDC_DEVICECONTEXT_MT_DEFERRED_PER_CHUNK 11
#define IDC_TOGGLELIGHTVIEW                     12

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
bool                        g_bClearStateUponBeginCommandList = false;
bool                        g_bClearStateUponFinishCommandList = false;
bool                        g_bClearStateUponExecuteCommandList = false;

CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
#ifdef ADJUSTABLE_LIGHT
CDXUTDirectionWidget        g_LightControl;
#endif
CD3DSettingsDlg             g_D3DSettingsDlg;       // Device settings dialog
CDXUTDialog                 g_HUD;                  // manages the 3D   
CDXUTDialog                 g_SampleUI;             // dialog for sample specific controls
CDXUTTextHelper*            g_pTxtHelper = nullptr;
bool                        g_bShowHelp = false;    // If true, it renders the UI control text
bool                        g_bWireFrame = false;

//--------------------------------------------------------------------------------------
// Default view parameters
//--------------------------------------------------------------------------------------
CModelViewerCamera          g_Camera;               // A model viewing camera

static const XMVECTORF32    s_vDefaultEye = { 30.0f, 150.0f, -150.0f, 0.f };
static const XMVECTORF32    s_vDefaultLookAt = { 0.0f, 60.0f, 0.0f, 0.f };
static const FLOAT          s_fNearPlane            = 2.0f;
static const FLOAT          s_fFarPlane             = 4000.0f;
static const FLOAT          s_fFOV                  = XM_PI / 4.0f;
static const XMVECTORF32    s_vSceneCenter          = { 0.0f, 350.0f, 0.0f, 0.f };
static const FLOAT          s_fSceneRadius          = 600.0f;
static const FLOAT          s_fDefaultCameraRadius  = 300.0f;
static const FLOAT          s_fMinCameraRadius      = 150.0f;
static const FLOAT          s_fMaxCameraRadius      = 450.0f;

#ifdef RENDER_SCENE_LIGHT_POV
bool                        g_bRenderSceneLightPOV  = false;
#endif

//--------------------------------------------------------------------------------------
// Lighting params (to be read from content when the pipeline supports it)
//--------------------------------------------------------------------------------------
static const XMVECTORF32    s_vAmbientColor = { 0.04f * 0.760f, 0.04f * 0.793f, 0.04f * 0.822f, 1.000f };
static const XMVECTORF32    s_vMirrorTint = { 0.3f, 0.5f, 1.0f, 1.0f };

XMFLOAT4                    g_vLightColor[g_iNumLights];
XMVECTOR                    g_vLightPos[g_iNumLights];
XMVECTOR                    g_vLightDir[g_iNumLights];
FLOAT                       g_fLightFalloffDistEnd[g_iNumLights];
FLOAT                       g_fLightFalloffDistRange[g_iNumLights];
FLOAT                       g_fLightFalloffCosAngleEnd[g_iNumLights];
FLOAT                       g_fLightFalloffCosAngleRange[g_iNumLights];
FLOAT                       g_fLightFOV[g_iNumLights];
FLOAT                       g_fLightAspect[g_iNumLights];
FLOAT                       g_fLightNearPlane[g_iNumLights];
FLOAT                       g_fLightFarPlane[g_iNumLights];

// The scene data
CMultiDeviceContextDXUTMesh g_Mesh11;

//--------------------------------------------------------------------------------------
// Rendering interfaces
//--------------------------------------------------------------------------------------
ID3D11InputLayout*          g_pVertexLayout11 = nullptr;
ID3D11VertexShader*         g_pVertexShader = nullptr;
ID3D11PixelShader*          g_pPixelShader = nullptr;
ID3D11SamplerState*         g_pSamPointClamp = nullptr;
ID3D11SamplerState*         g_pSamLinearWrap = nullptr;
ID3D11RasterizerState*      g_pRasterizerStateNoCull = nullptr;
ID3D11RasterizerState*      g_pRasterizerStateBackfaceCull = nullptr;
ID3D11RasterizerState*      g_pRasterizerStateFrontfaceCull = nullptr;
ID3D11RasterizerState*      g_pRasterizerStateNoCullWireFrame = nullptr;
ID3D11DepthStencilState*    g_pDepthStencilStateNoStencil = nullptr;

//--------------------------------------------------------------------------------------
// Shadow map data and interface
//--------------------------------------------------------------------------------------
ID3D11Texture2D*            g_pShadowTexture[g_iNumShadows] = { nullptr };
ID3D11ShaderResourceView*   g_pShadowResourceView[g_iNumShadows] = { nullptr };
ID3D11DepthStencilView*     g_pShadowDepthStencilView[g_iNumShadows] = { nullptr };
D3D11_VIEWPORT              g_ShadowViewport[g_iNumShadows] = { 0 };
FLOAT                       g_fShadowResolutionX[g_iNumShadows];
FLOAT                       g_fShadowResolutionY[g_iNumShadows];

//--------------------------------------------------------------------------------------
// Mirror data and interfaces
//--------------------------------------------------------------------------------------
XMVECTOR                    g_vMirrorCenter[g_iNumMirrors];
XMVECTOR                    g_vMirrorNormal[g_iNumMirrors];
XMVECTOR                    g_vMirrorPlane[g_iNumMirrors];
FLOAT                       g_fMirrorWidth[g_iNumMirrors];
FLOAT                       g_fMirrorHeight[g_iNumMirrors];
FLOAT                       g_fMirrorResolutionX[g_iNumMirrors];
FLOAT                       g_fMirrorResolutionY[g_iNumMirrors];
XMFLOAT3                    g_vMirrorCorner[4];
MirrorRect                  g_MirrorRect[g_iNumMirrors];
const UINT8                 g_iStencilMask = 0x01;
const UINT8                 g_iStencilRef = 0x01;
ID3D11DepthStencilState*    g_pMirrorDepthStencilStateDepthTestStencilOverwrite = nullptr;
ID3D11DepthStencilState*    g_pMirrorDepthStencilStateDepthOverwriteStencilTest = nullptr;
ID3D11DepthStencilState*    g_pMirrorDepthStencilStateDepthWriteStencilTest = nullptr;
ID3D11DepthStencilState*    g_pMirrorDepthStencilStateDepthOverwriteStencilClear = nullptr;
ID3D11Buffer*               g_pMirrorVertexBuffer = nullptr;
ID3D11InputLayout*          g_pMirrorVertexLayout11 = nullptr;

//--------------------------------------------------------------------------------------
// Per-scene-worker-thread values
//--------------------------------------------------------------------------------------
unsigned int WINAPI         _PerSceneRenderDeferredProc( LPVOID lpParameter );
const int                   g_iNumPerSceneRenderThreads = g_iNumShadows + g_iNumMirrors + 1;    // One thread per scene
HANDLE                      g_hPerSceneRenderDeferredThread[g_iNumPerSceneRenderThreads];
HANDLE                      g_hBeginPerSceneRenderDeferredEvent[g_iNumPerSceneRenderThreads];
HANDLE                      g_hEndPerSceneRenderDeferredEvent[g_iNumPerSceneRenderThreads];
ID3D11DeviceContext*        g_pd3dPerSceneDeferredContext[g_iNumPerSceneRenderThreads] = {nullptr};
ID3D11CommandList*          g_pd3dPerSceneCommandList[g_iNumPerSceneRenderThreads] = {nullptr};
int                         g_iPerSceneThreadInstanceData[g_iNumPerSceneRenderThreads];

//--------------------------------------------------------------------------------------
// Per-chunk-worker-thread values
//--------------------------------------------------------------------------------------
unsigned int WINAPI         _PerChunkRenderDeferredProc( LPVOID lpParameter );
const int                   g_iMaxPerChunkRenderThreads = 32;   // For true scalability, this should not be fixed at compile-time 
const int                   g_iMaxPendingQueueEntries = 1024;     // Max value of g_hBeginPerChunkRenderDeferredSemaphore
int                         g_iNumPerChunkRenderThreads;    // One thread per physical processor, minus the main thread
HANDLE                      g_hPerChunkRenderDeferredThread[g_iMaxPerChunkRenderThreads];
HANDLE                      g_hBeginPerChunkRenderDeferredSemaphore[g_iMaxPerChunkRenderThreads];
HANDLE                      g_hEndPerChunkRenderDeferredEvent[g_iMaxPerChunkRenderThreads];
ID3D11DeviceContext*        g_pd3dPerChunkDeferredContext[g_iMaxPerChunkRenderThreads] = {nullptr};
ID3D11CommandList*          g_pd3dPerChunkCommandList[g_iMaxPerChunkRenderThreads] = {nullptr};
int                         g_iPerChunkThreadInstanceData[g_iMaxPerChunkRenderThreads];
ChunkQueue                  g_ChunkQueue[g_iMaxPerChunkRenderThreads];
int                         g_iPerChunkQueueOffset[g_iMaxPerChunkRenderThreads]; // next free portion of the queue to add an entry to

// The default render pathway
DEVICECONTEXT_TYPE          g_iDeviceContextType = DEVICECONTEXT_IMMEDIATE;

SceneParamsStatic           g_StaticParamsDirect;
SceneParamsStatic           g_StaticParamsShadow[g_iNumShadows];
SceneParamsStatic           g_StaticParamsMirror[g_iNumMirrors];

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
// Convenient checks for the current render pathway
//--------------------------------------------------------------------------------------
inline bool IsRenderDeferredPerScene() 
{ 
    return g_iDeviceContextType == DEVICECONTEXT_ST_DEFERRED_PER_SCENE
        || g_iDeviceContextType == DEVICECONTEXT_MT_DEFERRED_PER_SCENE;
}
inline bool IsRenderMultithreadedPerScene() 
{ 
    return g_iDeviceContextType == DEVICECONTEXT_MT_DEFERRED_PER_SCENE;
}
inline bool IsRenderDeferredPerChunk() 
{ 
    return g_iDeviceContextType == DEVICECONTEXT_ST_DEFERRED_PER_CHUNK
        || g_iDeviceContextType == DEVICECONTEXT_MT_DEFERRED_PER_CHUNK;
}
inline bool IsRenderMultithreadedPerChunk() 
{ 
    return g_iDeviceContextType == DEVICECONTEXT_MT_DEFERRED_PER_CHUNK;
}
inline bool IsRenderDeferred()
{
    return IsRenderDeferredPerScene() || IsRenderDeferredPerChunk();
}


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
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackFrameMove( OnFrameMove );

    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );
    
    InitApp();
    DXUTInit( true, true, lpCmdLine ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"MultithreadedRendering11" );
    DXUTCreateDevice( D3D_FEATURE_LEVEL_10_0, true, 800, 600 );
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

    g_HUD.SetCallback( OnGUIEvent ); 
    int iY = 30;
    int iYo = 26;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 0, iY, 170, 22 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 0, iY += iYo, 170, 22, VK_F3 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 0, iY += iYo, 170, 22, VK_F2 );
#ifdef RENDER_SCENE_LIGHT_POV
    g_HUD.AddButton( IDC_TOGGLELIGHTVIEW, L"Toggle view (F4)", 0, iY += iYo, 170, 22, VK_F4 );
#endif
    g_HUD.AddButton( IDC_TOGGLEWIRE, L"Toggle Wires (F6)", 0, iY += iYo, 170, 22, VK_F6 );
    g_HUD.AddRadioButton( IDC_DEVICECONTEXT_IMMEDIATE, IDC_DEVICECONTEXT_GROUP, L"Immediate", 0, iY += iYo, 170, 22 );
    g_HUD.AddRadioButton( IDC_DEVICECONTEXT_ST_DEFERRED_PER_SCENE, IDC_DEVICECONTEXT_GROUP, L"ST Def/Scene", 0, iY += iYo, 170, 22 );
    g_HUD.AddRadioButton( IDC_DEVICECONTEXT_MT_DEFERRED_PER_SCENE, IDC_DEVICECONTEXT_GROUP, L"MT Def/Scene", 0, iY += iYo, 170, 22 );
    g_HUD.AddRadioButton( IDC_DEVICECONTEXT_ST_DEFERRED_PER_CHUNK, IDC_DEVICECONTEXT_GROUP, L"ST Def/Chunk", 0, iY += iYo, 170, 22 );
    g_HUD.AddRadioButton( IDC_DEVICECONTEXT_MT_DEFERRED_PER_CHUNK, IDC_DEVICECONTEXT_GROUP, L"MT Def/Chunk", 0, iY += iYo, 170, 22 );

    CDXUTRadioButton* pRadioButton = g_HUD.GetRadioButton( IDC_DEVICECONTEXT_IMMEDIATE );
    pRadioButton->SetChecked( true );

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
    static float fTotalTime = 0.0f;
    fTotalTime += fElapsedTime;

    // Jigger the overhead lights --- these are hard-coded to indices 1,2,3
    // Ideally, we'd attach the lights to the relevant objects in the mesh
    // file and animate those objects.  But for now, just some hard-coded
    // swinging...
    XMVECTOR cycle1 = XMVectorSet( 0.f,
                                   0.f,
                                   0.20f * sinf( 2.0f * ( fTotalTime + 0.0f * XM_PI ) ),
                                   0.f );
    XMVECTOR v = g_XMNegIdentityR1 + cycle1;
    g_vLightDir[1] = XMVector3Normalize( v );

    XMVECTOR cycle2 = XMVectorSet( 0.10f * cosf( 1.6f * ( fTotalTime + 0.3f * XM_PI ) ),
                                   0.f,
                                   0.10f * sinf( 1.6f * ( fTotalTime + 0.0f * XM_PI ) ),
                                   0.f );
    v = g_XMNegIdentityR1 + cycle2;
    g_vLightDir[2] = XMVector3Normalize( v );

    XMVECTOR cycle3 = XMVectorSet( 0.30f * cosf( 2.4f * ( fTotalTime + 0.3f * XM_PI ) ),
                                   0.f,
                                   0.f,
                                   0.f );
    v = g_XMNegIdentityR1 + cycle3;
    g_vLightDir[3] = XMVector3Normalize( v );

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

        g_pTxtHelper->SetInsertionPos( 350, nBackBufferHeight - 20 * 5 );
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

#ifdef ADJUSTABLE_LIGHT
    g_LightControl.HandleMessages( hWnd, uMsg, wParam, lParam );
#endif

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
            g_D3DSettingsDlg.SetActive( !g_D3DSettingsDlg.IsActive() );
            break;
#ifdef RENDER_SCENE_LIGHT_POV
        case IDC_TOGGLELIGHTVIEW:
            g_bRenderSceneLightPOV = !g_bRenderSceneLightPOV;
            break;
#endif
        case IDC_TOGGLEWIRE:
            g_bWireFrame = !g_bWireFrame;
            break;
        case IDC_DEVICECONTEXT_IMMEDIATE:
            g_iDeviceContextType = DEVICECONTEXT_IMMEDIATE;
            break;
        case IDC_DEVICECONTEXT_ST_DEFERRED_PER_SCENE:
            g_iDeviceContextType = DEVICECONTEXT_ST_DEFERRED_PER_SCENE;
            break;
        case IDC_DEVICECONTEXT_MT_DEFERRED_PER_SCENE:
            g_iDeviceContextType = DEVICECONTEXT_MT_DEFERRED_PER_SCENE;
            break;
        case IDC_DEVICECONTEXT_ST_DEFERRED_PER_CHUNK:
            g_iDeviceContextType = DEVICECONTEXT_ST_DEFERRED_PER_CHUNK;
            break;
        case IDC_DEVICECONTEXT_MT_DEFERRED_PER_CHUNK:
            g_iDeviceContextType = DEVICECONTEXT_MT_DEFERRED_PER_CHUNK;
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
void InitializeLights()
{
    // Our hand-tuned approximation to the sky light
    static const XMVECTORF32 s_lightDir0 = { -0.67f, -0.71f, +0.21f, 0.f };

    //g_vLightColor[0] =                  XMFLOAT4( 1.5f * 0.160f, 1.5f * 0.341f, 1.5f * 1.000f, 1.000f );
    g_vLightColor[0] =                  XMFLOAT4( 3.0f * 0.160f, 3.0f * 0.341f, 3.0f * 1.000f, 1.000f );
    g_vLightDir[0] =                    XMVector3Normalize( s_lightDir0 ); 
    g_vLightPos[0] =                    s_vSceneCenter - s_fSceneRadius * g_vLightDir[0];
    g_fLightFOV[0] =                    XM_PI / 4.0f;

    // The three overhead lamps
    static const XMVECTORF32 s_lightPos1 = { 0.0f, 400.0f, -250.0f, 0.f };
    static const XMVECTORF32 s_lightPos2 = { 0.0f, 400.0f, 0.0f, 0.f };
    static const XMVECTORF32 s_lightPos3 = { 0.0f, 400.0f, 250.0f, 0.f };

    g_vLightColor[1] =                  XMFLOAT4( 0.4f * 0.895f, 0.4f * 0.634f, 0.4f * 0.626f, 1.0f );
    g_vLightPos[1] =                    s_lightPos1;
    g_vLightDir[1] =                    g_XMNegIdentityR1;
    g_fLightFOV[1] =                    65.0f * ( XM_PI / 180.0f );
    
    g_vLightColor[2] =                  XMFLOAT4( 0.5f * 0.388f, 0.5f * 0.641f, 0.5f * 0.401f, 1.0f );
    g_vLightPos[2] =                    s_lightPos2;
    g_vLightDir[2] =                    g_XMNegIdentityR1;
    g_fLightFOV[2] =                    65.0f * ( XM_PI / 180.0f );
    
    g_vLightColor[3] =                  XMFLOAT4( 0.4f * 1.000f, 0.4f * 0.837f, 0.4f * 0.848f, 1.0f );
    g_vLightPos[3] =                    s_lightPos3;
    g_vLightDir[3] =                    g_XMNegIdentityR1;
    g_fLightFOV[3] =                    65.0f * ( XM_PI / 180.0f );
    
    // For the time beings, let's make these params follow the same pattern for all lights
    for ( int iLight = 0; iLight < g_iNumLights; ++iLight )
    {
        g_fLightAspect[iLight] = 1.0f;
        g_fLightNearPlane[iLight] = 100.f;
        g_fLightFarPlane[iLight] = 2.0f * s_fSceneRadius;

        g_fLightFalloffDistEnd[iLight] = g_fLightFarPlane[iLight];
        g_fLightFalloffDistRange[iLight] = 100.0f;

        g_fLightFalloffCosAngleEnd[iLight] = cosf( g_fLightFOV[iLight] / 2.0f );
        g_fLightFalloffCosAngleRange[iLight] = 0.1f;
    }

#ifdef ADJUSTABLE_LIGHT
    // The adjustable light is number 0
    g_LightControl.SetLightDirection( g_vLightDir[0] );
#endif
}


//--------------------------------------------------------------------------------------
// Create D3D11 resources for the shadows
//--------------------------------------------------------------------------------------
HRESULT InitializeShadows( ID3D11Device* pd3dDevice )
{
    HRESULT hr = S_OK;

    for ( int iShadow = 0; iShadow < g_iNumShadows; ++iShadow )
    {
        // constant for now
        g_fShadowResolutionX[iShadow] = 2048.0f;
        g_fShadowResolutionY[iShadow] = 2048.0f;

        // The shadow map, along with depth-stencil and texture view
        D3D11_TEXTURE2D_DESC ShadowDesc = {
            UINT(g_fShadowResolutionX[iShadow]),    // UINT Width;
            UINT(g_fShadowResolutionY[iShadow]),    // UINT Height;
            1,                                      // UINT MipLevels;
            1,                                      // UINT ArraySize;
            DXGI_FORMAT_R32_TYPELESS,               // DXGI_FORMAT Format;
            { 1, 0, },                              // DXGI_SAMPLE_DESC SampleDesc;
            D3D11_USAGE_DEFAULT,                    // D3D11_USAGE Usage;
            D3D11_BIND_SHADER_RESOURCE 
                | D3D11_BIND_DEPTH_STENCIL,         // UINT BindFlags;
            0,                                      // UINT CPUAccessFlags;
            0,                                      // UINT MiscFlags;
        };
        D3D11_DEPTH_STENCIL_VIEW_DESC ShadowDepthStencilViewDesc = {
            DXGI_FORMAT_D32_FLOAT,                  // DXGI_FORMAT Format;
            D3D11_DSV_DIMENSION_TEXTURE2D,          // D3D11_DSV_DIMENSION ViewDimension;
            0,                                      // UINT ReadOnlyUsage;
            {0, },                                  // D3D11_TEX2D_RTV Texture2D;
        };
        D3D11_SHADER_RESOURCE_VIEW_DESC ShadowResourceViewDesc = {
            DXGI_FORMAT_R32_FLOAT,                  // DXGI_FORMAT Format;
            D3D11_SRV_DIMENSION_TEXTURE2D,          // D3D11_SRV_DIMENSION ViewDimension;
            {0, 1, },                               // D3D11_TEX2D_SRV Texture2D;
        };
        V_RETURN( pd3dDevice->CreateTexture2D( &ShadowDesc, nullptr, &g_pShadowTexture[iShadow] ) );
        DXUT_SetDebugName( g_pShadowTexture[iShadow], "Shadow" );

        V_RETURN( pd3dDevice->CreateDepthStencilView( g_pShadowTexture[iShadow], &ShadowDepthStencilViewDesc, 
            &g_pShadowDepthStencilView[iShadow] ) );
        DXUT_SetDebugName( g_pShadowDepthStencilView[iShadow], "Shadow DSV" );

        V_RETURN( pd3dDevice->CreateShaderResourceView( g_pShadowTexture[iShadow], &ShadowResourceViewDesc, 
            &g_pShadowResourceView[iShadow] ) );
        DXUT_SetDebugName( g_pShadowResourceView[iShadow] , "Shadow RSV" );

        g_ShadowViewport[iShadow].Width = g_fShadowResolutionX[iShadow];
        g_ShadowViewport[iShadow].Height = g_fShadowResolutionY[iShadow];
        g_ShadowViewport[iShadow].MinDepth = 0;
        g_ShadowViewport[iShadow].MaxDepth = 1;
        g_ShadowViewport[iShadow].TopLeftX = 0;
        g_ShadowViewport[iShadow].TopLeftY = 0;

        // The parameters to pass to per-chunk threads for the shadow scenes
        g_StaticParamsShadow[iShadow].m_pDepthStencilState = g_pDepthStencilStateNoStencil;
        g_StaticParamsShadow[iShadow].m_iStencilRef = 0;
        g_StaticParamsShadow[iShadow].m_pRasterizerState = g_pRasterizerStateFrontfaceCull;
        XMStoreFloat4( &g_StaticParamsShadow[iShadow].m_vMirrorPlane, g_XMZero );
        XMStoreFloat4( &g_StaticParamsShadow[iShadow].m_vTintColor, Colors::White );
        g_StaticParamsShadow[iShadow].m_pDepthStencilView = g_pShadowDepthStencilView[iShadow];
        g_StaticParamsShadow[iShadow].m_pViewport = &g_ShadowViewport[iShadow];
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Create D3D11 resources for the mirrors
//--------------------------------------------------------------------------------------
HRESULT InitializeMirrors( ID3D11Device* pd3dDevice )
{
    HRESULT hr = S_OK;

    // The stencil method for the mirror rendering requires several different
    // depth-stencil states...

    // Write stencil if the depth test passes
    D3D11_DEPTH_STENCIL_DESC DepthStencilDescDepthTestStencilOverwrite = {
        TRUE,                           // BOOL DepthEnable;
        D3D11_DEPTH_WRITE_MASK_ZERO,    // D3D11_DEPTH_WRITE_MASK DepthWriteMask;
        D3D11_COMPARISON_LESS_EQUAL,    // D3D11_COMPARISON_FUNC DepthFunc;
        TRUE,                           // BOOL StencilEnable;
        0,                              // UINT8 StencilReadMask;
        g_iStencilMask,                 // UINT8 StencilWriteMask;
        {                               // D3D11_DEPTH_STENCILOP_DESC FrontFace;
            D3D11_STENCIL_OP_REPLACE,   // D3D11_STENCIL_OP StencilFailOp;
            D3D11_STENCIL_OP_KEEP,      // D3D11_STENCIL_OP StencilDepthFailOp;
            D3D11_STENCIL_OP_REPLACE,   // D3D11_STENCIL_OP StencilPassOp;
            D3D11_COMPARISON_ALWAYS,    // D3D11_COMPARISON_FUNC StencilFunc;
        }, 
        {                               // D3D11_DEPTH_STENCILOP_DESC BackFace;
            D3D11_STENCIL_OP_REPLACE,   // D3D11_STENCIL_OP StencilFailOp;
            D3D11_STENCIL_OP_KEEP,      // D3D11_STENCIL_OP StencilDepthFailOp;
            D3D11_STENCIL_OP_REPLACE,   // D3D11_STENCIL_OP StencilPassOp;
            D3D11_COMPARISON_ALWAYS,    // D3D11_COMPARISON_FUNC StencilFunc;
        }, 
    };
    V_RETURN( pd3dDevice->CreateDepthStencilState( 
        &DepthStencilDescDepthTestStencilOverwrite, 
        &g_pMirrorDepthStencilStateDepthTestStencilOverwrite ) );
    DXUT_SetDebugName( g_pMirrorDepthStencilStateDepthTestStencilOverwrite, "Mirror SO" );

    // Overwrite depth and if stencil test passes 
    D3D11_DEPTH_STENCIL_DESC DepthStencilDescDepthOverwriteStencilTest = {
        TRUE,                           // BOOL DepthEnable;
        D3D11_DEPTH_WRITE_MASK_ALL,     // D3D11_DEPTH_WRITE_MASK DepthWriteMask;
        D3D11_COMPARISON_ALWAYS,        // D3D11_COMPARISON_FUNC DepthFunc;
        TRUE,                           // BOOL StencilEnable;
        g_iStencilMask,                 // UINT8 StencilReadMask;
        0,                              // UINT8 StencilWriteMask;
        {                               // D3D11_DEPTH_STENCILOP_DESC FrontFace;
            D3D11_STENCIL_OP_KEEP,      // D3D11_STENCIL_OP StencilFailOp;
            D3D11_STENCIL_OP_KEEP,      // D3D11_STENCIL_OP StencilDepthFailOp;
            D3D11_STENCIL_OP_KEEP,      // D3D11_STENCIL_OP StencilPassOp;
            D3D11_COMPARISON_EQUAL,     // D3D11_COMPARISON_FUNC StencilFunc;
        }, 
        {                               // D3D11_DEPTH_STENCILOP_DESC BackFace;
            D3D11_STENCIL_OP_KEEP,      // D3D11_STENCIL_OP StencilFailOp;
            D3D11_STENCIL_OP_KEEP,      // D3D11_STENCIL_OP StencilDepthFailOp;
            D3D11_STENCIL_OP_KEEP,      // D3D11_STENCIL_OP StencilPassOp;
            D3D11_COMPARISON_EQUAL,     // D3D11_COMPARISON_FUNC StencilFunc;
        }, 
    };
    V_RETURN( pd3dDevice->CreateDepthStencilState( 
        &DepthStencilDescDepthOverwriteStencilTest, 
        &g_pMirrorDepthStencilStateDepthOverwriteStencilTest ) );
    DXUT_SetDebugName( g_pMirrorDepthStencilStateDepthOverwriteStencilTest, "Mirror DO" );

    // Perform normal depth test/write if the stencil test passes
    D3D11_DEPTH_STENCIL_DESC DepthStencilDescDepthWriteStencilTest = {
        TRUE,                           // BOOL DepthEnable;
        D3D11_DEPTH_WRITE_MASK_ALL,     // D3D11_DEPTH_WRITE_MASK DepthWriteMask;
        D3D11_COMPARISON_LESS_EQUAL,    // D3D11_COMPARISON_FUNC DepthFunc;
        TRUE,                           // BOOL StencilEnable;
        g_iStencilMask,                 // UINT8 StencilReadMask;
        0,                              // UINT8 StencilWriteMask;
        {                               // D3D11_DEPTH_STENCILOP_DESC FrontFace;
            D3D11_STENCIL_OP_KEEP,      // D3D11_STENCIL_OP StencilFailOp;
            D3D11_STENCIL_OP_KEEP,      // D3D11_STENCIL_OP StencilDepthFailOp;
            D3D11_STENCIL_OP_KEEP,      // D3D11_STENCIL_OP StencilPassOp;
            D3D11_COMPARISON_EQUAL,     // D3D11_COMPARISON_FUNC StencilFunc;
        }, 
        {                               // D3D11_DEPTH_STENCILOP_DESC BackFace;
            D3D11_STENCIL_OP_KEEP,      // D3D11_STENCIL_OP StencilFailOp;
            D3D11_STENCIL_OP_KEEP,      // D3D11_STENCIL_OP StencilDepthFailOp;
            D3D11_STENCIL_OP_KEEP,      // D3D11_STENCIL_OP StencilPassOp;
            D3D11_COMPARISON_EQUAL,     // D3D11_COMPARISON_FUNC StencilFunc;
        }, 
    };
    V_RETURN( pd3dDevice->CreateDepthStencilState( 
        &DepthStencilDescDepthWriteStencilTest, 
        &g_pMirrorDepthStencilStateDepthWriteStencilTest ) );
    DXUT_SetDebugName( g_pMirrorDepthStencilStateDepthWriteStencilTest, "Mirror Normal" );

    // Overwrite depth and clear stencil if stencil test passes 
    D3D11_DEPTH_STENCIL_DESC DepthStencilDescDepthOverwriteStencilClear = {
        TRUE,                           // BOOL DepthEnable;
        D3D11_DEPTH_WRITE_MASK_ALL,     // D3D11_DEPTH_WRITE_MASK DepthWriteMask;
        D3D11_COMPARISON_ALWAYS,        // D3D11_COMPARISON_FUNC DepthFunc;
        TRUE,                           // BOOL StencilEnable;
        g_iStencilMask,                 // UINT8 StencilReadMask;
        g_iStencilMask,                 // UINT8 StencilWriteMask;
        {                               // D3D11_DEPTH_STENCILOP_DESC FrontFace;
            D3D11_STENCIL_OP_ZERO,      // D3D11_STENCIL_OP StencilFailOp;
            D3D11_STENCIL_OP_KEEP,      // D3D11_STENCIL_OP StencilDepthFailOp;
            D3D11_STENCIL_OP_ZERO,      // D3D11_STENCIL_OP StencilPassOp;
            D3D11_COMPARISON_EQUAL,     // D3D11_COMPARISON_FUNC StencilFunc;
        }, 
        {                               // D3D11_DEPTH_STENCILOP_DESC BackFace;
            D3D11_STENCIL_OP_ZERO,      // D3D11_STENCIL_OP StencilFailOp;
            D3D11_STENCIL_OP_KEEP,      // D3D11_STENCIL_OP StencilDepthFailOp;
            D3D11_STENCIL_OP_ZERO,      // D3D11_STENCIL_OP StencilPassOp;
            D3D11_COMPARISON_EQUAL,     // D3D11_COMPARISON_FUNC StencilFunc;
        }, 
    };
    V_RETURN( pd3dDevice->CreateDepthStencilState( 
        &DepthStencilDescDepthOverwriteStencilClear, 
        &g_pMirrorDepthStencilStateDepthOverwriteStencilClear ) );
    DXUT_SetDebugName( g_pMirrorDepthStencilStateDepthOverwriteStencilClear, "Mirror Clear" );

    // These values are hard-coded based on the sdkmesh contents, plus some
    // hand-fiddling, pending a better solution in the pipeline.
    static const XMVECTORF32 s_vMirrorCenter0 = { -35.1688f, 89.279683f, -0.7488765f, 0.f };
    static const XMVECTORF32 s_vMirrorCenter1 = { 41.2174f, 89.279683f, -0.7488745f, 0.f };
    static const XMVECTORF32 s_vMirrorCenter2 = { 3.024275f, 89.279683f, -54.344299f, 0.f };
    static const XMVECTORF32 s_vMirrorCenter3 = { 3.02427475f, 89.279683f, 52.8466f, 0.f };

    g_vMirrorCenter[0] = s_vMirrorCenter0;
    g_vMirrorCenter[1] = s_vMirrorCenter1;
    g_vMirrorCenter[2] = s_vMirrorCenter2;
    g_vMirrorCenter[3] = s_vMirrorCenter3;

    g_fMirrorWidth [0]   =   104.190895f;
    g_fMirrorHeight[0]   =   92.19922656f;
    g_fMirrorWidth [1]   =   104.190899f;
    g_fMirrorHeight[1]   =   92.19923178f;
    g_fMirrorWidth [2]   =   76.3862f;
    g_fMirrorHeight[2]   =   92.3427325f;
    g_fMirrorWidth [3]   =   76.386196f;
    g_fMirrorHeight[3]   =   92.34274043f;

    static const XMVECTORF32 s_vMirrorNormal0 = { -0.998638464f, -0.052165297f, 0.0f, 0.f };
    static const XMVECTORF32 s_vMirrorNormal1 = { 0.998638407f, -0.052166381f, 3.15017E-08f, 0.f };
    static const XMVECTORF32 s_vMirrorNormal2 = { 0.0f, -0.076278878f, -0.997086522f, 0.f };
    static const XMVECTORF32 s_vMirrorNormal3 = { -5.22129E-08f, -0.076279957f, 0.99708644f, 0.f };

    g_vMirrorNormal[0] = s_vMirrorNormal0;
    g_vMirrorNormal[1] = s_vMirrorNormal1;
    g_vMirrorNormal[2] = s_vMirrorNormal2;
    g_vMirrorNormal[3] = s_vMirrorNormal3;

    g_fMirrorResolutionX[0] = 320.0f;
    g_fMirrorResolutionY[0] = ( g_fMirrorResolutionX[0] * g_fMirrorHeight[0] / g_fMirrorWidth[0] );
    g_fMirrorResolutionX[1] = 320.0f;
    g_fMirrorResolutionY[1] = ( g_fMirrorResolutionX[1] * g_fMirrorHeight[1] / g_fMirrorWidth[1] );
    g_fMirrorResolutionX[2] = 320.0f;
    g_fMirrorResolutionY[2] = ( g_fMirrorResolutionX[2] * g_fMirrorHeight[2] / g_fMirrorWidth[2] );
    g_fMirrorResolutionX[3] = 320.0f;
    g_fMirrorResolutionY[3] = ( g_fMirrorResolutionX[3] * g_fMirrorHeight[3] / g_fMirrorWidth[3] );

    g_vMirrorCorner[0].x = -1.0f;
    g_vMirrorCorner[0].y = -1.0f;
    g_vMirrorCorner[0].z =  0.0f;
    g_vMirrorCorner[1].x =  1.0f;
    g_vMirrorCorner[1].y = -1.0f;
    g_vMirrorCorner[1].z =  0.0f;
    g_vMirrorCorner[2].x = -1.0f;
    g_vMirrorCorner[2].y =  1.0f;
    g_vMirrorCorner[2].z =  0.0f;
    g_vMirrorCorner[3].x =  1.0f;
    g_vMirrorCorner[3].y =  1.0f;
    g_vMirrorCorner[3].z =  0.0f;

    D3D11_BUFFER_DESC BufDesc;
    BufDesc.ByteWidth = sizeof( MirrorRect );
    BufDesc.Usage = D3D11_USAGE_DYNAMIC;
    BufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    BufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    BufDesc.MiscFlags = 0;
    pd3dDevice->CreateBuffer( &BufDesc, nullptr, &g_pMirrorVertexBuffer );
    DXUT_SetDebugName( g_pMirrorVertexBuffer, "Mirror VB" );

    for ( int iMirror = 0; iMirror < g_iNumMirrors; ++iMirror )
    {
        XMVECTOR p = XMPlaneFromPointNormal( g_vMirrorCenter[iMirror], g_vMirrorNormal[iMirror] );
        g_vMirrorPlane[iMirror] = p;

        // The vertex buffer contents for the mirror quad, in local space.
        for ( UINT iCorner = 0; iCorner < 4; ++ iCorner )
        {
            g_MirrorRect[iMirror][iCorner].Position.x = 0.5f * g_fMirrorWidth [iMirror] * g_vMirrorCorner[iCorner].x;
            g_MirrorRect[iMirror][iCorner].Position.y = 0.5f * g_fMirrorHeight[iMirror] * g_vMirrorCorner[iCorner].y;
            g_MirrorRect[iMirror][iCorner].Position.z =                                   g_vMirrorCorner[iCorner].z;

            g_MirrorRect[iMirror][iCorner].Normal.x = 
            g_MirrorRect[iMirror][iCorner].Normal.y = 
            g_MirrorRect[iMirror][iCorner].Normal.z = 0.0f;

            g_MirrorRect[iMirror][iCorner].Texcoord.x = 
            g_MirrorRect[iMirror][iCorner].Texcoord.y = 0.0f;

            g_MirrorRect[iMirror][iCorner].Tangent.x = 
            g_MirrorRect[iMirror][iCorner].Tangent.y = 
            g_MirrorRect[iMirror][iCorner].Tangent.z = 0.0f;
        }

        // The parameters to pass to per-chunk threads for the mirror scenes
        g_StaticParamsMirror[iMirror].m_pDepthStencilState = g_pMirrorDepthStencilStateDepthWriteStencilTest;
        g_StaticParamsMirror[iMirror].m_iStencilRef = g_iStencilRef;
        g_StaticParamsMirror[iMirror].m_pRasterizerState = g_pRasterizerStateBackfaceCull;
        XMStoreFloat4( &g_StaticParamsMirror[iMirror].m_vMirrorPlane, p );
        XMStoreFloat4( &g_StaticParamsMirror[iMirror].m_vTintColor, s_vMirrorTint );
        g_StaticParamsMirror[iMirror].m_pDepthStencilView = nullptr;
        g_StaticParamsMirror[iMirror].m_pViewport = nullptr;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Helper functions for querying information about the processors in the current
// system.  ( Copied from the doc page for GetLogicalProcessorInformation() )
//--------------------------------------------------------------------------------------
typedef BOOL (WINAPI *LPFN_GLPI)(
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, 
    PDWORD);


//  Helper function to count bits in the processor mask
static DWORD CountBits(ULONG_PTR bitMask)
{
    DWORD LSHIFT = sizeof(ULONG_PTR)*8 - 1;
    DWORD bitSetCount = 0;
    ULONG_PTR bitTest = ULONG_PTR(1) << LSHIFT;
    DWORD i;

    for( i = 0; i <= LSHIFT; ++i)
    {
        bitSetCount += ((bitMask & bitTest)?1:0);
        bitTest/=2;
    }

    return bitSetCount;
}


static int GetPhysicalProcessorCount()
{
    DWORD procCoreCount = 0;    // Return 0 on any failure.  That'll show them.

    HMODULE hMod = GetModuleHandle( L"kernel32" );
    assert( hMod );
    _Analysis_assume_( hMod );

    auto Glpi = reinterpret_cast<LPFN_GLPI>( GetProcAddress( hMod, "GetLogicalProcessorInformation" ) );
    if ( !Glpi )
    {
        // GetLogicalProcessorInformation is not supported
        return procCoreCount;
    }

    bool done = false;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = nullptr;
    DWORD returnLength = 0;

    while (!done) 
    {
        BOOL rc = Glpi(buffer, &returnLength);

        if (FALSE == rc) 
        {
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) 
            {
                if (buffer) 
                    free(buffer);

                buffer = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION>( malloc( returnLength ) );

                if ( !buffer ) 
                {
                    // Allocation failure
                    return procCoreCount;
                }
            } 
            else 
            {
                // Unanticipated error
                return procCoreCount;
            }
        } 
        else done = true;
    }

    assert( buffer );
    _Analysis_assume_( buffer );

    DWORD byteOffset = 0;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = buffer;
    while (byteOffset < returnLength) 
    {
        if (ptr->Relationship == RelationProcessorCore) 
        {
            if(ptr->ProcessorCore.Flags)
            {
                //  Hyperthreading or SMT is enabled.
                //  Logical processors are on the same core.
                procCoreCount += 1;
            }
            else
            {
                //  Logical processors are on different cores.
                procCoreCount += CountBits(ptr->ProcessorMask);
            }
        }
        byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
        ptr++;
    }

    free (buffer);

    return procCoreCount;
}


//--------------------------------------------------------------------------------------
// Create per-worker-thread resources
//--------------------------------------------------------------------------------------
HRESULT InitializeWorkerThreads( ID3D11Device* pd3dDevice )
{
    HRESULT hr;

    // Per-scene data init
    for ( int iInstance = 0; iInstance < g_iNumPerSceneRenderThreads; ++iInstance )
    {
        g_iPerSceneThreadInstanceData[iInstance] = iInstance;

        g_hBeginPerSceneRenderDeferredEvent[iInstance] = CreateEvent( nullptr, FALSE, FALSE, nullptr );
        g_hEndPerSceneRenderDeferredEvent[iInstance] = CreateEvent( nullptr, FALSE, FALSE, nullptr );

        V_RETURN( pd3dDevice->CreateDeferredContext( 0 /*Reserved for future use*/, 
            &g_pd3dPerSceneDeferredContext[iInstance] ) );

        g_hPerSceneRenderDeferredThread[iInstance] = ( HANDLE )_beginthreadex( 
            nullptr, 
            0, 
            _PerSceneRenderDeferredProc, 
            &g_iPerSceneThreadInstanceData[iInstance], 
            CREATE_SUSPENDED, 
            nullptr );

#if defined(PROFILE) || defined(DEBUG)
        char threadid[ 16 ];
        sprintf_s( threadid, sizeof(threadid), "PS %d", iInstance );
        DXUT_SetDebugName( g_pd3dPerSceneDeferredContext[iInstance], threadid );
#endif

        ResumeThread( g_hPerSceneRenderDeferredThread[iInstance] );
    }

    // Per-chunk data init

    // Reserve one core for the main thread if possible
    g_iNumPerChunkRenderThreads = GetPhysicalProcessorCount() - 1; 

    // Restrict to the max static allocation --- can be easily relaxed if need be
    g_iNumPerChunkRenderThreads = std::min( g_iNumPerChunkRenderThreads, g_iMaxPerChunkRenderThreads );

    // Need at least on worker thread, even on a single-core machine
    g_iNumPerChunkRenderThreads = std::max( g_iNumPerChunkRenderThreads, 1 );

    // uncomment to force exactly one worker context (and therefore predictable render order)
    //g_iNumPerChunkRenderThreads = 1; 

    for ( int iInstance = 0; iInstance < g_iNumPerChunkRenderThreads; ++iInstance )
    {
        g_iPerChunkThreadInstanceData[iInstance] = iInstance;

        g_hBeginPerChunkRenderDeferredSemaphore[iInstance] = CreateSemaphore( nullptr, 0, 
            g_iMaxPendingQueueEntries, nullptr );
        g_hEndPerChunkRenderDeferredEvent[iInstance] = CreateEvent( nullptr, FALSE, FALSE, nullptr );

        V_RETURN( pd3dDevice->CreateDeferredContext( 0 /*Reserved for future use*/, 
            &g_pd3dPerChunkDeferredContext[iInstance] ) );

        g_hPerChunkRenderDeferredThread[iInstance] = ( HANDLE )_beginthreadex( nullptr, 
            0, 
            _PerChunkRenderDeferredProc, 
            &g_iPerChunkThreadInstanceData[iInstance], 
            CREATE_SUSPENDED, 
            nullptr );

#if defined(PROFILE) || defined(DEBUG)
        char threadid[ 16 ];
        sprintf_s( threadid, sizeof(threadid), "PC %d", iInstance );
        DXUT_SetDebugName( g_pd3dPerChunkDeferredContext[iInstance], threadid );
#endif

        ResumeThread( g_hPerChunkRenderDeferredThread[iInstance] );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext )
{
    HRESULT hr;

    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );
    g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15 );

    // Compile the shaders 
    ID3DBlob* pVertexShaderBuffer = nullptr;
    V_RETURN( DXUTCompileFromFile( L"MultithreadedRendering11_VS.hlsl", nullptr, "VSMain", "vs_4_0",
                                   D3DCOMPILE_ENABLE_STRICTNESS, 0, &pVertexShaderBuffer ) );

    ID3DBlob* pPixelShaderBuffer = nullptr;
    V_RETURN( DXUTCompileFromFile( L"MultithreadedRendering11_PS.hlsl", nullptr, "PSMain", "ps_4_0",
                                   D3DCOMPILE_ENABLE_STRICTNESS, 0, &pPixelShaderBuffer ) );

    // Create the shaders
    V_RETURN( pd3dDevice->CreateVertexShader( pVertexShaderBuffer->GetBufferPointer(),
                                              pVertexShaderBuffer->GetBufferSize(), nullptr, &g_pVertexShader ) );
    V_RETURN( pd3dDevice->CreatePixelShader( pPixelShaderBuffer->GetBufferPointer(),
                                             pPixelShaderBuffer->GetBufferSize(), nullptr, &g_pPixelShader ) );

    DXUT_SetDebugName( g_pVertexShader, "VSMain" );
    DXUT_SetDebugName( g_pPixelShader, "PSMain" );

    // Create our vertex input layout
    // The content exporter supports either compressed or uncompressed formats for 
    // normal/tangent/binormal.  Unfortunately the relevant compressed formats are
    // deprecated for DX10+.  So they required special handling in the vertex shader.
    // If we use uncompressed data here, need to also #define UNCOMPRESSED_VERTEX_DATA
    // in the HLSL file.
    const D3D11_INPUT_ELEMENT_DESC UncompressedLayout[] =
    {
        { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT,   0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,      0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    const D3D11_INPUT_ELEMENT_DESC CompressedLayout[] =
    {
        { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT,   0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",    0, DXGI_FORMAT_R10G10B10A2_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",  0, DXGI_FORMAT_R16G16_FLOAT,      0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",   0, DXGI_FORMAT_R10G10B10A2_UNORM, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

#ifdef UNCOMPRESSED_VERTEX_DATA
    V_RETURN( pd3dDevice->CreateInputLayout( UncompressedLayout, 
        ARRAYSIZE( UncompressedLayout ), 
        pVertexShaderBuffer->GetBufferPointer(),
        pVertexShaderBuffer->GetBufferSize(), 
        &g_pVertexLayout11 ) );
    DXUT_SetDebugName(g_pVertexLayout11, "Uncompressed" );
#else
    V_RETURN( pd3dDevice->CreateInputLayout( CompressedLayout, 
        ARRAYSIZE( CompressedLayout ), 
        pVertexShaderBuffer->GetBufferPointer(),
        pVertexShaderBuffer->GetBufferSize(), 
        &g_pVertexLayout11 ) );

    DXUT_SetDebugName(g_pVertexLayout11, "Compressed" );
#endif

    V_RETURN( pd3dDevice->CreateInputLayout( UncompressedLayout, 
        ARRAYSIZE( UncompressedLayout ), 
        pVertexShaderBuffer->GetBufferPointer(),
        pVertexShaderBuffer->GetBufferSize(), 
        &g_pMirrorVertexLayout11 ) );
    DXUT_SetDebugName( g_pMirrorVertexLayout11, "Mirror" );
    
    SAFE_RELEASE( pVertexShaderBuffer );
    SAFE_RELEASE( pPixelShaderBuffer );

    // The standard depth-stencil state
    D3D11_DEPTH_STENCIL_DESC DepthStencilDescNoStencil = {
        TRUE,                           // BOOL DepthEnable;
        D3D11_DEPTH_WRITE_MASK_ALL,     // D3D11_DEPTH_WRITE_MASK DepthWriteMask;
        D3D11_COMPARISON_LESS_EQUAL,    // D3D11_COMPARISON_FUNC DepthFunc;
        FALSE,                          // BOOL StencilEnable;
        0,                              // UINT8 StencilReadMask;
        0,                              // UINT8 StencilWriteMask;
        {                               // D3D11_DEPTH_STENCILOP_DESC FrontFace;
            D3D11_STENCIL_OP_KEEP,      // D3D11_STENCIL_OP StencilFailOp;
            D3D11_STENCIL_OP_KEEP,      // D3D11_STENCIL_OP StencilDepthFailOp;
            D3D11_STENCIL_OP_KEEP,      // D3D11_STENCIL_OP StencilPassOp;
            D3D11_COMPARISON_NEVER,     // D3D11_COMPARISON_FUNC StencilFunc;
        }, 
        {                               // D3D11_DEPTH_STENCILOP_DESC BackFace;
            D3D11_STENCIL_OP_KEEP,      // D3D11_STENCIL_OP StencilFailOp;
            D3D11_STENCIL_OP_KEEP,      // D3D11_STENCIL_OP StencilDepthFailOp;
            D3D11_STENCIL_OP_KEEP,      // D3D11_STENCIL_OP StencilPassOp;
            D3D11_COMPARISON_NEVER,     // D3D11_COMPARISON_FUNC StencilFunc;
        }, 
    };
    V_RETURN( pd3dDevice->CreateDepthStencilState( 
        &DepthStencilDescNoStencil, 
        &g_pDepthStencilStateNoStencil ) );
    DXUT_SetDebugName( g_pDepthStencilStateNoStencil, "No Stencil" );

    // Provide the intercept callback for CMultiDeviceContextDXUTMesh, which allows
    // us to farm out different mesh chunks to different device contexts
    void RenderMesh( CMultiDeviceContextDXUTMesh* pMesh, 
        UINT iMesh,
        bool bAdjacent,
        ID3D11DeviceContext* pd3dDeviceContext,
        UINT iDiffuseSlot,
        UINT iNormalSlot,
        UINT iSpecularSlot );
    MDC_SDKMESH_CALLBACKS11 MeshCallbacks = {};
    MeshCallbacks.pRenderMesh = RenderMesh;

    // Load the mesh
    V_RETURN( g_Mesh11.Create( pd3dDevice, L"SquidRoom\\SquidRoom.sdkmesh", &MeshCallbacks ) );

    // Create sampler states for point/clamp (shadow map) and linear/wrap (everything else)
    D3D11_SAMPLER_DESC SamDesc;
    SamDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    SamDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamDesc.MipLODBias = 0.0f;
    SamDesc.MaxAnisotropy = 1;
    SamDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    SamDesc.BorderColor[0] = SamDesc.BorderColor[1] = SamDesc.BorderColor[2] = SamDesc.BorderColor[3] = 0;
    SamDesc.MinLOD = 0;
    SamDesc.MaxLOD = D3D11_FLOAT32_MAX;
    V_RETURN( pd3dDevice->CreateSamplerState( &SamDesc, &g_pSamPointClamp ) );
    DXUT_SetDebugName( g_pSamPointClamp, "PointClamp" );

    SamDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SamDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    V_RETURN( pd3dDevice->CreateSamplerState( &SamDesc, &g_pSamLinearWrap ) );
    DXUT_SetDebugName( g_pSamLinearWrap, "LinearWrap" );

    // Setup constant buffers
    D3D11_BUFFER_DESC Desc;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Desc.MiscFlags = 0;

    Desc.ByteWidth = sizeof( CB_VS_PER_SCENE );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, nullptr, &g_pcbVSPerScene ) );
    DXUT_SetDebugName( g_pcbVSPerScene, "CB_VS_PER_SCENE" );

    Desc.ByteWidth = sizeof( CB_VS_PER_OBJECT );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, nullptr, &g_pcbVSPerObject ) );
    DXUT_SetDebugName( g_pcbVSPerObject, "CB_VS_PER_OBJECT" );

    Desc.ByteWidth = sizeof( CB_PS_PER_SCENE );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, nullptr, &g_pcbPSPerScene ) );
    DXUT_SetDebugName( g_pcbPSPerScene, "CB_PS_PER_SCENE" );

    Desc.ByteWidth = sizeof( CB_PS_PER_OBJECT );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, nullptr, &g_pcbPSPerObject ) );
    DXUT_SetDebugName( g_pcbPSPerObject, "CB_PS_PER_OBJECT" );

    Desc.ByteWidth = sizeof( CB_PS_PER_LIGHT );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, nullptr, &g_pcbPSPerLight ) );
    DXUT_SetDebugName( g_pcbPSPerLight, "CB_PS_PER_LIGHT" );

    // Setup the camera's view parameters
    g_Camera.SetViewParams( s_vDefaultEye, s_vDefaultLookAt );
    g_Camera.SetRadius( s_fDefaultCameraRadius, s_fMinCameraRadius, s_fMaxCameraRadius );

    // Setup backface culling states:
    //  1) g_pRasterizerStateNoCull --- no culling (debugging only)
    //  2) g_pRasterizerStateBackfaceCull --- backface cull (mirror quads and the assets 
    //      reflected in the mirrors)
    //  3) g_pRasterizerStateFrontfaceCull --- frontface cull (pre-built assets from 
    //      the content pipeline)
    D3D11_RASTERIZER_DESC RasterizerDescNoCull = {
        D3D11_FILL_SOLID,   // D3D11_FILL_MODE FillMode;
        D3D11_CULL_NONE,    // D3D11_CULL_MODE CullMode;
        TRUE,               // BOOL FrontCounterClockwise;
        0,                  // INT DepthBias;
        0,                  // FLOAT DepthBiasClamp;
        0,                  // FLOAT SlopeScaledDepthBias;
        FALSE,              // BOOL DepthClipEnable;
        FALSE,              // BOOL ScissorEnable;
        TRUE,               // BOOL MultisampleEnable;
        FALSE,              // BOOL AntialiasedLineEnable;
    };
    V_RETURN( pd3dDevice->CreateRasterizerState( &RasterizerDescNoCull, &g_pRasterizerStateNoCull ) );
    DXUT_SetDebugName( g_pRasterizerStateNoCull, "NoCull" );

    RasterizerDescNoCull.FillMode = D3D11_FILL_WIREFRAME;
    V_RETURN( pd3dDevice->CreateRasterizerState( &RasterizerDescNoCull, &g_pRasterizerStateNoCullWireFrame ) );
    DXUT_SetDebugName( g_pRasterizerStateNoCullWireFrame, "Wireframe" );

    D3D11_RASTERIZER_DESC RasterizerDescBackfaceCull = {
        D3D11_FILL_SOLID,   // D3D11_FILL_MODE FillMode;
        D3D11_CULL_BACK,    // D3D11_CULL_MODE CullMode;
        TRUE,               // BOOL FrontCounterClockwise;
        0,                  // INT DepthBias;
        0,                  // FLOAT DepthBiasClamp;
        0,                  // FLOAT SlopeScaledDepthBias;
        FALSE,              // BOOL DepthClipEnable;
        FALSE,              // BOOL ScissorEnable;
        TRUE,               // BOOL MultisampleEnable;
        FALSE,              // BOOL AntialiasedLineEnable;
    };
    V_RETURN( pd3dDevice->CreateRasterizerState( &RasterizerDescBackfaceCull, &g_pRasterizerStateBackfaceCull ) );
    DXUT_SetDebugName( g_pRasterizerStateBackfaceCull, "BackfaceCull" );

    D3D11_RASTERIZER_DESC RasterizerDescFrontfaceCull = {
        D3D11_FILL_SOLID,   // D3D11_FILL_MODE FillMode;
        D3D11_CULL_FRONT,   // D3D11_CULL_MODE CullMode;
        TRUE,               // BOOL FrontCounterClockwise;
        0,                  // INT DepthBias;
        0,                  // FLOAT DepthBiasClamp;
        0,                  // FLOAT SlopeScaledDepthBias;
        FALSE,              // BOOL DepthClipEnable;
        FALSE,              // BOOL ScissorEnable;
        TRUE,               // BOOL MultisampleEnable;
        FALSE,              // BOOL AntialiasedLineEnable;
    };
    V_RETURN( pd3dDevice->CreateRasterizerState( &RasterizerDescFrontfaceCull, &g_pRasterizerStateFrontfaceCull ) );
    DXUT_SetDebugName( g_pRasterizerStateFrontfaceCull, "FrontfaceCull" );

    // The parameters to pass to per-chunk threads for the main scene
    g_StaticParamsDirect.m_pDepthStencilState = g_pDepthStencilStateNoStencil;
    g_StaticParamsDirect.m_iStencilRef = 0;
    g_StaticParamsDirect.m_pRasterizerState = g_pRasterizerStateFrontfaceCull;
    XMStoreFloat4( &g_StaticParamsDirect.m_vMirrorPlane, g_XMZero );
    XMStoreFloat4( &g_StaticParamsDirect.m_vTintColor, Colors::White );
    g_StaticParamsDirect.m_pDepthStencilView = nullptr;
    g_StaticParamsDirect.m_pViewport = nullptr;

#ifdef DEBUG
    // These checks are important for avoiding implicit assumptions of D3D state carry-over 
    // across device contexts.  A very common source of error in multithreaded rendering  
    // is setting some state in one context and inadvertently relying on that state in 
    // another context.  Setting all these flags to true should expose all such errors
    // (at non-trivial performance cost).
    // 
    // The names mean a bit more than they say.  The flags force that state be cleared when:
    //
    //  1) We actually perform the action in question (e.g. call FinishCommandList)
    //  2) We reach any point in the frame when the action could have been
    // performed (e.g. we are using DEVICECONTEXT_IMMEDIATE but would otherwise 
    // have called FinishCommandList)
    //
    // This usage guarantees consistent behavior across the different pathways.
    //
    g_bClearStateUponBeginCommandList = true;
    g_bClearStateUponFinishCommandList = true;
    g_bClearStateUponExecuteCommandList = true;
#endif

    InitializeLights();

    V_RETURN( InitializeShadows( pd3dDevice ) );

    V_RETURN( InitializeMirrors( pd3dDevice ) );

    V_RETURN( InitializeWorkerThreads( pd3dDevice ) );

    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( s_fFOV, fAspectRatio, s_fNearPlane, s_fFarPlane );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( MOUSE_MIDDLE_BUTTON, MOUSE_WHEEL, MOUSE_LEFT_BUTTON );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 170, 300 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Figure out the ViewProj matrix from the light's perspective
//--------------------------------------------------------------------------------------
XMMATRIX CalcLightViewProj( int iLight )
{
    XMVECTOR vLightDir = g_vLightDir[iLight];
    XMVECTOR vLightPos = g_vLightPos[iLight];

    XMVECTOR vLookAt = vLightPos + s_fSceneRadius * vLightDir;

    XMMATRIX mLightView = XMMatrixLookAtLH( vLightPos, vLookAt, g_XMIdentityR1 );

    XMMATRIX mLightProj = XMMatrixPerspectiveFovLH( g_fLightFOV[iLight], g_fLightAspect[iLight], g_fLightNearPlane[iLight], g_fLightFarPlane[iLight] );

    return mLightView * mLightProj;
}

//--------------------------------------------------------------------------------------
// The RenderMesh version which always calls the regular DXUT pathway.
// Here we set up the per-object constant buffers.
//--------------------------------------------------------------------------------------
void RenderMeshDirect( ID3D11DeviceContext* pd3dContext, 
                      UINT iMesh )
{
    HRESULT hr = S_OK;
    D3D11_MAPPED_SUBRESOURCE MappedResource;

    // Set the VS per-object constant data
    // This should eventually differ per object
    XMMATRIX id  = XMMatrixIdentity();    // should actually vary per-object

    V( pd3dContext->Map( g_pcbVSPerObject, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    auto pVSPerObject = reinterpret_cast<CB_VS_PER_OBJECT*>( MappedResource.pData );
    XMStoreFloat4x4( &pVSPerObject->m_mWorld, id );
    pd3dContext->Unmap( g_pcbVSPerObject, 0 );

    pd3dContext->VSSetConstantBuffers( g_iCBVSPerObjectBind, 1, &g_pcbVSPerObject );

    // Set the PS per-object constant data
    // This should eventually differ per object
    V( pd3dContext->Map( g_pcbPSPerObject, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    auto pPSPerObject = reinterpret_cast<CB_PS_PER_OBJECT*>( MappedResource.pData );
    XMStoreFloat4( &pPSPerObject->m_vObjectColor, Colors::White );
    pd3dContext->Unmap( g_pcbPSPerObject, 0 );

    pd3dContext->PSSetConstantBuffers( g_iCBPSPerObjectBind, 1, &g_pcbPSPerObject );

    g_Mesh11.RenderMesh( iMesh,
        false,
        pd3dContext,
        0,
        1,
        INVALID_SAMPLER_SLOT );
}


//--------------------------------------------------------------------------------------
// The RenderMesh version which may redirect to another device context and/or thread.  
// This function gets called from the main thread or a per-scene thread, but not from 
// a per-chunk worker thread.
//
// There are three cases to consider:
//
//  1) If we are not using per-chunk deferred contexts, the call gets routed straight 
// back to DXUT with the given device context.
//  2) If we are using singlethreaded per-chunk deferred contexts, the call gets added
// to the next deferred context, and the draw submission occurs inline here.
//  3) If we are using multithreaded per-chunk deferred contexts, the call gets recorded
// in the next per-chunk work queue, and the corresponding semaphore gets incremented.  
// The appropriate worker thread detects the semaphore signal, grabs the work queue
// entry, and submits the draw call from its deferred context.
// 
// We ignore most of the arguments to this function, because they are constant for this
// sample.
//--------------------------------------------------------------------------------------
void RenderMesh( CMultiDeviceContextDXUTMesh* pMesh, 
                UINT iMesh,
                bool bAdjacent,
                ID3D11DeviceContext* pd3dDeviceContext,
                UINT iDiffuseSlot,
                UINT iNormalSlot,
                UINT iSpecularSlot )
{
    static int iNextAvailableChunkQueue = 0;   // next per-chunk deferred context to assign to

    if ( IsRenderMultithreadedPerChunk() )
    {
        // Create and submit a worker queue entry
        ChunkQueue& WorkerQueue = g_ChunkQueue[iNextAvailableChunkQueue];
        int iQueueOffset = g_iPerChunkQueueOffset[iNextAvailableChunkQueue];
        HANDLE hSemaphore = g_hBeginPerChunkRenderDeferredSemaphore[iNextAvailableChunkQueue];

        g_iPerChunkQueueOffset[iNextAvailableChunkQueue] += sizeof(WorkQueueEntryChunk);
        assert( g_iPerChunkQueueOffset[iNextAvailableChunkQueue] < g_iSceneQueueSizeInBytes );

        auto pEntry = reinterpret_cast<WorkQueueEntryChunk*>( &WorkerQueue[iQueueOffset] );
        pEntry->m_iType = WORK_QUEUE_ENTRY_TYPE_CHUNK;
        pEntry->m_iMesh = iMesh;

        ReleaseSemaphore( hSemaphore, 1, nullptr );

    }
    else if ( IsRenderDeferredPerChunk() )
    {
        // Replace the incoming device context by a deferred context
        ID3D11DeviceContext* pd3dDeferredContext = g_pd3dPerChunkDeferredContext[iNextAvailableChunkQueue];
        RenderMeshDirect( pd3dDeferredContext, iMesh );
    }
    else
    {
        // Draw as normal
        RenderMeshDirect( pd3dDeviceContext, iMesh );
    }

    iNextAvailableChunkQueue = ++iNextAvailableChunkQueue % g_iNumPerChunkRenderThreads;
}


//--------------------------------------------------------------------------------------
// Perform per-scene d3d context set-up.  This should be enough setup that you can
// start with a completely new device context, and then successfully call RenderMesh
// afterwards.
//--------------------------------------------------------------------------------------
HRESULT RenderSceneSetup( ID3D11DeviceContext* pd3dContext, const SceneParamsStatic* pStaticParams, 
                         const SceneParamsDynamic* pDynamicParams )
{
    HRESULT hr;
    D3D11_MAPPED_SUBRESOURCE MappedResource;

    bool bShadow = ( pStaticParams->m_pDepthStencilView != nullptr );

    // Use all shadow maps as textures, or else one shadow map as depth-stencil
    if ( bShadow )
    {
        // No shadow maps as textures
        ID3D11ShaderResourceView* ppNullResources[g_iNumShadows] = { nullptr };
        pd3dContext->PSSetShaderResources( 2, g_iNumShadows, ppNullResources );

        // Given shadow map as depth-stencil, no render target
        pd3dContext->RSSetViewports( 1, pStaticParams->m_pViewport );
        pd3dContext->OMSetRenderTargets( 0, nullptr, pStaticParams->m_pDepthStencilView );
    }
    else
    {
        // Standard DXUT render target and depth-stencil
        V( DXUTSetupD3D11Views( pd3dContext ) );

        // All shadow maps as textures
        pd3dContext->PSSetShaderResources( 2, g_iNumShadows, g_pShadowResourceView );
    }

    // Set the depth-stencil state
    pd3dContext->OMSetDepthStencilState( pStaticParams->m_pDepthStencilState, pStaticParams->m_iStencilRef );

    // Set the rasterizer state
    pd3dContext->RSSetState( g_bWireFrame ? g_pRasterizerStateNoCullWireFrame: pStaticParams->m_pRasterizerState );

    // Set the shaders
    pd3dContext->VSSetShader( g_pVertexShader, nullptr, 0 );

    // Set the vertex buffer format
    pd3dContext->IASetInputLayout( g_pVertexLayout11 );
    
    // Set the VS per-scene constant data
    V( pd3dContext->Map( g_pcbVSPerScene, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    auto pVSPerScene = reinterpret_cast<CB_VS_PER_SCENE*>( MappedResource.pData );
    XMMATRIX mvp = XMLoadFloat4x4( &pDynamicParams->m_mViewProj );
    XMStoreFloat4x4( &pVSPerScene->m_mViewProj, XMMatrixTranspose( mvp ) );
    pd3dContext->Unmap( g_pcbVSPerScene, 0 );

    pd3dContext->VSSetConstantBuffers( g_iCBVSPerSceneBind, 1, &g_pcbVSPerScene );

    if ( bShadow )
    {
        pd3dContext->PSSetShader( nullptr, nullptr, 0 );
    }
    else
    {
        pd3dContext->PSSetShader( g_pPixelShader, nullptr, 0 );

        ID3D11SamplerState* ppSamplerStates[2] = { g_pSamPointClamp, g_pSamLinearWrap };
        pd3dContext->PSSetSamplers( 0, 2, ppSamplerStates );

        // Set the PS per-scene constant data
        // A user clip plane prevents drawing things into the mirror which are behind the mirror plane
        V( pd3dContext->Map( g_pcbPSPerScene, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
        auto pPSPerScene = reinterpret_cast<CB_PS_PER_SCENE*>( MappedResource.pData );
        pPSPerScene->m_vMirrorPlane = pStaticParams->m_vMirrorPlane;
        XMStoreFloat4( &pPSPerScene->m_vAmbientColor, s_vAmbientColor );
        pPSPerScene->m_vTintColor = pStaticParams->m_vTintColor;
        pd3dContext->Unmap( g_pcbPSPerScene, 0 );

        pd3dContext->PSSetConstantBuffers( g_iCBPSPerSceneBind, 1, &g_pcbPSPerScene );

        // Set the PS per-light constant data
        V( pd3dContext->Map( g_pcbPSPerLight, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
        auto pPSPerLight = reinterpret_cast<CB_PS_PER_LIGHT*>( MappedResource.pData );
        for ( int iLight = 0; iLight < g_iNumLights; ++iLight )
        {
            XMVECTOR vLightPos = XMVectorSetW( g_vLightPos[iLight], 1.0f ); 
            XMVECTOR vLightDir = XMVectorSetW( g_vLightDir[iLight], 0.0f ); 

            XMMATRIX mLightViewProj = CalcLightViewProj( iLight );
            
            pPSPerLight->m_LightData[iLight].m_vLightColor = g_vLightColor[iLight];
            XMStoreFloat4( &pPSPerLight->m_LightData[iLight].m_vLightPos, vLightPos );
            XMStoreFloat4( &pPSPerLight->m_LightData[iLight].m_vLightDir, vLightDir );
            XMStoreFloat4x4( &pPSPerLight->m_LightData[iLight].m_mLightViewProj, XMMatrixTranspose( mLightViewProj ) );
            pPSPerLight->m_LightData[iLight].m_vFalloffs = XMFLOAT4(
                g_fLightFalloffDistEnd[iLight], 
                g_fLightFalloffDistRange[iLight], 
                g_fLightFalloffCosAngleEnd[iLight], 
                g_fLightFalloffCosAngleRange[iLight]);
        }
        pd3dContext->Unmap( g_pcbPSPerLight, 0 );

        pd3dContext->PSSetConstantBuffers( g_iCBPSPerLightBind, 1, &g_pcbPSPerLight );
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Render the scene from either:
//      - The immediate context in main thread, or 
//      - A deferred context in the main thread, or
//      - A deferred context in a worker thread
//      - Several deferred contexts in the main thread, handling objects alternately 
//      - Several deferred contexts in worker threads, handling objects alternately
// The scene can be either the main scene, a mirror scene, or a shadow map scene
//--------------------------------------------------------------------------------------
HRESULT RenderScene( ID3D11DeviceContext* pd3dContext, const SceneParamsStatic *pStaticParams, 
                    const SceneParamsDynamic *pDynamicParams )
{
    HRESULT hr = S_OK;

    // Make sure we're not relying on any state being inherited
    if ( g_bClearStateUponBeginCommandList )
    {
        pd3dContext->ClearState();
    }

    // Clear the shadow buffer
    if ( pStaticParams->m_pDepthStencilView )
    {
        pd3dContext->ClearDepthStencilView( pStaticParams->m_pDepthStencilView, 
            D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0, 0 );
    }

    // Perform scene setup on every d3d context we will use
    if ( IsRenderMultithreadedPerChunk() )
    {
        for ( int iInstance = 0; iInstance < g_iNumPerChunkRenderThreads; ++iInstance )
        {
            // Reset count 
            g_iPerChunkQueueOffset[iInstance] = 0;

            // Create and submit a worker queue entry
            ChunkQueue& WorkerQueue = g_ChunkQueue[iInstance];
            int iQueueOffset = g_iPerChunkQueueOffset[iInstance];
            HANDLE hSemaphore = g_hBeginPerChunkRenderDeferredSemaphore[iInstance];

            g_iPerChunkQueueOffset[iInstance] += sizeof(WorkQueueEntrySetup);
            assert( g_iPerChunkQueueOffset[iInstance] < g_iSceneQueueSizeInBytes );

            auto pEntry = reinterpret_cast<WorkQueueEntrySetup*>( &WorkerQueue[iQueueOffset] );
            pEntry->m_iType = WORK_QUEUE_ENTRY_TYPE_SETUP;
            pEntry->m_pSceneParamsStatic = pStaticParams;    // shallow copy, mmm
            pEntry->m_SceneParamsDynamic = *pDynamicParams; // deep copy, gulp

            ReleaseSemaphore( hSemaphore, 1, nullptr );
        }
    }
    else if ( IsRenderDeferredPerChunk() )
    {
        for ( int iInstance = 0; iInstance < g_iNumPerChunkRenderThreads; ++iInstance )
        {
            ID3D11DeviceContext* pd3dDeferredContext = g_pd3dPerChunkDeferredContext[iInstance];
            V( RenderSceneSetup( pd3dDeferredContext, pStaticParams, pDynamicParams ) );
        }
    }
    else
    {
        V( RenderSceneSetup( pd3dContext, pStaticParams, pDynamicParams ) );
    }

    //Render
    g_Mesh11.Render( pd3dContext, 0, 1 );

    // If we are doing ST_DEFERRED_PER_CHUNK or MT_DEFERRED_PER_CHUNK, generate and execute command lists now.
    if ( IsRenderDeferredPerChunk() )
    {
        if ( IsRenderMultithreadedPerChunk() )
        {
            // Signal all worker threads to finalize their command lists
            for ( int iInstance = 0; iInstance < g_iNumPerChunkRenderThreads; ++iInstance )
            {
                // Create and submit a worker queue entry
                ChunkQueue& WorkerQueue = g_ChunkQueue[iInstance];
                int iQueueOffset = g_iPerChunkQueueOffset[iInstance];
                HANDLE hSemaphore = g_hBeginPerChunkRenderDeferredSemaphore[iInstance];

                g_iPerChunkQueueOffset[iInstance] += sizeof(WorkQueueEntryFinalize);
                assert( g_iPerChunkQueueOffset[iInstance] < g_iSceneQueueSizeInBytes );

                auto pEntry = reinterpret_cast<WorkQueueEntryFinalize*>( &WorkerQueue[iQueueOffset] );
                pEntry->m_iType = WORK_QUEUE_ENTRY_TYPE_FINALIZE;

                ReleaseSemaphore( hSemaphore, 1, nullptr );
            }

            // Wait until all worker threads signal that their command lists are finalized
            WaitForMultipleObjects( g_iNumPerChunkRenderThreads, 
                g_hEndPerChunkRenderDeferredEvent, TRUE, INFINITE );
        }
        else
        {
            // Directly finalize all command lists
            for ( int iInstance = 0; iInstance < g_iNumPerChunkRenderThreads; ++iInstance )
            {
                V( g_pd3dPerChunkDeferredContext[iInstance]->FinishCommandList( 
                    !g_bClearStateUponFinishCommandList, &g_pd3dPerChunkCommandList[iInstance] ) );
            }
        }

        // Execute all command lists.  Note these now produce a scattered render order.
        for ( int iInstance = 0; iInstance < g_iNumPerChunkRenderThreads; ++iInstance )
        {
            pd3dContext->ExecuteCommandList( g_pd3dPerChunkCommandList[iInstance], 
                !g_bClearStateUponExecuteCommandList );
            SAFE_RELEASE( g_pd3dPerChunkCommandList[iInstance] );
        }
    }
    else
    {
        // If we rendered directly, optionally clear state for consistent behavior with
        // the other render pathways.
        if ( g_bClearStateUponFinishCommandList || g_bClearStateUponExecuteCommandList )
        {
            pd3dContext->ClearState();
        }
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Render the shadow map
//--------------------------------------------------------------------------------------
VOID RenderShadow( int iShadow, ID3D11DeviceContext* pd3dContext )
{
    HRESULT hr;

    XMMATRIX m = CalcLightViewProj( iShadow );

    SceneParamsDynamic DynamicParams;   
    XMStoreFloat4x4( &DynamicParams.m_mViewProj, m );

    V( RenderScene( pd3dContext, &g_StaticParamsShadow[iShadow], &DynamicParams ) );
}


//--------------------------------------------------------------------------------------
// Render the mirror quad into the stencil buffer, and then render the world into
// the stenciled area, using the mirrored projection matrix
//--------------------------------------------------------------------------------------
VOID RenderMirror( int iMirror, ID3D11DeviceContext* pd3dContext )
{
    HRESULT hr;
    D3D11_MAPPED_SUBRESOURCE MappedResource;

    XMVECTOR vEyePoint;
    XMMATRIX mViewProj;

#ifdef RENDER_SCENE_LIGHT_POV
    if ( g_bRenderSceneLightPOV ) 
    {
        vEyePoint = g_vLightPos[0];
        mViewProj = CalcLightViewProj( 0 );
    }
    else
#endif
    {
        // Find the right view matrix for the mirror.  
        vEyePoint = g_Camera.GetEyePt();
        mViewProj = g_Camera.GetViewMatrix() * g_Camera.GetProjMatrix();
    }

    // Test for back-facing mirror (from whichever pov we are using)
    if ( XMVectorGetX( XMPlaneDotCoord( g_vMirrorPlane[iMirror], vEyePoint ) ) < 0.0f )
    {
        return;
    }

    XMMATRIX mReflect = XMMatrixReflect( g_vMirrorPlane[iMirror] );

    // Set up the mirror local-to-world matrix (could be done at initialize time)
    XMVECTOR vMirrorPointAt = XMVectorAdd( g_vMirrorNormal[iMirror], g_vMirrorCenter[iMirror] );
    XMMATRIX mMirrorWorld = XMMatrixLookAtLH( vMirrorPointAt, g_vMirrorCenter[iMirror], g_XMIdentityR1 );
    mMirrorWorld = XMMatrixTranspose( mMirrorWorld );
    mMirrorWorld.r[0] = XMVectorSetW( mMirrorWorld.r[0], 0.f );
    mMirrorWorld.r[1] = XMVectorSetW( mMirrorWorld.r[1], 0.f );
    mMirrorWorld.r[2] = XMVectorSetW( mMirrorWorld.r[2], 0.f );
    mMirrorWorld.r[3] = XMVectorSetW( g_vMirrorCenter[iMirror], 1.f );

    if ( g_bClearStateUponBeginCommandList )
    {
        pd3dContext->ClearState();
    }

    // Restore the main view 
    DXUTSetupD3D11Views( pd3dContext );

    //--------------------------------------------------------------------------------------
    // Draw the mirror quad into the stencil buffer, setting the stencil ref value
    //--------------------------------------------------------------------------------------

    // Set the depth-stencil state
    pd3dContext->OMSetDepthStencilState( g_pMirrorDepthStencilStateDepthTestStencilOverwrite, 
        g_iStencilRef );

    // Set the cull state
    pd3dContext->RSSetState( g_pRasterizerStateBackfaceCull );

    // Set inputs for the mirror shader
    pd3dContext->IASetInputLayout( g_pMirrorVertexLayout11 );
    pd3dContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );
    ID3D11Buffer* pVB[1] = { g_pMirrorVertexBuffer };
    UINT pStride[1] = { sizeof(MirrorVertex) };
    UINT pOffset[1] = { 0 };
    pd3dContext->IASetVertexBuffers( 0, 1, pVB, pStride, pOffset );

    pd3dContext->VSSetShader( g_pVertexShader, nullptr, 0 );
    pd3dContext->PSSetShader( nullptr, nullptr, 0 );

    // Set the corners of the mirror vertex buffer.  The UVs aren't used here
    V( pd3dContext->Map( g_pMirrorVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    memcpy( MappedResource.pData, g_MirrorRect[iMirror], sizeof(g_MirrorRect[iMirror]) );
    pd3dContext->Unmap( g_pMirrorVertexBuffer, 0 );

    // Set up the transform matrices in the constant buffer
    V( pd3dContext->Map( g_pcbVSPerObject, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    auto pVSPerObject = reinterpret_cast<CB_VS_PER_OBJECT*>(MappedResource.pData );
    XMStoreFloat4x4( &pVSPerObject->m_mWorld, XMMatrixTranspose( mMirrorWorld ) );
    pd3dContext->Unmap( g_pcbVSPerObject, 0 );

    pd3dContext->VSSetConstantBuffers( g_iCBVSPerObjectBind, 1, &g_pcbVSPerObject );

    V( pd3dContext->Map( g_pcbVSPerScene, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    auto pVSPerScene = reinterpret_cast<CB_VS_PER_SCENE*>( MappedResource.pData );
    XMStoreFloat4x4( &pVSPerScene->m_mViewProj, XMMatrixTranspose( mViewProj ) );
    pd3dContext->Unmap( g_pcbVSPerScene, 0 );

    pd3dContext->VSSetConstantBuffers( g_iCBVSPerSceneBind, 1, &g_pcbVSPerScene );

    pd3dContext->Draw( 4, 0 );

    //--------------------------------------------------------------------------------------
    // Clear depth, only within the stencilled area
    //--------------------------------------------------------------------------------------

    // Set the depth-stencil state
    pd3dContext->OMSetDepthStencilState( g_pMirrorDepthStencilStateDepthOverwriteStencilTest, 
        g_iStencilRef );

    // Set up the transform matrices to alway output depth equal to the far plane (z = w of output)
    XMFLOAT4X4 mvp4x4;
    XMStoreFloat4x4( &mvp4x4, mViewProj );

    V( pd3dContext->Map( g_pcbVSPerScene, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    pVSPerScene = reinterpret_cast<CB_VS_PER_SCENE*>( MappedResource.pData );
    XMStoreFloat4x4( &pVSPerScene->m_mViewProj, XMMatrixTranspose( mViewProj ) );
    pVSPerScene->m_mViewProj._31 = mvp4x4._14;
    pVSPerScene->m_mViewProj._32 = mvp4x4._24;
    pVSPerScene->m_mViewProj._33 = mvp4x4._34;
    pVSPerScene->m_mViewProj._34 = mvp4x4._44;
    pd3dContext->Unmap( g_pcbVSPerScene, 0 );

    pd3dContext->Draw( 4, 0 );

    //--------------------------------------------------------------------------------------
    // Draw the mirrored world into the stencilled area
    //--------------------------------------------------------------------------------------

    XMMATRIX mvp = mReflect * mViewProj;
    SceneParamsDynamic DynamicParams;
    XMStoreFloat4x4( &DynamicParams.m_mViewProj, mvp );

    V( RenderScene( pd3dContext, &g_StaticParamsMirror[iMirror], &DynamicParams ) );

    //--------------------------------------------------------------------------------------
    // Clear the stencil bit to 0 over the mirror quad.
    // At the same time, set the depth buffer to the depth value of the mirror.
    //--------------------------------------------------------------------------------------

    // Assume this context is completely from scratch, since we've just come back from 
    // scene rendering
    V( DXUTSetupD3D11Views( pd3dContext ) );

    // Set the depth-stencil state
    pd3dContext->OMSetDepthStencilState( g_pMirrorDepthStencilStateDepthOverwriteStencilClear, 
        g_iStencilRef );

    // Set the cull state
    pd3dContext->RSSetState( g_pRasterizerStateBackfaceCull );

    // Set inputs for the mirror shader
    pd3dContext->IASetInputLayout( g_pMirrorVertexLayout11 );
    pd3dContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );
    pd3dContext->IASetVertexBuffers( 0, 1, pVB, pStride, pOffset );

    pd3dContext->VSSetShader( g_pVertexShader, nullptr, 0 );
    pd3dContext->PSSetShader( nullptr, nullptr, 0 );

    // Set up the transform matrices
    V( pd3dContext->Map( g_pcbVSPerObject, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    pVSPerObject = reinterpret_cast<CB_VS_PER_OBJECT*>( MappedResource.pData );
    XMStoreFloat4x4( &pVSPerObject->m_mWorld, XMMatrixTranspose( mMirrorWorld ) );
    pd3dContext->Unmap( g_pcbVSPerObject, 0 );

    pd3dContext->VSSetConstantBuffers( g_iCBVSPerObjectBind, 1, &g_pcbVSPerObject );

    V( pd3dContext->Map( g_pcbVSPerScene, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    pVSPerScene = reinterpret_cast<CB_VS_PER_SCENE*>(MappedResource.pData );
    XMStoreFloat4x4( &pVSPerScene->m_mViewProj, XMMatrixTranspose( mViewProj ) );
    pd3dContext->Unmap( g_pcbVSPerScene, 0 );

    pd3dContext->VSSetConstantBuffers( g_iCBVSPerSceneBind, 1, &g_pcbVSPerScene );

    pd3dContext->Draw( 4, 0 );
}


//--------------------------------------------------------------------------------------
// Render the scene into the world (not into a mirror or a shadow map)
//--------------------------------------------------------------------------------------
VOID RenderSceneDirect( ID3D11DeviceContext* pd3dContext )
{
    HRESULT hr;

    XMMATRIX mvp;

#ifdef RENDER_SCENE_LIGHT_POV
    if ( g_bRenderSceneLightPOV )
    {
        mvp = CalcLightViewProj( 0 );
    }
    else
#endif
    {
        mvp = g_Camera.GetViewMatrix() * g_Camera.GetProjMatrix();
    }

    SceneParamsDynamic DynamicParams;
    XMStoreFloat4x4( &DynamicParams.m_mViewProj, mvp );

    V( RenderScene( pd3dContext, &g_StaticParamsDirect, &DynamicParams ) );
}


//--------------------------------------------------------------------------------------
// The per-scene worker thread entry point.  Loops infinitely, rendering either a 
// shadow scene or a mirror scene or the main scene into a command list.
//--------------------------------------------------------------------------------------
unsigned int WINAPI _PerSceneRenderDeferredProc( LPVOID lpParameter )
{
    HRESULT hr;

    // thread local data
    const int iInstance = *( int*)lpParameter;
    ID3D11DeviceContext* pd3dDeferredContext = g_pd3dPerSceneDeferredContext[iInstance];
    ID3D11CommandList*& pd3dCommandList = g_pd3dPerSceneCommandList[iInstance];

    for (;;)
    {
        // Wait for main thread to signal ready
        WaitForSingleObject( g_hBeginPerSceneRenderDeferredEvent[iInstance], INFINITE );

        if ( g_bClearStateUponBeginCommandList )
        {
            pd3dDeferredContext->ClearState();
        }

        if ( iInstance < g_iNumShadows )
        {
            RenderShadow( iInstance, pd3dDeferredContext );
        }
        else if ( iInstance < g_iNumShadows + g_iNumMirrors )
        {
            RenderMirror( iInstance - g_iNumShadows, pd3dDeferredContext );
        }
        else
        {
            RenderSceneDirect( pd3dDeferredContext );
        }

        V( pd3dDeferredContext->FinishCommandList( !g_bClearStateUponFinishCommandList, &pd3dCommandList ) );

        // Tell main thread command list is finished
        SetEvent( g_hEndPerSceneRenderDeferredEvent[iInstance] );
    }
}


//--------------------------------------------------------------------------------------
// The per-chunk worker thread entry point.  Loops infinitely, rendering an arbitrary
// set of objects, from an arbitrary type of scene, into a command list.
//--------------------------------------------------------------------------------------
unsigned int WINAPI _PerChunkRenderDeferredProc( LPVOID lpParameter )
{
    HRESULT hr;

    // thread local data
    const int iInstance = *( int*)lpParameter;
    ID3D11DeviceContext* pd3dDeferredContext = g_pd3dPerChunkDeferredContext[iInstance];
    ID3D11CommandList*& pd3dCommandList = g_pd3dPerChunkCommandList[iInstance];
    const ChunkQueue& LocalQueue = g_ChunkQueue[iInstance];

    // The next queue entry to be read.  Since we wait for the semaphore signal count to be greater
    // than zero, this index doesn't require explicit synchronization.
    int iQueueOffset = 0;

    for (;;)
    {
        // Wait for a work queue entry
        WaitForSingleObject( g_hBeginPerChunkRenderDeferredSemaphore[iInstance], INFINITE );

        assert( iQueueOffset < g_iSceneQueueSizeInBytes );
        auto pEntry = reinterpret_cast<const WorkQueueEntryBase*>( &LocalQueue[iQueueOffset] );

        switch ( pEntry->m_iType )
        {
        // Begin the scene by setting all required state
        case WORK_QUEUE_ENTRY_TYPE_SETUP:
            {
                auto pSetupEntry = reinterpret_cast<const WorkQueueEntrySetup*>( pEntry );

                if ( g_bClearStateUponBeginCommandList )
                {
                    pd3dDeferredContext->ClearState();
                }

                V( RenderSceneSetup( pd3dDeferredContext, pSetupEntry->m_pSceneParamsStatic, 
                    &pSetupEntry->m_SceneParamsDynamic ) );

                iQueueOffset += sizeof(WorkQueueEntrySetup);
                break;
            }

        // Submit a single chunk to the deferred context
        case WORK_QUEUE_ENTRY_TYPE_CHUNK:
            {
                auto pChunkEntry = reinterpret_cast<const WorkQueueEntryChunk*>( pEntry );

                // Submit work to deferred context
                RenderMeshDirect( pd3dDeferredContext, pChunkEntry->m_iMesh );

                iQueueOffset += sizeof(WorkQueueEntryChunk);
                break;
            }

        // Finalize scene rendering
        case WORK_QUEUE_ENTRY_TYPE_FINALIZE:
            {
                // Finalize preceding work
                V( pd3dDeferredContext->FinishCommandList( !g_bClearStateUponFinishCommandList, &pd3dCommandList ) );

                // Tell main thread command list is finished
                SetEvent( g_hEndPerChunkRenderDeferredEvent[iInstance] );

                iQueueOffset += sizeof(WorkQueueEntryFinalize); // unnecessary currently as this is the last item

                // Reset queue
                iQueueOffset = 0;
                break;
            }

        // Error --- unrecognized entry type
        default:
            assert( false );
            break;
        }
    }
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                  float fElapsedTime, void* pUserContext )
{
    HRESULT hr;

#ifdef ADJUSTABLE_LIGHT
    g_vLightDir[0] = g_LightControl.GetLightDirection();
    g_vLightPos[0] = s_vSceneCenter - s_fSceneRadius * g_vLightDir[0];
#endif

    if ( g_bClearStateUponBeginCommandList )
    {
        pd3dImmediateContext->ClearState();
        V( DXUTSetupD3D11Views( pd3dImmediateContext ) );
    }

    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    // Clear the render target
    pd3dImmediateContext->ClearRenderTargetView( DXUTGetD3D11RenderTargetView(), Colors::MidnightBlue );
    pd3dImmediateContext->ClearDepthStencilView( DXUTGetD3D11DepthStencilView(), 
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0, 0 );

    // Three possible render pathways:
    if ( IsRenderMultithreadedPerScene() )
    {
        // Signal all worker threads, then wait for completion
        for ( int iInstance = 0; iInstance < g_iNumPerSceneRenderThreads; ++iInstance )
        {
            // signal ready for scene kickoff
            SetEvent( g_hBeginPerSceneRenderDeferredEvent[iInstance] );
        }

        // wait for completion
        WaitForMultipleObjects( g_iNumPerSceneRenderThreads, 
            g_hEndPerSceneRenderDeferredEvent, 
            TRUE, 
            INFINITE );
    }
    else if ( IsRenderDeferredPerScene() )
    {
        // Perform the same tasks, serialized on the main thread but using deferred contexts
        for ( int iShadow = 0; iShadow < g_iNumShadows; ++iShadow )
        {
            RenderShadow( iShadow, g_pd3dPerSceneDeferredContext[iShadow] );
            V( g_pd3dPerSceneDeferredContext[iShadow]->FinishCommandList( 
                !g_bClearStateUponFinishCommandList, 
                &g_pd3dPerSceneCommandList[iShadow] ) );
        }

        for ( int iMirror = 0; iMirror < g_iNumMirrors; ++iMirror )
        {
            RenderMirror( iMirror, g_pd3dPerSceneDeferredContext[iMirror] );
            V( g_pd3dPerSceneDeferredContext[iMirror]->FinishCommandList( 
                !g_bClearStateUponFinishCommandList, 
                &g_pd3dPerSceneCommandList[g_iNumShadows + iMirror] ) );
        }

        RenderSceneDirect( g_pd3dPerSceneDeferredContext[g_iNumMirrors] );
        V( g_pd3dPerSceneDeferredContext[g_iNumMirrors]->FinishCommandList( 
            !g_bClearStateUponFinishCommandList, 
            &g_pd3dPerSceneCommandList[g_iNumShadows + g_iNumMirrors] ) );
    }
    else
    {
        // Perform the same tasks, serialized on the main thread using the immediate context
        for ( int iShadow = 0; iShadow < g_iNumShadows; ++iShadow )
        {
            RenderShadow( iShadow, pd3dImmediateContext );
        }

        for ( int iMirror = 0; iMirror < g_iNumMirrors; ++iMirror )
        {
            RenderMirror( iMirror, pd3dImmediateContext );
        }

        RenderSceneDirect( pd3dImmediateContext );
    }

    // If we are doing ST_DEFERRED_PER_SCENE or MT_DEFERRED_PER_SCENE, we have generated a 
    // bunch of command lists.  Execute those lists now.
    if ( IsRenderDeferredPerScene() )
    {
        for ( int iInstance = 0; iInstance < g_iNumPerSceneRenderThreads; ++iInstance )
        {
            pd3dImmediateContext->ExecuteCommandList( g_pd3dPerSceneCommandList[iInstance], 
                !g_bClearStateUponExecuteCommandList );
            SAFE_RELEASE( g_pd3dPerSceneCommandList[iInstance] );
        }
    }
    else
    {
        // If we rendered directly, optionally clear state for consistent behavior with
        // the other render pathways.
        if ( g_bClearStateUponFinishCommandList || g_bClearStateUponExecuteCommandList )
        {
            pd3dImmediateContext->ClearState();
        }
    }

    // Assume this context is completely from scratch for purposes of subsequent HUD rendering
    V( DXUTSetupD3D11Views( pd3dImmediateContext ) );

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
    for ( int iInstance = 0; iInstance < g_iNumPerSceneRenderThreads; ++iInstance )
    {
        CloseHandle( g_hPerSceneRenderDeferredThread[iInstance] );
        CloseHandle( g_hEndPerSceneRenderDeferredEvent[iInstance] );
        CloseHandle( g_hBeginPerSceneRenderDeferredEvent[iInstance] );
        SAFE_RELEASE( g_pd3dPerSceneDeferredContext[iInstance] );
    }

    for ( int iInstance = 0; iInstance < g_iNumPerChunkRenderThreads; ++iInstance )
    {
        CloseHandle( g_hPerChunkRenderDeferredThread[iInstance] );
        CloseHandle( g_hEndPerChunkRenderDeferredEvent[iInstance] );
        CloseHandle( g_hBeginPerChunkRenderDeferredSemaphore[iInstance] );
        SAFE_RELEASE( g_pd3dPerChunkDeferredContext[iInstance] );
    }

    g_DialogResourceManager.OnD3D11DestroyDevice();
    g_D3DSettingsDlg.OnD3D11DestroyDevice();
    CDXUTDirectionWidget::StaticOnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE( g_pTxtHelper );

    g_Mesh11.Destroy();
                
    SAFE_RELEASE( g_pVertexLayout11 );
    SAFE_RELEASE( g_pVertexShader );
    SAFE_RELEASE( g_pPixelShader );
    SAFE_RELEASE( g_pSamPointClamp );
    SAFE_RELEASE( g_pSamLinearWrap );
    SAFE_RELEASE( g_pRasterizerStateNoCull );
    SAFE_RELEASE( g_pRasterizerStateBackfaceCull );
    SAFE_RELEASE( g_pRasterizerStateFrontfaceCull );
    SAFE_RELEASE( g_pRasterizerStateNoCullWireFrame );

    for ( int iShadow = 0; iShadow < g_iNumShadows; ++iShadow )
    {
        SAFE_RELEASE( g_pShadowTexture[iShadow] );
        SAFE_RELEASE( g_pShadowResourceView[iShadow] );
        SAFE_RELEASE( g_pShadowDepthStencilView[iShadow] );
    }

    SAFE_RELEASE( g_pMirrorVertexLayout11 );
    SAFE_RELEASE( g_pMirrorVertexBuffer );
    SAFE_RELEASE( g_pDepthStencilStateNoStencil );
    SAFE_RELEASE( g_pMirrorDepthStencilStateDepthTestStencilOverwrite );
    SAFE_RELEASE( g_pMirrorDepthStencilStateDepthOverwriteStencilTest );
    SAFE_RELEASE( g_pMirrorDepthStencilStateDepthWriteStencilTest );
    SAFE_RELEASE( g_pMirrorDepthStencilStateDepthOverwriteStencilClear );

    SAFE_RELEASE( g_pcbVSPerScene );
    SAFE_RELEASE( g_pcbVSPerObject );
    SAFE_RELEASE( g_pcbPSPerScene );
    SAFE_RELEASE( g_pcbPSPerObject );
    SAFE_RELEASE( g_pcbPSPerLight );
}
