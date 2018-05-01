//--------------------------------------------------------------------------------------
// File: TessellatorCS40.cpp
//
// Demos how to use Compute Shader 4.0 to do adaptive tessellation schemes
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DXUT.h"
#include "SDKmisc.h"
#include "ScanCS.h"
#include "TessellatorCS40.h"

#include "TessellatorCS40_defines.h"

using namespace DirectX;

ID3D11ComputeShader* CTessellator::s_pEdgeFactorCS = nullptr;
ID3D11ComputeShader* CTessellator::s_pNumVerticesIndicesCSs[] = { nullptr, nullptr, nullptr, nullptr };
ID3D11ComputeShader* CTessellator::s_pScatterVertexTriIDIndexIDCS = nullptr;
ID3D11ComputeShader* CTessellator::s_pScatterIndexTriIDIndexIDCS = nullptr;
ID3D11ComputeShader* CTessellator::s_pTessVerticesCSs[] = { nullptr, nullptr, nullptr, nullptr };
ID3D11ComputeShader* CTessellator::s_pTessIndicesCSs[] = { nullptr, nullptr, nullptr, nullptr };
ID3D11Buffer*        CTessellator::s_pEdgeFactorCSCB = nullptr;
ID3D11Buffer*        CTessellator::s_pLookupTableCSCB = nullptr;
ID3D11Buffer*        CTessellator::s_pCSCB = nullptr;
ID3D11Buffer*        CTessellator::s_pCSReadBackBuf = nullptr;

CScanCS              CTessellator::s_ScanCS;

// finalPointPositionTable[i] < insideNumHalfTessFactorPoints, scan of [0], scatter, inverse scatter
static int insidePointIndex[MAX_FACTOR / 2 + 1][MAX_FACTOR / 2 + 2][4];
// finalPointPositionTable[i] < outsideNumHalfTessFactorPoints, scan of [0], scatter, inverse scatter
static int outsidePointIndex[MAX_FACTOR / 2 + 1][MAX_FACTOR / 2 + 2][4];

//--------------------------------------------------------------------------------------
void InitLookupTables()
{
    int finalPointPositionTable[MAX_FACTOR / 2 + 1];
    finalPointPositionTable[0] = 0;
    finalPointPositionTable[1] = MAX_FACTOR / 2;
    for (int i = 2; i < MAX_FACTOR / 2 + 1; ++ i)
    {
        int level = 0;
        for (;;)
        {
            if (0 == (((i - 2) - ((1UL << level) - 1)) & ((1UL << (level + 1)) - 1)))
            {
                break;
            }
            ++ level;
        }

        finalPointPositionTable[i] = ((MAX_FACTOR >> 1) + ((i - 2) - ((1UL << level) - 1))) >> (level + 1);
    }

    for (int h = 0; h <= MAX_FACTOR / 2; ++ h)
    {
        for (int i = 0; i <= MAX_FACTOR / 2; ++ i)
        {
            if (i == 0)
            {
                insidePointIndex[h][i][0] = 0;
            }
            else
            {
                insidePointIndex[h][i][0] = finalPointPositionTable[i] < h;
            }
        }
        insidePointIndex[h][MAX_FACTOR / 2 + 1][0] = 0;
        for (int i = 0; i <= MAX_FACTOR / 2 + 1; ++ i)
        {
            if (i == 0)
            {
                insidePointIndex[h][i][1] = 0;
            }
            else
            {
                insidePointIndex[h][i][1] = insidePointIndex[h][i - 1][0] + insidePointIndex[h][i - 1][1];
            }

            if (insidePointIndex[h][i][0])
            {
                insidePointIndex[h][insidePointIndex[h][i][1]][2] = i;
            }
        }
        for (int i = MAX_FACTOR / 2; i >= 0; -- i)
        {
            if (insidePointIndex[h][i][0])
            {
                insidePointIndex[h][insidePointIndex[h][MAX_FACTOR / 2 + 1][1] - insidePointIndex[h][i + 1][1]][3] = i;
            }
        }
    }
    for (int h = 0; h <= MAX_FACTOR / 2; ++ h)
    {
        for (int i = 0; i <= MAX_FACTOR / 2; ++ i)
        {
            outsidePointIndex[h][i][0] = finalPointPositionTable[i] < h;
        }
        outsidePointIndex[h][MAX_FACTOR / 2 + 1][0] = 0;
        for (int i = 0; i <= MAX_FACTOR / 2 + 1; ++ i)
        {
            if (i == 0)
            {
                outsidePointIndex[h][i][1] = 0;
            }
            else
            {
                outsidePointIndex[h][i][1] = outsidePointIndex[h][i - 1][0] + outsidePointIndex[h][i - 1][1];
            }

            if (outsidePointIndex[h][i][0])
            {
                outsidePointIndex[h][outsidePointIndex[h][i][1]][2] = i;
            }
        }
        for (int i = MAX_FACTOR / 2; i >= 0; -- i)
        {
            if (outsidePointIndex[h][i][0])
            {
                outsidePointIndex[h][outsidePointIndex[h][MAX_FACTOR / 2 + 1][1] - outsidePointIndex[h][i + 1][1]][3] = i;
            }
        }
    }
}


//--------------------------------------------------------------------------------------
CTessellator::CTessellator() :
    m_pd3dDevice(nullptr),
    m_pd3dImmediateContext(nullptr),
    m_pBaseVB(nullptr),
    m_pBaseVBSRV(nullptr),
    m_pEdgeFactorBufSRV(nullptr),
    m_pEdgeFactorBufUAV(nullptr),
    m_pScanBuf0(nullptr),
    m_pScanBuf0SRV(nullptr),
    m_pScanBuf0UAV(nullptr),
    m_pScatterVertexBuf(nullptr),
    m_pScatterIndexBuf(nullptr),
    m_pScatterVertexBufSRV(nullptr),
    m_pScatterIndexBufSRV(nullptr),
    m_pScatterVertexBufUAV(nullptr),
    m_pScatterIndexBufUAV(nullptr),
    m_pTessedVerticesBufSRV(nullptr),
    m_pTessedVerticesBufUAV(nullptr),
    m_pTessedIndicesBufUAV(nullptr),
    m_nCachedTessedVertices(0),
    m_nCachedTessedIndices(0),
    m_nVertices(0)
{
    InitLookupTables();
}


//--------------------------------------------------------------------------------------
CTessellator::~CTessellator()
{
    DeleteDeviceObjects();    
}


//--------------------------------------------------------------------------------------
void CTessellator::DeleteDeviceObjects()
{
    SAFE_RELEASE( m_pEdgeFactorBufSRV );
    SAFE_RELEASE( m_pEdgeFactorBufUAV );
    SAFE_RELEASE( m_pEdgeFactorBuf );
    SAFE_RELEASE( m_pScanBuf0 );
    SAFE_RELEASE( m_pScanBuf1 );
    SAFE_RELEASE( m_pScanBuf0SRV );
    SAFE_RELEASE( m_pScanBuf1SRV );
    SAFE_RELEASE( m_pScanBuf0UAV );
    SAFE_RELEASE( m_pScanBuf1UAV );
    SAFE_RELEASE( m_pScatterIndexBuf );
    SAFE_RELEASE( m_pScatterVertexBuf );
    SAFE_RELEASE( m_pScatterVertexBufSRV );
    SAFE_RELEASE( m_pScatterIndexBufSRV );
    SAFE_RELEASE( m_pScatterVertexBufUAV );
    SAFE_RELEASE( m_pScatterIndexBufUAV );
    SAFE_RELEASE( m_pTessedVerticesBufSRV );
    SAFE_RELEASE( m_pTessedVerticesBufUAV );
    SAFE_RELEASE( m_pTessedIndicesBufUAV );
    SAFE_RELEASE( m_pBaseVBSRV );
}


//--------------------------------------------------------------------------------------
HRESULT CTessellator::OnD3D11CreateDevice( ID3D11Device* pd3dDevice )
{
    HRESULT hr = S_OK;
    V_RETURN( s_ScanCS.OnD3D11CreateDevice( pd3dDevice ) );

    return hr;
}


//--------------------------------------------------------------------------------------
void CTessellator::OnDestroyDevice()
{
    s_ScanCS.OnD3D11DestroyDevice();
    
    DeleteDeviceObjects();
    
    SAFE_RELEASE( s_pCSReadBackBuf );
    SAFE_RELEASE( s_pEdgeFactorCSCB );
    SAFE_RELEASE( s_pLookupTableCSCB );
    SAFE_RELEASE( s_pCSCB );
    SAFE_RELEASE( s_pEdgeFactorCS );
    SAFE_RELEASE( s_pNumVerticesIndicesCSs[0] );
    SAFE_RELEASE( s_pNumVerticesIndicesCSs[1] );
    SAFE_RELEASE( s_pNumVerticesIndicesCSs[2] );
    SAFE_RELEASE( s_pNumVerticesIndicesCSs[3] );
    SAFE_RELEASE( s_pScatterVertexTriIDIndexIDCS );
    SAFE_RELEASE( s_pScatterIndexTriIDIndexIDCS );
    SAFE_RELEASE( s_pTessVerticesCSs[0] );
    SAFE_RELEASE( s_pTessVerticesCSs[1] );
    SAFE_RELEASE( s_pTessVerticesCSs[2] );
    SAFE_RELEASE( s_pTessVerticesCSs[3] );
    SAFE_RELEASE( s_pTessIndicesCSs[0] );
    SAFE_RELEASE( s_pTessIndicesCSs[1] );
    SAFE_RELEASE( s_pTessIndicesCSs[2] );
    SAFE_RELEASE( s_pTessIndicesCSs[3] );
}


//--------------------------------------------------------------------------------------
HRESULT CTessellator::OnD3D11ResizedSwapChain( const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc )
{
    const float adaptive_scale_in_pixels= 15.0f;
    m_tess_edge_len_scale.x= (pBackBufferSurfaceDesc->Width * 0.5f) / adaptive_scale_in_pixels;
    m_tess_edge_len_scale.y= (pBackBufferSurfaceDesc->Height * 0.5f) / adaptive_scale_in_pixels;

    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT CTessellator::CreateCSForPartitioningMode( PARTITIONING_MODE mode, 
                                                   ID3D11ComputeShader** pNumVerticesIndicesCS, ID3D11ComputeShader** pTessVerticesCS, ID3D11ComputeShader** pTessIndicesCS )
{
    HRESULT hr;
    
    LPCSTR define_map[] = { "D3D11_TESSELLATOR_PARTITIONING_INTEGER", "D3D11_TESSELLATOR_PARTITIONING_POW2", 
                            "D3D11_TESSELLATOR_PARTITIONING_FRACTIONAL_ODD", "D3D11_TESSELLATOR_PARTITIONING_FRACTIONAL_EVEN" };

    D3D_SHADER_MACRO define[] = 
    {
        { "g_partitioning", define_map[mode] },
        { nullptr, nullptr }
    };

    ID3DBlob* pBlobCS = nullptr;
    V_RETURN( DXUTCompileFromFile( L"TessellatorCS40_NumVerticesIndicesCS.hlsl", define, "CSNumVerticesIndices", "cs_4_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &pBlobCS ) );
    V_RETURN( m_pd3dDevice->CreateComputeShader( pBlobCS->GetBufferPointer(), pBlobCS->GetBufferSize(), nullptr, pNumVerticesIndicesCS ) );
    SAFE_RELEASE( pBlobCS );
    DXUT_SetDebugName( *pNumVerticesIndicesCS, "CSNumVerticesIndices" );

    V_RETURN( DXUTCompileFromFile( L"TessellatorCS40_TessellateVerticesCS.hlsl", define, "CSTessellationVertices", "cs_4_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &pBlobCS ) );
    V_RETURN( m_pd3dDevice->CreateComputeShader( pBlobCS->GetBufferPointer(), pBlobCS->GetBufferSize(), nullptr, pTessVerticesCS ) );
    SAFE_RELEASE( pBlobCS );
    DXUT_SetDebugName( *pTessVerticesCS, "CSTessellationVertices" );

    V_RETURN( DXUTCompileFromFile( L"TessellatorCS40_TessellateIndicesCS.hlsl", define, "CSTessellationIndices", "cs_4_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &pBlobCS ) );
    V_RETURN( m_pd3dDevice->CreateComputeShader( pBlobCS->GetBufferPointer(), pBlobCS->GetBufferSize(), nullptr, pTessIndicesCS ) );
    SAFE_RELEASE( pBlobCS );
    DXUT_SetDebugName( *pTessIndicesCS, "CSTessellationIndices" );

    return S_OK;
}


//--------------------------------------------------------------------------------------
HRESULT CTessellator::SetBaseMesh( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, 
                                   INT nVertices,
                                   ID3D11Buffer* pBaseVB )
{
    DeleteDeviceObjects();

    HRESULT hr;
    
    m_pd3dDevice = pd3dDevice;
    m_pd3dImmediateContext = pd3dImmediateContext;
    m_nVertices = nVertices;

    if ( !s_pEdgeFactorCS )
    {
        SetPartitioningMode( PARTITIONING_MODE_FRACTIONAL_EVEN );
        
        ID3DBlob* pBlobCS = nullptr;
        V_RETURN( DXUTCompileFromFile( L"TessellatorCS40_EdgeFactorCS.hlsl", nullptr, "CSEdgeFactor", "cs_4_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &pBlobCS ) );
        V_RETURN( pd3dDevice->CreateComputeShader( pBlobCS->GetBufferPointer(), pBlobCS->GetBufferSize(), nullptr, &s_pEdgeFactorCS ) );
        SAFE_RELEASE( pBlobCS );
        DXUT_SetDebugName( s_pEdgeFactorCS, "CSEdgeFactor" );

        V_RETURN( DXUTCompileFromFile( L"TessellatorCS40_ScatterIDCS.hlsl", nullptr, "CSScatterVertexTriIDIndexID", "cs_4_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &pBlobCS ) );
        V_RETURN( pd3dDevice->CreateComputeShader( pBlobCS->GetBufferPointer(), pBlobCS->GetBufferSize(), nullptr, &s_pScatterVertexTriIDIndexIDCS ) );
        SAFE_RELEASE( pBlobCS );
        DXUT_SetDebugName( s_pScatterVertexTriIDIndexIDCS, "CSScatterVertexTriIDIndexID" );

        V_RETURN( DXUTCompileFromFile( L"TessellatorCS40_ScatterIDCS.hlsl", nullptr, "CSScatterIndexTriIDIndexID", "cs_4_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &pBlobCS ) );
        V_RETURN( pd3dDevice->CreateComputeShader( pBlobCS->GetBufferPointer(), pBlobCS->GetBufferSize(), nullptr, &s_pScatterIndexTriIDIndexIDCS ) );
        SAFE_RELEASE( pBlobCS );
        DXUT_SetDebugName( s_pScatterIndexTriIDIndexIDCS, "CSScatterIndexTriIDIndexID" );

        V_RETURN( CreateCSForPartitioningMode( PARTITIONING_MODE_INTEGER, 
                                               &s_pNumVerticesIndicesCSs[PARTITIONING_MODE_INTEGER], 
                                               &s_pTessVerticesCSs[PARTITIONING_MODE_INTEGER], 
                                               &s_pTessIndicesCSs[PARTITIONING_MODE_INTEGER] ) );
        V_RETURN( CreateCSForPartitioningMode( PARTITIONING_MODE_POW2, 
                                               &s_pNumVerticesIndicesCSs[PARTITIONING_MODE_POW2], 
                                               &s_pTessVerticesCSs[PARTITIONING_MODE_POW2], 
                                               &s_pTessIndicesCSs[PARTITIONING_MODE_POW2] ) );
        V_RETURN( CreateCSForPartitioningMode( PARTITIONING_MODE_FRACTIONAL_ODD, 
                                               &s_pNumVerticesIndicesCSs[PARTITIONING_MODE_FRACTIONAL_ODD], 
                                               &s_pTessVerticesCSs[PARTITIONING_MODE_FRACTIONAL_ODD], 
                                               &s_pTessIndicesCSs[PARTITIONING_MODE_FRACTIONAL_ODD] ) );
        V_RETURN( CreateCSForPartitioningMode( PARTITIONING_MODE_FRACTIONAL_EVEN, 
                                               &s_pNumVerticesIndicesCSs[PARTITIONING_MODE_FRACTIONAL_EVEN], 
                                               &s_pTessVerticesCSs[PARTITIONING_MODE_FRACTIONAL_EVEN], 
                                               &s_pTessIndicesCSs[PARTITIONING_MODE_FRACTIONAL_EVEN] ) );


        // constant buffers used to pass parameters to CS
        D3D11_BUFFER_DESC Desc;
        
        int lut[(sizeof(insidePointIndex) + sizeof(outsidePointIndex)) / sizeof(int)];
        memcpy(&lut[0], &insidePointIndex[0][0][0], sizeof(insidePointIndex));
        memcpy(&lut[sizeof(insidePointIndex) / sizeof(int)], &outsidePointIndex[0][0][0], sizeof(outsidePointIndex));

        Desc.Usage = D3D11_USAGE_IMMUTABLE;
        Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        Desc.CPUAccessFlags = 0;
        Desc.MiscFlags = 0;
        Desc.ByteWidth = sizeof(lut);
        D3D11_SUBRESOURCE_DATA init_data;
        init_data.pSysMem = &lut[0];
        init_data.SysMemPitch = sizeof(lut);
        V_RETURN( pd3dDevice->CreateBuffer( &Desc, &init_data, &s_pLookupTableCSCB ) );
        DXUT_SetDebugName( s_pLookupTableCSCB, "Lookup Table" );

        Desc.Usage = D3D11_USAGE_DEFAULT;
        Desc.CPUAccessFlags = 0;
        Desc.ByteWidth = sizeof( CB_EdgeFactorCS );
        V_RETURN( pd3dDevice->CreateBuffer( &Desc, nullptr, &s_pEdgeFactorCSCB ) );
        DXUT_SetDebugName( s_pEdgeFactorCSCB, "Edge Factor" );

        Desc.ByteWidth = sizeof(INT) * 4;
        V_RETURN( pd3dDevice->CreateBuffer( &Desc, nullptr, &s_pCSCB ) );
        DXUT_SetDebugName( s_pCSCB, "sizeof(INT)*4" );

        // read back buffer
        Desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        Desc.Usage = D3D11_USAGE_STAGING;
        Desc.BindFlags = 0;
        Desc.MiscFlags = 0;
        Desc.ByteWidth = sizeof(INT) * 2;
        V_RETURN( pd3dDevice->CreateBuffer( &Desc, nullptr, &s_pCSReadBackBuf ) );
        DXUT_SetDebugName( s_pCSReadBackBuf, "Read Back Buffer" );
    }

    // shader resource view of base mesh vertex data 
    D3D11_SHADER_RESOURCE_VIEW_DESC DescRV = {};
    DescRV.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    DescRV.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    DescRV.Buffer.FirstElement = 0;
    DescRV.Buffer.NumElements = nVertices;
    V_RETURN( pd3dDevice->CreateShaderResourceView( pBaseVB, &DescRV, &m_pBaseVBSRV ) );
    DXUT_SetDebugName( m_pBaseVBSRV, "Base VB" );

    // Buffer for edge tessellation factor
    D3D11_BUFFER_DESC desc = {};
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    desc.ByteWidth = sizeof(float) * m_nVertices / 3 * 4;
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride = sizeof(float) * 4;
    desc.Usage = D3D11_USAGE_DEFAULT;
    V_RETURN( pd3dDevice->CreateBuffer(&desc, nullptr, &m_pEdgeFactorBuf) );    
    DXUT_SetDebugName( m_pEdgeFactorBuf, "Edge Tessellation Factor" );

    // shader resource view of the buffer above
    ZeroMemory( &DescRV, sizeof( DescRV ) );
    DescRV.Format = DXGI_FORMAT_UNKNOWN;
    DescRV.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    DescRV.Buffer.FirstElement = 0;
    DescRV.Buffer.NumElements = m_nVertices / 3;
    V_RETURN( pd3dDevice->CreateShaderResourceView( m_pEdgeFactorBuf, &DescRV, &m_pEdgeFactorBufSRV ) );
    DXUT_SetDebugName( m_pEdgeFactorBufSRV, "Edge Tessellation Factor SRV" );

    // UAV of the buffer above
    D3D11_UNORDERED_ACCESS_VIEW_DESC DescUAV = {};
    DescUAV.Format = DXGI_FORMAT_UNKNOWN;
    DescUAV.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    DescUAV.Buffer.FirstElement = 0;
    DescUAV.Buffer.NumElements = m_nVertices / 3;
    V_RETURN( pd3dDevice->CreateUnorderedAccessView( m_pEdgeFactorBuf, &DescUAV, &m_pEdgeFactorBufUAV ) );
    DXUT_SetDebugName( m_pEdgeFactorBufUAV, "Edge Tessellation Factor UAV" );
    
    // Buffers for scan
    desc.ByteWidth = sizeof(INT)* 2 * m_nVertices / 3;
    desc.StructureByteStride = sizeof(INT)* 2;
    V_RETURN( pd3dDevice->CreateBuffer(&desc, nullptr, &m_pScanBuf0) );
    V_RETURN( pd3dDevice->CreateBuffer(&desc, nullptr, &m_pScanBuf1) );

    DXUT_SetDebugName( m_pScanBuf0, "Scan0" );
    DXUT_SetDebugName( m_pScanBuf1, "Scan1" );

    // shader resource views of the scan buffers
    DescRV.Buffer.NumElements = m_nVertices / 3;
    V_RETURN( pd3dDevice->CreateShaderResourceView( m_pScanBuf0, &DescRV, &m_pScanBuf0SRV ) );
    V_RETURN( pd3dDevice->CreateShaderResourceView( m_pScanBuf1, &DescRV, &m_pScanBuf1SRV ) );

    DXUT_SetDebugName( m_pScanBuf0SRV, "Scan0 SRV" );
    DXUT_SetDebugName( m_pScanBuf1SRV, "Scan1 SRV" );

    // UAV of the scan buffers
    DescUAV.Buffer.NumElements = m_nVertices / 3;
    V_RETURN( pd3dDevice->CreateUnorderedAccessView( m_pScanBuf0, &DescUAV, &m_pScanBuf0UAV ) );
    V_RETURN( pd3dDevice->CreateUnorderedAccessView( m_pScanBuf1, &DescUAV, &m_pScanBuf1UAV ) );

    DXUT_SetDebugName( m_pScanBuf0UAV, "Scan0 UAV" );
    DXUT_SetDebugName( m_pScanBuf1UAV, "Scan1 UAV" );

    return S_OK;
}


//--------------------------------------------------------------------------------------
ID3D11Buffer* CreateAndCopyToDebugBuf( ID3D11Device* pDevice, ID3D11DeviceContext* pd3dImmediateContext, ID3D11Buffer* pBuffer )
{
    ID3D11Buffer* debugbuf = nullptr;

    D3D11_BUFFER_DESC desc = {};
    pBuffer->GetDesc( &desc );
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;
    if (FAILED(pDevice->CreateBuffer(&desc, nullptr, &debugbuf)))
        return nullptr;

    DXUT_SetDebugName( debugbuf, "Debug" );

    pd3dImmediateContext->CopyResource( debugbuf, pBuffer );

    return debugbuf;
}


//--------------------------------------------------------------------------------------
void RunComputeShader( ID3D11DeviceContext* pd3dImmediateContext,
                       ID3D11ComputeShader* pComputeShader,
                       UINT nNumViews, ID3D11ShaderResourceView** pShaderResourceViews, 
                       ID3D11Buffer* pNeverChangesCBCS,
                       ID3D11Buffer* pCBCS, void* pCSData, DWORD dwNumDataBytes,
                       ID3D11UnorderedAccessView* pUnorderedAccessView,
                       UINT X, UINT Y, UINT Z )
{
    pd3dImmediateContext->CSSetShader( pComputeShader, nullptr, 0 );
    pd3dImmediateContext->CSSetShaderResources( 0, nNumViews, pShaderResourceViews );
    pd3dImmediateContext->CSSetUnorderedAccessViews( 0, 1, &pUnorderedAccessView, nullptr );
    if ( pCBCS )
    {
        pd3dImmediateContext->UpdateSubresource(pCBCS, D3D11CalcSubresource(0, 0, 1), nullptr, pCSData,
                        dwNumDataBytes, dwNumDataBytes);
    }
    if ( pNeverChangesCBCS && pCBCS )
    {
        ID3D11Buffer* ppCB[2] = { pNeverChangesCBCS, pCBCS };
        pd3dImmediateContext->CSSetConstantBuffers( 0, 2, ppCB );
    }
    if ( pNeverChangesCBCS && !pCBCS )
    {
        ID3D11Buffer* ppCB[1] = { pNeverChangesCBCS };
        pd3dImmediateContext->CSSetConstantBuffers( 0, 1, ppCB );
    }
    if ( !pNeverChangesCBCS && pCBCS )
    {
        ID3D11Buffer* ppCB[1] = { pCBCS };
        pd3dImmediateContext->CSSetConstantBuffers( 0, 1, ppCB );
    }

    pd3dImmediateContext->Dispatch( X, Y, Z );

    ID3D11UnorderedAccessView* ppUAViewNULL[1] = { nullptr };
    pd3dImmediateContext->CSSetUnorderedAccessViews( 0, 1, ppUAViewNULL, nullptr );

    ID3D11ShaderResourceView* ppSRVNULL[3] = { nullptr, nullptr, nullptr };
    pd3dImmediateContext->CSSetShaderResources( 0, 3, ppSRVNULL );
    pd3dImmediateContext->CSSetConstantBuffers( 0, 0, nullptr );
}


//--------------------------------------------------------------------------------------
void CTessellator::PerEdgeTessellation( CXMMATRIX matWVP,
                                        ID3D11Buffer** ppTessedVerticesBuf, ID3D11Buffer** ppTessedIndicesBuf, 
                                        DWORD* num_tessed_vertices, DWORD* num_tessed_indices )
{
    HRESULT hr;
    
    // Update per-edge tessellation factors
    {
        CB_EdgeFactorCS cbCS;
        XMStoreFloat4x4( &cbCS.matWVP, matWVP );
        cbCS.tess_edge_length_scale = m_tess_edge_len_scale;
        cbCS.num_triangles = m_nVertices/3;

        ID3D11ShaderResourceView* aRViews[1] = { m_pBaseVBSRV };
        RunComputeShader( m_pd3dImmediateContext, 
                          s_pEdgeFactorCS,
                          1, aRViews,
                          nullptr,
                          s_pEdgeFactorCSCB, &cbCS, sizeof(cbCS),
                          m_pEdgeFactorBufUAV,
                          INT(ceil(m_nVertices/3 / 128.0f)), 1, 1 );
    }    

    // How many vertices/indices are needed for the tessellated mesh?
    {
        INT cbCS[4] = {m_nVertices/3, 0, 0, 0};
        ID3D11ShaderResourceView* aRViews[1] = { m_pEdgeFactorBufSRV };
        RunComputeShader( m_pd3dImmediateContext,
                          s_pNumVerticesIndicesCSs[m_PartitioningMode],
                          1, aRViews,
                          s_pLookupTableCSCB,
                          s_pCSCB, cbCS, sizeof(INT)*4,
                          m_pScanBuf0UAV,
                          INT(ceil(m_nVertices/3 / 128.0f)), 1, 1 );
        s_ScanCS.ScanCS( m_pd3dImmediateContext, m_nVertices/3, m_pScanBuf0SRV, m_pScanBuf0UAV, m_pScanBuf1SRV, m_pScanBuf1UAV );

        // read back the number of vertices/indices for tessellation output
        D3D11_BOX box;
        box.left = sizeof(INT)*2 * m_nVertices/3 - sizeof(INT)*2;
        box.right = sizeof(INT)*2 * m_nVertices/3;
        box.top = 0;
        box.bottom = 1;
        box.front = 0;
        box.back = 1;
        m_pd3dImmediateContext->CopySubresourceRegion(s_pCSReadBackBuf, 0, 0, 0, 0, m_pScanBuf0, 0, &box);
        D3D11_MAPPED_SUBRESOURCE MappedResource; 
        V( m_pd3dImmediateContext->Map( s_pCSReadBackBuf, 0, D3D11_MAP_READ, 0, &MappedResource ) );       
        *num_tessed_vertices = ((DWORD*)MappedResource.pData)[0];
        *num_tessed_indices = ((DWORD*)MappedResource.pData)[1];
        m_pd3dImmediateContext->Unmap( s_pCSReadBackBuf, 0 );
    }

    if ( *num_tessed_vertices == 0 || *num_tessed_indices == 0 )
        return;

    // Turn on this and set a breakpoint on the line beginning with "p = " and see what has been written to m_pScanBuf0
#if 0
    {
        ID3D11Buffer* debugbuf = CreateAndCopyToDebugBuf( m_pd3dDevice, m_pd3dImmediateContext, m_pScanBuf0 );
        D3D11_MAPPED_SUBRESOURCE MappedResource; 
        struct VT
        {
            UINT v, t;
        } *p;
        V( m_pd3dImmediateContext->Map( debugbuf, 0, D3D11_MAP_READ, 0, &MappedResource ) );
        p = (VT*)MappedResource.pData;
        m_pd3dImmediateContext->Unmap( debugbuf, 0 );

        SAFE_RELEASE( debugbuf );
    }
#endif


    // Generate buffers for scattering TriID and IndexID for both vertex data and index data,
    // also generate buffers for output tessellated vertex data and index data
    {
        if ( !m_pScatterVertexBuf || m_nCachedTessedVertices < *num_tessed_vertices )
        {
            SAFE_RELEASE( m_pScatterVertexBuf );
            SAFE_RELEASE( m_pScatterVertexBufSRV );
            SAFE_RELEASE( m_pScatterVertexBufUAV );

            SAFE_RELEASE( *ppTessedVerticesBuf );
            SAFE_RELEASE( m_pTessedVerticesBufUAV );
            SAFE_RELEASE( m_pTessedVerticesBufSRV );
            
            D3D11_BUFFER_DESC desc = {};
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
            desc.ByteWidth = sizeof(INT) * 2 * *num_tessed_vertices;
            desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
            desc.StructureByteStride = sizeof(INT) * 2;
            desc.Usage = D3D11_USAGE_DEFAULT;
            V( m_pd3dDevice->CreateBuffer(&desc, nullptr, &m_pScatterVertexBuf) );  
            DXUT_SetDebugName( m_pScatterVertexBuf, "ScatterVB" );

            D3D11_SHADER_RESOURCE_VIEW_DESC DescRV = {};
            DescRV.Format = DXGI_FORMAT_UNKNOWN;
            DescRV.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
            DescRV.Buffer.FirstElement = 0;
            DescRV.Buffer.NumElements = *num_tessed_vertices;
            V( m_pd3dDevice->CreateShaderResourceView( m_pScatterVertexBuf, &DescRV, &m_pScatterVertexBufSRV ) );
            DXUT_SetDebugName( m_pScatterVertexBufSRV, "ScatterVB SRV" );

            D3D11_UNORDERED_ACCESS_VIEW_DESC DescUAV = {};
            DescUAV.Format = DXGI_FORMAT_UNKNOWN;
            DescUAV.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
            DescUAV.Buffer.FirstElement = 0;
            DescUAV.Buffer.NumElements = *num_tessed_vertices;
            V( m_pd3dDevice->CreateUnorderedAccessView( m_pScatterVertexBuf, &DescUAV, &m_pScatterVertexBufUAV ) );
            DXUT_SetDebugName( m_pScatterVertexBufUAV, "ScatterVB UAV" );

            // generate the output tessellated vertices buffer
            ZeroMemory( &desc, sizeof(desc) );
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
            desc.ByteWidth = sizeof(float) * 3 * *num_tessed_vertices;
            desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
            desc.StructureByteStride = sizeof(float) * 3;
            desc.Usage = D3D11_USAGE_DEFAULT;
            V( m_pd3dDevice->CreateBuffer(&desc, nullptr, ppTessedVerticesBuf) );
            V( m_pd3dDevice->CreateUnorderedAccessView( *ppTessedVerticesBuf, &DescUAV, &m_pTessedVerticesBufUAV ) );
            V( m_pd3dDevice->CreateShaderResourceView( *ppTessedVerticesBuf, &DescRV, &m_pTessedVerticesBufSRV ) );

            DXUT_SetDebugName( *ppTessedVerticesBuf, "TessedVB" );
            DXUT_SetDebugName( m_pTessedVerticesBufUAV, "TessedVB UAV" );
            DXUT_SetDebugName( m_pTessedVerticesBufSRV, "TessedVB SRV" );

            m_nCachedTessedVertices = *num_tessed_vertices;
        }

        if ( !m_pScatterIndexBuf || m_nCachedTessedIndices < *num_tessed_indices )
        {
            SAFE_RELEASE( m_pScatterIndexBuf );
            SAFE_RELEASE( m_pScatterIndexBufSRV );
            SAFE_RELEASE( m_pScatterIndexBufUAV );

            SAFE_RELEASE( *ppTessedIndicesBuf );
            SAFE_RELEASE( m_pTessedIndicesBufUAV );
            
            D3D11_BUFFER_DESC desc = {};
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
            desc.ByteWidth = sizeof(INT) * 2 * *num_tessed_indices;
            desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
            desc.StructureByteStride = sizeof(INT) * 2;
            desc.Usage = D3D11_USAGE_DEFAULT;
            V( m_pd3dDevice->CreateBuffer(&desc, nullptr, &m_pScatterIndexBuf) ); 
            DXUT_SetDebugName( m_pScatterIndexBuf, "ScatterIB" );

            D3D11_SHADER_RESOURCE_VIEW_DESC DescRV = {};
            DescRV.Format = DXGI_FORMAT_UNKNOWN;
            DescRV.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
            DescRV.Buffer.FirstElement = 0;
            DescRV.Buffer.NumElements = *num_tessed_indices;
            V( m_pd3dDevice->CreateShaderResourceView( m_pScatterIndexBuf, &DescRV, &m_pScatterIndexBufSRV ) );
            DXUT_SetDebugName( m_pScatterIndexBufSRV, "ScatterIB SRV" );

            D3D11_UNORDERED_ACCESS_VIEW_DESC DescUAV = {};
            DescUAV.Format = DXGI_FORMAT_UNKNOWN;
            DescUAV.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
            DescUAV.Buffer.FirstElement = 0;
            DescUAV.Buffer.NumElements = *num_tessed_indices;
            V( m_pd3dDevice->CreateUnorderedAccessView( m_pScatterIndexBuf, &DescUAV, &m_pScatterIndexBufUAV ) );
            DXUT_SetDebugName( m_pScatterIndexBufUAV, "ScatterIB UAV" );

            // generate the output tessellated indices buffer
            ZeroMemory( &desc, sizeof(desc) );
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_INDEX_BUFFER;
            desc.ByteWidth = sizeof(UINT) * *num_tessed_indices;
            desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
            desc.Usage = D3D11_USAGE_DEFAULT;
            V( m_pd3dDevice->CreateBuffer(&desc, nullptr, ppTessedIndicesBuf) );
            DXUT_SetDebugName( *ppTessedIndicesBuf, "TessedIB" );

            DescUAV.Format = DXGI_FORMAT_R32_TYPELESS;
            DescUAV.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
            V( m_pd3dDevice->CreateUnorderedAccessView( *ppTessedIndicesBuf, &DescUAV, &m_pTessedIndicesBufUAV ) );
            DXUT_SetDebugName( m_pTessedIndicesBufUAV, "TessedIB UAV" );

            m_nCachedTessedIndices = *num_tessed_indices;
        }
    }

    // Scatter TriID, IndexID
    {
        INT cbCS[4] = {m_nVertices/3, 0, 0, 0};

        // Scatter vertex TriID, IndexID
        ID3D11ShaderResourceView* aRViews[1] = { m_pScanBuf0SRV };
        RunComputeShader( m_pd3dImmediateContext,
                          s_pScatterVertexTriIDIndexIDCS,
                          1, aRViews,
                          nullptr,
                          s_pCSCB, cbCS, sizeof(INT)*4,
                          m_pScatterVertexBufUAV,
                          INT(ceil(m_nVertices/3 / 128.0f)), 1, 1 );
        
        // Scatter index TriID, IndexID
        RunComputeShader( m_pd3dImmediateContext,
                          s_pScatterIndexTriIDIndexIDCS,
                          1, aRViews,
                          nullptr,
                          s_pCSCB, cbCS, sizeof(INT)*4,
                          m_pScatterIndexBufUAV,
                          INT(ceil(m_nVertices/3 / 128.0f)), 1, 1 );
    }

    // Turn on this and set a breakpoint on the line beginning with "p = " and see what has been written to m_pScatterVertexBuf
#if 0
    {
        ID3D11Buffer* debugbuf = CreateAndCopyToDebugBuf( m_pd3dDevice, m_pd3dImmediateContext , m_pScatterVertexBuf );
        D3D11_MAPPED_SUBRESOURCE MappedResource; 
        struct VT
        {
            UINT v, t;
        } *p;
        V( m_pd3dImmediateContext->Map( debugbuf, 0, D3D11_MAP_READ, 0, &MappedResource ) );
        p = (VT*)MappedResource.pData;
        m_pd3dImmediateContext->Unmap( debugbuf, 0 );

        SAFE_RELEASE( debugbuf );        
    }
#endif

    // Tessellate vertex
    {
        INT cbCS[4] = { static_cast<INT>(*num_tessed_vertices), 0, 0, 0};
        ID3D11ShaderResourceView* aRViews[2] = { m_pScatterVertexBufSRV, m_pEdgeFactorBufSRV };
        RunComputeShader( m_pd3dImmediateContext,
                          s_pTessVerticesCSs[m_PartitioningMode],
                          2, aRViews,
                          s_pLookupTableCSCB,
                          s_pCSCB, cbCS, sizeof(INT)*4,
                          m_pTessedVerticesBufUAV,
                          INT(ceil(*num_tessed_vertices/128.0f)), 1, 1 );
    }

    // Turn on this and set a breakpoint on the line beginning with "p = " and see what has been written to *ppTessedVerticesBuf
#if 0
    {
        ID3D11Buffer* debugbuf = CreateAndCopyToDebugBuf( m_pd3dDevice, m_pd3dImmediateContext, *ppTessedVerticesBuf );
        D3D11_MAPPED_SUBRESOURCE MappedResource; 
        struct VT
        {
            UINT id;
            float u; float v;
        } *p;
        V( m_pd3dImmediateContext->Map( debugbuf, 0, D3D11_MAP_READ, 0, &MappedResource ) );
        p = (VT*)MappedResource.pData;
        m_pd3dImmediateContext->Unmap( debugbuf, 0 );

        SAFE_RELEASE( debugbuf );
    }
#endif

    // Tessellate indices
    {
        INT cbCS[4] = { static_cast<INT>(*num_tessed_indices), 0, 0, 0};
        ID3D11ShaderResourceView* aRViews[3] = { m_pScatterIndexBufSRV, m_pEdgeFactorBufSRV, m_pScanBuf0SRV };
        RunComputeShader( m_pd3dImmediateContext,
            s_pTessIndicesCSs[m_PartitioningMode],
            3, aRViews,
            s_pLookupTableCSCB,
            s_pCSCB, cbCS, sizeof(INT)*4,
            m_pTessedIndicesBufUAV,
            INT(ceil(*num_tessed_indices/128.0f)), 1, 1 );
    }

    // Turn on this and set a breakpoint on the line beginning with "p = " and see what has been written to *ppTessedIndicesBuf
#if 0
    {
        ID3D11Buffer* debugbuf = CreateAndCopyToDebugBuf( m_pd3dDevice, m_pd3dImmediateContext, *ppTessedIndicesBuf );
        D3D11_MAPPED_SUBRESOURCE MappedResource; 
        INT *p = nullptr;
        V( m_pd3dImmediateContext->Map( debugbuf, 0, D3D11_MAP_READ, 0, &MappedResource ) );
        p = (INT *)MappedResource.pData;
        m_pd3dImmediateContext->Unmap( debugbuf, 0 );

        SAFE_RELEASE( debugbuf );
    }
#endif
}
