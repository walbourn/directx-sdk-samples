//--------------------------------------------------------------------------------------
// File: TessellatorCS40.h
//
// Demos how to use Compute Shader 4.0 to do one simple adaptive tessellation scheme
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "ScanCS.h"

class CTessellator
{
public:
    CTessellator();
    ~CTessellator();

    HRESULT SetBaseMesh( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, 
                         INT nVertices,
                         ID3D11Buffer* pBaseVB );

    enum PARTITIONING_MODE
    {
        PARTITIONING_MODE_INTEGER,	
        PARTITIONING_MODE_POW2,				
        PARTITIONING_MODE_FRACTIONAL_ODD,
        PARTITIONING_MODE_FRACTIONAL_EVEN,
    };
    void SetPartitioningMode( PARTITIONING_MODE mode ) { m_PartitioningMode = mode; }
    void PerEdgeTessellation( DirectX::CXMMATRIX matWVP,
                              ID3D11Buffer** ppTessedVerticesBuf, ID3D11Buffer** ppTessedIndicesBuf, 
                              DWORD* num_tessed_vertices, DWORD* num_tessed_indices);

    HRESULT OnD3D11CreateDevice( ID3D11Device* pd3dDevice );
    void OnDestroyDevice();
    HRESULT OnD3D11ResizedSwapChain( const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc );

    void DeleteDeviceObjects();

    ID3D11ShaderResourceView*   m_pBaseVBSRV;
    ID3D11ShaderResourceView*   m_pTessedVerticesBufSRV;

    INT                         m_nVertices;

private:
    ID3D11Device*               m_pd3dDevice;
    ID3D11DeviceContext*        m_pd3dImmediateContext;

    ID3D11Buffer*               m_pBaseVB;

    ID3D11Buffer*               m_pEdgeFactorBuf;
    ID3D11ShaderResourceView*   m_pEdgeFactorBufSRV;
    ID3D11UnorderedAccessView*  m_pEdgeFactorBufUAV;

    ID3D11Buffer*               m_pScanBuf0;
    ID3D11Buffer*               m_pScanBuf1;
    ID3D11ShaderResourceView*   m_pScanBuf0SRV;
    ID3D11ShaderResourceView*   m_pScanBuf1SRV;
    ID3D11UnorderedAccessView*  m_pScanBuf0UAV;
    ID3D11UnorderedAccessView*  m_pScanBuf1UAV;

    ID3D11Buffer*               m_pScatterVertexBuf;    
    ID3D11Buffer*               m_pScatterIndexBuf;
    ID3D11ShaderResourceView*   m_pScatterVertexBufSRV;
    ID3D11ShaderResourceView*   m_pScatterIndexBufSRV;
    ID3D11UnorderedAccessView*  m_pScatterVertexBufUAV;
    ID3D11UnorderedAccessView*  m_pScatterIndexBufUAV;

    DirectX::XMFLOAT2           m_tess_edge_len_scale;

    UINT                        m_nCachedTessedVertices;
    UINT                        m_nCachedTessedIndices;
    
    ID3D11UnorderedAccessView*  m_pTessedVerticesBufUAV;
    ID3D11UnorderedAccessView*  m_pTessedIndicesBufUAV;

    static ID3D11ComputeShader* s_pEdgeFactorCS;
    static ID3D11ComputeShader* s_pNumVerticesIndicesCSs[4];
    static ID3D11ComputeShader* s_pScatterVertexTriIDIndexIDCS;
    static ID3D11ComputeShader* s_pScatterIndexTriIDIndexIDCS;
    static ID3D11ComputeShader* s_pTessVerticesCSs[4];
    static ID3D11ComputeShader* s_pTessIndicesCSs[4];
    static ID3D11Buffer*        s_pEdgeFactorCSCB;
    static ID3D11Buffer*        s_pLookupTableCSCB;
    static ID3D11Buffer*        s_pCSCB;
    static ID3D11Buffer*        s_pCSReadBackBuf;

    PARTITIONING_MODE           m_PartitioningMode;

    struct CB_EdgeFactorCS
    {
        DirectX::XMFLOAT4X4 matWVP;
        DirectX::XMFLOAT2 tess_edge_length_scale;
        int num_triangles;
        float dummy;
    };

    static CScanCS              s_ScanCS;

    HRESULT CreateCSForPartitioningMode( PARTITIONING_MODE mode, 
        ID3D11ComputeShader** pNumVerticesIndicesCS, ID3D11ComputeShader** pTessVerticesCS, ID3D11ComputeShader** pTessIndicesCS );
};
