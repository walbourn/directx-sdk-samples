//--------------------------------------------------------------------------------------
// File: BC7EncoderCS10.cpp
//
// Compute Shader 4.0 Accelerated BC7 Encoder
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <d3d11.h>
#include <vector>
#include "utils.h"
#include "EncoderBase.h"
#include "BC7EncoderCS10.h"

namespace
{
#include "Shaders\Compiled\BC7Encode_EncodeBlockCS.inc"
#include "Shaders\Compiled\BC7Encode_TryMode02CS.inc"
#include "Shaders\Compiled\BC7Encode_TryMode137CS.inc"
#include "Shaders\Compiled\BC7Encode_TryMode456CS.inc"
}

//--------------------------------------------------------------------------------------
// Initialize the encoder
//--------------------------------------------------------------------------------------
HRESULT CGPUBC7Encoder::Initialize( ID3D11Device* pDevice, ID3D11DeviceContext* pContext )
{
    HRESULT hr = S_OK;    
    
    V_RETURN( EncoderBase::Initialize( pDevice, pContext ) );
    
    // Compile and create Compute Shader 
    V_RETURN( pDevice->CreateComputeShader( BC7Encode_TryMode456CS, sizeof(BC7Encode_TryMode456CS), nullptr, &m_pTryMode456CS ) );
    V_RETURN( pDevice->CreateComputeShader( BC7Encode_TryMode137CS, sizeof(BC7Encode_TryMode137CS), nullptr, &m_pTryMode137CS ) );
    V_RETURN( pDevice->CreateComputeShader( BC7Encode_TryMode02CS, sizeof(BC7Encode_TryMode02CS), nullptr, &m_pTryMode02CS ) );
    V_RETURN( pDevice->CreateComputeShader( BC7Encode_EncodeBlockCS, sizeof(BC7Encode_EncodeBlockCS), nullptr, &m_pEncodeBlockCS ) );

#if defined(_DEBUG) || defined(PROFILE)
    if (m_pTryMode456CS)
        m_pTryMode456CS->SetPrivateData( WKPDID_D3DDebugObjectName, sizeof( "BC7Encode_TryMode456CS" ) - 1, "BC7Encode_TryMode456CS" );
    if (m_pTryMode137CS)
        m_pTryMode137CS->SetPrivateData( WKPDID_D3DDebugObjectName, sizeof( "BC7Encode_TryMode137CS" ) - 1, "BC7Encode_TryMode137CS" );
    if (m_pTryMode02CS)
        m_pTryMode02CS->SetPrivateData( WKPDID_D3DDebugObjectName, sizeof( "BC7Encode_TryMode02CS" ) - 1, "BC7Encode_TryMode02CS" );
    if (m_pEncodeBlockCS)
        m_pEncodeBlockCS->SetPrivateData( WKPDID_D3DDebugObjectName, sizeof( "BC7Encode_EncodeBlockCS" ) -1, "BC7Encode_EncodeBlockCS" );
#endif

    return hr;
}

//--------------------------------------------------------------------------------------
// Cleanup before exit
//--------------------------------------------------------------------------------------
void CGPUBC7Encoder::Cleanup()
{    
    SAFE_RELEASE( m_pTryMode456CS );
    SAFE_RELEASE( m_pTryMode137CS );
    SAFE_RELEASE( m_pTryMode02CS );
    SAFE_RELEASE( m_pEncodeBlockCS );
}

//--------------------------------------------------------------------------------------
// Encode the source texture to BC7 and store the result in a buffer
// The source texture can only have 1 sub resource, i.e. it must be a signle 2D texture which has only 1 mip level
// The job of breaking down texture arrays, or texture with multiple mip levels is taken care of in the base class
//--------------------------------------------------------------------------------------
HRESULT CGPUBC7Encoder::GPU_Encode( ID3D11Device* pDevice, ID3D11DeviceContext* pContext,
                                    ID3D11Texture2D* pSrcTexture, 
                                    DXGI_FORMAT dstFormat, ID3D11Buffer** ppDstTextureAsBufOut )
{
    ID3D11ShaderResourceView* pSRV = nullptr;
    ID3D11Buffer* pErrBestModeBuffer[2] = { nullptr, nullptr };
    ID3D11UnorderedAccessView* pUAV = nullptr;
    ID3D11UnorderedAccessView* pErrBestModeUAV[2] = { nullptr, nullptr };
    ID3D11ShaderResourceView* pErrBestModeSRV[2] = { nullptr, nullptr };
    ID3D11Buffer* pCBCS = nullptr;

    if ( !(dstFormat == DXGI_FORMAT_BC7_UNORM || dstFormat == DXGI_FORMAT_BC7_UNORM_SRGB) || 
         !ppDstTextureAsBufOut )
    {
        return E_INVALIDARG;
    }

    D3D11_TEXTURE2D_DESC texSrcDesc;
    pSrcTexture->GetDesc( &texSrcDesc );

    HRESULT hr = S_OK;

    // Create a SRV for input texture        
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
        SRVDesc.Texture2D.MipLevels = texSrcDesc.MipLevels;
        SRVDesc.Texture2D.MostDetailedMip = 0;
        SRVDesc.Format = texSrcDesc.Format;
        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        V_GOTO( pDevice->CreateShaderResourceView( pSrcTexture, &SRVDesc, &pSRV ) );

#if defined(_DEBUG) || defined(PROFILE)
        if ( pSRV )
        {
            pSRV->SetPrivateData( WKPDID_D3DDebugObjectName, sizeof( "BC7 SRV" ) - 1, "BC7 SRV" );
        }
#endif
    }

    // Create output buffer    
    D3D11_BUFFER_DESC sbOutDesc;
    {
        sbOutDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
        sbOutDesc.CPUAccessFlags = 0;
        sbOutDesc.Usage = D3D11_USAGE_DEFAULT;
        sbOutDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        sbOutDesc.StructureByteStride = sizeof( BufferBC6HBC7 );
        sbOutDesc.ByteWidth = texSrcDesc.Height * texSrcDesc.Width * sizeof( BufferBC6HBC7 ) / BLOCK_SIZE;
        //+ texSrcDesc.Height * texSrcDesc.Width * sizeof( BufferBC7 ) * 5;//For dump
        V_GOTO( pDevice->CreateBuffer(&sbOutDesc, nullptr, ppDstTextureAsBufOut) );
        V_GOTO( pDevice->CreateBuffer(&sbOutDesc, nullptr, &pErrBestModeBuffer[0]) );
        V_GOTO( pDevice->CreateBuffer(&sbOutDesc, nullptr, &pErrBestModeBuffer[1]) );

        _Analysis_assume_( pErrBestModeBuffer[0] != 0 );

#if defined(_DEBUG) || defined(PROFILE)
        if ( *ppDstTextureAsBufOut )
        {
            (*ppDstTextureAsBufOut)->SetPrivateData( WKPDID_D3DDebugObjectName, sizeof( "BC7 Dest" ) - 1, "BC7 Dest" );
        }
        if ( pErrBestModeBuffer[0] )
        {
            pErrBestModeBuffer[0]->SetPrivateData( WKPDID_D3DDebugObjectName, sizeof( "BC7 ErrBest0" ) - 1, "BC7 ErrBest0" );
        }
        if ( pErrBestModeBuffer[1] )
        {
            pErrBestModeBuffer[1]->SetPrivateData( WKPDID_D3DDebugObjectName, sizeof( "BC7 ErrBest1" ) - 1, "BC7 ErrBest1" );
        }
#endif
    }

    // Create UAV of the output texture    
    {
        D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
        UAVDesc.Buffer.FirstElement = 0;
        UAVDesc.Buffer.NumElements = sbOutDesc.ByteWidth / sbOutDesc.StructureByteStride;
        UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
        UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
#pragma warning (push)
#pragma warning (disable:6387)
        V_GOTO( pDevice->CreateUnorderedAccessView( *ppDstTextureAsBufOut, &UAVDesc, &pUAV ) );
        V_GOTO( pDevice->CreateUnorderedAccessView( pErrBestModeBuffer[0], &UAVDesc, &pErrBestModeUAV[0] ) );
        V_GOTO( pDevice->CreateUnorderedAccessView( pErrBestModeBuffer[1], &UAVDesc, &pErrBestModeUAV[1] ) );
#pragma warning (pop)

#if defined(_DEBUG) || defined(PROFILE)
        if ( pUAV )
        {
            pUAV->SetPrivateData( WKPDID_D3DDebugObjectName, sizeof( "BC7 Dest UAV" ) - 1, "BC7 Dest UAV" );
        }
        if ( pErrBestModeUAV[0] )
        {
            pErrBestModeUAV[0]->SetPrivateData( WKPDID_D3DDebugObjectName, sizeof( "BC7 ErrBest0 UAV" ) - 1, "BC7 ErrBest0 UAV" );
        }
        if ( pErrBestModeUAV[1] )
        {
            pErrBestModeUAV[1]->SetPrivateData( WKPDID_D3DDebugObjectName, sizeof( "BC7 ErrBest1 UAV" ) - 1, "BC7 ErrBest1 UAV" );
        }
#endif
    }
    
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
        SRVDesc.Buffer.FirstElement = 0;
        SRVDesc.Buffer.NumElements = texSrcDesc.Height * texSrcDesc.Width / BLOCK_SIZE;
        SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
#pragma warning (push)
#pragma warning (disable:6387)
        V_GOTO( pDevice->CreateShaderResourceView( pErrBestModeBuffer[0], &SRVDesc, &pErrBestModeSRV[0]) );
        V_GOTO( pDevice->CreateShaderResourceView( pErrBestModeBuffer[1], &SRVDesc, &pErrBestModeSRV[1]) );
#pragma warning (pop)

#if defined(_DEBUG) || defined(PROFILE)
        if ( pErrBestModeSRV[0] )
        {
            pErrBestModeSRV[0]->SetPrivateData( WKPDID_D3DDebugObjectName, sizeof( "BC7 ErrBest0 SRV" ) - 1, "BC7 ErrBest0 SRV" );
        }
        if ( pErrBestModeSRV[1] )
        {
            pErrBestModeSRV[1]->SetPrivateData( WKPDID_D3DDebugObjectName, sizeof( "BC7 ErrBest1 SRV" ) - 1, "BC7 ErrBest1 SRV" );
        }
#endif
    }

    // Create constant buffer    
    {
        D3D11_BUFFER_DESC cbDesc;
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        cbDesc.MiscFlags = 0;
        cbDesc.ByteWidth = sizeof( UINT ) * 8;
        V_GOTO( pDevice->CreateBuffer( &cbDesc, nullptr, &pCBCS ) );

#if defined(_DEBUG) || defined(PROFILE)
        if ( pCBCS )
        {
            pCBCS->SetPrivateData( WKPDID_D3DDebugObjectName, sizeof( "BC7Encode" ) - 1, "BC7Encode" );
        }
#endif
    }

    int const MAX_BLOCK_BATCH = 64;

    int num_total_blocks = texSrcDesc.Width / BLOCK_SIZE_X * texSrcDesc.Height / BLOCK_SIZE_Y;
    int num_blocks = num_total_blocks;
    int start_block_id = 0;
    while (num_blocks > 0)
    {
        int n = min(num_blocks, MAX_BLOCK_BATCH);
        UINT uThreadGroupCount = n;

        {
            D3D11_MAPPED_SUBRESOURCE cbMapped;
            pContext->Map( pCBCS, 0, D3D11_MAP_WRITE_DISCARD, 0, &cbMapped );

            UINT param[8];
            param[0] = texSrcDesc.Width;
            param[1] = texSrcDesc.Width / BLOCK_SIZE_X;
            param[2] = dstFormat;
            param[3] = 0;
            param[4] = start_block_id;
            param[5] = num_total_blocks;
            *((float*)&param[6]) = m_fAlphaWeight;
            memcpy( cbMapped.pData, param, sizeof( param ) );
            pContext->Unmap( pCBCS, 0 );
        }

        ID3D11ShaderResourceView* pSRVs[] = { pSRV, nullptr };
        RunComputeShader( pContext, m_pTryMode456CS, pSRVs, 2, pCBCS, pErrBestModeUAV[0], __max(uThreadGroupCount / 4, 1), 1, 1 );        

        for (int i = 0; i < 3; ++ i)
        {
            int modes[] = { 1, 3, 7 };
            {
                D3D11_MAPPED_SUBRESOURCE cbMapped;
                pContext->Map( pCBCS, 0, D3D11_MAP_WRITE_DISCARD, 0, &cbMapped );

                UINT param[8];
                param[0] = texSrcDesc.Width;
                param[1] = texSrcDesc.Width / BLOCK_SIZE_X;
                param[2] = dstFormat;
                param[3] = modes[i];
                param[4] = start_block_id;
                param[5] = num_total_blocks;
                *((float*)&param[6]) = m_fAlphaWeight;
                memcpy( cbMapped.pData, param, sizeof( param ) );
                pContext->Unmap( pCBCS, 0 );
            }

            pSRVs[1] = pErrBestModeSRV[i & 1];
            RunComputeShader( pContext, m_pTryMode137CS, pSRVs, 2, pCBCS,  pErrBestModeUAV[!(i & 1)], uThreadGroupCount, 1, 1 );
        }               

        for (int i = 0; i < 2; ++ i)
        {
            int modes[] = { 0, 2 };
            {
                D3D11_MAPPED_SUBRESOURCE cbMapped;
                pContext->Map( pCBCS, 0, D3D11_MAP_WRITE_DISCARD, 0, &cbMapped );

                UINT param[8];
                param[0] = texSrcDesc.Width;
                param[1] = texSrcDesc.Width / BLOCK_SIZE_X;
                param[2] = dstFormat;
                param[3] = modes[i];
                param[4] = start_block_id;
                param[5] = num_total_blocks;
                *((float*)&param[6]) = m_fAlphaWeight;
                memcpy( cbMapped.pData, param, sizeof( param ) );
                pContext->Unmap( pCBCS, 0 );
            }

            pSRVs[1] = pErrBestModeSRV[!(i & 1)];
            RunComputeShader( pContext, m_pTryMode02CS, pSRVs, 2, pCBCS,  pErrBestModeUAV[i & 1], uThreadGroupCount, 1, 1 );
        }

        pSRVs[1] = pErrBestModeSRV[1];
        RunComputeShader( pContext, m_pEncodeBlockCS, pSRVs, 2, pCBCS,  pUAV, __max(uThreadGroupCount / 4, 1), 1, 1 );        

        start_block_id += n;
        num_blocks -= n;
    }

quit:
    SAFE_RELEASE(pSRV);
    SAFE_RELEASE(pUAV);
    SAFE_RELEASE(pErrBestModeSRV[0]);
    SAFE_RELEASE(pErrBestModeSRV[1]);
    SAFE_RELEASE(pErrBestModeUAV[0]);
    SAFE_RELEASE(pErrBestModeUAV[1]);
    SAFE_RELEASE(pErrBestModeBuffer[0]);
    SAFE_RELEASE(pErrBestModeBuffer[1]);
    SAFE_RELEASE(pCBCS);

    return hr;
}