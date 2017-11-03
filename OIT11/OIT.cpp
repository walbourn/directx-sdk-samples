//-----------------------------------------------------------------------------
// File: OIT.cpp
//
// Desc: Description for a class that handles Order Independent Transparency.
//   The algorithm uses a series of passes:
//
// 1. Determine the number of transparent fragments in each pixel by drawing
//    each of the transparent primitives into an overdraw accumlation buffer
//
// 2. Create a prefix sum for each pixel location.  This holds the sum of all 
//    the fragments in each of the preceding pixels.  The last pixel will hold
//    a count of all fragments in the scene.
//
// 3. Render the fragments to a deep frame buffer that holds both depth and color
//    for each of the fragments.  The prefix sum buffer is used to determine
//    the placement of each fragment in the deep buffer.
//
// 4. Sort the fragments and render to the final frame buffer.  The prefix
//    sum is used to locate fragments in the deep frame buffer.
//
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "DXUT.h"
#include "SDKmisc.h"
#include "OIT.h"

using namespace DirectX;

// Creates additional buffers for debugging scenarios (see the end of the file)
//#define DEBUG_CS

//-----------------------------------------------------------------------------
// OIT Constructor
//-----------------------------------------------------------------------------
OIT::OIT() :
    m_pFragmentCountPS(nullptr),
    m_pCreatePrefixSum_Pass0_CS(nullptr),
    m_pCreatePrefixSum_Pass1_CS(nullptr),
    m_pFillDeepBufferPS(nullptr),
    m_pSortAndRenderCS(nullptr),
    m_pDepthStencilState(nullptr),
    m_pCS_CB(nullptr),
    m_pPS_CB(nullptr),
    m_pFragmentCountBuffer(nullptr),
    m_pPrefixSum(nullptr),
    m_pDeepBuffer(nullptr),
    m_pDeepBufferColor(nullptr),
    m_pPrefixSumDebug(nullptr),
    m_pDeepBufferDebug(nullptr),
    m_pDeepBufferColorDebug(nullptr),
    m_pFragmentCountUAV(nullptr),
    m_pPrefixSumUAV(nullptr),
    m_pDeepBufferUAV(nullptr),
    m_pDeepBufferColorUAV(nullptr),
    m_pDeepBufferColorUAV_UINT(nullptr),
    m_pFragmentCountRV(nullptr)
{
}


//-----------------------------------------------------------------------------
// Create shaders, buffers, and states
//-----------------------------------------------------------------------------
HRESULT OIT::OnD3D11CreateDevice( ID3D11Device* pDevice )
{
    HRESULT hr;
    
    // Create Shaders
    ID3DBlob* pBlob = nullptr;
    V_RETURN( DXUTCompileFromFile( L"OIT_PS.hlsl", nullptr, "FragmentCountPS", "ps_5_0",
                                   D3DCOMPILE_ENABLE_STRICTNESS, 0, &pBlob ) );
    V_RETURN( pDevice->CreatePixelShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), nullptr, &m_pFragmentCountPS ) );
    SAFE_RELEASE( pBlob );
    DXUT_SetDebugName( m_pFragmentCountPS, "FragmentCountPS" );

    V_RETURN( DXUTCompileFromFile( L"OIT_CS.hlsl", nullptr, "CreatePrefixSum_Pass0_CS", "cs_5_0",
                                   D3DCOMPILE_ENABLE_STRICTNESS, 0, &pBlob ) );
    V_RETURN( pDevice->CreateComputeShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), nullptr, &m_pCreatePrefixSum_Pass0_CS ) );
    SAFE_RELEASE( pBlob );
    DXUT_SetDebugName( m_pCreatePrefixSum_Pass0_CS, "CreatePrefixSum_Pass0_CS" );

    V_RETURN( DXUTCompileFromFile( L"OIT_CS.hlsl", nullptr, "CreatePrefixSum_Pass1_CS", "cs_5_0",
                                   D3DCOMPILE_ENABLE_STRICTNESS, 0, &pBlob ) );
    V_RETURN( pDevice->CreateComputeShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), nullptr, &m_pCreatePrefixSum_Pass1_CS ) );
    SAFE_RELEASE( pBlob );
    DXUT_SetDebugName( m_pCreatePrefixSum_Pass1_CS, "CreatePrefixSum_Pass1_CS" );

    V_RETURN( DXUTCompileFromFile( L"OIT_PS.hlsl", nullptr, "FillDeepBufferPS", "ps_5_0",
                                   D3DCOMPILE_ENABLE_STRICTNESS, 0, &pBlob ) );
    V_RETURN( pDevice->CreatePixelShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), nullptr, &m_pFillDeepBufferPS ) );
    SAFE_RELEASE( pBlob );
    DXUT_SetDebugName( m_pFillDeepBufferPS, "FillDeepBufferPS" ); 

    V_RETURN( DXUTCompileFromFile( L"OIT_CS.hlsl", nullptr, "SortAndRenderCS", "cs_5_0",
                                   D3DCOMPILE_ENABLE_STRICTNESS, 0, &pBlob ) );
    V_RETURN( pDevice->CreateComputeShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), nullptr, &m_pSortAndRenderCS ) );
    SAFE_RELEASE( pBlob );
    DXUT_SetDebugName( m_pSortAndRenderCS, "SortAndRenderCS" );

    // Create constant buffers
    D3D11_BUFFER_DESC Desc;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Desc.MiscFlags = 0;    
    Desc.ByteWidth = sizeof( CS_CB );
    V_RETURN( pDevice->CreateBuffer( &Desc, nullptr, &m_pCS_CB ) );
    DXUT_SetDebugName( m_pCS_CB, "CS_CB" );

    Desc.ByteWidth = sizeof( PS_CB );
    V_RETURN( pDevice->CreateBuffer( &Desc, nullptr, &m_pPS_CB ) );
    DXUT_SetDebugName( m_pPS_CB, "PS_CB" );

    // Create depth/stencil state
    D3D11_DEPTH_STENCIL_DESC DSDesc;
    ZeroMemory( &DSDesc, sizeof( D3D11_DEPTH_STENCIL_DESC ) );
    DSDesc.DepthEnable = FALSE;
    DSDesc.StencilEnable = FALSE;
    V_RETURN( pDevice->CreateDepthStencilState( &DSDesc, &m_pDepthStencilState ) );
    DXUT_SetDebugName( m_pDepthStencilState, "DepthSclOff" );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Create resolution dependent resources
//-----------------------------------------------------------------------------
HRESULT OIT::OnD3D11ResizedSwapChain( const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, ID3D11Device* pDevice )
{
    HRESULT hr;

    if ( !pDevice )
        return E_FAIL;

    m_nFrameWidth = pBackBufferSurfaceDesc->Width;
    m_nFrameHeight = pBackBufferSurfaceDesc->Height;

    // Create buffers
    D3D11_BUFFER_DESC descBuf;
    ZeroMemory( &descBuf, sizeof( D3D11_BUFFER_DESC) );
    descBuf.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;

    // Create the deep frame buffer.
    // This simple allocation scheme for the deep frame buffer allocates space for 8 times the size of the
    // frame buffer, which means that it can hold an average of 8 fragments per pixel.  This will usually waste some
    // space, and in some cases of high overdraw the buffer could run into problems with overflow.  It 
    // may be useful to make the buffer size more intelligent to avoid these problems.
    descBuf.ByteWidth = pBackBufferSurfaceDesc->Width * pBackBufferSurfaceDesc->Height * 8 * sizeof(float);
    descBuf.StructureByteStride = sizeof(float);
    V_RETURN( pDevice->CreateBuffer( &descBuf, nullptr, &m_pDeepBuffer ));
    DXUT_SetDebugName( m_pDeepBuffer, "Deep" );

    // Create deep frame buffer for color
    descBuf.StructureByteStride = 4 * sizeof(BYTE);
    descBuf.ByteWidth = pBackBufferSurfaceDesc->Width * pBackBufferSurfaceDesc->Height * 8 * 4 * sizeof(BYTE);
    V_RETURN( pDevice->CreateBuffer( &descBuf, nullptr, &m_pDeepBufferColor ));
    DXUT_SetDebugName( m_pDeepBufferColor, "DeepClr" );

    // Create prefix sum buffer
    descBuf.StructureByteStride = sizeof(float);
    descBuf.ByteWidth = pBackBufferSurfaceDesc->Width * pBackBufferSurfaceDesc->Height * sizeof(UINT);
    V_RETURN( pDevice->CreateBuffer( &descBuf, nullptr, &m_pPrefixSum ));
    DXUT_SetDebugName( m_pPrefixSum, "PrefixSum" );

#ifdef DEBUG_CS
    // Create debug buffers
    D3D11_BUFFER_DESC descDebugBuf;
    ZeroMemory( &descDebugBuf, sizeof( D3D11_BUFFER_DESC) );
    descDebugBuf.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    descDebugBuf.Usage = D3D11_USAGE_STAGING;

    // Prefix Sum debug buffer
    descDebugBuf.StructureByteStride = sizeof(float);
    descDebugBuf.ByteWidth = pBackBufferSurfaceDesc->Width * pBackBufferSurfaceDesc->Height * sizeof(UINT);
    V_RETURN( pDevice->CreateBuffer( &descDebugBuf, nullptr, &m_pPrefixSumDebug ) );  
    DXUT_SetDebugName( m_pPrefixSumDebug, "PrefixSum Dbg" );

    // Deep Buffer debug 
    descDebugBuf.StructureByteStride = sizeof(float);
    descDebugBuf.ByteWidth = pBackBufferSurfaceDesc->Width * pBackBufferSurfaceDesc->Height * 8 * sizeof(float);
    V_RETURN( pDevice->CreateBuffer( &descDebugBuf, nullptr, &m_pDeepBufferDebug ) );    
    DXUT_SetDebugName( m_pDeepBufferDebug, "Deep Dbg" );

    // Deep Buffer Color debug
    descDebugBuf.StructureByteStride = 4;
    descDebugBuf.ByteWidth = pBackBufferSurfaceDesc->Width * pBackBufferSurfaceDesc->Height * 8 * 4;
    V_RETURN( pDevice->CreateBuffer( &descDebugBuf, nullptr, &m_pDeepBufferColorDebug ) );    
    DXUT_SetDebugName( m_pDeepBufferColorDebug, "DeepClr Dbg" );
#endif

    // Create fragment count buffer
    D3D11_TEXTURE2D_DESC desc2D;
    ZeroMemory( &desc2D, sizeof( D3D11_TEXTURE2D_DESC ) );
    desc2D.ArraySize = 1;
    desc2D.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    desc2D.Usage = D3D11_USAGE_DEFAULT;
    desc2D.Format = DXGI_FORMAT_R32_UINT;
    desc2D.Width = pBackBufferSurfaceDesc->Width;
    desc2D.Height = pBackBufferSurfaceDesc->Height;
    desc2D.MipLevels = 1;  
    desc2D.SampleDesc.Count = 1;
    desc2D.SampleDesc.Quality = 0;
    V_RETURN( pDevice->CreateTexture2D( &desc2D, nullptr, &m_pFragmentCountBuffer ) );
    DXUT_SetDebugName( m_pFragmentCountBuffer, "FragCount" );

    // Create Fragment Count Resource View
    D3D11_SHADER_RESOURCE_VIEW_DESC descRV;
    descRV.Format = desc2D.Format;
    descRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    descRV.Texture2D.MipLevels = 1;
    descRV.Texture2D.MostDetailedMip = 0;
    V_RETURN( pDevice->CreateShaderResourceView( m_pFragmentCountBuffer, &descRV, &m_pFragmentCountRV ) );
    DXUT_SetDebugName( m_pFragmentCountRV, "FragCount SRV" );

    // Create Unordered Access Views
    D3D11_UNORDERED_ACCESS_VIEW_DESC descUAV;
    descUAV.Format = DXGI_FORMAT_R32_FLOAT;
    descUAV.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    descUAV.Buffer.FirstElement = 0;
    descUAV.Buffer.NumElements =     pBackBufferSurfaceDesc->Width * pBackBufferSurfaceDesc->Height * 8;
    descUAV.Buffer.Flags = 0;
    V_RETURN( pDevice->CreateUnorderedAccessView( m_pDeepBuffer, &descUAV, &m_pDeepBufferUAV ) );
    DXUT_SetDebugName( m_pDeepBufferUAV, "Deep UAV" );
   
    descUAV.Format = DXGI_FORMAT_R8G8B8A8_UINT;
    V_RETURN( pDevice->CreateUnorderedAccessView( m_pDeepBufferColor, &descUAV, &m_pDeepBufferColorUAV ) );
    DXUT_SetDebugName( m_pDeepBufferColorUAV, "DeepClr UAV" );

    descUAV.Format = DXGI_FORMAT_R32_UINT;
    V_RETURN( pDevice->CreateUnorderedAccessView( m_pDeepBufferColor, &descUAV, &m_pDeepBufferColorUAV_UINT ) );
    DXUT_SetDebugName( m_pDeepBufferColorUAV_UINT, "DeepClr UAV UINT" );

    descUAV.Format = DXGI_FORMAT_R32_UINT;
    descUAV.Buffer.NumElements = pBackBufferSurfaceDesc->Width * pBackBufferSurfaceDesc->Height;
    V_RETURN( pDevice->CreateUnorderedAccessView( m_pPrefixSum, &descUAV, &m_pPrefixSumUAV ) );
    DXUT_SetDebugName( m_pPrefixSumUAV, "PrefixSum UAV" );

    descUAV.Format = desc2D.Format;
    descUAV.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    descUAV.Texture2D.MipSlice = 0;
    V_RETURN( pDevice->CreateUnorderedAccessView( m_pFragmentCountBuffer, &descUAV, &m_pFragmentCountUAV ) );
    DXUT_SetDebugName( m_pFragmentCountUAV, "FragCount UAV" );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Release resolution dependent resources
//-----------------------------------------------------------------------------
void OIT::OnD3D11ReleasingSwapChain()
{
    SAFE_RELEASE( m_pFragmentCountBuffer );
    SAFE_RELEASE( m_pPrefixSum );
    SAFE_RELEASE( m_pDeepBuffer );
    SAFE_RELEASE( m_pDeepBufferColor );

    SAFE_RELEASE( m_pPrefixSumDebug );
    SAFE_RELEASE( m_pDeepBufferDebug );
    SAFE_RELEASE( m_pDeepBufferColorDebug );

    SAFE_RELEASE( m_pFragmentCountUAV );
    SAFE_RELEASE( m_pPrefixSumUAV );
    SAFE_RELEASE( m_pDeepBufferUAV );
    SAFE_RELEASE( m_pDeepBufferColorUAV_UINT );
    SAFE_RELEASE( m_pDeepBufferColorUAV );

    SAFE_RELEASE( m_pFragmentCountRV );
}


//-----------------------------------------------------------------------------
// Release device resources
//-----------------------------------------------------------------------------
void OIT::OnD3D11DestroyDevice()
{
    SAFE_RELEASE( m_pFragmentCountPS );
    SAFE_RELEASE( m_pCreatePrefixSum_Pass0_CS );
    SAFE_RELEASE( m_pCreatePrefixSum_Pass1_CS );
    SAFE_RELEASE( m_pFillDeepBufferPS );
    SAFE_RELEASE( m_pSortAndRenderCS );

    SAFE_RELEASE( m_pDepthStencilState );

    SAFE_RELEASE( m_pCS_CB );
    SAFE_RELEASE( m_pPS_CB );
}


//-----------------------------------------------------------------------------
// Render transparent objects with an order independent algorithm.  The method
// is broken up into a number of passes:
//
// 1. Determine the number of transparent fragments in each pixel by drawing
//    each of the transparent primitives into an overdraw accumlation buffer
//
// 2. Create a prefix sum for each pixel location.  This holds the sum of all 
//    the fragments in each of the preceding pixels.  The last pixel will hold
//    a count of all fragments in the scene.
//
// 3. Render the fragments to a deep frame buffer that holds both depth and color
//    for each of the fragments.  The prefix sum buffer is used to determine
//    the placement of each fragment in the deep buffer.
//
// 4. Sort the fragments and render to the final frame buffer.  The prefix
//    sum is used to locate fragments in the deep frame buffer.
//-----------------------------------------------------------------------------
void OIT::Render( ID3D11DeviceContext* pD3DContext, ID3D11Device* pDevice, 
                  CScene* pScene, CXMMATRIX mWorldViewProjection,
                  ID3D11RenderTargetView* pRTV, ID3D11DepthStencilView* pDSV)
{
    // Cache off the old depth/stencil state as we'll be mucking around with it a bit.
    ID3D11DepthStencilState* pDepthStencilStateStored = nullptr;
    UINT stencilRef;
    pD3DContext->OMGetDepthStencilState( &pDepthStencilStateStored, &stencilRef );

    // Create a count of the number of fragments at each pixel location
    CreateFragmentCount( pD3DContext, pScene, mWorldViewProjection, pRTV, pDSV );

    // Create a prefix sum of the fragment counts.  Each pixel location will hold
    // a count of the total number of fragments of every preceding pixel location.
    CreatePrefixSum( pD3DContext );

    // Fill in the deep frame buffer with depth and color values.  Use the prefix
    // sum to determine where in the deep buffer to place the current fragment.
    FillDeepBuffer( pD3DContext, pRTV, pDSV, pScene, mWorldViewProjection );

    // Sort and render the fragments.  Use the prefix sum to determine where the 
    // fragments for each pixel reside.
    SortAndRenderFragments( pD3DContext, pDevice, pRTV );

    // Restore the cached depth/stencil state
    pD3DContext->OMSetDepthStencilState( pDepthStencilStateStored, stencilRef );
  
}


//-----------------------------------------------------------------------------
// Creates a frame buffer that holds the number of fragments for each pixel.
// The scene is rendered with depth tests disabled.  The pixel shader
// simply increments the fragment count by one for each pixel rendered.
//-----------------------------------------------------------------------------
void OIT::CreateFragmentCount( ID3D11DeviceContext* pD3DContext, CScene* pScene,
                               CXMMATRIX mWVP, ID3D11RenderTargetView* pRTV,
                               ID3D11DepthStencilView* pDSV )
{
    // Clear the render target & depth/stencil
    pD3DContext->ClearRenderTargetView( pRTV, Colors::Black );
    pD3DContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );    

    // Clear the fragment count buffer
    static const UINT clearValueUINT[1] = { 0 };
    pD3DContext->ClearUnorderedAccessViewUint( m_pFragmentCountUAV, clearValueUINT );

    // Draw the transparent geometry
    ID3D11UnorderedAccessView* pUAVs[ 3 ];
    pUAVs[0] = m_pFragmentCountUAV;
    pD3DContext->OMSetRenderTargetsAndUnorderedAccessViews(1, &pRTV, pDSV, 1, 1, pUAVs, nullptr);
    pD3DContext->OMSetDepthStencilState( m_pDepthStencilState, 0 );
    pD3DContext->PSSetShader( m_pFragmentCountPS, nullptr, 0 );

    pScene->D3D11Render( mWVP, pD3DContext );

    // Set render target and depth/stencil views to nullptr,
    //   we'll need to read the RTV in a shader later
    ID3D11RenderTargetView* pViewNULL[1] = {nullptr};
    ID3D11DepthStencilView* pDSVNULL = nullptr;
    pD3DContext->OMSetRenderTargets( 1, pViewNULL, pDSVNULL );
}


//-----------------------------------------------------------------------------
// Create a prefix sum for each pixel which holds the count of the fragments
// in preceding pixels.  Two pass types are used:
//
// 1. The first pass converts a 2D buffer to a 1D buffer, and sums every other
//    value with the previous value.
// 
// 2. The second and following passes distribute the sum of the first half of each group
//    to the second half of the group.  There are n/groupsize groups in each pass.
//    Each pass doubles the group size until it reaches the size of the buffer.
//    The resulting buffer holds the prefix sum of all preceding values at each
//    location. 
//-----------------------------------------------------------------------------
void OIT::CreatePrefixSum( ID3D11DeviceContext* pD3DContext )
{
    ID3D11UnorderedAccessView* ppUAViewNULL[4] = { nullptr, nullptr, nullptr, nullptr };
 
    HRESULT hr;

    // prepare the constant buffer
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    V( pD3DContext->Map( m_pCS_CB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    auto pCS_CB = reinterpret_cast<CS_CB*>( MappedResource.pData ); 
    pCS_CB->nFrameWidth = m_nFrameWidth;
    pCS_CB->nFrameHeight = m_nFrameHeight;
    pD3DContext->Unmap( m_pCS_CB, 0 );
    pD3DContext->CSSetConstantBuffers( 0, 1, &m_pCS_CB );

    // First pass : convert the 2D frame buffer to a 1D array.  We could simply 
    //   copy the contents over, but while we're at it, we may as well do 
    //   some work and save a pass later, so we do the first summation pass;  
    //   add the values at the even indices to the values at the odd indices.
    pD3DContext->CSSetShader( m_pCreatePrefixSum_Pass0_CS, nullptr, 0 );

    ID3D11UnorderedAccessView* pUAViews[ 1 ] = { m_pPrefixSumUAV };
    pD3DContext->CSSetUnorderedAccessViews( 3, 1, pUAViews, (UINT*)(&pUAViews) );

    pD3DContext->CSSetShaderResources( 0, 1, &m_pFragmentCountRV );
    pD3DContext->Dispatch( m_nFrameWidth, m_nFrameHeight, 1);

    // Second and following passes : each pass distributes the sum of the first half of the group
    //   to the second half of the group.  There are n/groupsize groups in each pass.
    //   Each pass doubles the group size until it is the size of the buffer.
    //   The resulting buffer holds the prefix sum of all preceding values in each
    //   position 
    ID3D11ShaderResourceView* ppRVNULL[3] = {nullptr, nullptr, nullptr};
    pD3DContext->CSSetShaderResources( 0, 1, ppRVNULL );
    pD3DContext->CSSetUnorderedAccessViews( 3, 1, ppUAViewNULL, (UINT*)(&ppUAViewNULL) );

    // Perform the passes.  The first pass would have been i = 2, but it was performed earlier
    for( UINT i = 4; i < (m_nFrameWidth*m_nFrameHeight*2); i*=2)
    {
        V( pD3DContext->Map( m_pCS_CB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
        pCS_CB = reinterpret_cast<CS_CB*>( MappedResource.pData );
        pCS_CB->nPassSize = i ;
        pCS_CB->nFrameWidth = m_nFrameWidth;
        pCS_CB->nFrameHeight = m_nFrameHeight;
        pD3DContext->Unmap( m_pCS_CB, 0 );
        pD3DContext->CSSetConstantBuffers( 0, 1, &m_pCS_CB );

        pD3DContext->CSSetShader( m_pCreatePrefixSum_Pass1_CS, nullptr, 0 );

        pUAViews[0] = m_pPrefixSumUAV;
        pD3DContext->CSSetUnorderedAccessViews( 3, 1, pUAViews, (UINT*)(&pUAViews) );

        pD3DContext->CSSetShaderResources( 0, 1, &m_pFragmentCountRV );

        // the "ceil((float) m_nFrameWidth*m_nFrameHeight/i)" calculation ensures that 
        //    we dispatch enough threads to cover the entire range.
        pD3DContext->Dispatch( (int)(ceil((float)m_nFrameWidth*m_nFrameHeight / i)), 1, 1 );

    }

    // Clear out the resource and unordered access views
    pD3DContext->CSSetShaderResources( 0, 1, ppRVNULL );
    pD3DContext->CSSetUnorderedAccessViews( 3, 1, ppUAViewNULL, (UINT*)(&ppUAViewNULL) );
}


//-----------------------------------------------------------------------------
// Fill the deep frame buffer with the fragment color and depth.  The shader
// uses the prefix sum to determine the placement of each fragment.  The result
// of this pass is a continuous buffer of fragment values.
//-----------------------------------------------------------------------------
void OIT::FillDeepBuffer( ID3D11DeviceContext* pD3DContext, ID3D11RenderTargetView* pRTV,
                          ID3D11DepthStencilView* pDSV, CScene* pScene, CXMMATRIX mWVP )
{
    static const FLOAT clearValueFLOAT[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
    static const UINT clearValueUINT[1] = { 0 };

    // Clear buffers, render target, and depth/stencil
    pD3DContext->ClearUnorderedAccessViewFloat( m_pDeepBufferUAV, clearValueFLOAT );
    pD3DContext->ClearUnorderedAccessViewUint( m_pFragmentCountUAV, clearValueUINT );
    pD3DContext->ClearUnorderedAccessViewUint( m_pDeepBufferColorUAV_UINT, clearValueUINT );

    float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f } ; 
    pD3DContext->ClearRenderTargetView( pRTV, ClearColor );
    pD3DContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );    

    // Render the Deep Frame buffer using the Prefix Sum buffer to place the fragments in the correct bin
    ID3D11UnorderedAccessView* pUAVs[ 4 ];
    pUAVs[0] = m_pFragmentCountUAV;
    pUAVs[1] = m_pDeepBufferUAV;
    pUAVs[2] = m_pDeepBufferColorUAV;
    pUAVs[3] = m_pPrefixSumUAV;
    pD3DContext->OMSetRenderTargetsAndUnorderedAccessViews( 1, &pRTV, pDSV, 1, 4, pUAVs, nullptr );

    pD3DContext->PSSetShader( m_pFillDeepBufferPS, nullptr, 0 );

    HRESULT hr;
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    V( pD3DContext->Map( m_pPS_CB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    auto pPS_CB = reinterpret_cast<PS_CB*>( MappedResource.pData ); 
    pPS_CB->nFrameWidth = m_nFrameWidth;
    pPS_CB->nFrameHeight = m_nFrameHeight;
    pD3DContext->Unmap( m_pPS_CB, 0 );
    pD3DContext->PSSetConstantBuffers( 0, 1, &m_pPS_CB );

    pScene->D3D11Render( mWVP, pD3DContext );

    ID3D11RenderTargetView* pViews[1] = {nullptr};
    pD3DContext->OMSetRenderTargets( 1, pViews, pDSV );
}


//-----------------------------------------------------------------------------
// Sort and render the fragments.  The compute shader iterates through each of
// the pixels and sorts the fragments for each using a bitonic sort.  It then
// combines the fragments in back to front order to arrive at the final pixel
// value.  It uses the prefix sum buffer to determine the placement of the 
// fragments.
//-----------------------------------------------------------------------------
void OIT::SortAndRenderFragments( ID3D11DeviceContext* pD3DContext, ID3D11Device* pDevice,
                                  ID3D11RenderTargetView* pRTV )
{
    float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; 
    ID3D11UnorderedAccessView* ppUAViewNULL[4] = { nullptr, nullptr, nullptr, nullptr };
    pD3DContext->ClearRenderTargetView( pRTV, ClearColor );
 
    ID3D11Resource* pBackBufferRes = nullptr;
    pRTV->GetResource( &pBackBufferRes );

    ID3D11UnorderedAccessView* pUAView = nullptr;
    D3D11_UNORDERED_ACCESS_VIEW_DESC descUAV;
    descUAV.Format = DXUTGetDXGIBackBufferSurfaceDesc()->Format;
    descUAV.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    descUAV.Texture2D.MipSlice = 0;
    pDevice->CreateUnorderedAccessView( pBackBufferRes, &descUAV, &pUAView );
    DXUT_SetDebugName( pUAView, "BackBuffer UAV" );

    ID3D11UnorderedAccessView* pUAViews[ 4 ] = { m_pDeepBufferUAV, m_pDeepBufferColorUAV_UINT, pUAView, m_pPrefixSumUAV };
    pD3DContext->CSSetUnorderedAccessViews( 0, 4, pUAViews, (UINT*)(&pUAViews) );
    pD3DContext->CSSetShader( m_pSortAndRenderCS, nullptr, 0 );
    pD3DContext->CSSetShaderResources( 0, 1, &m_pFragmentCountRV );

    pD3DContext->Dispatch( m_nFrameWidth, m_nFrameHeight, 1);

    ID3D11ShaderResourceView* ppRVNULL[3] = {nullptr, nullptr, nullptr};
    pD3DContext->CSSetShaderResources( 0, 1, ppRVNULL );

    // Unbind resources for CS
    pD3DContext->CSSetUnorderedAccessViews( 0, 4, ppUAViewNULL, (UINT*)(&ppUAViewNULL) );

    SAFE_RELEASE(pUAView);
    SAFE_RELEASE(pBackBufferRes);

}


#if 0
// Fragments that require use of DEBUG_CS above
    {
        HRESULT hr;
 /*       pD3DContext->CopyResource( m_pTex2DDebug, m_pPixelOverdrawBuffer );
        D3D11_MAPPED_SUBRESOURCE MappedResource; 
        V( pD3DContext->Map( m_pTex2DDebug, 0, D3D11_MAP_READ, 0, &MappedResource ) );
        // set a break point here, and drag MappedResource.pData into in your Watch window and cast it as (float*)
        pD3DContext->Unmap( m_pTex2DDebug, 0 );
   */
        pD3DContext->CopyResource( m_pDeepBufferDebug, m_pDeepBuffer );
        D3D11_MAPPED_SUBRESOURCE MappedResource2; 
        V( pD3DContext->Map( m_pDeepBufferDebug, 0, D3D11_MAP_READ, 0, &MappedResource2 ) );
        // set a break point here, and drag MappedResource.pData into in your Watch window and cast it as (float*)
        pD3DContext->Unmap( m_pDeepBufferDebug, 0 );

        pD3DContext->CopyResource( m_pDeepBufferColorDebug, m_pDeepBufferColor );
        D3D11_MAPPED_SUBRESOURCE MappedResource3; 
        V( pD3DContext->Map( m_pDeepBufferColorDebug, 0, D3D11_MAP_READ, 0, &MappedResource3 ) );
        // set a break point here, and drag MappedResource.pData into in your Watch window and cast it as (float*)
        pD3DContext->Unmap( m_pDeepBufferColorDebug, 0 );

        pD3DContext->CopyResource( m_pPrefixSumDebug, m_pPrefixSum );
        D3D11_MAPPED_SUBRESOURCE MappedResource4; 
        V( pD3DContext->Map( m_pPrefixSumDebug, 0, D3D11_MAP_READ, 0, &MappedResource4 ) );
        // set a break point here, and drag MappedResource.pData into in your Watch window and cast it as (float*)
        pD3DContext->Unmap( m_pPrefixSumDebug, 0 );

    }
#endif
