//-----------------------------------------------------------------------------
// File: OIT.h
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
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once

#include "Scene.h"

class OIT
{
public:
    OIT();

    HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pDevice );

    HRESULT OnD3D11ResizedSwapChain( const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, ID3D11Device* pDevice );
    void    OnD3D11ReleasingSwapChain();
    void    OnD3D11DestroyDevice();

    void    Render( ID3D11DeviceContext* pD3DContext, ID3D11Device* pDevice, CScene* pScene, DirectX::CXMMATRIX mWVP, ID3D11RenderTargetView* pRTV, ID3D11DepthStencilView* pDSV);
    

private:
    void    CreateFragmentCount( ID3D11DeviceContext* pD3DContext, CScene* pScene, DirectX::CXMMATRIX mWVP, ID3D11RenderTargetView* pRTV, ID3D11DepthStencilView* pDSV );
    void    CreatePrefixSum( ID3D11DeviceContext* pD3DContext );
    void    FillDeepBuffer( ID3D11DeviceContext* pD3DContext, ID3D11RenderTargetView* pRTV, ID3D11DepthStencilView* pDSV, CScene* pScene, DirectX::CXMMATRIX mWVP );
    void    SortAndRenderFragments( ID3D11DeviceContext* pD3DContext, ID3D11Device* pDevice, ID3D11RenderTargetView* pRTV );

protected:
    struct CS_CB
    {
        UINT nFrameWidth;
        UINT nFrameHeight;
        UINT nPassSize;
        UINT nReserved;
    };

    struct PS_CB
    {
        UINT nFrameWidth;
        UINT nFrameHeight;
        UINT nReserved0;
        UINT nReserved1;
    };    


    UINT m_nFrameHeight;
    UINT m_nFrameWidth;

    // Shaders
    ID3D11PixelShader*          m_pFragmentCountPS;        // Counts the number of fragments in each pixel
    ID3D11ComputeShader*        m_pCreatePrefixSum_Pass0_CS;    // Creates the prefix sum in two passes, converting the
    ID3D11ComputeShader*        m_pCreatePrefixSum_Pass1_CS;    //   two dimensional frame buffer to a 1D prefix sum
    ID3D11PixelShader*          m_pFillDeepBufferPS;            // Fills the deep frame buffer with depth and color values
    ID3D11ComputeShader*        m_pSortAndRenderCS;             // Sorts and renders the fragments to the final frame buffer
    
    // States
    ID3D11DepthStencilState*    m_pDepthStencilState;

    // Constant Buffers
    ID3D11Buffer*               m_pCS_CB;                   // Compute shader constant buffer
    ID3D11Buffer*               m_pPS_CB;                   // Pixel shader constant buffer

    ID3D11Texture2D*            m_pFragmentCountBuffer;     // Keeps a count of the number of fragments rendered to each pixel
    ID3D11Buffer*               m_pPrefixSum;               // Count of total fragments in the frame buffer preceding each pixel
    ID3D11Buffer*               m_pDeepBuffer;              // Buffer that holds the depth of each fragment
    ID3D11Buffer*               m_pDeepBufferColor;         // Buffer that holds the color of each fragment

    // Debug Buffers used to copy resources to main memory to view more easily
    ID3D11Buffer*               m_pPrefixSumDebug;          
    ID3D11Buffer*               m_pDeepBufferDebug;         
    ID3D11Buffer*               m_pDeepBufferColorDebug;    

    // Unordered Access views of the buffers
    ID3D11UnorderedAccessView*  m_pFragmentCountUAV;
    ID3D11UnorderedAccessView*  m_pPrefixSumUAV;         
    ID3D11UnorderedAccessView*  m_pDeepBufferUAV;        
    ID3D11UnorderedAccessView*  m_pDeepBufferColorUAV;   
    ID3D11UnorderedAccessView*  m_pDeepBufferColorUAV_UINT; // Used to veiw the color buffer as a single UINT instead of 4 bytes

    // Shader Resource Views
    ID3D11ShaderResourceView*   m_pFragmentCountRV;
};