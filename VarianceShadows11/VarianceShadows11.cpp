//--------------------------------------------------------------------------------------
// File: VarianceShadows11.cpp
//
// This sample demonstrates variance shadow maps.
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

#include "ShadowSampleMisc.h"
#include "VarianceShadowsManager.h"
#include <commdlg.h>
#include "WaitDlg.h"

#pragma warning( disable : 4100 )

using namespace DirectX;

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
VarianceShadowsManager      g_VarianceShadow;

CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
CFirstPersonCamera          g_ViewerCamera;          
CFirstPersonCamera          g_LightCamera;         
CFirstPersonCamera*         g_pActiveCamera = &g_ViewerCamera;

CascadeConfig               g_CascadeConfig;
// This enum is used to allow the user to select the number of cascades in the scene.
enum CASCADE_LEVELS 
{
    L1COMBO,
    L2COMBO,
    L3COMBO,
    L4COMBO,
    L5COMBO,
    L6COMBO,
    L7COMBO,
    L8COMBO
};

// DXUT GUI stuff
CDXUTComboBox*              g_DepthBufferFormatCombo;
CDXUTComboBox*              g_ShadowBufferTypeCombo;
CDXUTComboBox*              g_CascadeLevelsCombo;
CDXUTComboBox*              g_CameraSelectCombo;
CDXUTComboBox*              g_SceneSelectCombo;
CDXUTComboBox*              g_ShadowFilterSelectCombo;
CDXUTComboBox*              g_FitToCascadesCombo;
CDXUTComboBox*              g_FitToNearFarCombo;
CDXUTComboBox*              g_CascadeSelectionCombo;
CD3DSettingsDlg             g_D3DSettingsDlg;       // Device settings dialog
CDXUTDialog                 g_HUD;                  // manages the 3D   
CDXUTDialog                 g_SampleUI;             // dialog for sample specific controls
CDXUTTextHelper*            g_pTxtHelper = nullptr;

bool                        g_bShowHelp = false;    // If true, it renders the UI control text
bool                        g_bVisualizeCascades = FALSE;
bool                        g_bMoveLightTexelSize = TRUE;
FLOAT                       g_fAspectRatio = 1.0f;
CDXUTSDKMesh                g_MeshPowerPlant;
CDXUTSDKMesh                g_MeshTestScene;
CDXUTSDKMesh*               g_pSelectedMesh;                


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN         1
#define IDC_TOGGLEWARP               2
#define IDC_CHANGEDEVICE             3

#define IDC_TOGGLEVISUALIZECASCADES  4
#define IDC_DEPTHBUFFERFORMAT        5

#define IDC_BUFFER_SIZE              6
#define IDC_BUFFER_SIZETEXT          7
#define IDC_SELECTED_CAMERA          8

#define IDC_SELECTED_SCENE           9
#define IDC_SELECTED_SHADOW_FILTER   10

#define IDC_CASCADELEVELS            11

#define IDC_CASCADELEVEL1            12
#define IDC_CASCADELEVEL2            13
#define IDC_CASCADELEVEL3            14
#define IDC_CASCADELEVEL4            15
#define IDC_CASCADELEVEL5            16
#define IDC_CASCADELEVEL6            17
#define IDC_CASCADELEVEL7            18
#define IDC_CASCADELEVEL8            19

#define IDC_CASCADELEVEL1TEXT        20
#define IDC_CASCADELEVEL2TEXT        21
#define IDC_CASCADELEVEL3TEXT        22
#define IDC_CASCADELEVEL4TEXT        23
#define IDC_CASCADELEVEL5TEXT        24
#define IDC_CASCADELEVEL6TEXT        25
#define IDC_CASCADELEVEL7TEXT        26
#define IDC_CASCADELEVEL8TEXT        27

#define IDC_MOVE_LIGHT_IN_TEXEL_INC  28

#define IDC_FIT_TO_CASCADE           29
#define IDC_FIT_TO_NEARFAR           30
#define IDC_CASCADE_SELECT           31
#define IDC_SHADOW_BLUR_SIZE         32
#define IDC_SHADOW_BLUR_SIZETEXT     33

#define IDC_BLEND_BETWEEN_MAPS_CHECK 34
#define IDC_BLEND_MAPS_SLIDER        35

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, FLOAT fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, INT nControlID, CDXUTControl* pControl, void* pUserContext );
bool CALLBACK IsD3D11DeviceAcceptable(const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext );
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D11DestroyDevice( void* pUserContext );
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                  FLOAT fElapsedTime, void* pUserContext );

void InitApp();
void RenderText();
HRESULT DestroyD3DComponents();
HRESULT CreateD3DComponents( ID3D11Device* pd3dDevice );
void UpdateViewerCameraNearFar();

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
    DXUTCreateWindow( L"VarianceShadows11" );
    CWaitDlg CompilingShadersDlg;
    if ( DXUT_EnsureD3D11APIs() )
        CompilingShadersDlg.ShowDialog( L"Compiling Shaders and loading models." );
    DXUTCreateDevice ( D3D_FEATURE_LEVEL_10_0, true, 800, 600 );
    CompilingShadersDlg.DestroyDialog();
    
    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{

    g_CascadeConfig.m_nCascadeLevels = 3;
    g_CascadeConfig.m_iBufferSize = 1024;


    g_VarianceShadow.m_iCascadePartitionsZeroToOne[0] = 5;
    g_VarianceShadow.m_iCascadePartitionsZeroToOne[1] = 15;
    g_VarianceShadow.m_iCascadePartitionsZeroToOne[2] = 60;
    g_VarianceShadow.m_iCascadePartitionsZeroToOne[3] = 100;
    g_VarianceShadow.m_iCascadePartitionsZeroToOne[4] = 100;
    g_VarianceShadow.m_iCascadePartitionsZeroToOne[5] = 100;
    g_VarianceShadow.m_iCascadePartitionsZeroToOne[6] = 100;
    g_VarianceShadow.m_iCascadePartitionsZeroToOne[7] = 100;

    g_VarianceShadow.m_iCascadePartitionsMax = 100;
    // Initialize dialogs
    g_D3DSettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); INT iY = 10;

    // Add tons of GUI stuff
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 0, iY, 170, 23 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 0, iY += 26, 170, 23, VK_F2 );
    g_HUD.AddButton( IDC_TOGGLEWARP, L"Toggle WARP (F4)", 0, iY += 26, 170, 23, VK_F4 );

    g_HUD.AddCheckBox( IDC_TOGGLEVISUALIZECASCADES, L"Visualize Cascades", 0, iY+=26, 170, 23, g_bVisualizeCascades, VK_F8 );

    g_HUD.AddComboBox( IDC_DEPTHBUFFERFORMAT, 0, iY += 26, 170, 23, VK_F10, false, &g_DepthBufferFormatCombo );
    g_DepthBufferFormatCombo->AddItem (L"32 bit Buffer", ULongToPtr( DXGI_FORMAT_R32G32_FLOAT) );
    g_DepthBufferFormatCombo->AddItem (L"16 bit Buffer", ULongToPtr( DXGI_FORMAT_R16G16_FLOAT ) );

    DXGI_FORMAT sbt = ( DXGI_FORMAT ) PtrToUlong( g_DepthBufferFormatCombo->GetSelectedData() );
    g_CascadeConfig.m_ShadowBufferFormat = sbt;

    WCHAR desc[256];
    swprintf_s( desc, L"Texture Size: %d ", g_CascadeConfig.m_iBufferSize ); 

    g_HUD.AddStatic( IDC_BUFFER_SIZETEXT, desc, 0, iY+26, 30, 10);
    g_HUD.AddSlider( IDC_BUFFER_SIZE, 0, iY+=46, 128, 15,1, 128, g_CascadeConfig.m_iBufferSize / 32 );

    g_HUD.AddStatic( IDC_SHADOW_BLUR_SIZETEXT, L"Shadow Blur: 3", 0, iY+16, 30, 10);
    g_HUD.AddSlider( IDC_SHADOW_BLUR_SIZE, 115, iY+=20, 28, 15,1, 7, g_VarianceShadow.m_iShadowBlurSize / 2 + 1 );
    
    swprintf_s(desc, L"Cascade Blur %0.03f", g_VarianceShadow.m_fBlurBetweenCascadesAmount );
    bool bValue;
    if( g_VarianceShadow.m_iBlurBetweenCascades == 0 ) bValue = false;
    else bValue = true;

    g_HUD.AddCheckBox( IDC_BLEND_BETWEEN_MAPS_CHECK, desc, 0, iY+15, 170, 23, bValue );
    g_HUD.AddSlider( IDC_BLEND_MAPS_SLIDER, 40, iY+33, 100, 15, 0, 100, ( INT )
        ( g_VarianceShadow.m_fBlurBetweenCascadesAmount * 2000.0f ) );
    iY+=26;

    WCHAR dta[60];
    
    g_HUD.AddComboBox( IDC_SELECTED_SCENE, 0, iY+=26, 170, 23, VK_F8, false, &g_SceneSelectCombo );
    g_SceneSelectCombo->AddItem( L"Power Plant", ULongToPtr( POWER_PLANT_SCENE ) );
    g_SceneSelectCombo->AddItem( L"Test Scene", ULongToPtr( TEST_SCENE ) );

    g_HUD.AddComboBox( IDC_SELECTED_SHADOW_FILTER, 0, iY+=26, 170, 23, VK_F8, false, &g_ShadowFilterSelectCombo );
    g_ShadowFilterSelectCombo->AddItem( L"Anisotropic 16x", ULongToPtr( SHADOW_FILTER_ANISOTROPIC_16 ) );
    g_ShadowFilterSelectCombo->AddItem( L"Anisotropic 8x", ULongToPtr( SHADOW_FILTER_ANISOTROPIC_8 ) );
    g_ShadowFilterSelectCombo->AddItem( L"Anisotropic 4x", ULongToPtr( SHADOW_FILTER_ANISOTROPIC_4 ) );
    g_ShadowFilterSelectCombo->AddItem( L"Anisotropic 2x", ULongToPtr( SHADOW_FILTER_ANISOTROPIC_2 ) );
    g_ShadowFilterSelectCombo->AddItem( L"Linear", ULongToPtr( SHADOW_FILTER_LINEAR ) );
    g_ShadowFilterSelectCombo->AddItem( L"Point", ULongToPtr( SHADOW_FILTER_POINT ) );

    g_HUD.AddComboBox( IDC_SELECTED_CAMERA, 0, iY +=26,  170, 23, VK_F9, false, &g_CameraSelectCombo );
    g_CameraSelectCombo->AddItem( L"Eye Camera", ULongToPtr( EYE_CAMERA ) );     
    g_CameraSelectCombo->AddItem( L"Light Camera", ULongToPtr( LIGHT_CAMERA) );     
    for( int index=0; index < g_CascadeConfig.m_nCascadeLevels; ++index ) 
    {
        swprintf_s( dta, L"Cascade Cam %d", index + 1 );
        g_CameraSelectCombo->AddItem(dta, ULongToPtr( ORTHO_CAMERA1+index ) );     
    }

    g_HUD.AddCheckBox( IDC_MOVE_LIGHT_IN_TEXEL_INC, L"Fit Light to Texels", 
        0, iY+=26, 170, 23, g_bMoveLightTexelSize, VK_F8 );
    g_VarianceShadow.m_bMoveLightTexelSize = g_bMoveLightTexelSize;
    g_HUD.AddComboBox( IDC_FIT_TO_CASCADE, 0, iY +=26,  170, 23, VK_F9, false, &g_FitToCascadesCombo );
    g_FitToCascadesCombo->AddItem( L"Fit Scene", ULongToPtr( FIT_TO_SCENE ) );
    g_FitToCascadesCombo->AddItem( L"Fit Cascades", ULongToPtr( FIT_TO_CASCADES ) );
    g_VarianceShadow.m_eSelectedCascadesFit = FIT_TO_SCENE;
    
    g_HUD.AddComboBox( IDC_FIT_TO_NEARFAR, 0, iY +=26,  170, 23, VK_F9, false, &g_FitToNearFarCombo );
    g_FitToNearFarCombo->AddItem( L"AABB/Scene NearFar", ULongToPtr( FIT_NEARFAR_SCENE_AABB ) );
    g_FitToNearFarCombo->AddItem( L"0:1 NearFar", ULongToPtr( FIT_NEARFAR_ZERO_ONE ) );
    g_FitToNearFarCombo->AddItem( L"AABB NearFar", ULongToPtr( FIT_NEARFAR_AABB ) );
    g_VarianceShadow.m_eSelectedNearFarFit =  FIT_NEARFAR_SCENE_AABB;

    g_HUD.AddComboBox( IDC_CASCADE_SELECT, 0, iY +=26,  170, 23, VK_F9, false, &g_CascadeSelectionCombo );
    g_CascadeSelectionCombo->AddItem( L"Map Selection", ULongToPtr( CASCADE_SELECTION_MAP ) );
    g_CascadeSelectionCombo->AddItem( L"Interval Selection", ULongToPtr( CASCADE_SELECTION_INTERVAL ) );

    g_VarianceShadow.m_eSelectedCascadeSelection = CASCADE_SELECTION_MAP;

    g_HUD.AddComboBox( IDC_CASCADELEVELS, 0, iY += 26, 170, 23, VK_F11, false, &g_CascadeLevelsCombo );
    
    swprintf_s( dta, L"%d Level", 1 );
    g_CascadeLevelsCombo->AddItem (dta, ULongToPtr( L1COMBO+1 ) );
    for( INT index=1; index < MAX_CASCADES; ++index ) 
    {
        swprintf_s( dta, L"%d Levels", index + 1 );
        g_CascadeLevelsCombo->AddItem ( dta, ULongToPtr( L1COMBO+index ) );
    }

    g_CascadeLevelsCombo->SetSelectedByIndex( g_CascadeConfig.m_nCascadeLevels-1 );
    
    INT sp = 12;
    iY+=20;
    WCHAR label[16];
    // Color the cascade labels similar to the visualization.
    D3DCOLOR tcolors[] = 
    { 
        0xFFFF0000,
        0xFF00FF00,
        0xFF0000FF,
        0xFFFF00FF,
        0xFFFFFF00,
        0xFFFFFFFF,
        0xFF00AAFF,
        0xFFAAFFAA 
    };  
    
    for( INT index=0; index < MAX_CASCADES; ++index ) 
    {
        swprintf_s( label,L"L%d: %d", ( index + 1 ), g_VarianceShadow.m_iCascadePartitionsZeroToOne[index] );
        g_HUD.AddStatic( index+IDC_CASCADELEVEL1TEXT, label, 0, iY+sp, 30, 10);
        g_HUD.GetStatic( index+IDC_CASCADELEVEL1TEXT )->SetTextColor( tcolors[index] );
        g_HUD.AddSlider( index+IDC_CASCADELEVEL1, 50, iY+=15, 100, 15,0, 100, 
            g_VarianceShadow.m_iCascadePartitionsZeroToOne[index] );
    }

    for( INT index=0; index < g_CascadeConfig.m_nCascadeLevels; ++index ) 
    {
        g_HUD.GetStatic( IDC_CASCADELEVEL1TEXT + index )->SetVisible( true );
        g_HUD.GetSlider( IDC_CASCADELEVEL1 + index )->SetVisible( true );
    }
    for( int index=g_CascadeConfig.m_nCascadeLevels; index < MAX_CASCADES; ++index ) 
    {
        g_HUD.GetStatic( IDC_CASCADELEVEL1TEXT + index )->SetVisible( false );
        g_HUD.GetSlider( IDC_CASCADELEVEL1 + index )->SetVisible( false  );
    }
    
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
void CALLBACK OnFrameMove( double fTime, FLOAT fElapsedTime, void* pUserContext )
{
    // Update the camera's position based on user input 
    g_LightCamera.FrameMove( fElapsedTime );
    g_ViewerCamera.FrameMove( fElapsedTime );
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
        g_pTxtHelper->DrawTextLine( L"Move forward and backward with 'E' and 'D'\n"
                                    L"Move left and right with 'S' and 'D' \n"
                                    L"Click the mouse button to roate the camera\n");

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

    // Pass all remaining windows messages to camera so it can respond to user input
    g_pActiveCamera->HandleMessages( hWnd, uMsg, wParam, lParam );

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
void CALLBACK OnGUIEvent( UINT nEvent, INT nControlID, CDXUTControl* pControl, void* pUserContext )
{
    switch( nControlID )
    {
        case IDC_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen();
            break;
        case IDC_TOGGLEWARP:
            DXUTToggleWARP();
            break;
        case IDC_CHANGEDEVICE:
            g_D3DSettingsDlg.SetActive( !g_D3DSettingsDlg.IsActive() );
            break;
        case IDC_FIT_TO_CASCADE:
            g_VarianceShadow.m_eSelectedCascadesFit = 
                (FIT_PROJECTION_TO_CASCADES)PtrToUlong( g_FitToCascadesCombo->GetSelectedData() );
           break;
        case IDC_FIT_TO_NEARFAR:
            g_VarianceShadow.m_eSelectedNearFarFit = 
                (FIT_TO_NEAR_FAR)PtrToUlong( g_FitToNearFarCombo->GetSelectedData() );
           break;
        case IDC_CASCADE_SELECT:
        {
            static int iSaveLastCascadeValue = 100;
            if ( (CASCADE_SELECTION)PtrToUlong( g_CascadeSelectionCombo->GetSelectedData() ) == CASCADE_SELECTION_MAP ) 
            {
                g_VarianceShadow.m_iCascadePartitionsZeroToOne[g_CascadeConfig.m_nCascadeLevels -1] = iSaveLastCascadeValue;               
            }
            else 
            {
                iSaveLastCascadeValue = g_VarianceShadow.m_iCascadePartitionsZeroToOne[g_CascadeConfig.m_nCascadeLevels -1];
                g_VarianceShadow.m_iCascadePartitionsZeroToOne[g_CascadeConfig.m_nCascadeLevels -1] = 100;               
            }
            g_VarianceShadow.m_eSelectedCascadeSelection =
                (CASCADE_SELECTION)PtrToUlong( g_CascadeSelectionCombo->GetSelectedData() );

            g_HUD.GetSlider( IDC_CASCADELEVEL1 + g_CascadeConfig.m_nCascadeLevels - 1 )->SetValue( 
                g_VarianceShadow.m_iCascadePartitionsZeroToOne[g_CascadeConfig.m_nCascadeLevels - 1] );
            WCHAR label[16];
            swprintf_s( label, L"L%d: %d", g_CascadeConfig.m_nCascadeLevels,
                g_VarianceShadow.m_iCascadePartitionsZeroToOne[g_CascadeConfig.m_nCascadeLevels - 1]  );
            g_HUD.GetStatic( IDC_CASCADELEVEL1TEXT + g_CascadeConfig.m_nCascadeLevels - 1 )->SetText( label );

        }
        break;
        case IDC_MOVE_LIGHT_IN_TEXEL_INC:
            g_bMoveLightTexelSize = !g_bMoveLightTexelSize;
            g_VarianceShadow.m_bMoveLightTexelSize = g_bMoveLightTexelSize;
            break;

        case IDC_TOGGLEVISUALIZECASCADES :
            g_bVisualizeCascades = !g_bVisualizeCascades;  
            break;
        
        case IDC_SHADOW_BLUR_SIZE : 
        {
            INT PCFSize = g_HUD.GetSlider( IDC_SHADOW_BLUR_SIZE )->GetValue();
            PCFSize *= 2;
            PCFSize -=1;
            WCHAR desc[256];
            swprintf_s( desc, L"Shadow Blur: %d ", PCFSize ); 
            g_HUD.GetStatic( IDC_SHADOW_BLUR_SIZETEXT )->SetText( desc );
            g_VarianceShadow.m_iShadowBlurSize = PCFSize;
        }
        break;

        case IDC_BLEND_BETWEEN_MAPS_CHECK : 
            if( g_HUD.GetCheckBox( IDC_BLEND_BETWEEN_MAPS_CHECK )->GetChecked() )
                g_VarianceShadow.m_iBlurBetweenCascades = 1;
            else 
                g_VarianceShadow.m_iBlurBetweenCascades = 0;
            break;

        case IDC_BLEND_MAPS_SLIDER : 
        {
            INT val = g_HUD.GetSlider( IDC_BLEND_MAPS_SLIDER )->GetValue();
            g_VarianceShadow.m_fBlurBetweenCascadesAmount = (float)val * 0.005f;
            WCHAR dta[256];
            swprintf_s( dta, L"Cascade Blur %0.03f", g_VarianceShadow.m_fBlurBetweenCascadesAmount );
            g_HUD.GetCheckBox( IDC_BLEND_BETWEEN_MAPS_CHECK )->SetText( dta );
        }
        break;

        case IDC_BUFFER_SIZE : 
        {
            INT value = 32 * g_HUD.GetSlider( IDC_BUFFER_SIZE )->GetValue();
            INT max = 8192 / g_CascadeConfig.m_nCascadeLevels;
            if( value > max ) 
            { 
                value = max;
                g_HUD.GetSlider( IDC_BUFFER_SIZE )->SetValue( value / 32 );
            }
            WCHAR desc[256];
            swprintf_s( desc, L"Texture Size: %d ", value ); 
            g_HUD.GetStatic( IDC_BUFFER_SIZETEXT )->SetText( desc );

            //Only tell the app to recreate buffers once the user is through moving the slider. 
            if( nEvent == EVENT_SLIDER_VALUE_CHANGED_UP ) 
            {
                g_CascadeConfig.m_iBufferSize = value;
            }
        }
        break;

        case IDC_SELECTED_SHADOW_FILTER:
        {
            SHADOW_FILTER eFilterType = (SHADOW_FILTER)PtrToUlong( g_ShadowFilterSelectCombo->GetSelectedData() );
            g_VarianceShadow.m_eShadowFilter = eFilterType;
            
        }
        break;

        case IDC_SELECTED_SCENE:
        {
            SCENE_SELECTION ss = (SCENE_SELECTION) PtrToUlong( g_SceneSelectCombo->GetSelectedData() );
            if( ss == POWER_PLANT_SCENE ) 
            {
                g_pSelectedMesh = &g_MeshPowerPlant;
            } 
            else if (ss == TEST_SCENE ) 
            { 
                g_pSelectedMesh = &g_MeshTestScene;
            }
            DestroyD3DComponents();
            CreateD3DComponents( DXUTGetD3D11Device() );
            UpdateViewerCameraNearFar();
        }
        case IDC_SELECTED_CAMERA:
        {    
            g_VarianceShadow.m_eSelectedCamera = ( CAMERA_SELECTION ) 
                    ( g_CameraSelectCombo->GetSelectedIndex( ) );

            if( g_VarianceShadow.m_eSelectedCamera < 1 ) 
            {
                g_pActiveCamera = &g_ViewerCamera;
            }
            else 
            {
                g_pActiveCamera = &g_LightCamera;
            }
        }
        break;

        case IDC_CASCADELEVELS:
        {
            INT ind = 1 + g_CascadeLevelsCombo->GetSelectedIndex( );
            g_CascadeConfig.m_nCascadeLevels = ind;
            for( INT index=0; index < ind; ++index ) 
            {
                g_HUD.GetStatic( IDC_CASCADELEVEL1TEXT + index )->SetVisible( true );
                g_HUD.GetSlider( IDC_CASCADELEVEL1 + index )->SetVisible( true );
            }
            for( int index=ind; index < MAX_CASCADES; ++index ) 
            {
                g_HUD.GetStatic( IDC_CASCADELEVEL1TEXT + index )->SetVisible( false );
                g_HUD.GetSlider( IDC_CASCADELEVEL1 + index )->SetVisible( false  );
            }
            INT value = 32 * g_HUD.GetSlider( IDC_BUFFER_SIZE )->GetValue();
            INT max = 8192 / g_CascadeConfig.m_nCascadeLevels;
            if( value > max ) 
            {
                WCHAR desc[256];
                value = max;

                swprintf_s( desc, L"Texture Size: %d ", value ); 
                g_HUD.GetStatic( IDC_BUFFER_SIZETEXT )->SetText( desc );
                g_HUD.GetSlider( IDC_BUFFER_SIZE )->SetValue( value / 32);
                g_CascadeConfig.m_iBufferSize = value;
            }
            
            // update the selected camera based on these changes.
            INT selected = g_CameraSelectCombo->GetSelectedIndex();
            WCHAR dta[60];
            g_CameraSelectCombo->RemoveAllItems();
            g_CameraSelectCombo->AddItem( L"Eye Camera", ULongToPtr( EYE_CAMERA ) );
            g_CameraSelectCombo->AddItem( L"Light Camera", ULongToPtr( LIGHT_CAMERA) );
            for( int index=0; index < g_CascadeConfig.m_nCascadeLevels; ++index) 
            {
                swprintf_s( dta, L"Cascade Cam %d", index + 1 );
                g_CameraSelectCombo->AddItem( dta, ULongToPtr( ORTHO_CAMERA1+index ) );     
            }
            if( selected - 1 >= ind ) 
            { 
                selected = ind + 1;
            }
            g_CameraSelectCombo->SetSelectedByIndex ( selected );

            g_VarianceShadow.m_eSelectedCamera = ( CAMERA_SELECTION ) 
                    ( g_CameraSelectCombo->GetSelectedIndex() );

            if( g_VarianceShadow.m_eSelectedCamera < 1 ) 
            {
                g_pActiveCamera = &g_ViewerCamera;
            }
            else 
            {
                g_pActiveCamera = &g_LightCamera;
            }            
        }
        break;

        case IDC_DEPTHBUFFERFORMAT:
        {
            DXGI_FORMAT format = (DXGI_FORMAT) PtrToUlong( g_DepthBufferFormatCombo->GetSelectedData() );
            g_CascadeConfig.m_ShadowBufferFormat = format;
        }
        break;

        case IDC_CASCADELEVEL1:
        case IDC_CASCADELEVEL2:
        case IDC_CASCADELEVEL3:
        case IDC_CASCADELEVEL4:
        case IDC_CASCADELEVEL5:
        case IDC_CASCADELEVEL6:
        case IDC_CASCADELEVEL7:
        case IDC_CASCADELEVEL8:
            {
                INT ind = nControlID - IDC_CASCADELEVEL1;
                INT move = g_HUD.GetSlider( nControlID )->GetValue();
                CDXUTSlider* selecteSlider;
                CDXUTStatic* selectedStatic;
                WCHAR label[16];
                for( int index=0; index < ind; ++index ) 
                {
                    selecteSlider = g_HUD.GetSlider( IDC_CASCADELEVEL1 + index );
                    INT sVal = selecteSlider->GetValue();
                    if( move < sVal ) 
                    {
                        selecteSlider->SetValue( move );
                        selectedStatic = g_HUD.GetStatic( IDC_CASCADELEVEL1TEXT + index );
                        swprintf_s( label, L"L%d: %d", index+1, move );
                        selectedStatic->SetText( label );
                        g_VarianceShadow.m_iCascadePartitionsZeroToOne[index] = move;
                    }
                }
                for ( int index=ind; index < MAX_CASCADES; ++index ) 
                {
                    selecteSlider = g_HUD.GetSlider( IDC_CASCADELEVEL1 + index );
                    INT sVal = selecteSlider->GetValue();
                    if( move >= sVal ) 
                    {
                        selecteSlider->SetValue( move );
                        selectedStatic = g_HUD.GetStatic( IDC_CASCADELEVEL1TEXT + index );
                        swprintf_s( label, L"L%d: %d", index+1, move );
                        selectedStatic->SetText( label );
                        g_VarianceShadow.m_iCascadePartitionsZeroToOne[index] = move;
                    }
                }


            }
            break;
    }

}


//--------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo* AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo* DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// When the user changes scene, recreate these components as they are scene 
// dependent.
//--------------------------------------------------------------------------------------
HRESULT CreateD3DComponents( ID3D11Device* pd3dDevice ) 
{
    HRESULT hr;
    
    auto pd3dImmediateContext = DXUTGetD3D11DeviceContext();
    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );
    g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15 );
    
    static const XMVECTORF32 s_vecEye = { 100.0f, 5.0f, 5.0f, 0.f };
    XMFLOAT3 vMin = XMFLOAT3( -1000.0f, -1000.0f, -1000.0f );
    XMFLOAT3 vMax = XMFLOAT3( 1000.0f, 1000.0f, 1000.0f );

    g_ViewerCamera.SetViewParams( s_vecEye, g_XMZero );
    g_ViewerCamera.SetRotateButtons(TRUE, FALSE, FALSE);
    g_ViewerCamera.SetScalers( 0.01f, 10.0f );
    g_ViewerCamera.SetDrag( true );
    g_ViewerCamera.SetEnableYAxisMovement( true );
    g_ViewerCamera.SetClipToBoundary( TRUE, &vMin, &vMax );
    g_ViewerCamera.FrameMove( 0 );

    static const XMVECTORF32 s_lightEye = { -320.0f, 300.0f, -220.3f, 0.f };
    g_LightCamera.SetViewParams( s_lightEye, g_XMZero );
    g_LightCamera.SetRotateButtons( TRUE, FALSE, FALSE );
    g_LightCamera.SetScalers( 0.01f, 50.0f );
    g_LightCamera.SetDrag( true );
    g_LightCamera.SetEnableYAxisMovement( true );
    g_LightCamera.SetClipToBoundary( TRUE, &vMin, &vMax );
    g_LightCamera.SetProjParams( XM_PI / 4, 1.0f, 0.1f , 1000.0f);
    g_LightCamera.FrameMove( 0 );

    g_VarianceShadow.Init( pd3dDevice, g_pSelectedMesh, &g_ViewerCamera, &g_LightCamera, &g_CascadeConfig );
    
    return S_OK;
}



//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
    g_MeshPowerPlant.Destroy();
    g_MeshTestScene.Destroy();

    DestroyD3DComponents();
}

HRESULT DestroyD3DComponents() 
{
    g_DialogResourceManager.OnD3D11DestroyDevice();
    g_D3DSettingsDlg.OnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE( g_pTxtHelper );


    g_VarianceShadow.DestroyAndDeallocateShadowResources();
    return S_OK;

}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext )
{
    HRESULT hr = S_OK;
    V_RETURN( g_MeshPowerPlant.Create( pd3dDevice, L"powerplant\\powerplant.sdkmesh" ) );
    V_RETURN( g_MeshTestScene.Create( pd3dDevice, L"ShadowColumns\\testscene.sdkmesh" ) );
    g_pSelectedMesh = &g_MeshPowerPlant;

    return CreateD3DComponents( pd3dDevice );
}


//--------------------------------------------------------------------------------------
// Calcaulte the camera based on size of the current scene
//--------------------------------------------------------------------------------------
void UpdateViewerCameraNearFar () 
{
    XMVECTOR vMeshExtents = g_VarianceShadow.GetSceneAABBMax() - g_VarianceShadow.GetSceneAABBMin();
    XMVECTOR vMeshLength = XMVector3Length( vMeshExtents );
    FLOAT fMeshLength = XMVectorGetByIndex( vMeshLength, 0);
    g_ViewerCamera.SetProjParams( XM_PI / 4, g_fAspectRatio, 0.05f, fMeshLength );
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

    g_fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT ) pBackBufferSurfaceDesc->Height;

    UpdateViewerCameraNearFar();
        
    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 170, 300 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11ReleasingSwapChain();

}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                  FLOAT fElapsedTime, void* pUserContext )
{

    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    auto pRTV = DXUTGetD3D11RenderTargetView();
    pd3dImmediateContext->ClearRenderTargetView( pRTV, Colors::MidnightBlue );

    auto pDSV = DXUTGetD3D11DepthStencilView();
    pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );

    g_VarianceShadow.InitFrame( pd3dDevice );

    g_VarianceShadow.RenderShadowsForAllCascades( pd3dImmediateContext, g_pSelectedMesh );
    
    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)DXUTGetDXGIBackBufferSurfaceDesc()->Width;
    vp.Height = (FLOAT)DXUTGetDXGIBackBufferSurfaceDesc()->Height;
    vp.MinDepth = 0;
    vp.MaxDepth = 1;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;

    g_VarianceShadow.RenderScene( 
        pd3dImmediateContext, pRTV, pDSV, g_pSelectedMesh, g_pActiveCamera,  &vp, g_bVisualizeCascades );
    
    pd3dImmediateContext->RSSetViewports( 1, &vp);            
    pd3dImmediateContext->OMSetRenderTargets( 1, &pRTV, pDSV );

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );

    g_HUD.OnRender( fElapsedTime );
    g_SampleUI.OnRender( fElapsedTime );
    RenderText();
    DXUT_EndPerfEvent();
}
