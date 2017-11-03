//--------------------------------------------------------------------------------------
// File: XAudio2Sound3D.cpp
//
// 3D positional audio and environmental reverb using XAudio2
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTgui.h"
#include "DXUTmisc.h"
#include "DXUTSettingsDlg.h"
#include "SDKmisc.h"
#include "audio.h"

#include <algorithm>

#pragma warning( disable : 4100 )

using namespace DirectX;

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CDXUTDialogResourceManager      g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg                 g_SettingsDlg;           // Device settings dialog
CDXUTTextHelper*                g_pTxtHelper = nullptr;
CDXUTDialog                     g_HUD;                   // dialog for standard controls
CDXUTDialog                     g_SampleUI;              // dialog for sample specific controls

// Direct3D 11 resources
ID3D11VertexShader*             g_pVertexShader11 = nullptr;
ID3D11PixelShader*              g_pPixelShader11 = nullptr;
ID3D11InputLayout*              g_pLayout11 = nullptr;
ID3D11DepthStencilState*        g_pDepthState = nullptr;
ID3D11Buffer*                   g_pvbFloor = nullptr;
ID3D11Buffer*                   g_pvbSource = nullptr;
ID3D11Buffer*                   g_pvbListener = nullptr;
ID3D11Buffer*                   g_pvbListenerCone = nullptr;
ID3D11Buffer*                   g_pvbInnerRadius = nullptr;
ID3D11Buffer*                   g_pvbGrid = nullptr;

const LPWSTR g_SOUND_NAMES[] =
{
    L"Heli.wav",
    L"MusicMono.wav",
};

enum CONTROL_MODE
{
    CONTROL_SOURCE=0,
    CONTROL_LISTENER
} g_eControlMode = CONTROL_SOURCE;

// Must match order of g_PRESET_PARAMS
const LPWSTR g_PRESET_NAMES[ NUM_PRESETS ] =
{
    L"Forest",
    L"Default",
    L"Generic",
    L"Padded cell",
    L"Room",
    L"Bathroom",
    L"Living room",
    L"Stone room",
    L"Auditorium",
    L"Concert hall",
    L"Cave",
    L"Arena",
    L"Hangar",
    L"Carpeted hallway",
    L"Hallway",
    L"Stone Corridor",
    L"Alley",
    L"City",
    L"Mountains",
    L"Quarry",
    L"Plain",
    L"Parking lot",
    L"Sewer pipe",
    L"Underwater",
    L"Small room",
    L"Medium room",
    L"Large room",
    L"Medium hall",
    L"Large hall",
    L"Plate",
};

#define FLAG_MOVE_UP        0x1
#define FLAG_MOVE_LEFT      0x2
#define FLAG_MOVE_RIGHT     0x4
#define FLAG_MOVE_DOWN      0x8

int                             g_moveFlags = 0;

const float                     MOTION_SCALE = 10.0f;


//--------------------------------------------------------------------------------------
// Constant buffers
//--------------------------------------------------------------------------------------
#pragma pack(push,1)
struct Vertex
{
    XMFLOAT3 Pos;
    DWORD color;
};

struct CB_VS_PER_OBJECT
{
    XMFLOAT4X4  m_Transform;
};
#pragma pack(pop)

ID3D11Buffer*   g_pcbVSPerObject11 = nullptr;


//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------

// UI control IDs
#define IDC_STATIC              -1
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           2
#define IDC_CHANGEDEVICE        3
#define IDC_TOGGLEWARP          4
#define IDC_SOUND               5
#define IDC_CONTROL_MODE        6
#define IDC_PRESET              7
#define IDC_UP                  8
#define IDC_LEFT                9
#define IDC_RIGHT               10
#define IDC_DOWN                11
#define IDC_LISTENERCONE        12
#define IDC_INNERRADIUS         13

// Constants for colors
static const DWORD              SOURCE_COLOR = 0xffea1b1b;
static const DWORD              LISTENER_COLOR = 0xff2b2bff;
static const DWORD              FLOOR_COLOR = 0xff101010;
static const DWORD              GRID_COLOR = 0xff00a000;


//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );

bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output,
                                       const CD3D11EnumDeviceInfo *DeviceInfo,
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

    // DXUT will create and use the best device (either D3D9 or D3D10)
    // that is available on the system depending on which D3D callbacks are set below

    // Set DXUT callbacks
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );

    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );

    InitApp();

    HRESULT hr = InitAudio();
    if( FAILED( hr ) )
    {
        OutputDebugString( L"InitAudio() failed.  Disabling audio support\n" );
    }

    DXUTInit( true, true, nullptr ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true );
    DXUTCreateWindow( L"XAudio2Sound3D" );
    DXUTCreateDevice( D3D_FEATURE_LEVEL_9_1, true, 800, 600 );

    hr = PrepareAudio( g_SOUND_NAMES[0] );
    if( FAILED( hr ) )
    {
        OutputDebugString( L"PrepareAudio() failed\n" );
    }

    DXUTMainLoop(); // Enter into the DXUT render loop

    CleanupAudio();

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app
//--------------------------------------------------------------------------------------
void InitApp()
{
    g_SettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent );
    int iY = 30;
    int iYo = 26;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 0, iY, 170, 22 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 0, iY += iYo, 170, 22, VK_F2 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 0, iY += iYo, 170, 22, VK_F3 );
    g_HUD.AddButton( IDC_TOGGLEWARP, L"Toggle WARP (F4)", 0, iY += iYo, 170, 22, VK_F4 );

    g_SampleUI.SetCallback( OnGUIEvent );

    //
    // Sound control
    //

    CDXUTComboBox* pComboBox = nullptr;
    g_SampleUI.AddStatic( IDC_STATIC, L"S(o)und", 10, 0, 170, 25 );
    g_SampleUI.AddComboBox( IDC_SOUND, 10, 25, 140, 24, 'O', false, &pComboBox );
    if( pComboBox )
    {
        pComboBox->SetDropHeight( 50 );

        for( int i = 0; i < sizeof( g_SOUND_NAMES ) / sizeof( WCHAR* ); i++ )
        {
            pComboBox->AddItem( g_SOUND_NAMES[i], IntToPtr( i ) );
        }
    }

    //
    // Control mode
    //

    g_SampleUI.AddStatic( IDC_STATIC, L"(C)ontrol mode", 10, 45, 170, 25 );
    g_SampleUI.AddComboBox( IDC_CONTROL_MODE, 10, 70, 140, 24, 'C', false, &pComboBox );
    if( pComboBox )
    {
        pComboBox->SetDropHeight( 30 );

        pComboBox->AddItem( L"Source", IntToPtr( CONTROL_SOURCE ) );
        pComboBox->AddItem( L"Listener", IntToPtr( CONTROL_LISTENER ) );
    }

    //
    // I3DL2 reverb preset control
    //

    g_SampleUI.AddStatic( IDC_STATIC, L"(R)everb", 10, 90, 170, 25 );
    g_SampleUI.AddComboBox( IDC_PRESET, 10, 115, 140, 24, 'R', false, &pComboBox );
    if( pComboBox )
    {
        pComboBox->SetDropHeight( 50 );

        for( int i = 0; i < sizeof( g_PRESET_NAMES ) / sizeof( WCHAR* ); i++ )
        {
            pComboBox->AddItem( g_PRESET_NAMES[i], IntToPtr( i ) );
        }
    }

    //
    // Movement buttons
    //

    iY = 160;
    g_SampleUI.AddButton( IDC_UP, L"(W)", 40, iY, 70, 24 );
    g_SampleUI.AddButton( IDC_LEFT, L"(A)", 5, iY += 30, 70, 24 );
    g_SampleUI.AddButton( IDC_RIGHT, L"(D)", 75, iY, 70, 24 );
    g_SampleUI.AddButton( IDC_DOWN, L"(S)", 40, iY += 30, 70, 24 );

    //
    // Listener cone and inner radius buttons
    //
    g_SampleUI.AddButton( IDC_LISTENERCONE, L"Toggle Listener Cone", 10, iY += 50, 170, 22);
    g_SampleUI.AddButton( IDC_INNERRADIUS, L"Toggle Inner Radius", 10, iY += 24, 170, 22);
}


//--------------------------------------------------------------------------------------
// Render the help and statistics text. This function uses the ID3DXFont interface for
// efficient text rendering.
//--------------------------------------------------------------------------------------
void RenderText()
{
    g_pTxtHelper->Begin();

    g_pTxtHelper->SetInsertionPos( 5, 5 );
    g_pTxtHelper->SetForegroundColor( Colors::Yellow );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );

    g_pTxtHelper->SetForegroundColor( Colors::Green );
    g_pTxtHelper->DrawFormattedTextLine( L"Source: %.1f, %.1f, %.1f",
                                         g_audioState.emitter.Position.x, g_audioState.emitter.Position.y,
                                         g_audioState.emitter.Position.z );

    g_pTxtHelper->SetForegroundColor( Colors::LightBlue );
    g_pTxtHelper->DrawFormattedTextLine( L"Listener: %.1f, %.1f, %.1f",
                                         g_audioState.listener.Position.x, g_audioState.listener.Position.y,
                                         g_audioState.listener.Position.z );

    g_pTxtHelper->SetForegroundColor( Colors::White );
    g_pTxtHelper->DrawTextLine( L"Coefficients:" );

    // Interpretation of channels depends on channel mask
    switch( g_audioState.dwChannelMask )
    {
        case SPEAKER_MONO:
            g_pTxtHelper->DrawFormattedTextLine( L" C: %.3f", g_audioState.dspSettings.pMatrixCoefficients[0] );
            break;

        case SPEAKER_STEREO:
            g_pTxtHelper->DrawFormattedTextLine( L" L: %.3f", g_audioState.dspSettings.pMatrixCoefficients[0] );
            g_pTxtHelper->DrawFormattedTextLine( L" R: %.3f", g_audioState.dspSettings.pMatrixCoefficients[1] );
            break;

        case SPEAKER_2POINT1:
            g_pTxtHelper->DrawFormattedTextLine( L" L: %.3f", g_audioState.dspSettings.pMatrixCoefficients[0] );
            g_pTxtHelper->DrawFormattedTextLine( L" R: %.3f", g_audioState.dspSettings.pMatrixCoefficients[1] );
            g_pTxtHelper->DrawFormattedTextLine( L" LFE: %.3f", g_audioState.dspSettings.pMatrixCoefficients[2] );
            break;

        case SPEAKER_SURROUND:
            g_pTxtHelper->DrawFormattedTextLine( L" L: %.3f", g_audioState.dspSettings.pMatrixCoefficients[0] );
            g_pTxtHelper->DrawFormattedTextLine( L" R: %.3f", g_audioState.dspSettings.pMatrixCoefficients[1] );
            g_pTxtHelper->DrawFormattedTextLine( L" C: %.3f", g_audioState.dspSettings.pMatrixCoefficients[2] );
            g_pTxtHelper->DrawFormattedTextLine( L" B: %.3f", g_audioState.dspSettings.pMatrixCoefficients[3] );
            break;

        case SPEAKER_QUAD:
            g_pTxtHelper->DrawFormattedTextLine( L" L: %.3f", g_audioState.dspSettings.pMatrixCoefficients[0] );
            g_pTxtHelper->DrawFormattedTextLine( L" R: %.3f", g_audioState.dspSettings.pMatrixCoefficients[1] );
            g_pTxtHelper->DrawFormattedTextLine( L" Lb: %.3f", g_audioState.dspSettings.pMatrixCoefficients[2] );
            g_pTxtHelper->DrawFormattedTextLine( L" Rb: %.3f", g_audioState.dspSettings.pMatrixCoefficients[3] );
            break;

        case SPEAKER_4POINT1:
            g_pTxtHelper->DrawFormattedTextLine( L" L: %.3f", g_audioState.dspSettings.pMatrixCoefficients[0] );
            g_pTxtHelper->DrawFormattedTextLine( L" R: %.3f", g_audioState.dspSettings.pMatrixCoefficients[1] );
            g_pTxtHelper->DrawFormattedTextLine( L" LFE: %.3f", g_audioState.dspSettings.pMatrixCoefficients[2] );
            g_pTxtHelper->DrawFormattedTextLine( L" Lb: %.3f", g_audioState.dspSettings.pMatrixCoefficients[3] );
            g_pTxtHelper->DrawFormattedTextLine( L" Rb: %.3f", g_audioState.dspSettings.pMatrixCoefficients[4] );
            break;

        case SPEAKER_5POINT1:
            g_pTxtHelper->DrawFormattedTextLine( L" L: %.3f", g_audioState.dspSettings.pMatrixCoefficients[0] );
            g_pTxtHelper->DrawFormattedTextLine( L" R: %.3f", g_audioState.dspSettings.pMatrixCoefficients[1] );
            g_pTxtHelper->DrawFormattedTextLine( L" C: %.3f", g_audioState.dspSettings.pMatrixCoefficients[2] );
            g_pTxtHelper->DrawFormattedTextLine( L" LFE: %.3f", g_audioState.dspSettings.pMatrixCoefficients[3] );
            g_pTxtHelper->DrawFormattedTextLine( L" Lb: %.3f", g_audioState.dspSettings.pMatrixCoefficients[4] );
            g_pTxtHelper->DrawFormattedTextLine( L" Rb: %.3f", g_audioState.dspSettings.pMatrixCoefficients[5] );
            break;

        case SPEAKER_7POINT1:
            g_pTxtHelper->DrawFormattedTextLine( L" L: %.3f", g_audioState.dspSettings.pMatrixCoefficients[0] );
            g_pTxtHelper->DrawFormattedTextLine( L" R: %.3f", g_audioState.dspSettings.pMatrixCoefficients[1] );
            g_pTxtHelper->DrawFormattedTextLine( L" C: %.3f", g_audioState.dspSettings.pMatrixCoefficients[2] );
            g_pTxtHelper->DrawFormattedTextLine( L" LFE: %.3f", g_audioState.dspSettings.pMatrixCoefficients[3] );
            g_pTxtHelper->DrawFormattedTextLine( L" Lb: %.3f", g_audioState.dspSettings.pMatrixCoefficients[4] );
            g_pTxtHelper->DrawFormattedTextLine( L" Rb: %.3f", g_audioState.dspSettings.pMatrixCoefficients[5] );
            g_pTxtHelper->DrawFormattedTextLine( L" Lfc: %.3f", g_audioState.dspSettings.pMatrixCoefficients[6] );
            g_pTxtHelper->DrawFormattedTextLine( L" Rfc: %.3f", g_audioState.dspSettings.pMatrixCoefficients[7] );
            break;

        case SPEAKER_5POINT1_SURROUND:
            g_pTxtHelper->DrawFormattedTextLine( L" L: %.3f", g_audioState.dspSettings.pMatrixCoefficients[0] );
            g_pTxtHelper->DrawFormattedTextLine( L" R: %.3f", g_audioState.dspSettings.pMatrixCoefficients[1] );
            g_pTxtHelper->DrawFormattedTextLine( L" C: %.3f", g_audioState.dspSettings.pMatrixCoefficients[2] );
            g_pTxtHelper->DrawFormattedTextLine( L" LFE: %.3f", g_audioState.dspSettings.pMatrixCoefficients[3] );
            g_pTxtHelper->DrawFormattedTextLine( L" Ls: %.3f", g_audioState.dspSettings.pMatrixCoefficients[4] );
            g_pTxtHelper->DrawFormattedTextLine( L" Rs: %.3f", g_audioState.dspSettings.pMatrixCoefficients[5] );
            break;

        case SPEAKER_7POINT1_SURROUND:
            g_pTxtHelper->DrawFormattedTextLine( L" L: %.3f", g_audioState.dspSettings.pMatrixCoefficients[0] );
            g_pTxtHelper->DrawFormattedTextLine( L" R: %.3f", g_audioState.dspSettings.pMatrixCoefficients[1] );
            g_pTxtHelper->DrawFormattedTextLine( L" C: %.3f", g_audioState.dspSettings.pMatrixCoefficients[2] );
            g_pTxtHelper->DrawFormattedTextLine( L" LFE: %.3f", g_audioState.dspSettings.pMatrixCoefficients[3] );
            g_pTxtHelper->DrawFormattedTextLine( L" Lb: %.3f", g_audioState.dspSettings.pMatrixCoefficients[4] );
            g_pTxtHelper->DrawFormattedTextLine( L" Rb: %.3f", g_audioState.dspSettings.pMatrixCoefficients[5] );
            g_pTxtHelper->DrawFormattedTextLine( L" Ls: %.3f", g_audioState.dspSettings.pMatrixCoefficients[6] );
            g_pTxtHelper->DrawFormattedTextLine( L" Rs: %.3f", g_audioState.dspSettings.pMatrixCoefficients[7] );
            break;

        default:
            // Generic channel output for non-standard channel masks
            g_pTxtHelper->DrawFormattedTextLine( L" [0]: %.3f", g_audioState.dspSettings.pMatrixCoefficients[0] );
            g_pTxtHelper->DrawFormattedTextLine( L" [1]: %.3f", g_audioState.dspSettings.pMatrixCoefficients[1] );
            g_pTxtHelper->DrawFormattedTextLine( L" [2]: %.3f", g_audioState.dspSettings.pMatrixCoefficients[2] );
            g_pTxtHelper->DrawFormattedTextLine( L" [3]: %.3f", g_audioState.dspSettings.pMatrixCoefficients[3] );
            g_pTxtHelper->DrawFormattedTextLine( L" [4]: %.3f", g_audioState.dspSettings.pMatrixCoefficients[4] );
            g_pTxtHelper->DrawFormattedTextLine( L" [5]: %.3f", g_audioState.dspSettings.pMatrixCoefficients[5] );
            g_pTxtHelper->DrawFormattedTextLine( L" [6]: %.3f", g_audioState.dspSettings.pMatrixCoefficients[6] );
            g_pTxtHelper->DrawFormattedTextLine( L" [7]: %.3f", g_audioState.dspSettings.pMatrixCoefficients[7] );
            break;
    }

    g_pTxtHelper->SetForegroundColor( Colors::Gray );
    g_pTxtHelper->DrawFormattedTextLine( L"Distance: %.3f", g_audioState.dspSettings.EmitterToListenerDistance );

    g_pTxtHelper->SetForegroundColor( Colors::White );
    g_pTxtHelper->DrawFormattedTextLine( L"Doppler factor: %.3f", g_audioState.dspSettings.DopplerFactor );

    g_pTxtHelper->SetForegroundColor( Colors::Gray );
    g_pTxtHelper->DrawFormattedTextLine( L"LPF Direct: %.3f", g_audioState.dspSettings.LPFDirectCoefficient );
    g_pTxtHelper->DrawFormattedTextLine( L"LPF Reverb: %.3f", g_audioState.dspSettings.LPFReverbCoefficient );
    g_pTxtHelper->DrawFormattedTextLine( L"Reverb: %.3f", g_audioState.dspSettings.ReverbLevel );

    g_pTxtHelper->End();
}



//--------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output,
                                       const CD3D11EnumDeviceInfo *DeviceInfo,
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
    V_RETURN( g_SettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );
    g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15 );

    // Read the HLSL file
    // You should use the lowest possible shader profile for your shader to enable various feature levels. These
    // shaders are simple enough to work well within the lowest possible profile, and will run on all feature levels
    ID3DBlob* pVertexShaderBuffer = nullptr;
    V_RETURN( DXUTCompileFromFile( L"XAudio2Sound3D.fx", nullptr, "RenderSceneVS", "vs_4_0_level_9_1", D3DCOMPILE_ENABLE_STRICTNESS, 0,
                                   &pVertexShaderBuffer ) );

    ID3DBlob* pPixelShaderBuffer = nullptr;
    V_RETURN( DXUTCompileFromFile( L"XAudio2Sound3D.fx", nullptr, "RenderScenePS", "ps_4_0_level_9_1", D3DCOMPILE_ENABLE_STRICTNESS, 0, 
                                   &pPixelShaderBuffer ) );

    // Create the shaders
    V_RETURN( pd3dDevice->CreateVertexShader( pVertexShaderBuffer->GetBufferPointer(),
                                              pVertexShaderBuffer->GetBufferSize(), nullptr, &g_pVertexShader11 ) );
    DXUT_SetDebugName( g_pVertexShader11, "RenderSceneVS" );

    V_RETURN( pd3dDevice->CreatePixelShader( pPixelShaderBuffer->GetBufferPointer(),
                                             pPixelShaderBuffer->GetBufferSize(), nullptr, &g_pPixelShader11 ) );
    DXUT_SetDebugName( g_pPixelShader11, "RenderScenePS" );

    // Create a layout for the object data
    const D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "SV_Position", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",       0, DXGI_FORMAT_B8G8R8A8_UNORM,  0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    V_RETURN( pd3dDevice->CreateInputLayout( layout, ARRAYSIZE( layout ), pVertexShaderBuffer->GetBufferPointer(),
                                             pVertexShaderBuffer->GetBufferSize(), &g_pLayout11 ) );
    DXUT_SetDebugName( g_pLayout11, "Primary" );

    // No longer need the shader blobs
    SAFE_RELEASE( pVertexShaderBuffer );
    SAFE_RELEASE( pPixelShaderBuffer );

    // Create state
    D3D11_DEPTH_STENCIL_DESC desc={0};
    desc.DepthEnable = false;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

    desc.StencilEnable = false;
    desc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
    desc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;

    desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;

    desc.BackFace = desc.FrontFace;

    V_RETURN( pd3dDevice->CreateDepthStencilState(&desc, &g_pDepthState) );
    DXUT_SetDebugName( g_pDepthState, "DisableZ" );

    pd3dImmediateContext->OMSetDepthStencilState( g_pDepthState, 0 );
  
    // Create constant buffers
    D3D11_BUFFER_DESC cbDesc;
    ZeroMemory( &cbDesc, sizeof(cbDesc) );
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    cbDesc.ByteWidth = sizeof( CB_VS_PER_OBJECT );
    V_RETURN( pd3dDevice->CreateBuffer( &cbDesc, nullptr, &g_pcbVSPerObject11 ) );
    DXUT_SetDebugName( g_pcbVSPerObject11, "CB_VS_PER_OBJECT" );

    g_HUD.GetButton( IDC_TOGGLEWARP )->SetEnabled( true );

    // Create vertex buffers
    D3D11_BUFFER_DESC vbdesc = {0};
    vbdesc.Usage = D3D11_USAGE_DEFAULT;
    vbdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initData ={0};

    // Floor
    static const Vertex s_floor[4] = 
    {
        { XMFLOAT3( float(XMIN), float(ZMIN), 0 ), FLOOR_COLOR },
        { XMFLOAT3( float(XMIN), float(ZMAX), 0 ), FLOOR_COLOR },
        { XMFLOAT3( float(XMAX), float(ZMIN), 0 ), FLOOR_COLOR },
        { XMFLOAT3( float(XMAX), float(ZMAX), 0 ), FLOOR_COLOR },
    };

    vbdesc.ByteWidth = sizeof( Vertex ) * 4;
    initData.pSysMem = s_floor;
    V_RETURN( pd3dDevice->CreateBuffer( &vbdesc, &initData, &g_pvbFloor ) );
    DXUT_SetDebugName( g_pvbFloor, "Floor" );

    // Source
    static const Vertex s_source[4] =
    {
        { XMFLOAT3(-0.5f, -0.5f, 0),    SOURCE_COLOR },
        { XMFLOAT3(-0.5f, 0.5f, 0),     SOURCE_COLOR },
        { XMFLOAT3(0.5f, -0.5f, 0),     SOURCE_COLOR },
        { XMFLOAT3(0.5f, 0.5f, 0),      SOURCE_COLOR },
    };

    vbdesc.ByteWidth = sizeof( Vertex ) * 4;
    initData.pSysMem = s_source;
    V_RETURN( pd3dDevice->CreateBuffer( &vbdesc, &initData, &g_pvbSource ) );
    DXUT_SetDebugName( g_pvbSource, "Source" );

    // Listener
    static const Vertex s_listener[3] = 
    {
        { XMFLOAT3(-0.5f, -1.f, 0), LISTENER_COLOR },
        { XMFLOAT3(0, 1.f, 0 ),     LISTENER_COLOR },
        { XMFLOAT3(0.5f, -1.f, 0),  LISTENER_COLOR },
    };

    vbdesc.ByteWidth = sizeof( Vertex ) * 3;
    initData.pSysMem = s_listener;
    V_RETURN( pd3dDevice->CreateBuffer( &vbdesc, &initData, &g_pvbListener ) );
    DXUT_SetDebugName( g_pvbListener, "Listener" );

    // Listener Cone
    static const Vertex s_listenerCone[7] =
    {
        { XMFLOAT3(-1.04f, -3.86f, 0),  LISTENER_COLOR },
        { XMFLOAT3(0, 0, 0),            LISTENER_COLOR },
        { XMFLOAT3(-3.86f, 1.04f, 0),   LISTENER_COLOR },
        { XMFLOAT3(0, 0, 0),            LISTENER_COLOR },
        { XMFLOAT3(3.86f, 1.04f, 0),    LISTENER_COLOR },
        { XMFLOAT3(0, 0, 0),            LISTENER_COLOR },
        { XMFLOAT3(1.04f, -3.86f, 0),   LISTENER_COLOR },
    };

    vbdesc.ByteWidth = sizeof( Vertex ) * 7;
    initData.pSysMem = s_listenerCone;
    V_RETURN( pd3dDevice->CreateBuffer( &vbdesc, &initData, &g_pvbListenerCone ) );
    DXUT_SetDebugName( g_pvbListenerCone, "ListenerCone" );

    // Inner Radius
    static const Vertex s_innerRaduius[9] =
    {
        { XMFLOAT3(0.0f, -2.0f, 0), LISTENER_COLOR },
        { XMFLOAT3(1.4f, -1.4f, 0), LISTENER_COLOR },
        { XMFLOAT3(2.0f, 0.0f, 0),  LISTENER_COLOR },
        { XMFLOAT3(1.4f, 1.4f, 0 ), LISTENER_COLOR },
        { XMFLOAT3(0.0f, 2.0f, 0),  LISTENER_COLOR },
        { XMFLOAT3(-1.4f, 1.4f, 0), LISTENER_COLOR },
        { XMFLOAT3(-2.0f, 0.0f, 0), LISTENER_COLOR },
        { XMFLOAT3(-1.4f, -1.4f, 0),LISTENER_COLOR },
        { XMFLOAT3(0.0f, -2.0f, 0), LISTENER_COLOR },
    };

    vbdesc.ByteWidth = sizeof( Vertex ) * 9;
    initData.pSysMem = s_innerRaduius;
    V_RETURN( pd3dDevice->CreateBuffer( &vbdesc, &initData, &g_pvbInnerRadius ) );
    DXUT_SetDebugName( g_pvbInnerRadius, "InnerRadius" );

    // Grid
    const UINT lcount = 2 * ( ( ZMAX - ZMIN + 1 ) + ( XMAX - XMIN + 1 ) );
    std::unique_ptr<Vertex> vbData( new Vertex[ lcount ] );

    auto pVertices = vbData.get();
    int i, j;
    for( i = ZMIN, j = 0; i <= ZMAX; ++i, ++j )
    {
        pVertices[ j * 2 + 0 ].Pos = XMFLOAT3( float(XMIN), float(i), 0 );
        pVertices[ j * 2 + 0 ].color = GRID_COLOR;
        pVertices[ j * 2 + 1 ].Pos = XMFLOAT3( float(XMAX), float(i), 0 );
        pVertices[ j * 2 + 1 ].color = GRID_COLOR;
    }
    for( i = XMIN; i <= XMAX; ++i, ++j )
    {
        pVertices[ j * 2 + 0 ].Pos = XMFLOAT3( float(i), float(ZMIN), 0 );
        pVertices[ j * 2 + 0 ].color = GRID_COLOR;
        pVertices[ j * 2 + 1 ].Pos = XMFLOAT3( float(i), float(ZMAX), 0 );
        pVertices[ j * 2 + 1 ].color = GRID_COLOR;
    }
    
    vbdesc.ByteWidth = sizeof( Vertex ) * lcount;
    initData.pSysMem = pVertices;
    V_RETURN( pd3dDevice->CreateBuffer( &vbdesc, &initData, &g_pvbGrid ) );
    DXUT_SetDebugName( g_pvbGrid, "Grid" );

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

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 180, pBackBufferSurfaceDesc->Height - 375 );
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
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.OnRender( fElapsedTime );
        return;
    }       

    auto pRTV = DXUTGetD3D11RenderTargetView();
    pd3dImmediateContext->ClearRenderTargetView( pRTV, Colors::MidnightBlue );

    // Clear the depth stencil
    auto pDSV = DXUTGetD3D11DepthStencilView();
    pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );

    // Set render resources
    pd3dImmediateContext->IASetInputLayout( g_pLayout11 );
    pd3dImmediateContext->VSSetShader( g_pVertexShader11, nullptr, 0 );
    pd3dImmediateContext->PSSetShader( g_pPixelShader11, nullptr, 0 );

    // Draw the floor
    XMMATRIX mScale = XMMatrixScaling( 1.f / ( XMAX - XMIN ), 1.f / ( ZMAX - ZMIN ), 1 );

    HRESULT hr;
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    V( pd3dImmediateContext->Map( g_pcbVSPerObject11, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    auto pVSPerObject = reinterpret_cast<CB_VS_PER_OBJECT*>( MappedResource.pData );
    XMStoreFloat4x4( &pVSPerObject->m_Transform, XMMatrixTranspose( mScale ) );
    pd3dImmediateContext->Unmap( g_pcbVSPerObject11, 0 );
    pd3dImmediateContext->VSSetConstantBuffers( 0, 1, &g_pcbVSPerObject11 );

    UINT vbstride = sizeof(Vertex);
    UINT vboffset = 0;

    auto vb = g_pvbFloor;
    pd3dImmediateContext->IASetVertexBuffers( 0, 1, &vb, &vbstride, &vboffset );
    pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );
    pd3dImmediateContext->Draw( 4, 0 );

    // Draw the grid
    vb = g_pvbGrid;
    pd3dImmediateContext->IASetVertexBuffers( 0, 1, &vb, &vbstride, &vboffset );
    pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_LINELIST );
    const UINT lcount = 2 * ( ( ZMAX - ZMIN + 1 ) + ( XMAX - XMIN + 1 ) );
    pd3dImmediateContext->Draw( lcount, 0 );

    // Draw the listener
    {
        XMMATRIX mTrans = XMMatrixTranslation( g_audioState.vListenerPos.x, g_audioState.vListenerPos.z, 0 );

        XMMATRIX mRot = XMMatrixRotationZ( -g_audioState.fListenerAngle );

        XMMATRIX mat = mRot * mTrans * mScale;
 
        V( pd3dImmediateContext->Map( g_pcbVSPerObject11, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
        pVSPerObject = reinterpret_cast<CB_VS_PER_OBJECT*>( MappedResource.pData );
        XMStoreFloat4x4( &pVSPerObject->m_Transform, XMMatrixTranspose( mat ) );
        pd3dImmediateContext->Unmap( g_pcbVSPerObject11, 0 );
        pd3dImmediateContext->VSSetConstantBuffers( 0, 1, &g_pcbVSPerObject11 );

        if (g_audioState.fUseListenerCone)
        {
            vb = g_pvbListenerCone;
            pd3dImmediateContext->IASetVertexBuffers( 0, 1, &vb, &vbstride, &vboffset );
            pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP );
            pd3dImmediateContext->Draw( 7, 0 );
        }

        if (g_audioState.fUseInnerRadius)
        {
            vb = g_pvbInnerRadius;
            pd3dImmediateContext->IASetVertexBuffers( 0, 1, &vb, &vbstride, &vboffset );
            pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP );
            pd3dImmediateContext->Draw( 9, 0 );
        }

        vb = g_pvbListener;
        pd3dImmediateContext->IASetVertexBuffers( 0, 1, &vb, &vbstride, &vboffset );
        pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
        pd3dImmediateContext->Draw( 3, 0 );
    }


    // Draw the source
    {
        XMMATRIX mTrans = XMMatrixTranslation( g_audioState.vEmitterPos.x, g_audioState.vEmitterPos.z, 0 );

        XMMATRIX mat = mTrans * mScale;
 
        V( pd3dImmediateContext->Map( g_pcbVSPerObject11, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
        pVSPerObject = reinterpret_cast<CB_VS_PER_OBJECT*>( MappedResource.pData );
        XMStoreFloat4x4( &pVSPerObject->m_Transform, XMMatrixTranspose( mat ) );
        pd3dImmediateContext->Unmap( g_pcbVSPerObject11, 0 );
        pd3dImmediateContext->VSSetConstantBuffers( 0, 1, &g_pcbVSPerObject11 );

        vb = g_pvbSource;
        pd3dImmediateContext->IASetVertexBuffers( 0, 1, &vb, &vbstride, &vboffset );
        pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );
        pd3dImmediateContext->Draw( 4, 0 );
    }

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
    g_HUD.OnRender( fElapsedTime );
    g_SampleUI.OnRender( fElapsedTime );
    RenderText();
    DXUT_EndPerfEvent();

    static ULONGLONG timefirst = GetTickCount64();
    if ( GetTickCount64() - timefirst > 5000 )
    {    
        OutputDebugString( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
        OutputDebugString( L"\n" );
        timefirst = GetTickCount64();
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
    g_SettingsDlg.OnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE( g_pTxtHelper );

    SAFE_RELEASE( g_pVertexShader11 );
    SAFE_RELEASE( g_pPixelShader11 )
    SAFE_RELEASE( g_pLayout11 );
    SAFE_RELEASE( g_pDepthState );

    SAFE_RELEASE( g_pvbFloor );
    SAFE_RELEASE( g_pvbSource );
    SAFE_RELEASE( g_pvbListener );
    SAFE_RELEASE( g_pvbListenerCone );
    SAFE_RELEASE( g_pvbInnerRadius );
    SAFE_RELEASE( g_pvbGrid );

    SAFE_RELEASE( g_pcbVSPerObject11 );
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
    if( fElapsedTime > 0 )
    {
        auto& vec = ( g_eControlMode == CONTROL_LISTENER ) ? g_audioState.vListenerPos : g_audioState.vEmitterPos;

        if( g_moveFlags & FLAG_MOVE_UP )
        {
            vec.z += fElapsedTime * MOTION_SCALE;
            vec.z = std::min<float>( float(ZMAX), vec.z );
        }

        if( g_moveFlags & FLAG_MOVE_LEFT )
        {
            vec.x -= fElapsedTime * MOTION_SCALE;
            vec.x = std::max<float>( float(XMIN), vec.x );
        }

        if( g_moveFlags & FLAG_MOVE_RIGHT )
        {
            vec.x += fElapsedTime * MOTION_SCALE;
            vec.x = std::min<float>( float(XMAX), vec.x );
        }

        if( g_moveFlags & FLAG_MOVE_DOWN )
        {
            vec.z -= fElapsedTime * MOTION_SCALE;
            vec.z = std::max<float>( float(ZMIN), vec.z );
        }
    }

    UpdateAudio( fElapsedTime );
}


//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext )
{
    // We use a simple sound focus model of not hearing the sound if the application is full-screen and minimized
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

    return 0;
}


//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
    switch( nChar )
    {
        case 'W':
        case 'w':
            if( bKeyDown )
                g_moveFlags |= FLAG_MOVE_UP;
            else
                g_moveFlags &= ~FLAG_MOVE_UP;
            break;

        case 'A':
        case 'a':
            if( bKeyDown )
                g_moveFlags |= FLAG_MOVE_LEFT;
            else
                g_moveFlags &= ~FLAG_MOVE_LEFT;
            break;

        case 'D':
        case 'd':
            if( bKeyDown )
                g_moveFlags |= FLAG_MOVE_RIGHT;
            else
                g_moveFlags &= ~FLAG_MOVE_RIGHT;
            break;

        case 'S':
        case 's':
            if( bKeyDown )
                g_moveFlags |= FLAG_MOVE_DOWN;
            else
                g_moveFlags &= ~FLAG_MOVE_DOWN;
            break;
    }

}


//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    CDXUTComboBox* pComboBox = nullptr;

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

        case IDC_SOUND:
            pComboBox = reinterpret_cast<CDXUTComboBox*>( pControl );
            PrepareAudio( g_SOUND_NAMES[ PtrToInt( pComboBox->GetSelectedData() ) ] );
            break;

        case IDC_CONTROL_MODE:
            pComboBox = reinterpret_cast<CDXUTComboBox*>( pControl );
            g_eControlMode = ( CONTROL_MODE )( int )PtrToInt( pComboBox->GetSelectedData() );
            break;

        case IDC_PRESET:
            pComboBox = reinterpret_cast<CDXUTComboBox*>( pControl );
            SetReverb( ( int )PtrToInt( pComboBox->GetSelectedData() ) );
            break;

        case IDC_UP:
        {
            auto& vec = ( g_eControlMode == CONTROL_LISTENER ) ? g_audioState.vListenerPos : g_audioState.vEmitterPos;
            vec.z += 0.5f;
            vec.z = std::min<float>( float( ZMAX ), vec.z );
        }
            break;

        case IDC_LEFT:
        {
            auto& vec = ( g_eControlMode == CONTROL_LISTENER ) ? g_audioState.vListenerPos : g_audioState.vEmitterPos;
            vec.x -= 0.5f;
            vec.x = std::max<float>( float( XMIN ), vec.x );
        }
            break;

        case IDC_RIGHT:
        {
            auto& vec = ( g_eControlMode == CONTROL_LISTENER ) ? g_audioState.vListenerPos : g_audioState.vEmitterPos;
            vec.x += 0.5f;
            vec.x = std::min<float>( float( XMAX ), vec.x );
        }
            break;

        case IDC_DOWN:
        {
            auto& vec = ( g_eControlMode == CONTROL_LISTENER ) ? g_audioState.vListenerPos : g_audioState.vEmitterPos;
            vec.z -= 0.5f;
            vec.z = std::max<float>( float( ZMIN ), vec.z );
        }
            break;
        case IDC_LISTENERCONE:
        {
            g_audioState.fUseListenerCone = !g_audioState.fUseListenerCone;
        }
            break;
        case IDC_INNERRADIUS:
        {
            g_audioState.fUseInnerRadius = !g_audioState.fUseInnerRadius;
        }
            break;
    }
}