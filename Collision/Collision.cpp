//--------------------------------------------------------------------------------------
// File: Collision.cpp
//
// Sample demonstrating DirectXMath's collision types using Direct3D 11, DXUT,
// and DirectXTK
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License (MIT).
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "DXUTgui.h"
#include "DXUTmisc.h"
#include "DXUTCamera.h"
#include "DXUTSettingsDlg.h"
#include "SDKmisc.h"
#include "resource.h"

#include <DirectXColors.h>
#include <DirectXCollision.h>

#include "CommonStates.h"
#include "Effects.h"
#include "PrimitiveBatch.h"
#include "VertexTypes.h"

#pragma warning( disable : 4100 )

using namespace DirectX;

//--------------------------------------------------------------------------------------
// Types
//--------------------------------------------------------------------------------------

// Collision objects
struct CollisionSphere
{
    BoundingSphere sphere;
    ContainmentType collision;
};

struct CollisionBox
{
    BoundingOrientedBox obox;
    ContainmentType collision;
};

struct CollisionAABox
{
    BoundingBox aabox;
    ContainmentType collision;
};

struct CollisionFrustum
{
    BoundingFrustum frustum;
    ContainmentType collision;
};

struct CollisionTriangle
{
    XMVECTOR pointa;
    XMVECTOR pointb;
    XMVECTOR pointc;
    ContainmentType collision;
};

struct CollisionRay
{
    XMVECTOR origin;
    XMVECTOR direction;
};

//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------
const int GROUP_COUNT = 4;
const int CAMERA_COUNT = 4;

const float CAMERA_SPACING = 50.f;

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CModelViewerCamera          g_Camera;               // A model viewing camera
CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg             g_SettingsDlg;          // Device settings dialog
CDXUTTextHelper*            g_pTxtHelper = nullptr;
CDXUTDialog                 g_HUD;                  // dialog for standard controls
CDXUTDialog                 g_SampleUI;             // dialog for sample specific controls

ID3D11InputLayout*                  g_pBatchInputLayout = nullptr;

std::unique_ptr<CommonStates>                           g_States;
std::unique_ptr<BasicEffect>                            g_BatchEffect;
std::unique_ptr<PrimitiveBatch<VertexPositionColor>>    g_Batch;

// Primary collision objects
BoundingFrustum g_PrimaryFrustum;
BoundingOrientedBox g_PrimaryOrientedBox;
BoundingBox g_PrimaryAABox;
CollisionRay g_PrimaryRay;

// Secondary collision objects
CollisionSphere     g_SecondarySpheres[GROUP_COUNT];
CollisionBox        g_SecondaryOrientedBoxes[GROUP_COUNT];
CollisionAABox      g_SecondaryAABoxes[GROUP_COUNT];
CollisionTriangle   g_SecondaryTriangles[GROUP_COUNT];

// Ray testing results display object
CollisionAABox g_RayHitResultBox;

// Camera preset locations
XMVECTOR g_CameraOrigins[CAMERA_COUNT];

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_STATIC              -1
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           2
#define IDC_CHANGEDEVICE        3
#define IDC_TOGGLEWARP          4
#define IDC_GROUP               5

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

void InitializeObjects();
void Animate( double fTime );
void Collide();
void RenderObjects();
void SetViewForGroup( int group );

void DrawGrid( FXMVECTOR xAxis, FXMVECTOR yAxis, FXMVECTOR origin, size_t xdivs, size_t ydivs, GXMVECTOR color );
void DrawFrustum( const BoundingFrustum& frustum, FXMVECTOR color );
void DrawAabb( const BoundingBox& box, FXMVECTOR color );
void DrawObb( const BoundingOrientedBox& obb, FXMVECTOR color );
void DrawSphere( const BoundingSphere& sphere, FXMVECTOR color );
void DrawRay( FXMVECTOR Origin, FXMVECTOR Direction, bool bNormalize, FXMVECTOR color );
void DrawTriangle( FXMVECTOR PointA, FXMVECTOR PointB, FXMVECTOR PointC, CXMVECTOR color );

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

    // DirectXMath uses SSE/SSE2 instructions on Windows. We should verify the CPU supports these instructions
    // as early in the program as possible
    if ( !XMVerifyCPUSupport() )
    {
        MessageBox( NULL, TEXT("This application requires the processor support SSE2 instructions."),
                    TEXT("Collision"), MB_OK | MB_ICONEXCLAMATION);
        return -1;
    }

    // DXUT will create and use the best device
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
    DXUTInit( true, true, nullptr ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true );
    DXUTCreateWindow( L"Collision for DirectXMath" );

    // Only require 10-level hardware, change to D3D_FEATURE_LEVEL_11_0 to require 11-class hardware
    // Switch to D3D_FEATURE_LEVEL_9_x for 10level9 hardware
    DXUTCreateDevice( D3D_FEATURE_LEVEL_10_0, true, 800, 600 );

    DXUTMainLoop(); // Enter into the DXUT render loop

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

    CDXUTComboBox* pComboBox = nullptr;
    g_SampleUI.AddStatic( IDC_STATIC, L"(G)roup", 10, 0, 170, 25 );
    g_SampleUI.AddComboBox( IDC_GROUP, 0, 25, 170, 24, 'G', false, &pComboBox );
    if( pComboBox )
        pComboBox->SetDropHeight( 50 );

    pComboBox->AddItem( L"Frustum", IntToPtr( 0 ) );
    pComboBox->AddItem( L"Axis-aligned Box", IntToPtr( 1 ) );
    pComboBox->AddItem( L"Oriented Box", IntToPtr( 2 ) );
    pComboBox->AddItem( L"Ray", IntToPtr( 3 ) );

    InitializeObjects();
}


//--------------------------------------------------------------------------------------
// Render the help and statistics text.
//--------------------------------------------------------------------------------------
void RenderText()
{
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 5, 5 );
    g_pTxtHelper->SetForegroundColor( Colors::Yellow );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );
    g_pTxtHelper->End();
}


//--------------------------------------------------------------------------------------
// Initialize the starting positions of the collision objects
//--------------------------------------------------------------------------------------
void InitializeObjects()
{
    const XMVECTOR XMZero = XMVectorZero();

    // Set up the primary frustum object from a D3D projection matrix
    // NOTE: This can also be done on your camera's projection matrix.  The projection
    // matrix built here is somewhat contrived so it renders well.
    XMMATRIX xmProj = XMMatrixPerspectiveFovLH( XM_PIDIV4, 1.77778f, 0.5f, 10.0f );
    BoundingFrustum::CreateFromMatrix( g_PrimaryFrustum, xmProj );
    g_PrimaryFrustum.Origin.z = -7.0f;
    g_CameraOrigins[0] = XMVectorSet( 0, 0, 0, 0 );

    // Set up the primary axis aligned box
    g_PrimaryAABox.Center = XMFLOAT3( CAMERA_SPACING, 0, 0 );
    g_PrimaryAABox.Extents = XMFLOAT3( 5, 5, 5 );
    g_CameraOrigins[1] = XMVectorSet( CAMERA_SPACING, 0, 0, 0 );

    // Set up the primary oriented box with some rotation
    g_PrimaryOrientedBox.Center = XMFLOAT3( -CAMERA_SPACING, 0, 0 );
    g_PrimaryOrientedBox.Extents = XMFLOAT3( 5, 5, 5 );
    XMStoreFloat4( &g_PrimaryOrientedBox.Orientation, XMQuaternionRotationRollPitchYaw( XM_PIDIV4, XM_PIDIV4, 0 ) );
    g_CameraOrigins[2] = XMVectorSet( -CAMERA_SPACING, 0, 0, 0 );

    // Set up the primary ray
    g_PrimaryRay.origin = XMVectorSet( 0, 0, CAMERA_SPACING, 0 );
    g_PrimaryRay.direction = g_XMIdentityR2;
    g_CameraOrigins[3] = XMVectorSet( 0, 0, CAMERA_SPACING, 0 );

    // Initialize all of the secondary objects with default values
    for( UINT i = 0; i < GROUP_COUNT; i++ )
    {
        g_SecondarySpheres[i].sphere.Radius = 1.0f;
        g_SecondarySpheres[i].sphere.Center = XMFLOAT3( 0, 0, 0 );
        g_SecondarySpheres[i].collision = DISJOINT;

        g_SecondaryOrientedBoxes[i].obox.Center = XMFLOAT3( 0, 0, 0 );
        g_SecondaryOrientedBoxes[i].obox.Extents = XMFLOAT3( 0.5f, 0.5f, 0.5f );
        g_SecondaryOrientedBoxes[i].obox.Orientation = XMFLOAT4( 0, 0, 0, 1 );
        g_SecondaryOrientedBoxes[i].collision = DISJOINT;

        g_SecondaryAABoxes[i].aabox.Center = XMFLOAT3( 0, 0, 0 );
        g_SecondaryAABoxes[i].aabox.Extents = XMFLOAT3( 0.5f, 0.5f, 0.5f );
        g_SecondaryAABoxes[i].collision = DISJOINT;

        g_SecondaryTriangles[i].pointa = XMZero;
        g_SecondaryTriangles[i].pointb = XMZero;
        g_SecondaryTriangles[i].pointc = XMZero;
        g_SecondaryTriangles[i].collision = DISJOINT;
    }

    // Set up ray hit result box
    g_RayHitResultBox.aabox.Center = XMFLOAT3( 0, 0, 0 );
    g_RayHitResultBox.aabox.Extents = XMFLOAT3( 0.05f, 0.05f, 0.05f );
}


//--------------------------------------------------------------------------------------
// Move objects around over time
//--------------------------------------------------------------------------------------
void Animate( double fTime )
{
    float t = ( FLOAT )(fTime * 0.2);

    const float camera0OriginX = XMVectorGetX( g_CameraOrigins[0] );
    const float camera1OriginX = XMVectorGetX( g_CameraOrigins[1] );
    const float camera2OriginX = XMVectorGetX( g_CameraOrigins[2] );
    const float camera3OriginX = XMVectorGetX( g_CameraOrigins[3] );
    const float camera3OriginZ = XMVectorGetZ( g_CameraOrigins[3] );

    // animate sphere 0 around the frustum
    g_SecondarySpheres[0].sphere.Center.x = 10 * sinf( 3 * t );
    g_SecondarySpheres[0].sphere.Center.y = 7 * cosf( 5 * t );

    // animate oriented box 0 around the frustum
    g_SecondaryOrientedBoxes[0].obox.Center.x = 8 * sinf( 3.5f * t );
    g_SecondaryOrientedBoxes[0].obox.Center.y = 5 * cosf( 5.1f * t );
    XMStoreFloat4( &( g_SecondaryOrientedBoxes[0].obox.Orientation ), XMQuaternionRotationRollPitchYaw( t * 1.4f,
                                                                                                          t * 0.2f,
                                                                                                          t ) );

    // animate aligned box 0 around the frustum
    g_SecondaryAABoxes[0].aabox.Center.x = 10 * sinf( 2.1f * t );
    g_SecondaryAABoxes[0].aabox.Center.y = 7 * cosf( 3.8f * t );

    // animate sphere 1 around the aligned box
    g_SecondarySpheres[1].sphere.Center.x = 8 * sinf( 2.9f * t ) + camera1OriginX;
    g_SecondarySpheres[1].sphere.Center.y = 8 * cosf( 4.6f * t );
    g_SecondarySpheres[1].sphere.Center.z = 8 * cosf( 1.6f * t );
  
    // animate oriented box 1 around the aligned box
    g_SecondaryOrientedBoxes[1].obox.Center.x = 8 * sinf( 3.2f * t ) + camera1OriginX;
    g_SecondaryOrientedBoxes[1].obox.Center.y = 8 * cosf( 2.1f * t );
    g_SecondaryOrientedBoxes[1].obox.Center.z = 8 * sinf( 1.6f * t );
    XMStoreFloat4( &( g_SecondaryOrientedBoxes[1].obox.Orientation ), XMQuaternionRotationRollPitchYaw( t * 0.7f,
                                                                                                          t * 1.3f,
                                                                                                          t ) );

    // animate aligned box 1 around the aligned box
    g_SecondaryAABoxes[1].aabox.Center.x = 8 * sinf( 1.1f * t ) + camera1OriginX;
    g_SecondaryAABoxes[1].aabox.Center.y = 8 * cosf( 5.8f * t );
    g_SecondaryAABoxes[1].aabox.Center.z = 8 * cosf( 3.0f * t );

    // animate sphere 2 around the oriented box
    g_SecondarySpheres[2].sphere.Center.x = 8 * sinf( 2.2f * t ) + camera2OriginX;
    g_SecondarySpheres[2].sphere.Center.y = 8 * cosf( 4.3f * t );
    g_SecondarySpheres[2].sphere.Center.z = 8 * cosf( 1.8f * t );

    // animate oriented box 2 around the oriented box
    g_SecondaryOrientedBoxes[2].obox.Center.x = 8 * sinf( 3.7f * t ) + camera2OriginX;
    g_SecondaryOrientedBoxes[2].obox.Center.y = 8 * cosf( 2.5f * t );
    g_SecondaryOrientedBoxes[2].obox.Center.z = 8 * sinf( 1.1f * t );
    XMStoreFloat4( &( g_SecondaryOrientedBoxes[2].obox.Orientation ), XMQuaternionRotationRollPitchYaw( t * 0.9f,
                                                                                                          t * 1.8f,
                                                                                                          t ) );

    // animate aligned box 2 around the oriented box
    g_SecondaryAABoxes[2].aabox.Center.x = 8 * sinf( 1.3f * t ) + camera2OriginX;
    g_SecondaryAABoxes[2].aabox.Center.y = 8 * cosf( 5.2f * t );
    g_SecondaryAABoxes[2].aabox.Center.z = 8 * cosf( 3.5f * t );

    // triangle points in local space - equilateral triangle with radius of 2
    const XMVECTOR TrianglePointA = { 0, 2, 0, 0 };
    const XMVECTOR TrianglePointB = { 1.732f, -1, 0, 0 };
    const XMVECTOR TrianglePointC = { -1.732f, -1, 0, 0 };

    // animate triangle 0 around the frustum
    XMMATRIX TriangleCoords = XMMatrixRotationRollPitchYaw( t * 1.4f, t * 2.5f, t );
    XMMATRIX Translation = XMMatrixTranslation( 5 * sinf( 5.3f * t ) + camera0OriginX,
                                                5 * cosf( 2.3f * t ),
                                                5 * sinf( 3.4f * t ) );
    TriangleCoords = XMMatrixMultiply( TriangleCoords, Translation );
    g_SecondaryTriangles[0].pointa = XMVector3Transform( TrianglePointA, TriangleCoords );
    g_SecondaryTriangles[0].pointb = XMVector3Transform( TrianglePointB, TriangleCoords );
    g_SecondaryTriangles[0].pointc = XMVector3Transform( TrianglePointC, TriangleCoords );

    // animate triangle 1 around the aligned box
    TriangleCoords = XMMatrixRotationRollPitchYaw( t * 1.4f, t * 2.5f, t );
    Translation = XMMatrixTranslation( 8 * sinf( 5.3f * t ) + camera1OriginX,
                                       8 * cosf( 2.3f * t ),
                                       8 * sinf( 3.4f * t ) );
    TriangleCoords = XMMatrixMultiply( TriangleCoords, Translation );
    g_SecondaryTriangles[1].pointa = XMVector3Transform( TrianglePointA, TriangleCoords );
    g_SecondaryTriangles[1].pointb = XMVector3Transform( TrianglePointB, TriangleCoords );
    g_SecondaryTriangles[1].pointc = XMVector3Transform( TrianglePointC, TriangleCoords );

    // animate triangle 2 around the oriented box
    TriangleCoords = XMMatrixRotationRollPitchYaw( t * 1.4f, t * 2.5f, t );
    Translation = XMMatrixTranslation( 8 * sinf( 5.3f * t ) + camera2OriginX,
                                       8 * cosf( 2.3f * t ),
                                       8 * sinf( 3.4f * t ) );
    TriangleCoords = XMMatrixMultiply( TriangleCoords, Translation );
    g_SecondaryTriangles[2].pointa = XMVector3Transform( TrianglePointA, TriangleCoords );
    g_SecondaryTriangles[2].pointb = XMVector3Transform( TrianglePointB, TriangleCoords );
    g_SecondaryTriangles[2].pointc = XMVector3Transform( TrianglePointC, TriangleCoords );

    // animate primary ray (this is the only animated primary object)
    g_PrimaryRay.direction = XMVectorSet( sinf( t * 3 ), 0, cosf( t * 3 ), 0 );

    // animate sphere 3 around the ray
    g_SecondarySpheres[3].sphere.Center = XMFLOAT3( camera3OriginX - 3,
                                                      0.5f * sinf( t * 5 ),
                                                      camera3OriginZ );

    // animate aligned box 3 around the ray
    g_SecondaryAABoxes[3].aabox.Center = XMFLOAT3( camera3OriginX + 3,
                                                     0.5f * sinf( t * 4 ),
                                                     camera3OriginZ );

    // animate oriented box 3 around the ray
    g_SecondaryOrientedBoxes[3].obox.Center = XMFLOAT3( camera3OriginX,
                                                          0.5f * sinf( t * 4.5f ),
                                                          camera3OriginZ + 3 );
    XMStoreFloat4( &( g_SecondaryOrientedBoxes[3].obox.Orientation ), XMQuaternionRotationRollPitchYaw( t * 0.9f,
                                                                                                          t * 1.8f,
                                                                                                          t ) );

    // animate triangle 3 around the ray
    TriangleCoords = XMMatrixRotationRollPitchYaw( t * 1.4f, t * 2.5f, t );
    Translation = XMMatrixTranslation( camera3OriginX,
                                       0.5f * cosf( 4.3f * t ),
                                       camera3OriginZ - 3 );
    TriangleCoords = XMMatrixMultiply( TriangleCoords, Translation );
    g_SecondaryTriangles[3].pointa = XMVector3Transform( TrianglePointA, TriangleCoords );
    g_SecondaryTriangles[3].pointb = XMVector3Transform( TrianglePointB, TriangleCoords );
    g_SecondaryTriangles[3].pointc = XMVector3Transform( TrianglePointC, TriangleCoords );
}


//--------------------------------------------------------------------------------------
// Test collisions between pairs of collision objects using XNACollision functions
//--------------------------------------------------------------------------------------
void Collide()
{
    // test collisions between objects and frustum
    g_SecondarySpheres[0].collision = g_PrimaryFrustum.Contains( g_SecondarySpheres[0].sphere );
    g_SecondaryOrientedBoxes[0].collision = g_PrimaryFrustum.Contains( g_SecondaryOrientedBoxes[0].obox );
    g_SecondaryAABoxes[0].collision = g_PrimaryFrustum.Contains( g_SecondaryAABoxes[0].aabox );
    g_SecondaryTriangles[0].collision = g_PrimaryFrustum.Contains( g_SecondaryTriangles[0].pointa,
                                                                   g_SecondaryTriangles[0].pointb,
                                                                   g_SecondaryTriangles[0].pointc );

    // test collisions between objects and aligned box
    g_SecondarySpheres[1].collision = g_PrimaryAABox.Contains( g_SecondarySpheres[1].sphere );
    g_SecondaryOrientedBoxes[1].collision = g_PrimaryAABox.Contains( g_SecondaryOrientedBoxes[1].obox );
    g_SecondaryAABoxes[1].collision = g_PrimaryAABox.Contains( g_SecondaryAABoxes[1].aabox );
    g_SecondaryTriangles[1].collision = g_PrimaryAABox.Contains( g_SecondaryTriangles[1].pointa,
                                                                 g_SecondaryTriangles[1].pointb,
                                                                 g_SecondaryTriangles[1].pointc );

    // test collisions between objects and oriented box
    g_SecondarySpheres[2].collision = g_PrimaryOrientedBox.Contains( g_SecondarySpheres[2].sphere );
    g_SecondaryOrientedBoxes[2].collision = g_PrimaryOrientedBox.Contains( g_SecondaryOrientedBoxes[2].obox );
    g_SecondaryAABoxes[2].collision = g_PrimaryOrientedBox.Contains( g_SecondaryAABoxes[2].aabox );
    g_SecondaryTriangles[2].collision = g_PrimaryOrientedBox.Contains( g_SecondaryTriangles[2].pointa,
                                                                       g_SecondaryTriangles[2].pointb,
                                                                       g_SecondaryTriangles[2].pointc );

    // test collisions between objects and ray
    float fDistance = -1.0f;

    float fDist;
    if ( g_SecondarySpheres[3].sphere.Intersects( g_PrimaryRay.origin, g_PrimaryRay.direction, fDist ) )
    {
        fDistance = fDist;
        g_SecondarySpheres[3].collision = INTERSECTS;
    }
    else
        g_SecondarySpheres[3].collision = DISJOINT;

    if ( g_SecondaryOrientedBoxes[3].obox.Intersects( g_PrimaryRay.origin, g_PrimaryRay.direction, fDist ) )
    {
        fDistance = fDist;
        g_SecondaryOrientedBoxes[3].collision = INTERSECTS;
    }
    else
        g_SecondaryOrientedBoxes[3].collision = DISJOINT;

    if ( g_SecondaryAABoxes[3].aabox.Intersects( g_PrimaryRay.origin, g_PrimaryRay.direction, fDist ) )
    {
        fDistance = fDist;
        g_SecondaryAABoxes[3].collision =  INTERSECTS;
    }
    else
        g_SecondaryAABoxes[3].collision =  DISJOINT;

    if ( TriangleTests::Intersects( g_PrimaryRay.origin, g_PrimaryRay.direction,
                                    g_SecondaryTriangles[3].pointa,
                                    g_SecondaryTriangles[3].pointb,
                                    g_SecondaryTriangles[3].pointc,
                                    fDist ) )
    {
        fDistance = fDist;
        g_SecondaryTriangles[3].collision = INTERSECTS;
    }
    else
        g_SecondaryTriangles[3].collision = DISJOINT;

    // If one of the ray intersection tests was successful, fDistance will be positive.
    // If so, compute the intersection location and store it in g_RayHitResultBox.
    if( fDistance > 0 )
    {
        // The primary ray's direction is assumed to be normalized.
        XMVECTOR HitLocation = XMVectorMultiplyAdd( g_PrimaryRay.direction, XMVectorReplicate( fDistance ),
                                                    g_PrimaryRay.origin );
        XMStoreFloat3( &g_RayHitResultBox.aabox.Center, HitLocation );
        g_RayHitResultBox.collision = INTERSECTS;
    }
    else
    {
        g_RayHitResultBox.collision = DISJOINT;
    }
}


//--------------------------------------------------------------------------------------
// Returns the color based on the collision result and the gruop number.
// Frustum tests (group 0) return 0, 1, or 2 for outside, partially inside, and fully inside;
// all other tests return 0 or 1 for no collision or collision.
//--------------------------------------------------------------------------------------
inline XMVECTOR GetCollisionColor( ContainmentType collision, int groupnumber )
{
    // special case: a value of 1 for groups 1 and higher needs to register as a full collision
    if( groupnumber >= 3 && collision > 0 )
        collision = CONTAINS;

    switch( collision )
    {
    case DISJOINT:      return Colors::Green;
    case INTERSECTS:    return Colors::Yellow;
    case CONTAINS:
    default:            return Colors::Red;
    }
}


//--------------------------------------------------------------------------------------
// Renders collision objects
//--------------------------------------------------------------------------------------
void RenderObjects()
{
    // Draw ground planes
    for( int i = 0; i < CAMERA_COUNT; ++i )
    {
        static const XMVECTORF32 s_vXAxis = { 20.f, 0.f, 0.f, 0.f };
        static const XMVECTORF32 s_vYAxis = { 0.f, 0.f, 20.f, 0.f };

        static const XMVECTORF32 s_Offset = { 0.f, 10.f, 0.f, 0.f };
        XMVECTOR vOrigin = g_CameraOrigins[i] - s_Offset;

        const int iXDivisions = 20;
        const int iYDivisions = 20;
        DrawGrid( s_vXAxis, s_vYAxis, vOrigin, iXDivisions, iYDivisions, Colors::Black );
    }

    // Draw primary collision objects in white
    DrawFrustum( g_PrimaryFrustum, Colors::White );
    DrawAabb( g_PrimaryAABox, Colors::White );
    DrawObb( g_PrimaryOrientedBox, Colors::White );

    {
        XMVECTOR Direction = XMVectorScale( g_PrimaryRay.direction, 10.0f );
        DrawRay( g_PrimaryRay.origin, Direction, false, Colors::LightGray );
        DrawRay( g_PrimaryRay.origin, Direction, false, Colors::White);
    }

    // Draw secondary collision objects in colors based on collision results
    for( int i = 0; i < GROUP_COUNT; ++i )
    {
        const CollisionSphere& sphere = g_SecondarySpheres[i];
        XMVECTOR c = GetCollisionColor( sphere.collision, i );
        DrawSphere( sphere.sphere, c );

        const CollisionBox& obox = g_SecondaryOrientedBoxes[i];
        c = GetCollisionColor( obox.collision, i );
        DrawObb( obox.obox, c );

        const CollisionAABox& aabox = g_SecondaryAABoxes[i];
        c = GetCollisionColor( aabox.collision, i );
        DrawAabb( aabox.aabox, c );

        const CollisionTriangle& tri = g_SecondaryTriangles[i];
        c = GetCollisionColor( tri.collision, i );
        DrawTriangle( tri.pointa, tri.pointb, tri.pointc, c );
    }

    // Draw results of ray-object intersection, if there was a hit this frame
    if( g_RayHitResultBox.collision != DISJOINT )
        DrawAabb( g_RayHitResultBox.aabox, Colors::Yellow );
}


//--------------------------------------------------------------------------------------
// Sets the camera to view a particular group of objects
//--------------------------------------------------------------------------------------
void SetViewForGroup( int group )
{
    assert( group < GROUP_COUNT );

    g_Camera.Reset();

    static const XMVECTORF32 s_Offset0 = { 0.f, 20.f, 20.f, 0.f };
    static const XMVECTORF32 s_Offset = { 0.f, 20.f, -20.f, 0.f };
    XMVECTOR vecEye = g_CameraOrigins[group] + ( ( group == 0 ) ? s_Offset0 : s_Offset );

    g_Camera.SetViewParams( vecEye, g_CameraOrigins[group] );

    XMFLOAT3 vecAt;
    XMStoreFloat3( &vecAt, g_CameraOrigins[group] );
    g_Camera.SetModelCenter( vecAt );
}


//--------------------------------------------------------------------------------------
void DrawGrid( FXMVECTOR xAxis, FXMVECTOR yAxis, FXMVECTOR origin, size_t xdivs, size_t ydivs, GXMVECTOR color )
{
    auto context = DXUTGetD3D11DeviceContext();
    g_BatchEffect->Apply( context );

    context->IASetInputLayout( g_pBatchInputLayout );

    g_Batch->Begin();

    xdivs = std::max<size_t>( 1, xdivs );
    ydivs = std::max<size_t>( 1, ydivs );

    for( size_t i = 0; i <= xdivs; ++i )
    {
        float fPercent = float(i) / float(xdivs);
        fPercent = ( fPercent * 2.0f ) - 1.0f;
        XMVECTOR vScale = XMVectorScale( xAxis, fPercent );
        vScale = XMVectorAdd( vScale, origin );

        VertexPositionColor v1( XMVectorSubtract( vScale, yAxis ), color );
        VertexPositionColor v2( XMVectorAdd( vScale, yAxis ), color );
        g_Batch->DrawLine( v1, v2 );
    }

    for( size_t i = 0; i <= ydivs; i++ )
    {
        FLOAT fPercent = float(i) / float(ydivs);
        fPercent = ( fPercent * 2.0f ) - 1.0f;
        XMVECTOR vScale = XMVectorScale( yAxis, fPercent );
        vScale = XMVectorAdd( vScale, origin );

        VertexPositionColor v1( XMVectorSubtract( vScale, xAxis ), color );
        VertexPositionColor v2( XMVectorAdd( vScale, xAxis ), color );
        g_Batch->DrawLine( v1, v2 );
    }

    g_Batch->End();
}


//--------------------------------------------------------------------------------------
void DrawFrustum( const BoundingFrustum& frustum, FXMVECTOR color )
{
    XMFLOAT3 corners[ BoundingFrustum::CORNER_COUNT ];
    frustum.GetCorners( corners );

    VertexPositionColor verts[24];
    verts[0].position = corners[0];
    verts[1].position = corners[1];
    verts[2].position = corners[1];
    verts[3].position = corners[2];
    verts[4].position = corners[2];
    verts[5].position = corners[3];
    verts[6].position = corners[3];
    verts[7].position = corners[0];

    verts[8].position = corners[0];
    verts[9].position = corners[4];
    verts[10].position = corners[1];
    verts[11].position = corners[5];
    verts[12].position = corners[2];
    verts[13].position = corners[6];
    verts[14].position = corners[3];
    verts[15].position = corners[7];

    verts[16].position = corners[4];
    verts[17].position = corners[5];
    verts[18].position = corners[5];
    verts[19].position = corners[6];
    verts[20].position = corners[6];
    verts[21].position = corners[7];
    verts[22].position = corners[7];
    verts[23].position = corners[4];

    for( size_t j = 0; j < _countof(verts); ++j )
    {
        XMStoreFloat4( &verts[j].color, color );
    }

    auto context = DXUTGetD3D11DeviceContext();
    g_BatchEffect->Apply( context );

    context->IASetInputLayout( g_pBatchInputLayout );

    g_Batch->Begin();

    g_Batch->Draw( D3D11_PRIMITIVE_TOPOLOGY_LINELIST, verts, _countof( verts ) );
    
    g_Batch->End();
}

//--------------------------------------------------------------------------------------
void DrawCube( CXMMATRIX mWorld, FXMVECTOR color )
{
    static const XMVECTOR s_verts[8] =
    {
        { -1, -1, -1, 0 },
        { 1, -1, -1, 0 },
        { 1, -1, 1, 0 },
        { -1, -1, 1, 0 },
        { -1, 1, -1, 0 },
        { 1, 1, -1, 0 },
        { 1, 1, 1, 0 },
        { -1, 1, 1, 0 }
    };
    static const WORD s_indices[] =
    {
        0, 1,
        1, 2,
        2, 3,
        3, 0,
        4, 5,
        5, 6,
        6, 7,
        7, 4,
        0, 4,
        1, 5,
        2, 6,
        3, 7
    };

    VertexPositionColor verts[8];
    for( int i=0; i < 8; ++i )
    {
        XMVECTOR v = XMVector3Transform( s_verts[i], mWorld );
        XMStoreFloat3( &verts[i].position, v );
        XMStoreFloat4( &verts[i].color, color );
    }

    auto context = DXUTGetD3D11DeviceContext();
    g_BatchEffect->Apply( context );

    context->IASetInputLayout( g_pBatchInputLayout );

    g_Batch->Begin();

    g_Batch->DrawIndexed( D3D11_PRIMITIVE_TOPOLOGY_LINELIST, s_indices, _countof( s_indices ), verts, 8 );

    g_Batch->End();
}


//--------------------------------------------------------------------------------------
void DrawAabb( const BoundingBox& box, FXMVECTOR color )
{
    XMMATRIX matWorld = XMMatrixScaling( box.Extents.x, box.Extents.y, box.Extents.z );
    XMVECTOR position = XMLoadFloat3( &box.Center );
    matWorld.r[3] = XMVectorSelect( matWorld.r[3], position, g_XMSelect1110 );

    DrawCube( matWorld, color );
}


//--------------------------------------------------------------------------------------
void DrawObb( const BoundingOrientedBox& obb, FXMVECTOR color )
{
    XMMATRIX matWorld = XMMatrixRotationQuaternion( XMLoadFloat4( &obb.Orientation ) );
    XMMATRIX matScale = XMMatrixScaling( obb.Extents.x, obb.Extents.y, obb.Extents.z );
    matWorld = XMMatrixMultiply( matScale, matWorld );
    XMVECTOR position = XMLoadFloat3( &obb.Center );
    matWorld.r[3] = XMVectorSelect( matWorld.r[3], position, g_XMSelect1110 );

    DrawCube( matWorld, color );
}


//--------------------------------------------------------------------------------------
void DrawRing( FXMVECTOR Origin, FXMVECTOR MajorAxis, FXMVECTOR MinorAxis, CXMVECTOR color )
{
    static const DWORD dwRingSegments = 32;

    VertexPositionColor verts[ dwRingSegments + 1 ];

    FLOAT fAngleDelta = XM_2PI / ( float )dwRingSegments;
    // Instead of calling cos/sin for each segment we calculate
    // the sign of the angle delta and then incrementally calculate sin
    // and cosine from then on.
    XMVECTOR cosDelta = XMVectorReplicate( cosf( fAngleDelta ) );
    XMVECTOR sinDelta = XMVectorReplicate( sinf( fAngleDelta ) );
    XMVECTOR incrementalSin = XMVectorZero();
    static const XMVECTOR initialCos =
    {
        1.0f, 1.0f, 1.0f, 1.0f
    };
    XMVECTOR incrementalCos = initialCos;
    for( DWORD i = 0; i < dwRingSegments; i++ )
    {
        XMVECTOR Pos;
        Pos = XMVectorMultiplyAdd( MajorAxis, incrementalCos, Origin );
        Pos = XMVectorMultiplyAdd( MinorAxis, incrementalSin, Pos );
        XMStoreFloat3( &verts[i].position, Pos );
        XMStoreFloat4( &verts[i].color, color );
        // Standard formula to rotate a vector.
        XMVECTOR newCos = incrementalCos * cosDelta - incrementalSin * sinDelta;
        XMVECTOR newSin = incrementalCos * sinDelta + incrementalSin * cosDelta;
        incrementalCos = newCos;
        incrementalSin = newSin;
    }
    verts[ dwRingSegments ] = verts[0];

    // Draw ring
    auto context = DXUTGetD3D11DeviceContext();
    g_BatchEffect->Apply( context );

    context->IASetInputLayout( g_pBatchInputLayout );

    g_Batch->Begin();

    g_Batch->Draw( D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP, verts, dwRingSegments+1 );

    g_Batch->End();
}


//--------------------------------------------------------------------------------------
void DrawSphere( const BoundingSphere& sphere, FXMVECTOR color )
{
    XMVECTOR origin = XMLoadFloat3( &sphere.Center );

    const float fRadius = sphere.Radius;

    XMVECTOR xaxis = g_XMIdentityR0 * fRadius;
    XMVECTOR yaxis = g_XMIdentityR1 * fRadius;
    XMVECTOR zaxis = g_XMIdentityR2 * fRadius;

    DrawRing( origin, xaxis, zaxis, color );
    DrawRing( origin, xaxis, yaxis, color );
    DrawRing( origin, yaxis, zaxis, color );
}


//--------------------------------------------------------------------------------------
void DrawRay( FXMVECTOR Origin, FXMVECTOR Direction, bool bNormalize, FXMVECTOR color )
{
    VertexPositionColor verts[3];
    XMStoreFloat3( &verts[0].position, Origin );

    XMVECTOR NormDirection = XMVector3Normalize( Direction );
    XMVECTOR RayDirection = ( bNormalize ) ? NormDirection : Direction;

    XMVECTOR PerpVector = XMVector3Cross( NormDirection, g_XMIdentityR1 );

    if( XMVector3Equal( XMVector3LengthSq( PerpVector ), g_XMZero ) )
    {
        PerpVector = XMVector3Cross( NormDirection, g_XMIdentityR2 );
    }
    PerpVector = XMVector3Normalize( PerpVector );

    XMStoreFloat3( &verts[1].position, XMVectorAdd( RayDirection, Origin ) );
    PerpVector = XMVectorScale( PerpVector, 0.0625f );
    NormDirection = XMVectorScale( NormDirection, -0.25f );
    RayDirection = XMVectorAdd( PerpVector, RayDirection );
    RayDirection = XMVectorAdd( NormDirection, RayDirection );
    XMStoreFloat3( &verts[2].position, XMVectorAdd( RayDirection, Origin ) );

    XMStoreFloat4( &verts[0].color, color );
    XMStoreFloat4( &verts[1].color, color );
    XMStoreFloat4( &verts[2].color, color );

    auto context = DXUTGetD3D11DeviceContext();
    g_BatchEffect->Apply( context );

    context->IASetInputLayout( g_pBatchInputLayout );

    g_Batch->Begin();

    g_Batch->Draw( D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP, verts, 2 );

    g_Batch->End();
}


//--------------------------------------------------------------------------------------
void DrawTriangle( FXMVECTOR PointA, FXMVECTOR PointB, FXMVECTOR PointC, CXMVECTOR color )
{
    VertexPositionColor verts[4];
    XMStoreFloat3( &verts[0].position, PointA );
    XMStoreFloat3( &verts[1].position, PointB );
    XMStoreFloat3( &verts[2].position, PointC );
    XMStoreFloat3( &verts[3].position, PointA );

    XMStoreFloat4( &verts[0].color, color );
    XMStoreFloat4( &verts[1].color, color );
    XMStoreFloat4( &verts[2].color, color );
    XMStoreFloat4( &verts[3].color, color );

    auto context = DXUTGetD3D11DeviceContext();
    g_BatchEffect->Apply( context );

    context->IASetInputLayout( g_pBatchInputLayout );

    g_Batch->Begin();

    g_Batch->Draw( D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP, verts, 4 );

    g_Batch->End();
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

    // Create other render resources here
    g_States = std::make_unique<CommonStates>( pd3dDevice );
    g_Batch = std::make_unique<PrimitiveBatch<VertexPositionColor>>( pd3dImmediateContext );

    g_BatchEffect = std::make_unique<BasicEffect>( pd3dDevice );
    g_BatchEffect->SetVertexColorEnabled(true);

    {
        void const* shaderByteCode;
        size_t byteCodeLength;

        g_BatchEffect->GetVertexShaderBytecode( &shaderByteCode, &byteCodeLength );

        hr = pd3dDevice->CreateInputLayout( VertexPositionColor::InputElements,
                                            VertexPositionColor::InputElementCount,
                                            shaderByteCode, byteCodeLength,
                                            &g_pBatchInputLayout );
        if( FAILED( hr ) )
            return hr;
    }

    // Setup the camera's view parameters
    auto pComboBox = g_SampleUI.GetComboBox( IDC_GROUP );
    SetViewForGroup( (pComboBox) ? (int)PtrToInt( pComboBox->GetSelectedData() ) : 0 );

    g_HUD.GetButton( IDC_TOGGLEWARP )->SetEnabled( true );

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

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( XM_PI / 4, fAspectRatio, 0.1f, 1000.0f );
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

    // Get the projection & view matrix from the camera class
    XMMATRIX mWorld = g_Camera.GetWorldMatrix();
    XMMATRIX mView = g_Camera.GetViewMatrix();
    XMMATRIX mProj = g_Camera.GetProjMatrix();

    g_BatchEffect->SetWorld( mWorld );
    g_BatchEffect->SetView( mView );
    g_BatchEffect->SetProjection( mProj );

    // Draw objects
    RenderObjects();

    // Render HUD
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

    g_States.reset();
    g_BatchEffect.reset();
    g_Batch.reset();

    SAFE_RELEASE( g_pBatchInputLayout );
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
    // Update position of collision objects
    Animate( fTime );

    // Compute collisions
    Collide();

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

    // Pass all remaining windows messages to camera so it can respond to user input
    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
}


//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
    switch (nChar)
    {
    case '1':
    case '2':
    case '3':
    case '4':
        {
            int group = (nChar - '1');
            auto pComboBox = g_SampleUI.GetComboBox( IDC_GROUP );
            assert(pComboBox != NULL);
            pComboBox->SetSelectedByData( IntToPtr( group ) );
            SetViewForGroup( group );
        }
        break;
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
        case IDC_TOGGLEWARP:
            DXUTToggleWARP();
            break;
        case IDC_CHANGEDEVICE:
            g_SettingsDlg.SetActive( !g_SettingsDlg.IsActive() );
            break;
        case IDC_GROUP:
            {
                auto pComboBox = reinterpret_cast<CDXUTComboBox*>( pControl );
                SetViewForGroup( (int)PtrToInt( pComboBox->GetSelectedData() ) );
            }
            break;
    }
}
