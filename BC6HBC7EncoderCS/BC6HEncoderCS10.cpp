//--------------------------------------------------------------------------------------
// File: BC6HEncoderCS10.cpp
//
// Compute Shader 4.0 Accelerated BC6H Encoder
//
// Advanced Technology Group (ATG)
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License (MIT).
//--------------------------------------------------------------------------------------

#define NOMINMAX 1
#include <algorithm>
#include <tuple>
#include <vector>

#include <d3d11.h>
#include "EncoderBase.h"
#include "BC6HEncoderCS10.h"
#include <DirectXTex.h>
#include "utils.h"

namespace
{
    constexpr INT MAX_BLOCK_BATCH = 64;

    namespace cs5
    {
#include "BC6HEncode_EncodeBlockCS.inc"
#include "BC6HEncode_TryModeG10CS.inc"
#include "BC6HEncode_TryModeLE10CS.inc"
    }

    namespace cs4
    {
#include "BC6HEncode_EncodeBlockCS_cs40.inc"
#include "BC6HEncode_TryModeG10CS_cs40.inc"
#include "BC6HEncode_TryModeLE10CS_cs40.inc"
    }
}

//--------------------------------------------------------------------------------------
// Initialize the encoder
//--------------------------------------------------------------------------------------
HRESULT CGPUBC6HEncoder::Initialize( ID3D11Device* pDevice, ID3D11DeviceContext* pContext )
{
    HRESULT hr = S_OK;

    // Check for DirectCompute support
    V_RETURN( EncoderBase::Initialize( pDevice, pContext ) );

    // Compile and create Compute Shader
    const D3D_FEATURE_LEVEL fl = pDevice->GetFeatureLevel();

    // Modes 11-14
    auto blob = (fl >= D3D_FEATURE_LEVEL_11_0) ? cs5::BC6HEncode_TryModeG10CS : cs4::BC6HEncode_TryModeG10CS;
    auto blobSize = (fl >= D3D_FEATURE_LEVEL_11_0) ? sizeof(cs5::BC6HEncode_TryModeG10CS) : sizeof(cs4::BC6HEncode_TryModeG10CS);
    V_RETURN(pDevice->CreateComputeShader(blob, blobSize, nullptr, &m_pTryModeG10CS));

    // Modes 1-10
    blob = (fl >= D3D_FEATURE_LEVEL_11_0) ? cs5::BC6HEncode_TryModeLE10CS : cs4::BC6HEncode_TryModeLE10CS;
    blobSize = (fl >= D3D_FEATURE_LEVEL_11_0) ? sizeof(cs5::BC6HEncode_TryModeLE10CS) : sizeof(cs4::BC6HEncode_TryModeLE10CS);
    V_RETURN(pDevice->CreateComputeShader(blob, blobSize, nullptr, &m_pTryModeLE10CS));

    // Encode
    blob = (fl >= D3D_FEATURE_LEVEL_11_0) ? cs5::BC6HEncode_EncodeBlockCS : cs4::BC6HEncode_EncodeBlockCS;
    blobSize = (fl >= D3D_FEATURE_LEVEL_11_0) ? sizeof(cs5::BC6HEncode_EncodeBlockCS) : sizeof(cs4::BC6HEncode_EncodeBlockCS);
    V_RETURN(pDevice->CreateComputeShader(blob, blobSize, nullptr, &m_pEncodeBlockCS));

#if defined(_DEBUG) || defined(PROFILE)
    if (m_pTryModeG10CS)
    {
        std::ignore = m_pTryModeG10CS->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof("BC6HEncode_TryModeG10CS") - 1, "BC6HEncode_TryModeG10CS");
    }
    if (m_pTryModeLE10CS)
    {
        std::ignore = m_pTryModeLE10CS->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof("BC6HEncode_TryModeLE10CS") - 1, "BC6HEncode_TryModeLE10CS");
    }
    if (m_pEncodeBlockCS)
    {
        std::ignore = m_pEncodeBlockCS->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof("BC6HEncode_EncodeBlockCS") - 1, "BC6HEncode_EncodeBlockCS");
    }
#endif

    return hr;
}

//--------------------------------------------------------------------------------------
// Cleanup before exit
//--------------------------------------------------------------------------------------
void CGPUBC6HEncoder::Cleanup()
{
    SAFE_RELEASE( m_pTryModeG10CS );
    SAFE_RELEASE( m_pTryModeLE10CS );
    SAFE_RELEASE( m_pEncodeBlockCS );
}

//--------------------------------------------------------------------------------------
// Encode the source texture to BC6H and store the result in a buffer
// The source texture can only have 1 sub resource, i.e. it must be a signle 2D texture which has only 1 mip level
// The job of breaking down texture arrays, or texture with multiple mip levels is taken care of in the base class
//--------------------------------------------------------------------------------------
HRESULT CGPUBC6HEncoder::GPU_Encode( ID3D11Device* pDevice, ID3D11DeviceContext* pContext,
                                     ID3D11Texture2D* pSrcTexture,
                                     DXGI_FORMAT dstFormat, ID3D11Buffer** ppDstTextureAsBufOut )
{
    ID3D11ShaderResourceView* pSRV = nullptr;
    ID3D11Buffer* pErrBestModeBuffer[2] = { nullptr, nullptr };
    ID3D11UnorderedAccessView* pUAV = nullptr;
    ID3D11UnorderedAccessView* pErrBestModeUAV[2] = { nullptr, nullptr };
    ID3D11ShaderResourceView* pErrBestModeSRV[2] = { nullptr, nullptr };
    ID3D11Buffer* pCBCS = nullptr;
    D3D11_BUFFER_DESC sbOutDesc = {};
    INT start_block_id = 0;
    INT num_total_blocks = 0;
    INT num_blocks = 0;

    if ( !(dstFormat == DXGI_FORMAT_BC6H_SF16 || dstFormat == DXGI_FORMAT_BC6H_UF16) ||
         !ppDstTextureAsBufOut )
        return E_INVALIDARG;

    HRESULT hr = S_OK;

    D3D11_TEXTURE2D_DESC texSrcDesc;
    pSrcTexture->GetDesc( &texSrcDesc );

    // Create a SRV for input texture
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
        SRVDesc.Texture2D.MipLevels = 1;
        SRVDesc.Texture2D.MostDetailedMip = 0;
        V_GOTO(pDevice->CreateShaderResourceView(pSrcTexture, &SRVDesc, &pSRV));
#if defined(_DEBUG) || defined(PROFILE)
        if (pSRV)
        {
            std::ignore = pSRV->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof("BC6H SRV") - 1, "BC6H SRV");
        }
#endif
    }

    // Create output buffer with its size identical to input texture
    {
        sbOutDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
        sbOutDesc.CPUAccessFlags = 0;
        sbOutDesc.Usage = D3D11_USAGE_DEFAULT;
        sbOutDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        sbOutDesc.StructureByteStride = sizeof( BufferBC6HBC7 );
        sbOutDesc.ByteWidth = texSrcDesc.Height * texSrcDesc.Width * sizeof( BufferBC6HBC7 ) / BLOCK_SIZE;
        V_GOTO( pDevice->CreateBuffer( &sbOutDesc, nullptr, ppDstTextureAsBufOut ) );
        V_GOTO( pDevice->CreateBuffer( &sbOutDesc, nullptr, &pErrBestModeBuffer[0] ) );
        V_GOTO( pDevice->CreateBuffer( &sbOutDesc, nullptr, &pErrBestModeBuffer[1] ) );

        _Analysis_assume_( pErrBestModeBuffer[0] != 0 );

#if defined(_DEBUG) || defined(PROFILE)
        if (*ppDstTextureAsBufOut)
        {
            std::ignore = (*ppDstTextureAsBufOut)->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof("BC6H Dest") - 1, "BC6H Dest");
        }
        if (pErrBestModeBuffer[0])
        {
            std::ignore = pErrBestModeBuffer[0]->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof("BC6H ErrBest0") - 1, "BC6H ErrBest0");
        }
        if (pErrBestModeBuffer[1])
        {
            std::ignore = pErrBestModeBuffer[1]->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof("BC6H ErrBest1") - 1, "BC6H ErrBest1");
        }
#endif
    }

    // Create UAV of the output resources
    {
        D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
        UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
        UAVDesc.Buffer.FirstElement = 0;
        UAVDesc.Buffer.NumElements = sbOutDesc.ByteWidth / sbOutDesc.StructureByteStride;
#pragma warning (push)
#pragma warning (disable:6387)
        V_GOTO( pDevice->CreateUnorderedAccessView( *ppDstTextureAsBufOut, &UAVDesc, &pUAV ) );
        V_GOTO( pDevice->CreateUnorderedAccessView( pErrBestModeBuffer[0], &UAVDesc, &pErrBestModeUAV[0] ) );
        V_GOTO( pDevice->CreateUnorderedAccessView( pErrBestModeBuffer[1], &UAVDesc, &pErrBestModeUAV[1] ) );
#pragma warning (pop)

#if defined(_DEBUG) || defined(PROFILE)
        if ( pUAV )
        {
            std::ignore = pUAV->SetPrivateData( WKPDID_D3DDebugObjectName, sizeof( "BC6H Dest UAV" ) - 1, "BC6H Dest UAV" );
        }
        if ( pErrBestModeUAV[0] )
        {
            std::ignore = pErrBestModeUAV[0]->SetPrivateData( WKPDID_D3DDebugObjectName, sizeof( "BC6H ErrBest0 UAV" ) - 1, "BC6H ErrBest0 UAV" );
        }
        if ( pErrBestModeUAV[1] )
        {
            std::ignore = pErrBestModeUAV[1]->SetPrivateData( WKPDID_D3DDebugObjectName, sizeof( "BC6H ErrBest1 UAV" ) - 1, "BC6H ErrBest1 UAV" );
        }
#endif
    }

    // Create SRV of the pErrBestModeBuffer
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
        SRVDesc.Buffer.FirstElement = 0;
        SRVDesc.Buffer.NumElements = sbOutDesc.ByteWidth / sbOutDesc.StructureByteStride;
#pragma warning (push)
#pragma warning (disable:6387)
        V_GOTO( pDevice->CreateShaderResourceView( pErrBestModeBuffer[0], &SRVDesc, &pErrBestModeSRV[0] ) );
        V_GOTO( pDevice->CreateShaderResourceView( pErrBestModeBuffer[1], &SRVDesc, &pErrBestModeSRV[1] ) );
#pragma warning (pop)

#if defined(_DEBUG) || defined(PROFILE)
        if ( pErrBestModeSRV[0] )
        {
            std::ignore = pErrBestModeSRV[0]->SetPrivateData( WKPDID_D3DDebugObjectName, sizeof( "BC6H ErrBest0 SRV" ) - 1, "BC6H ErrBest0 SRV" );
        }
        if ( pErrBestModeSRV[1] )
        {
            std::ignore = pErrBestModeSRV[1]->SetPrivateData( WKPDID_D3DDebugObjectName, sizeof( "BC6H ErrBest1 SRV" ) - 1, "BC6H ErrBest1 SRV" );
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
        pDevice->CreateBuffer( &cbDesc, nullptr, &pCBCS );

#if defined(_DEBUG) || defined(PROFILE)
        if ( pCBCS )
        {
            std::ignore = pCBCS->SetPrivateData( WKPDID_D3DDebugObjectName, sizeof( "BC6HEncode" ) - 1, "BC6HEncode" );
        }
#endif
    }

    num_blocks = num_total_blocks = texSrcDesc.Width / BLOCK_SIZE_X * texSrcDesc.Height / BLOCK_SIZE_Y;
    while ( num_blocks > 0 )
    {
        INT n = std::min( num_blocks, MAX_BLOCK_BATCH );
        auto uThreadGroupCount = static_cast<UINT>(n);

        {
            D3D11_MAPPED_SUBRESOURCE cbMapped;
            pContext->Map( pCBCS, 0, D3D11_MAP_WRITE_DISCARD, 0, &cbMapped );

            UINT param[8];
            param[0] = texSrcDesc.Width;
            param[1] = texSrcDesc.Width / BLOCK_SIZE_X;
            param[2] = static_cast<UINT>(dstFormat);
            param[3] = 0u;
            param[4] = static_cast<UINT>(start_block_id);
            param[5] = static_cast<UINT>(num_total_blocks);
            memcpy( cbMapped.pData, param, sizeof( param ) );
            pContext->Unmap( pCBCS, 0 );
        }

        ID3D11ShaderResourceView* pSRVs[] = { pSRV, nullptr };
        RunComputeShader( pContext, m_pTryModeG10CS, pSRVs, 2, pCBCS, pErrBestModeUAV[0], std::max(uThreadGroupCount / 4, 1u), 1, 1 );

        for ( INT modeID = 0; modeID < 10; ++modeID )
        {
            {
                D3D11_MAPPED_SUBRESOURCE cbMapped;
                pContext->Map( pCBCS, 0, D3D11_MAP_WRITE_DISCARD, 0, &cbMapped );

                UINT param[8];
                param[0] = texSrcDesc.Width;
                param[1] = texSrcDesc.Width / BLOCK_SIZE_X;
                param[2] = static_cast<UINT>(dstFormat);
                param[3] = static_cast<UINT>(modeID);
                param[4] = static_cast<UINT>(start_block_id);
                param[5] = static_cast<UINT>(num_total_blocks);
                memcpy( cbMapped.pData, param, sizeof( param ) );
                pContext->Unmap( pCBCS, 0 );
            }

            pSRVs[1] = pErrBestModeSRV[modeID & 1];
            RunComputeShader( pContext, m_pTryModeLE10CS, pSRVs, 2, pCBCS, pErrBestModeUAV[!(modeID & 1)], std::max(uThreadGroupCount / 2, 1u), 1, 1 );
        }

        pSRVs[1] = pErrBestModeSRV[0];
        RunComputeShader( pContext, m_pEncodeBlockCS, pSRVs, 2, pCBCS, pUAV, std::max(uThreadGroupCount / 2, 1u), 1, 1 );

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
