//--------------------------------------------------------------------------------------
// File: DyanmicShaderLinkageFX11.cpp
//
// This sample shows a simple example of the Microsoft Direct3D's High-Level 
// Shader Language (HLSL) using Dynamic Shader Linkage in conjunction
// with Effects 11.
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

#pragma warning( disable : 4100 )

using namespace DirectX;

// We show two ways of handling dynamic linkage binding.
// This #define selects between a single technique where
// bindings are done via effect variables and multiple
// techniques where the bindings are done with BindInterfaces
// in the technqiues.
#define USE_BIND_INTERFACES 0

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

// Lighting Settings
bool                        g_bHemiAmbientLighting = false;
bool                        g_bDirectLighting = false;
bool                        g_bLightingOnly = false;
bool                        g_bWireFrame = false;

// Resources
CDXUTTextHelper*            g_pTxtHelper = nullptr;

CDXUTSDKMesh                g_Mesh11;

ID3D11Buffer*               g_pVertexBuffer = nullptr;
ID3D11Buffer*               g_pIndexBuffer = nullptr;

ID3D11InputLayout*          g_pVertexLayout11 = nullptr;

ID3DX11Effect*              g_pEffect = nullptr;
ID3DX11EffectTechnique*     g_pTechnique = nullptr;

ID3DX11EffectMatrixVariable*         g_pWorldViewProjection = nullptr;
ID3DX11EffectMatrixVariable*         g_pWorld = nullptr;

ID3DX11EffectScalarVariable*         g_pFillMode = nullptr;

ID3D11ShaderResourceView*            g_pEnvironmentMapSRV = nullptr;
ID3DX11EffectShaderResourceVariable* g_pEnvironmentMapVar = nullptr;

// Shader Linkage Interface and Class variables
ID3DX11EffectInterfaceVariable*      g_pAmbientLightIface = nullptr;
ID3DX11EffectInterfaceVariable*      g_pDirectionalLightIface = nullptr;
ID3DX11EffectInterfaceVariable*      g_pEnvironmentLightIface = nullptr;
ID3DX11EffectInterfaceVariable*      g_pMaterialIface = nullptr;

ID3DX11EffectClassInstanceVariable*  g_pAmbientLightClass = nullptr;
ID3DX11EffectVectorVariable*         g_pAmbientLightColor = nullptr;
ID3DX11EffectScalarVariable*         g_pAmbientLightEnable = nullptr;
ID3DX11EffectClassInstanceVariable*  g_pHemiAmbientLightClass = nullptr;
ID3DX11EffectVectorVariable*         g_pHemiAmbientLightColor = nullptr;
ID3DX11EffectScalarVariable*         g_pHemiAmbientLightEnable = nullptr;
ID3DX11EffectVectorVariable*         g_pHemiAmbientLightGroundColor = nullptr;
ID3DX11EffectVectorVariable*         g_pHemiAmbientLightDirUp = nullptr;
ID3DX11EffectClassInstanceVariable*  g_pDirectionalLightClass = nullptr;
ID3DX11EffectVectorVariable*         g_pDirectionalLightColor = nullptr;
ID3DX11EffectScalarVariable*         g_pDirectionalLightEnable = nullptr;
ID3DX11EffectVectorVariable*         g_pDirectionalLightDir = nullptr;
ID3DX11EffectClassInstanceVariable*  g_pEnvironmentLightClass = nullptr;
ID3DX11EffectVectorVariable*         g_pEnvironmentLightColor = nullptr;
ID3DX11EffectScalarVariable*         g_pEnvironmentLightEnable = nullptr;

ID3DX11EffectVectorVariable*         g_pEyeDir = nullptr;

// Material Dynamic Permutation 
enum E_MATERIAL_TYPES
{
   MATERIAL_PLASTIC,
   MATERIAL_PLASTIC_TEXTURED,
   MATERIAL_PLASTIC_LIGHTING_ONLY,

   MATERIAL_ROUGH,
   MATERIAL_ROUGH_TEXTURED,
   MATERIAL_ROUGH_LIGHTING_ONLY,

   MATERIAL_TYPE_COUNT
};
char*  g_pMaterialClassNames[ MATERIAL_TYPE_COUNT ] = 
{
   "g_plasticMaterial",             // cPlasticMaterial              
   "g_plasticTexturedMaterial",     // cPlasticTexturedMaterial      
   "g_plasticLightingOnlyMaterial", // cPlasticLightingOnlyMaterial 
   "g_roughMaterial",               // cRoughMaterial        
   "g_roughTexturedMaterial",       // cRoughTexturedMaterial
   "g_roughLightingOnlyMaterial"    // cRoughLightingOnlyMaterial    
};
E_MATERIAL_TYPES            g_iMaterial = MATERIAL_PLASTIC_TEXTURED;

struct MaterialVars
{
    ID3DX11EffectTechnique*             pTechnique;
    ID3DX11EffectClassInstanceVariable* pClass;
    ID3DX11EffectVectorVariable*        pColor;
    ID3DX11EffectScalarVariable*        pSpecPower;
};

MaterialVars g_MaterialClasses[ MATERIAL_TYPE_COUNT ] = { nullptr };

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4
#define IDC_TOGGLEWIRE          5


// Lighting Controls
#define IDC_AMBIENT_LIGHTING_GROUP        6
#define IDC_LIGHT_CONST_AMBIENT           7
#define IDC_LIGHT_HEMI_AMBIENT            8
#define IDC_LIGHT_DIRECT                  9
#define IDC_LIGHTING_ONLY                 10

// Material Controls
#define IDC_MATERIAL_GROUP                11
#define IDC_MATERIAL_PLASTIC              12
#define IDC_MATERIAL_PLASTIC_TEXTURED     13
#define IDC_MATERIAL_ROUGH                14
#define IDC_MATERIAL_ROUGH_TEXTURED       15

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

    // DXUT will create and use the best device feature level available 
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
    DXUTCreateWindow( L"DynamicShaderLinkageFX11" );
    DXUTCreateDevice(D3D_FEATURE_LEVEL_9_3, true, 800, 600 );
    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    static const XMVECTORF32 s_vLightDir = { -1.f, 1.f, -1.f, 0.f };
    XMVECTOR vLightDir = XMVector3Normalize( s_vLightDir );
    g_LightControl.SetLightDirection( vLightDir );

    // Initialize dialogs
    g_D3DSettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 25;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 0, iY, 170, 22 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 0, iY += 26, 170, 22, VK_F3 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 0, iY += 26, 170, 22, VK_F2 );
    g_HUD.AddButton( IDC_TOGGLEWIRE, L"Toggle Wires (F4)", 0, iY += 26, 170, 22, VK_F4 );

    // Material Controls
    iY = 10;
    g_SampleUI.AddRadioButton( IDC_MATERIAL_PLASTIC, IDC_MATERIAL_GROUP, L"Plastic", 0, iY += 26, 170, 22 );
    g_SampleUI.AddRadioButton( IDC_MATERIAL_PLASTIC_TEXTURED, IDC_MATERIAL_GROUP, L"Plastic Textured", 0, iY += 26, 170, 22 );
    g_SampleUI.AddRadioButton( IDC_MATERIAL_ROUGH, IDC_MATERIAL_GROUP, L"Rough", 0, iY += 26, 170, 22 );
    g_SampleUI.AddRadioButton( IDC_MATERIAL_ROUGH_TEXTURED, IDC_MATERIAL_GROUP, L"Rough Textured", 0, iY += 26, 170, 22 );
    auto pRadioButton = g_SampleUI.GetRadioButton( IDC_MATERIAL_PLASTIC_TEXTURED );
    pRadioButton->SetChecked( true );

    iY += 24;
    // Lighting Controls
    g_SampleUI.AddRadioButton( IDC_LIGHT_CONST_AMBIENT, IDC_AMBIENT_LIGHTING_GROUP, L"Constant Ambient", 0, iY += 26, 170, 22 );
    g_SampleUI.AddRadioButton( IDC_LIGHT_HEMI_AMBIENT, IDC_AMBIENT_LIGHTING_GROUP, L"Hemi Ambient", 0, iY += 26, 170, 22 );
    pRadioButton = g_SampleUI.GetRadioButton( IDC_LIGHT_CONST_AMBIENT );
    pRadioButton->SetChecked( true );

    g_SampleUI.AddCheckBox( IDC_LIGHT_DIRECT, L"Direct Lighting", 0, iY += 26, 170, 22, g_bDirectLighting );
    g_SampleUI.AddCheckBox( IDC_LIGHTING_ONLY, L"Lighting Only", 0, iY += 26, 170, 22, g_bLightingOnly );

    g_SampleUI.SetCallback( OnGUIEvent ); 

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
        case IDC_CHANGEDEVICE:
            g_D3DSettingsDlg.SetActive( !g_D3DSettingsDlg.IsActive() ); 
            break;
        case IDC_TOGGLEWIRE:
            g_bWireFrame  = !g_bWireFrame;
            break;

        // Lighting Controls
        case IDC_LIGHT_CONST_AMBIENT:
            g_bHemiAmbientLighting = false;
            break;
        case IDC_LIGHT_HEMI_AMBIENT:
            g_bHemiAmbientLighting = true;
            break;
        case IDC_LIGHT_DIRECT:
            g_bDirectLighting = !g_bDirectLighting;
            break;
        case IDC_LIGHTING_ONLY:
            g_bLightingOnly = !g_bLightingOnly;
            break;

        // Material Controls
        case IDC_MATERIAL_PLASTIC:
            g_iMaterial = MATERIAL_PLASTIC;
            break;
        case IDC_MATERIAL_PLASTIC_TEXTURED:
            g_iMaterial = MATERIAL_PLASTIC_TEXTURED;
            break;
        case IDC_MATERIAL_ROUGH:
            g_iMaterial = MATERIAL_ROUGH;
            break;
        case IDC_MATERIAL_ROUGH_TEXTURED:
            g_iMaterial = MATERIAL_ROUGH_TEXTURED;
            break;

    }
 

}


//--------------------------------------------------------------------------------------
// Reject any D3D devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}

//--------------------------------------------------------------------------------------
// Create any D3D resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext )
{
    HRESULT hr = S_OK;

    auto pd3dImmediateContext = DXUTGetD3D11DeviceContext();
    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );
    g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15 );

    XMFLOAT3 vCenter( 0.25767413f, -28.503521f, 111.00689f );
    FLOAT fObjectRadius = 378.15607f;

    g_mCenterMesh = XMMatrixTranslation( -vCenter.x, -vCenter.y, -vCenter.z );
    XMMATRIX m;
    m = XMMatrixRotationY( XM_PI );
    g_mCenterMesh *= m;
    m = XMMatrixRotationX( XM_PI / 2.0f );
    g_mCenterMesh *= m;

    // Init the UI widget for directional lighting
    V_RETURN( CDXUTDirectionWidget::StaticOnD3D11CreateDevice( pd3dDevice,  pd3dImmediateContext ) );
    g_LightControl.SetRadius( fObjectRadius );

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

    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"DynamicShaderLinkageFX11.fx" ) );

    ID3DBlob* pErrorBlob = nullptr;
    hr = D3DX11CompileEffectFromFile( str, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, dwShaderFlags, D3DCOMPILE_EFFECT_ALLOW_SLOW_OPS, pd3dDevice, &g_pEffect, &pErrorBlob );
    if ( pErrorBlob )
    {
        OutputDebugStringA( reinterpret_cast<const char*>( pErrorBlob->GetBufferPointer() ) );
        pErrorBlob->Release();
    }
    if( FAILED(hr) )
    {
        return hr;
    }

#else

    ID3DBlob* pEffectBuffer = nullptr;
    V_RETURN( DXUTCompileFromFile( L"DynamicShaderLinkageFX11.fx", nullptr, "none", "fx_5_0", dwShaderFlags, D3DCOMPILE_EFFECT_ALLOW_SLOW_OPS, &pEffectBuffer ) );
    hr = D3DX11CreateEffectFromMemory( pEffectBuffer->GetBufferPointer(), pEffectBuffer->GetBufferSize(), 0, pd3dDevice, &g_pEffect );
    SAFE_RELEASE( pEffectBuffer );
    if ( FAILED(hr) )
        return hr;
    
#endif

    // Get the light Class Interfaces for setting values
    // and as potential binding sources.
    g_pAmbientLightClass = g_pEffect->GetVariableByName( "g_ambientLight" )->AsClassInstance();
    g_pAmbientLightColor = g_pAmbientLightClass->GetMemberByName( "m_vLightColor" )->AsVector();
    g_pAmbientLightEnable = g_pAmbientLightClass->GetMemberByName( "m_bEnable" )->AsScalar();

    g_pHemiAmbientLightClass = g_pEffect->GetVariableByName( "g_hemiAmbientLight" )->AsClassInstance();
    g_pHemiAmbientLightColor = g_pHemiAmbientLightClass->GetMemberByName( "m_vLightColor" )->AsVector();
    g_pHemiAmbientLightEnable = g_pHemiAmbientLightClass->GetMemberByName( "m_bEnable" )->AsScalar();
    g_pHemiAmbientLightGroundColor = g_pHemiAmbientLightClass->GetMemberByName( "m_vGroundColor" )->AsVector();
    g_pHemiAmbientLightDirUp = g_pHemiAmbientLightClass->GetMemberByName( "m_vDirUp" )->AsVector();

    g_pDirectionalLightClass = g_pEffect->GetVariableByName( "g_directionalLight")->AsClassInstance();
    g_pDirectionalLightColor = g_pDirectionalLightClass->GetMemberByName( "m_vLightColor" )->AsVector();
    g_pDirectionalLightEnable = g_pDirectionalLightClass->GetMemberByName( "m_bEnable" )->AsScalar();
    g_pDirectionalLightDir = g_pDirectionalLightClass->GetMemberByName( "m_vLightDir" )->AsVector();

    g_pEnvironmentLightClass = g_pEffect->GetVariableByName( "g_environmentLight")->AsClassInstance();
    g_pEnvironmentLightColor = g_pEnvironmentLightClass->GetMemberByName( "m_vLightColor" )->AsVector();
    g_pEnvironmentLightEnable = g_pEnvironmentLightClass->GetMemberByName( "m_bEnable" )->AsScalar();

    g_pEyeDir = g_pEffect->GetVariableByName( "g_vEyeDir" )->AsVector();
    
    // Acquire the material Class Instances for all possible material settings
    for( UINT i=0; i < MATERIAL_TYPE_COUNT; i++)
    {
        char pTechName[50];

        sprintf_s( pTechName, sizeof(pTechName),
                   "FeatureLevel11_%s",
                   g_pMaterialClassNames[ i ] );
        g_MaterialClasses[i].pTechnique = g_pEffect->GetTechniqueByName( pTechName );

        g_MaterialClasses[i].pClass = g_pEffect->GetVariableByName( g_pMaterialClassNames[i] )->AsClassInstance();
        g_MaterialClasses[i].pColor = g_MaterialClasses[i].pClass->GetMemberByName( "m_vColor" )->AsVector();
        g_MaterialClasses[i].pSpecPower = g_MaterialClasses[i].pClass->GetMemberByName( "m_iSpecPower" )->AsScalar();
    }

    // Select which technique to use based on the feature level we acquired
    D3D_FEATURE_LEVEL supportedFeatureLevel = DXUTGetD3D11DeviceFeatureLevel();
    if (supportedFeatureLevel >= D3D_FEATURE_LEVEL_11_0)
    {
        // We are going to use Dynamic shader linkage with SM5 so we need to look up interface and class instance variables

        // Get the abstract class interfaces so we can dynamically permute and assign linkages
        g_pAmbientLightIface = g_pEffect->GetVariableByName( "g_abstractAmbientLighting" )->AsInterface();
        g_pDirectionalLightIface = g_pEffect->GetVariableByName( "g_abstractDirectLighting" )->AsInterface();
        g_pEnvironmentLightIface = g_pEffect->GetVariableByName( "g_abstractEnvironmentLighting" )->AsInterface();
        g_pMaterialIface = g_pEffect->GetVariableByName( "g_abstractMaterial" )->AsInterface();

        g_pTechnique = g_pEffect->GetTechniqueByName( "FeatureLevel11" );
    }
    else // Lower feature levels than 11 have no support for Dynamic Shader Linkage - need to use a statically specialized shaders
    {
        LPCSTR pTechniqueName;
        
        switch( supportedFeatureLevel )
        {
        case D3D_FEATURE_LEVEL_10_1:
            pTechniqueName = "FeatureLevel10_1";
            break;
        case D3D_FEATURE_LEVEL_10_0:
            pTechniqueName = "FeatureLevel10";
            break;
        case D3D_FEATURE_LEVEL_9_3:
            pTechniqueName = "FeatureLevel9_3";
            break;

        default:
            return E_FAIL;
        }
        
        g_pTechnique = g_pEffect->GetTechniqueByName( pTechniqueName );
    }

    // Create our vertex input layout
    const D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",    0, DXGI_FORMAT_R10G10B10A2_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },

        { "TEXCOORD",  0, DXGI_FORMAT_R16G16_FLOAT,    0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },

        { "TANGENT",  0, DXGI_FORMAT_R10G10B10A2_UNORM,   0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BINORMAL", 0, DXGI_FORMAT_R10G10B10A2_UNORM,   0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 }

    };

    D3DX11_PASS_SHADER_DESC VsPassDesc;
    D3DX11_EFFECT_SHADER_DESC VsDesc;

    V_RETURN( g_pTechnique->GetPassByIndex(0)->GetVertexShaderDesc(&VsPassDesc) );
    V_RETURN( VsPassDesc.pShaderVariable->GetShaderDesc(VsPassDesc.ShaderIndex, &VsDesc) );

    V_RETURN( pd3dDevice->CreateInputLayout( layout, ARRAYSIZE( layout ),
                                             VsDesc.pBytecode,
                                             VsDesc.BytecodeLength,
                                             &g_pVertexLayout11 ) );
    DXUT_SetDebugName( g_pVertexLayout11, "Primary" );

    // Load the mesh
    V_RETURN( g_Mesh11.Create( pd3dDevice, L"Squid\\squid.sdkmesh", false ) ); 

    g_pWorldViewProjection = g_pEffect->GetVariableByName( "g_mWorldViewProjection" )->AsMatrix();
    g_pWorld = g_pEffect->GetVariableByName( "g_mWorld" )->AsMatrix();
    
    // Load a HDR Environment for reflections
    V_RETURN( DXUTCreateShaderResourceViewFromFile( pd3dDevice, L"Light Probes\\uffizi_cross.dds", &g_pEnvironmentMapSRV ));
    g_pEnvironmentMapVar = g_pEffect->GetVariableByName( "g_txEnvironmentMap" )->AsShaderResource();
    g_pEnvironmentMapVar->SetResource( g_pEnvironmentMapSRV );

    // Setup the camera's view parameters
    static const XMVECTORF32 s_vecEye = { 0.0f, 0.0f, -50.0f, 0.f };
    g_Camera.SetViewParams( s_vecEye, g_XMZero );
    g_Camera.SetRadius( fObjectRadius , fObjectRadius , fObjectRadius );

    // Find Rasterizer State Object index for WireFrame / Solid rendering
    g_pFillMode = g_pEffect->GetVariableByName( "g_fillMode" )->AsScalar();

    return hr;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr = S_OK;

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

    return hr;
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
    XMMATRIX mWorld = g_Camera.GetWorldMatrix();
    XMMATRIX mProj  = g_Camera.GetProjMatrix();
    XMMATRIX mView  = g_Camera.GetViewMatrix();

    // Get the light direction
    XMVECTOR vLightDir = g_LightControl.GetLightDirection();

    // Render the light arrow so the user can visually see the light dir
    V( g_LightControl.OnRender( Colors::Yellow, mView, mProj, g_Camera.GetEyePt() ) );

    // Ambient Light
    static const XMVECTORF32 s_vLightColorA = { 0.1f, 0.1f, 0.1f, 1.0f };
    g_pAmbientLightColor->SetFloatVector( s_vLightColorA );
    g_pAmbientLightEnable->SetBool(true);

    // Hemi Ambient Light
    static const XMVECTORF32 s_vLightColorH1 = { 0.3f, 0.3f, 0.4f, 1.0f };
    g_pHemiAmbientLightColor->SetFloatVector( s_vLightColorH1 );
    g_pHemiAmbientLightEnable->SetBool(true);

    XMFLOAT4 vLightGrndClr( 0.05f, 0.05f, 0.05f, 1.f );
    g_pHemiAmbientLightGroundColor->SetFloatVector( reinterpret_cast<float*>( &vLightGrndClr ) );

    XMFLOAT4 vVec(0.0f, 1.0f, 0.0f, 1.0f);
    g_pHemiAmbientLightDirUp->SetFloatVector( reinterpret_cast<float*>( &vVec ) );

    // Directional Light
    g_pDirectionalLightColor->SetFloatVector( Colors::White );
    g_pDirectionalLightEnable->SetBool(true);

    XMFLOAT4 tmp;
    XMStoreFloat4( &tmp, vLightDir );
    tmp.w = 1.f;
    g_pDirectionalLightDir->SetFloatVector( reinterpret_cast<float*>( &tmp ) );

    // Environment Light - color comes from the texture
    g_pEnvironmentLightColor->SetFloatVector( Colors::Black );
    g_pEnvironmentLightEnable->SetBool(true);

    // Setup the Eye based on the DXUT camera
    XMVECTOR vEyePt = g_Camera.GetEyePt();
    XMVECTOR vDir = g_Camera.GetLookAtPt() - vEyePt;
    XMStoreFloat4( &tmp, vDir );
    tmp.w = 1.f;
    g_pEyeDir->SetFloatVector( reinterpret_cast<float*>( &tmp ) );

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
 
    // Set the per object constant data
    XMMATRIX mWorldViewProjection = mWorld * mView * mProj;

    // VS Per object
    XMFLOAT4X4 tmp4x4;
    XMStoreFloat4x4( &tmp4x4, mWorldViewProjection );
    g_pWorldViewProjection->SetMatrix( reinterpret_cast<float*>( &tmp4x4 ) );
    XMStoreFloat4x4( &tmp4x4, mWorld );
    g_pWorld->SetMatrix( reinterpret_cast<float*>( &tmp4x4 ) );

    // Setup the Shader Linkage based on the user settings for Lighting
    ID3DX11EffectClassInstanceVariable* pLightClassVar;
    
    // Ambient Lighting First - Constant or Hemi?
    if ( g_bHemiAmbientLighting )
    {
        pLightClassVar = g_pHemiAmbientLightClass;
    }
    else
    {
        pLightClassVar = g_pAmbientLightClass;
    }
    if (g_pAmbientLightIface)
    {
        g_pAmbientLightIface->SetClassInstance(pLightClassVar);
    }
    
    // Direct Light - None or Directional 
    if (g_bDirectLighting) 
    {
        pLightClassVar = g_pDirectionalLightClass;
    }
    else
    {
        // Disable ALL Direct Lighting
        pLightClassVar = g_pAmbientLightClass;
    }
    if (g_pDirectionalLightIface)
    {
        g_pDirectionalLightIface->SetClassInstance(pLightClassVar);
    }

    // Setup the selected material class instance
    E_MATERIAL_TYPES iMaterialTech = g_iMaterial;
    switch( g_iMaterial )
    {
    case MATERIAL_PLASTIC:
    case MATERIAL_PLASTIC_TEXTURED:
        // Bind the Environment light for reflections
        pLightClassVar = g_pEnvironmentLightClass;
        if (g_bLightingOnly)
        {
            iMaterialTech = MATERIAL_PLASTIC_LIGHTING_ONLY;
        }
        break;
    case MATERIAL_ROUGH:
    case MATERIAL_ROUGH_TEXTURED:
        // UnBind the Environment light 
        pLightClassVar = g_pAmbientLightClass;
        if (g_bLightingOnly)
        {
            iMaterialTech = MATERIAL_ROUGH_LIGHTING_ONLY;
        }
        break;
    }
    if (g_pEnvironmentLightIface)
    {
        g_pEnvironmentLightIface->SetClassInstance(pLightClassVar);
    }

    ID3DX11EffectTechnique* pTechnique = g_pTechnique;

    if (g_pMaterialIface)
    {
#if USE_BIND_INTERFACES

        // We're using the techniques with pre-bound materials,
        // so select the appropriate technique.
        pTechnique = g_MaterialClasses[ iMaterialTech ].pTechnique;

#else

        // We're using a single technique and need to explicitly
        // bind a concrete material instance.
        g_pMaterialIface->SetClassInstance( g_MaterialClasses[ iMaterialTech ].pClass );

#endif
    }

    // PS Per Prim

    // Shiny Plastic
    XMFLOAT3 clr1(1, 0, 0.5f);
    g_MaterialClasses[MATERIAL_PLASTIC].pColor->SetFloatVector( reinterpret_cast<float*>( &clr1 ) );
    g_MaterialClasses[MATERIAL_PLASTIC].pSpecPower->SetInt(255);

    // Shiny Plastic with Textures
    XMFLOAT3 clr2(1, 0, 0.5f);
    g_MaterialClasses[MATERIAL_PLASTIC_TEXTURED].pColor->SetFloatVector( reinterpret_cast<float*>( &clr2 ) );
    g_MaterialClasses[MATERIAL_PLASTIC_TEXTURED].pSpecPower->SetInt(128);

    // Lighting Only Plastic
    XMFLOAT3 clr3(1, 1, 1);
    g_MaterialClasses[MATERIAL_PLASTIC_LIGHTING_ONLY].pColor->SetFloatVector( reinterpret_cast<float*>( &clr3 ) );
    g_MaterialClasses[MATERIAL_PLASTIC_LIGHTING_ONLY].pSpecPower->SetInt(128);

    // Rough Material
    XMFLOAT3 clr4(0, 0.5f, 1);
    g_MaterialClasses[MATERIAL_ROUGH].pColor->SetFloatVector( reinterpret_cast<float*>( &clr4 ) );
    g_MaterialClasses[MATERIAL_ROUGH].pSpecPower->SetInt(6);

    // Rough Material with Textures
    XMFLOAT3 clr5(0, 0.5f, 1);
    g_MaterialClasses[MATERIAL_ROUGH_TEXTURED].pColor->SetFloatVector( reinterpret_cast<float*>( &clr5 ) );
    g_MaterialClasses[MATERIAL_ROUGH_TEXTURED].pSpecPower->SetInt(6);

    // Lighting Only Rough
    XMFLOAT3 clr6(1, 1, 1);
    g_MaterialClasses[MATERIAL_ROUGH_LIGHTING_ONLY].pColor->SetFloatVector( reinterpret_cast<float*>( &clr6 ) );
    g_MaterialClasses[MATERIAL_ROUGH_LIGHTING_ONLY].pSpecPower->SetInt(6);

    if (g_bWireFrame)
        g_pFillMode->SetInt(1);
    else
        g_pFillMode->SetInt(0);

    // Apply the technique to update state.
    pTechnique->GetPassByIndex(0)->Apply(0, pd3dImmediateContext);

    //Render
    g_Mesh11.Render( pd3dImmediateContext, 0, 1, INVALID_SAMPLER_SLOT);

    // Tell the UI items to render 
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
    CDXUTDirectionWidget::StaticOnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE( g_pTxtHelper );

    g_Mesh11.Destroy();

    SAFE_RELEASE( g_pEffect );
    
    SAFE_RELEASE( g_pVertexLayout11 );
    SAFE_RELEASE( g_pVertexBuffer );
    SAFE_RELEASE( g_pIndexBuffer );

    SAFE_RELEASE( g_pEnvironmentMapSRV );
}
