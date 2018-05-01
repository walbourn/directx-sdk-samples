//--------------------------------------------------------------------------------------
// File: AdaptiveTessellationCS40.cpp
//
// Demos how to use Compute Shader 4.0 to do one simple adaptive tessellation scheme
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTgui.h"
#include "SDKmisc.h"
#include "DXUTcamera.h"
#include "DXUTsettingsdlg.h"
#include "resource.h"

#include "TessellatorCS40.h"

#include "WaitDlg.h"

#pragma warning( push )
#pragma warning( disable : 4995 )
#include <fstream>
#include <sstream>
#pragma warning( pop )

#include <vector>

#pragma warning( disable : 4100 )

using namespace DirectX;

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CDXUTDialogResourceManager          g_DialogResourceManager;    // manager for shared resources of dialogs
CFirstPersonCamera                  g_Camera;                   // A first person camera
CD3DSettingsDlg                     g_D3DSettingsDlg;           // Device settings dialog
CDXUTDialog                         g_HUD;                      // dialog for standard controls
CDXUTDialog                         g_SampleUI;                 // dialog for sample specific controls
CDXUTTextHelper*                    g_pTxtHelper = nullptr;

ID3D11Buffer*                       g_pBaseVB = nullptr;           // Vertex buffer of the input base mesh
ID3D11Buffer*                       g_pTessedVB = nullptr;         // Vertex buffer of the tessellated mesh
ID3D11Buffer*                       g_pTessedIB = nullptr;         // Index buffer of the tessellated mesh

ID3D11InputLayout*                  g_pBaseVBLayout = nullptr;     // Vertex layout for the input base mesh

CTessellator                        g_Tessellator;              // Our CS4.0 tessellator, implemented in TessellatorCS40.cpp

ID3D11Buffer*                       g_pVSCB = nullptr;             // Constant buffer to transfer world view projection matrix to VS
ID3D11VertexShader*                 g_pVS = nullptr;               // VS for rendering the tessellated mesh
ID3D11VertexShader*                 g_pBaseVS = nullptr;           // VS for rendering the base mesh
ID3D11PixelShader*                  g_pPS = nullptr;               // PS for rendering both the tessellated mesh and base mesh

ID3D11RasterizerState*              g_pRasWireFrame = nullptr;     // Wireframe rasterizer mode

bool                                g_bShowSampleUI = true;     // Sample UI on/off
bool                                g_bShowTessellated = true;  // Whether to show tessellated mesh or base mesh

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN                1
#define IDC_TOGGLEREF                       3
#define IDC_CHANGEDEVICE                    4
#define IDC_SHOWTESSELLATED                 5
#define IDC_PARTITIONING_INTEGER            6
#define IDC_PARTITIONING_POW2               7
#define IDC_PARTITIONING_FRACTIONAL_ODD     8
#define IDC_PARTITIONING_FRACTIONAL_EVEN    9

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

    DXUTInit( true, true );                 // Use this line instead to try to create a hardware device

    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"AdaptiveTessellationCS40" );
    CWaitDlg CompilingShadersDlg;
    if ( DXUT_EnsureD3D11APIs() )
        CompilingShadersDlg.ShowDialog( L"Compiling Shaders" );
    DXUTCreateDevice(D3D_FEATURE_LEVEL_10_0, true, 1024, 768 );
    CompilingShadersDlg.DestroyDialog();
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
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 0, iY += 26, 170, 23, VK_F3 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 0, iY += 26, 170, 23, VK_F2 );

    iY = 0;
    g_SampleUI.AddCheckBox( IDC_SHOWTESSELLATED, L"Show (t)essellated", 0, iY += 26, 125, 22, g_bShowTessellated, 'T' );
    g_SampleUI.AddRadioButton( IDC_PARTITIONING_INTEGER, 0, L"Integer Partitioning(1)", 0, iY += 39, 125, 22, false, '1' );
    g_SampleUI.AddRadioButton( IDC_PARTITIONING_POW2, 0, L"Pow2 Partitioning(2)", 0, iY += 26, 125, 22, false, '2' );
    g_SampleUI.AddRadioButton( IDC_PARTITIONING_FRACTIONAL_ODD, 0, L"Odd Fractional Partitioning(3)", 0, iY += 26, 125, 22, false, '3' );
    g_SampleUI.AddRadioButton( IDC_PARTITIONING_FRACTIONAL_EVEN, 0, L"Even Fractional Partitioning(4)", 0, iY += 26, 125, 22, true, '4' );
    g_SampleUI.SetCallback( OnGUIEvent ); 
}


//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// This callback function will be called once at the beginning of every frame. This is the
// best location for your application to handle updates to the scene, but is not 
// intended to contain actual rendering calls, which should instead be placed in the 
// OnFrameRender callback.  
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );    
}


//--------------------------------------------------------------------------------------
// Before handling window messages, DXUT passes incoming windows 
// messages to the application through this callback function. If the application sets 
// *pbNoFurtherProcessing to TRUE, then DXUT will not process this message.
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

    // Pass all windows messages to camera so it can respond to user input
    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
}


//--------------------------------------------------------------------------------------
void CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
    if( bKeyDown )
    {
        switch( nChar )
        {
        case VK_F1:
            g_bShowSampleUI = !g_bShowSampleUI; break;        
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
    case IDC_SHOWTESSELLATED:
        g_bShowTessellated = !g_bShowTessellated;
        break;

    case IDC_PARTITIONING_INTEGER:
        g_Tessellator.SetPartitioningMode( CTessellator::PARTITIONING_MODE_INTEGER );
        break;

    case IDC_PARTITIONING_POW2:
        g_Tessellator.SetPartitioningMode( CTessellator::PARTITIONING_MODE_POW2 );
        break;

    case IDC_PARTITIONING_FRACTIONAL_ODD:
        g_Tessellator.SetPartitioningMode( CTessellator::PARTITIONING_MODE_FRACTIONAL_ODD );
        break;

    case IDC_PARTITIONING_FRACTIONAL_EVEN:
        g_Tessellator.SetPartitioningMode( CTessellator::PARTITIONING_MODE_FRACTIONAL_EVEN );
        break;
    }
}


//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                      DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    // reject any device which doesn't support CS4x
    if ( DeviceInfo->ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x == FALSE )
        return false;

    return true;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice(ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
    void* pUserContext)
{
    HRESULT hr;

    static bool bFirstOnCreateDevice = true;

    // Warn the user that in order to support CS4x, a non-hardware device has been created, continue or quit?
    if (DXUTGetDeviceSettings().d3d11.DriverType != D3D_DRIVER_TYPE_HARDWARE && bFirstOnCreateDevice)
    {
        if (MessageBox(0, L"CS4x capability is missing. "\
            L"In order to continue, a non-hardware device has been created, "\
            L"it will be very slow, continue?", L"Warning", MB_ICONEXCLAMATION | MB_YESNO) != IDYES)
            return E_FAIL;
    }

    bFirstOnCreateDevice = false;

    auto pd3dImmediateContext = DXUTGetD3D11DeviceContext();
    V_RETURN(g_DialogResourceManager.OnD3D11CreateDevice(pd3dDevice, pd3dImmediateContext));
    V_RETURN(g_D3DSettingsDlg.OnD3D11CreateDevice(pd3dDevice));
    V_RETURN(g_Tessellator.OnD3D11CreateDevice(pd3dDevice));
    g_pTxtHelper = new CDXUTTextHelper(pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15);

    // find the file
    WCHAR str[MAX_PATH];
    V_RETURN(DXUTFindDXSDKMediaFileCch(str, MAX_PATH, L"BaseMesh.obj"));

    std::wifstream ifs(str);
    WCHAR line[256] = { 0 };
    std::vector<XMFLOAT4> initdata;

    // Parse the .obj file. Both triangle faces and quad faces are supported.
    // Only v and f tags are processed, other tags like vn, vt etc are ignored.
    {
        std::vector<XMFLOAT4> v;

        while (ifs >> line)
        {
            if (0 == wcscmp(line, L"#"))
                ifs.getline(line, 255);
            else
                if (0 == wcscmp(line, L"v"))
                {
                    XMFLOAT4 pos;
                    ifs >> pos.x >> pos.y >> pos.z;
                    pos.w = 1;
                    v.push_back(pos);
                }
        }

        ifs.clear(0);
        ifs.seekg(0);
        while (ifs >> line)
        {
            if (0 == wcscmp(line, L"#"))
                ifs.getline(line, 255);
            else
                if (0 == wcscmp(line, L"f"))
                {
                    ifs.getline(line, 255);
                    std::wstringstream ss(line);
                    int idx[4] = { 0 }, i = 0;
                    while (ss >> line)
                    {
                        std::wstringstream ss2(line);
                        ss2 >> idx[i++];
                    }

                    initdata.push_back(v[idx[0] - 1]); initdata.push_back(v[idx[1] - 1]); initdata.push_back(v[idx[2] - 1]);
                    if (i >= 4) // quad face?
                    {
                        initdata.push_back(v[idx[2] - 1]); initdata.push_back(v[idx[3] - 1]); initdata.push_back(v[idx[0] - 1]);
                    }
                }
        }
    }

    D3D11_BUFFER_DESC desc;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.ByteWidth = static_cast<UINT>(sizeof(initdata[0]) * initdata.size());
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    D3D11_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = &initdata[0];
    V_RETURN(pd3dDevice->CreateBuffer(&desc, &InitData, &g_pBaseVB));
    DXUT_SetDebugName(g_pBaseVB, "Primary");

    g_Tessellator.SetBaseMesh(pd3dDevice, pd3dImmediateContext, static_cast<UINT>(initdata.size()), g_pBaseVB);

    ID3DBlob* pBlob = nullptr;
    V_RETURN(DXUTCompileFromFile(L"Render.hlsl", nullptr, "RenderVS", "vs_4_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &pBlob));
    V_RETURN(pd3dDevice->CreateVertexShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), nullptr, &g_pVS));
    SAFE_RELEASE(pBlob);
    DXUT_SetDebugName(g_pVS, "RenderVS");

    V_RETURN(DXUTCompileFromFile(L"Render.hlsl", nullptr, "RenderBaseVS", "vs_4_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &pBlob));
    V_RETURN(pd3dDevice->CreateVertexShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), nullptr, &g_pBaseVS));
    DXUT_SetDebugName(g_pBaseVS, "RenderBaseVS");

    {
        D3D11_INPUT_ELEMENT_DESC layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        UINT numElements = sizeof(layout) / sizeof(layout[0]);
        V_RETURN(pd3dDevice->CreateInputLayout(layout, numElements, pBlob->GetBufferPointer(), pBlob->GetBufferSize(), &g_pBaseVBLayout));
        DXUT_SetDebugName(g_pBaseVBLayout, "Primary");
    }

    SAFE_RELEASE(pBlob);

    V_RETURN(DXUTCompileFromFile(L"Render.hlsl", nullptr, "RenderPS", "ps_4_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &pBlob));
    V_RETURN(pd3dDevice->CreatePixelShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), nullptr, &g_pPS));
    SAFE_RELEASE(pBlob);
    DXUT_SetDebugName(g_pPS, "RenderPS");

    // Setup constant buffer
    D3D11_BUFFER_DESC Desc;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Desc.MiscFlags = 0;
    Desc.ByteWidth = sizeof(XMFLOAT4X4);
    V_RETURN(pd3dDevice->CreateBuffer(&Desc, nullptr, &g_pVSCB));
    DXUT_SetDebugName(g_pVSCB, "XMMATRIX");

    // Rasterizer state
    D3D11_RASTERIZER_DESC descRast = {};
    descRast.CullMode = D3D11_CULL_NONE;
    descRast.FillMode = D3D11_FILL_WIREFRAME;
    V_RETURN( pd3dDevice->CreateRasterizerState( &descRast, &g_pRasWireFrame ) );
    DXUT_SetDebugName( g_pRasWireFrame, "WireFrame" );

    // Setup the camera's view parameters
    static const XMVECTORF32 s_vecEye = { 0.f, 0.f, -300.f, 0.f };
    static const XMVECTORF32 s_vecAt = { 10.0f, 20.0f, 0.0f, 0.f };
    g_Camera.SetViewParams( s_vecEye, s_vecAt );

    g_Camera.SetScalers( 0.005f, 50 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                         const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_Tessellator.OnD3D11ResizedSwapChain( pBackBufferSurfaceDesc ) );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( XM_PI / 4, fAspectRatio, 1.0f, 500000.0f );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 260, pBackBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 170, 300 );

    return hr;
}


//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11ReleasingSwapChain();    
}


//--------------------------------------------------------------------------------------
void RenderText()
{
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 2, 0 );
    g_pTxtHelper->SetForegroundColor( Colors::Yellow );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );

    if ( g_bShowSampleUI )
    {
        auto pBackBufferSurfaceDesc = DXUTGetDXGIBackBufferSurfaceDesc();
        g_pTxtHelper->SetInsertionPos( 2, pBackBufferSurfaceDesc->Height - 18 * 6 );
        g_pTxtHelper->SetForegroundColor( Colors::Orange );
        g_pTxtHelper->DrawTextLine( L"Controls (F1 to hide):" );

        g_pTxtHelper->SetInsertionPos( 20, pBackBufferSurfaceDesc->Height - 18 * 5 );
        g_pTxtHelper->DrawTextLine( L"Look: Left drag mouse\n"
                                    L"Move: A,W,S,D or Arrow Keys\n"
                                    L"Move up/down: Q,E or PgUp,PgDn\n"
                                    L"Reset camera: Home\n" );
    } else
    {
        g_pTxtHelper->SetForegroundColor( Colors::White );
        g_pTxtHelper->DrawTextLine( L"Press F1 for sample UI" );
    }

    g_pTxtHelper->End();
}


//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                 float fElapsedTime, void* pUserContext )
{
    HRESULT hr = S_OK;
    
    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }       

    auto pRTV = DXUTGetD3D11RenderTargetView();
    pd3dImmediateContext->ClearRenderTargetView( pRTV, Colors::MidnightBlue );
    auto pDSV = DXUTGetD3D11DepthStencilView();
    pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );

    // Get the projection & view matrix from the camera class
    XMMATRIX mView = g_Camera.GetViewMatrix();
    XMMATRIX mProj = g_Camera.GetProjMatrix();
    XMMATRIX mWorldViewProjection = mView * mProj;

    if ( g_bShowTessellated )
    {
        DWORD num_tessed_vertices = 0;
        DWORD num_tessed_indices = 0;
        g_Tessellator.PerEdgeTessellation( mWorldViewProjection, &g_pTessedVB, &g_pTessedIB, &num_tessed_vertices, &num_tessed_indices );

        // render tessellated mesh
        if ( num_tessed_vertices > 0 && num_tessed_indices > 0 )
        {
            pd3dImmediateContext->RSSetState( g_pRasWireFrame );

            pd3dImmediateContext->VSSetShader( g_pVS, nullptr, 0 );
            pd3dImmediateContext->PSSetShader( g_pPS, nullptr, 0 );

            D3D11_MAPPED_SUBRESOURCE MappedResource;
            V( pd3dImmediateContext->Map( g_pVSCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
            XMStoreFloat4x4( reinterpret_cast<XMFLOAT4X4*>( MappedResource.pData ), mWorldViewProjection );
            pd3dImmediateContext->Unmap( g_pVSCB, 0 );
            pd3dImmediateContext->VSSetConstantBuffers( 0, 1, &g_pVSCB );

            ID3D11ShaderResourceView* aRViews[2] = { g_Tessellator.m_pBaseVBSRV, g_Tessellator.m_pTessedVerticesBufSRV };
            pd3dImmediateContext->VSSetShaderResources( 0, 2, aRViews );

            pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);        
            pd3dImmediateContext->IASetIndexBuffer(g_pTessedIB, DXGI_FORMAT_R32_UINT, 0);
            pd3dImmediateContext->DrawIndexed(num_tessed_indices, 0, 0);

            ID3D11ShaderResourceView* ppSRVNULL[2] = { nullptr, nullptr };
            pd3dImmediateContext->VSSetShaderResources( 0, 2, ppSRVNULL );
            pd3dImmediateContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
        }
    } else
    {
        // render original mesh
        pd3dImmediateContext->RSSetState( g_pRasWireFrame );

        pd3dImmediateContext->VSSetShader( g_pBaseVS, nullptr, 0 );
        pd3dImmediateContext->PSSetShader( g_pPS, nullptr, 0 );

        D3D11_MAPPED_SUBRESOURCE MappedResource;
        V( pd3dImmediateContext->Map( g_pVSCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
        XMStoreFloat4x4( reinterpret_cast<XMFLOAT4X4*>( MappedResource.pData ), mWorldViewProjection );
        pd3dImmediateContext->Unmap( g_pVSCB, 0 );
        pd3dImmediateContext->VSSetConstantBuffers( 0, 1, &g_pVSCB );

        pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pd3dImmediateContext->IASetInputLayout( g_pBaseVBLayout );
        ID3D11Buffer* vbs[] = { g_pBaseVB };
        UINT strides[] = { sizeof(XMFLOAT4) };
        UINT offsets[] = { 0 };
        pd3dImmediateContext->IASetVertexBuffers( 0, 1, vbs, strides, offsets );

        pd3dImmediateContext->Draw( g_Tessellator.m_nVertices, 0 );

        ID3D11Buffer* vbsnull[] = { nullptr };
        pd3dImmediateContext->IASetVertexBuffers( 0, 1, vbsnull, strides, offsets );
    }    

    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
    g_HUD.OnRender( fElapsedTime );
    if ( g_bShowSampleUI )
        g_SampleUI.OnRender( fElapsedTime );
    RenderText();
    DXUT_EndPerfEvent();    
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11DestroyDevice();
    g_D3DSettingsDlg.OnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    g_Tessellator.OnDestroyDevice();
    SAFE_DELETE( g_pTxtHelper );    

    SAFE_RELEASE( g_pBaseVBLayout );
    SAFE_RELEASE( g_pBaseVB );
    SAFE_RELEASE( g_pTessedVB );
    SAFE_RELEASE( g_pTessedIB );
    SAFE_RELEASE( g_pVSCB );
    SAFE_RELEASE( g_pVS );
    SAFE_RELEASE( g_pBaseVS );
    SAFE_RELEASE( g_pPS );
    SAFE_RELEASE( g_pRasWireFrame );
}
