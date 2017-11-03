//--------------------------------------------------------------------------------------
// File: CascadedShadowsManger.cpp
//
// This is where the shadows are calculated and rendered.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "dxut.h"

#include "VarianceShadowsManager.h"
#include "DXUTcamera.h"
#include "SDKMesh.h"
#include "DirectXCollision.h"
#include "SDKmisc.h"
#include "resource.h"

using namespace DirectX;

static const XMVECTORF32 g_vFLTMAX = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };
static const XMVECTORF32 g_vFLTMIN = { -FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX };
static const XMVECTORF32 g_vHalfVector = { 0.5f, 0.5f, 0.5f, 0.5f };
static const XMVECTORF32 g_vMultiplySetzwToZero = { 1.0f, 1.0f, 0.0f, 0.0f };
static const XMVECTORF32 g_vZero = { 0.0f, 0.0f, 0.0f, 0.0f };


//--------------------------------------------------------------------------------------
// Initialize the Manager.  The manager performs all the work of caculating the render 
// paramters of the shadow, creating the D3D resources, rendering the shadow, and rendering
// the actual scene.
//--------------------------------------------------------------------------------------
VarianceShadowsManager::VarianceShadowsManager () 
                          : m_pVertexLayoutMesh( nullptr ),
                            m_pSamLinear( nullptr ),
                            m_pSamShadowPoint( nullptr ),
                            m_pSamShadowLinear( nullptr ),
                            m_pSamShadowAnisotropic2( nullptr ),
                            m_pSamShadowAnisotropic4( nullptr ),
                            m_pSamShadowAnisotropic8( nullptr ),
                            m_pSamShadowAnisotropic16( nullptr ),
                            m_pCascadedShadowMapVarianceTextureArray( nullptr ),
                            m_pCascadedShadowMapVarianceSRVArraySingle( nullptr ),
                            m_pTemporaryShadowDepthBufferTexture( nullptr ),
                            m_pTemporaryShadowDepthBufferDSV( nullptr ),
                            m_pCascadedShadowMapTempBlurTexture( nullptr ),
                            m_pCascadedShadowMapTempBlurRTV( nullptr ),
                            m_pCascadedShadowMapTempBlurSRV( nullptr ),
                            m_iBlurBetweenCascades( 0 ),
                            m_fBlurBetweenCascadesAmount( 0.0f ),
                            m_RenderOneTileVP( m_RenderVP[0] ),
                            m_pcbGlobalConstantBuffer( nullptr ),
                            m_prsShadow( nullptr ),
                            m_prsScene( nullptr ),
                            m_iShadowBlurSize( 3 ),
                            m_eShadowFilter( SHADOW_FILTER_ANISOTROPIC_16 ),
                            m_pvsQuadBlurBlob( nullptr ),
                            m_pvsRenderVarianceShadowBlob( nullptr ),
                            m_ppsRenderVarianceShadowBlob( nullptr )

{
    sprintf_s( m_cvsModel, "vs_4_0");
    sprintf_s( m_cpsModel, "ps_4_0");
    sprintf_s( m_cgsModel, "gs_4_0");

    for ( int index = 0; index < MAXIMUM_BLUR_LEVELS; ++ index ) 
    {
        m_ppsQuadBlurXBlob[index] = nullptr;
        m_ppsQuadBlurYBlob[index] = nullptr;
    }

    for ( int index = 0; index < MAX_CASCADES; ++ index ) 
    {
        m_pvsRenderSceneBlob[index] = nullptr;
        for (int iX = 0; iX < 2; ++iX ) {
            for (int iX2 = 0; iX2 < 2; ++iX2 ) {
                m_ppsRenderSceneAllShadersBlob[index][iX][iX2] = nullptr;
            }
        }

    }


    for (int index=0; index < MAX_CASCADES; ++index) {
        m_pCascadedShadowMapVarianceRTVArrayAll[index] = nullptr;
        m_pCascadedShadowMapVarianceSRVArrayAll[index] = nullptr;
    }


    for( INT index=0; index < MAX_CASCADES; ++index ) 
    {
        m_RenderVP[index].Height = (FLOAT)m_CopyOfCascadeConfig.m_iBufferSize;
        m_RenderVP[index].Width = (FLOAT)m_CopyOfCascadeConfig.m_iBufferSize;
        m_RenderVP[index].MaxDepth = 1.0f;
        m_RenderVP[index].MinDepth = 0.0f;
        m_RenderVP[index].TopLeftX = 0;
        m_RenderVP[index].TopLeftY = 0;
    }

};


//--------------------------------------------------------------------------------------
// Call into deallocator.  
//--------------------------------------------------------------------------------------
VarianceShadowsManager::~VarianceShadowsManager() 
{
    DestroyAndDeallocateShadowResources();

    SAFE_RELEASE( m_pvsQuadBlurBlob );
    SAFE_RELEASE( m_pvsRenderVarianceShadowBlob );
    SAFE_RELEASE( m_ppsRenderVarianceShadowBlob );

    for ( int index = 0; index < MAXIMUM_BLUR_LEVELS; ++ index ) 
    {
        SAFE_RELEASE( m_ppsQuadBlurXBlob[index] );
        SAFE_RELEASE( m_ppsQuadBlurYBlob[index] );
    }

    for ( int index = 0; index < MAX_CASCADES; ++ index ) 
    {
        SAFE_RELEASE( m_pvsRenderSceneBlob[index] );
        for (int iX = 0; iX < 2; ++iX ) 
        {
            for (int iX2 = 0; iX2 < 2; ++iX2 ) 
            {
                SAFE_RELEASE( m_ppsRenderSceneAllShadersBlob[index][iX][iX2] );
            }
        }

    }


};


//--------------------------------------------------------------------------------------
// Create the resources, compile shaders, etc.
// The rest of the resources are create in the allocator when the scene changes.
//--------------------------------------------------------------------------------------
HRESULT VarianceShadowsManager::Init ( ID3D11Device* pd3dDevice, 
                               CDXUTSDKMesh* pMesh, 
                               CFirstPersonCamera* pViewerCamera,
                               CFirstPersonCamera* pLightCamera,
                               CascadeConfig* pCascadeConfig
                              )  
{
    HRESULT hr = S_OK;

    m_CopyOfCascadeConfig = *pCascadeConfig;        
    // Initialize m_iBufferSize to 0 to trigger a reallocate on the first frame.   
    m_CopyOfCascadeConfig.m_iBufferSize = 0;
    // Save a pointer to cascade config.  Each frame we check our copy against the pointer.
    m_pCascadeConfig = pCascadeConfig;
    
    XMVECTOR vMeshMin;
    XMVECTOR vMeshMax;

    m_vSceneAABBMin = g_vFLTMAX; 
    m_vSceneAABBMax = g_vFLTMIN;
    // Calcaulte the AABB for the scene by iterating through all the meshes in the SDKMesh file.
    for( UINT i =0; i < pMesh->GetNumMeshes( ); ++i ) 
    {
        auto msh = pMesh->GetMesh( i );
        vMeshMin = XMVectorSet( msh->BoundingBoxCenter.x - msh->BoundingBoxExtents.x,
             msh->BoundingBoxCenter.y - msh->BoundingBoxExtents.y,
             msh->BoundingBoxCenter.z - msh->BoundingBoxExtents.z,
             1.0f );

        vMeshMax = XMVectorSet( msh->BoundingBoxCenter.x + msh->BoundingBoxExtents.x,
             msh->BoundingBoxCenter.y + msh->BoundingBoxExtents.y,
             msh->BoundingBoxCenter.z + msh->BoundingBoxExtents.z,
             1.0f );
        
        m_vSceneAABBMin = XMVectorMin( vMeshMin, m_vSceneAABBMin );
        m_vSceneAABBMax = XMVectorMax( vMeshMax, m_vSceneAABBMax );
    }

    m_pViewerCamera = pViewerCamera;          
    m_pLightCamera = pLightCamera;         

    if ( !m_ppsRenderVarianceShadowBlob ) 
    {
        V_RETURN( DXUTCompileFromFile( L"RenderVarianceShadow.hlsl", nullptr, "VSMain", m_cvsModel,
                                       D3DCOMPILE_ENABLE_STRICTNESS, 0, &m_pvsRenderVarianceShadowBlob ) );
    }
    V_RETURN( pd3dDevice->CreateVertexShader(m_pvsRenderVarianceShadowBlob->GetBufferPointer(), 
        m_pvsRenderVarianceShadowBlob->GetBufferSize(), 
        nullptr, &m_pvsRenderVarianceShadow) );
    DXUT_SetDebugName( m_pvsRenderVarianceShadow, "RenderVarianceShadow - VSMain" );

    if ( !m_ppsRenderVarianceShadowBlob ) 
    {
        V_RETURN( DXUTCompileFromFile( L"RenderVarianceShadow.hlsl", nullptr, "PSMain", m_cpsModel,
                                       D3DCOMPILE_ENABLE_STRICTNESS, 0, &m_ppsRenderVarianceShadowBlob ) );
    }
    V_RETURN( pd3dDevice->CreatePixelShader(m_ppsRenderVarianceShadowBlob->GetBufferPointer(), 
        m_ppsRenderVarianceShadowBlob->GetBufferSize(), 
        nullptr, &m_ppsRenderVarianceShadow) );
    DXUT_SetDebugName( m_ppsRenderVarianceShadow, "RenderVarianceShadow - PSMain" );

    if ( !m_pvsQuadBlurBlob ) 
    {
        V_RETURN( DXUTCompileFromFile( L"2DQuadShaders.hlsl", nullptr, "VSMain", m_cvsModel,
                                       D3DCOMPILE_ENABLE_STRICTNESS, 0, &m_pvsQuadBlurBlob ) );
    }
    V_RETURN( pd3dDevice->CreateVertexShader(m_pvsQuadBlurBlob->GetBufferPointer(), m_pvsQuadBlurBlob->GetBufferSize(), 
        nullptr, &m_pvsQuadBlur) );
    DXUT_SetDebugName( m_pvsQuadBlur, "2DQuadShaders - VSMain" );

    INT iKernelSize = 1;
    D3D_SHADER_MACRO kernelDefines[] = 
    {
        "SEPERABLE_BLUR_KERNEL_SIZE", "1",
        nullptr, nullptr
    };
    char cKernelSize[10];
    cKernelSize[0] = 0;
    for ( INT iBlurKernelIndex = 0; iBlurKernelIndex < MAXIMUM_BLUR_LEVELS; ++iBlurKernelIndex ) {
        iKernelSize+=2;
        sprintf_s( cKernelSize, "%d", iKernelSize );
        kernelDefines[0].Definition = cKernelSize;
        if ( !m_ppsQuadBlurXBlob[iBlurKernelIndex] )
        {
            V_RETURN( DXUTCompileFromFile( 
                L"2DQuadShaders.hlsl", kernelDefines, "PSBlurX", m_cpsModel,
                D3DCOMPILE_ENABLE_STRICTNESS, 0, &m_ppsQuadBlurXBlob[iBlurKernelIndex] ) );
        }
        V_RETURN( pd3dDevice->CreatePixelShader(m_ppsQuadBlurXBlob[iBlurKernelIndex]->GetBufferPointer(), 
            m_ppsQuadBlurXBlob[iBlurKernelIndex]->GetBufferSize(), 
            nullptr, &m_ppsQuadBlurX[iBlurKernelIndex]) );

        if ( !m_ppsQuadBlurYBlob[iBlurKernelIndex] ) 
        {
            V_RETURN( DXUTCompileFromFile( 
                L"2DQuadShaders.hlsl", kernelDefines, "PSBlurY", m_cpsModel,
                D3DCOMPILE_ENABLE_STRICTNESS, 0, &m_ppsQuadBlurYBlob[iBlurKernelIndex] ) );
        }
        V_RETURN( pd3dDevice->CreatePixelShader(
            m_ppsQuadBlurYBlob[iBlurKernelIndex]->GetBufferPointer(), 
            m_ppsQuadBlurYBlob[iBlurKernelIndex]->GetBufferSize(), 
            nullptr, &m_ppsQuadBlurY[iBlurKernelIndex]) );

#if defined(DEBUG) || defined(PROFILE)
        char str[64];
        sprintf_s(str, sizeof(str), "2DQuadShaders - PSBlurX (%d)", iKernelSize );
        DXUT_SetDebugName( m_ppsQuadBlurX[iBlurKernelIndex], str );

        sprintf_s(str, sizeof(str), "2DQuadShaders - PSBlurY (%d)", iKernelSize );
        DXUT_SetDebugName( m_ppsQuadBlurY[iBlurKernelIndex], str );
#endif
    }


    // In order to compile optimal versions of each shaders,compile out 64 versions of the same file.  
    // The if statments are dependent upon these macros.  This enables the compiler to optimize out code that can never be reached.
    // D3D11 Dynamic shader linkage would have this same effect without the need to compile 64 versions of the shader.
    D3D_SHADER_MACRO defines[] = 
    {
        "CASCADE_COUNT_FLAG", "1",
        "BLEND_BETWEEN_CASCADE_LAYERS_FLAG", "0",
        "SELECT_CASCADE_BY_INTERVAL_FLAG", "0",
        nullptr, nullptr
    };

    char cCascadeDefinition[32];
    char cBlendDefinition[32];
    char cIntervalDefinition[32];

    for( INT iCascadeIndex=0; iCascadeIndex < MAX_CASCADES; ++iCascadeIndex ) 
    {
        // There is just one vertex shader for the scene.                
        sprintf_s( cCascadeDefinition, "%d", iCascadeIndex + 1 );
        defines[0].Definition = cCascadeDefinition;
        defines[1].Definition = "0";
        defines[2].Definition = "0";
        // We don't want to release the last pVertexShaderBuffer until we create the input layout. 
        if ( !m_pvsRenderSceneBlob[iCascadeIndex] ) 
        {
            V_RETURN( DXUTCompileFromFile( L"RenderVarianceScene.hlsl", defines, "VSMain", 
                      m_cvsModel, D3DCOMPILE_ENABLE_STRICTNESS, 0, &m_pvsRenderSceneBlob[iCascadeIndex] ) );
        }
        V_RETURN( pd3dDevice->CreateVertexShader( m_pvsRenderSceneBlob[iCascadeIndex]->GetBufferPointer(), 
            m_pvsRenderSceneBlob[iCascadeIndex]->GetBufferSize(), nullptr, &m_pvsRenderScene[iCascadeIndex]) );

#if defined(DEBUG) || defined(PROFILE)
        char str[64];
        sprintf_s( str, sizeof(str), "RenderVarianceScene - VSMain (%d)", iCascadeIndex + 1 );
        DXUT_SetDebugName( m_pvsRenderScene[iCascadeIndex], str );
#endif

        for( INT iBlendIndex=0; iBlendIndex < 2; ++iBlendIndex ) 
        {
            for( INT iIntervalIndex=0; iIntervalIndex < 2; ++iIntervalIndex ) 
            {
                sprintf_s( cCascadeDefinition, "%d", iCascadeIndex + 1 );
                sprintf_s( cBlendDefinition, "%d", iBlendIndex );
                sprintf_s( cIntervalDefinition, "%d", iIntervalIndex );

                defines[0].Definition = cCascadeDefinition;
                defines[1].Definition = cBlendDefinition;
                defines[2].Definition = cIntervalDefinition;

                if ( !m_ppsRenderSceneAllShadersBlob[iCascadeIndex][iBlendIndex][iIntervalIndex] ) 
                {
                    V_RETURN( DXUTCompileFromFile( L"RenderVarianceScene.hlsl", defines, "PSMain", 
                              m_cpsModel, D3DCOMPILE_ENABLE_STRICTNESS, 0, 
                              &m_ppsRenderSceneAllShadersBlob[iCascadeIndex][iBlendIndex][iIntervalIndex] ) );
                }
                V_RETURN( pd3dDevice->CreatePixelShader( 
                    m_ppsRenderSceneAllShadersBlob[iCascadeIndex][iBlendIndex][iIntervalIndex]->GetBufferPointer(),
                    m_ppsRenderSceneAllShadersBlob[iCascadeIndex][iBlendIndex][iIntervalIndex]->GetBufferSize(), 
                    nullptr, 
                    &m_ppsRenderSceneAllShaders[iCascadeIndex][iBlendIndex][iIntervalIndex] ) );

#if defined(DEBUG) || defined(PROFILE)
                sprintf_s( str, sizeof(str), "RenderVarianceScene - PSMain [%d,%d,%d]", iCascadeIndex + 1, iBlendIndex, iIntervalIndex );
                DXUT_SetDebugName( m_ppsRenderSceneAllShaders[iCascadeIndex][iBlendIndex][iIntervalIndex], str );
#endif
            }
        }
    }

    const D3D11_INPUT_ELEMENT_DESC layout_mesh[] =
    {
        { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    
    V_RETURN( pd3dDevice->CreateInputLayout( 
        layout_mesh, ARRAYSIZE( layout_mesh ), 
        m_pvsRenderSceneBlob[0]->GetBufferPointer(),
        m_pvsRenderSceneBlob[0]->GetBufferSize(), 
        &m_pVertexLayoutMesh ) );
    DXUT_SetDebugName( m_pVertexLayoutMesh, "Vertices" );

    D3D11_RASTERIZER_DESC drd = 
    {
        D3D11_FILL_SOLID,//D3D11_FILL_MODE FillMode;
        D3D11_CULL_NONE,//D3D11_CULL_MODE CullMode;
        FALSE,//BOOL FrontCounterClockwise;
        0,//INT DepthBias;
        0.0,//FLOAT DepthBiasClamp;
        0.0,//FLOAT SlopeScaledDepthBias;
        TRUE,//BOOL DepthClipEnable;
        FALSE,//BOOL ScissorEnable;
        TRUE,//BOOL MultisampleEnable;
        FALSE//BOOL AntialiasedLineEnable;   
    };

    pd3dDevice->CreateRasterizerState( &drd, &m_prsScene );
    DXUT_SetDebugName( m_prsScene, "VSM Scene" );
    
    // Setting the slope scale depth biase greatly decreases surface acne and incorrect self shadowing.
    drd.SlopeScaledDepthBias = 1.0;
    pd3dDevice->CreateRasterizerState( &drd, &m_prsShadow );
    DXUT_SetDebugName( m_prsShadow, "VSM Shadow" );
    
    D3D11_BUFFER_DESC Desc;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Desc.MiscFlags = 0;

    Desc.ByteWidth = sizeof( CB_ALL_SHADOW_DATA );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, nullptr, &m_pcbGlobalConstantBuffer ) );
    DXUT_SetDebugName( m_pcbGlobalConstantBuffer, "VSM CB_ALL_SHADOW_DATA" ); 

    return hr;
}


//--------------------------------------------------------------------------------------
// These resources must be reallocated based on GUI control settings change.
//--------------------------------------------------------------------------------------
HRESULT VarianceShadowsManager::DestroyAndDeallocateShadowResources() 
{

    SAFE_RELEASE( m_pVertexLayoutMesh );
    SAFE_RELEASE( m_pSamLinear );
    SAFE_RELEASE( m_pSamShadowPoint );
    SAFE_RELEASE( m_pSamShadowLinear );
    SAFE_RELEASE( m_pSamShadowAnisotropic2 );
    SAFE_RELEASE( m_pSamShadowAnisotropic4 );
    SAFE_RELEASE( m_pSamShadowAnisotropic8 );
    SAFE_RELEASE( m_pSamShadowAnisotropic16 );


    SAFE_RELEASE( m_pCascadedShadowMapVarianceTextureArray );
    for (int index=0; index < MAX_CASCADES; ++index) {
        SAFE_RELEASE( m_pCascadedShadowMapVarianceRTVArrayAll[index] );
        SAFE_RELEASE( m_pCascadedShadowMapVarianceSRVArrayAll[index] );
    }
    SAFE_RELEASE( m_pCascadedShadowMapVarianceSRVArraySingle );

    SAFE_RELEASE( m_pTemporaryShadowDepthBufferTexture );
    SAFE_RELEASE( m_pTemporaryShadowDepthBufferDSV );

    SAFE_RELEASE( m_pCascadedShadowMapTempBlurTexture );
    SAFE_RELEASE( m_pCascadedShadowMapTempBlurRTV );
    SAFE_RELEASE( m_pCascadedShadowMapTempBlurSRV );

    SAFE_RELEASE( m_pcbGlobalConstantBuffer );

    SAFE_RELEASE( m_prsShadow );
    SAFE_RELEASE( m_prsScene );

    SAFE_RELEASE( m_pvsQuadBlur );
    for ( int index = 0; index < MAXIMUM_BLUR_LEVELS; ++index ) {
        SAFE_RELEASE( m_ppsQuadBlurX[index] );
        SAFE_RELEASE( m_ppsQuadBlurY[index] );
    }

    SAFE_RELEASE( m_pvsRenderVarianceShadow );
    SAFE_RELEASE( m_ppsRenderVarianceShadow );

    for( INT iCascadeIndex=0; iCascadeIndex < MAX_CASCADES; ++iCascadeIndex ) 
    { 
        SAFE_RELEASE( m_pvsRenderScene[iCascadeIndex] );
        for( INT iBlendIndex=0; iBlendIndex < 2; ++iBlendIndex ) 
        {
            for( INT iIntervalIndex=0; iIntervalIndex < 2; ++iIntervalIndex ) 
            {
                SAFE_RELEASE( m_ppsRenderSceneAllShaders[iCascadeIndex][iBlendIndex][iIntervalIndex] );
            }
        }
    }
    return S_OK;
}


//--------------------------------------------------------------------------------------
// These settings must be recreated based on GUI control.
//--------------------------------------------------------------------------------------
HRESULT VarianceShadowsManager::ReleaseAndAllocateNewShadowResources( ID3D11Device* pd3dDevice ) 
{
    HRESULT hr = S_OK;
    // If any of these 3 paramaters was changed, we must reallocate the D3D resources.
    if( m_CopyOfCascadeConfig.m_nCascadeLevels != m_pCascadeConfig->m_nCascadeLevels 
        || m_CopyOfCascadeConfig.m_ShadowBufferFormat != m_pCascadeConfig->m_ShadowBufferFormat 
        || m_CopyOfCascadeConfig.m_iBufferSize != m_pCascadeConfig->m_iBufferSize )
    {

        m_CopyOfCascadeConfig = *m_pCascadeConfig;        

        SAFE_RELEASE( m_pSamLinear );
        SAFE_RELEASE( m_pSamShadowPoint );
        SAFE_RELEASE( m_pSamShadowLinear );
        SAFE_RELEASE( m_pSamShadowAnisotropic16 );
        SAFE_RELEASE( m_pSamShadowAnisotropic8 );
        SAFE_RELEASE( m_pSamShadowAnisotropic4 );
        SAFE_RELEASE( m_pSamShadowAnisotropic2 );
        
        D3D11_SAMPLER_DESC SamDesc;
        SamDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        SamDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        SamDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        SamDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        SamDesc.MipLODBias = 0.0f;
        SamDesc.MaxAnisotropy = 1;
        SamDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        SamDesc.BorderColor[0] = SamDesc.BorderColor[1] = SamDesc.BorderColor[2] = SamDesc.BorderColor[3] = 0;
        SamDesc.MinLOD = 0;
        SamDesc.MaxLOD = D3D11_FLOAT32_MAX;
        V_RETURN( pd3dDevice->CreateSamplerState( &SamDesc, &m_pSamLinear ) );
        DXUT_SetDebugName( m_pSamLinear, "VSM Linear" );

        D3D11_SAMPLER_DESC SamDescShad = 
        {
            D3D11_FILTER_ANISOTROPIC,// D3D11_FILTER Filter;
            D3D11_TEXTURE_ADDRESS_CLAMP, //D3D11_TEXTURE_ADDRESS_MODE AddressU;
            D3D11_TEXTURE_ADDRESS_CLAMP, //D3D11_TEXTURE_ADDRESS_MODE AddressV;
            D3D11_TEXTURE_ADDRESS_BORDER, //D3D11_TEXTURE_ADDRESS_MODE AddressW;
            0,//FLOAT MipLODBias;
            16,//UINT MaxAnisotropy;
            D3D11_COMPARISON_NEVER , //D3D11_COMPARISON_FUNC ComparisonFunc;
            0.0,0.0,0.0,0.0,//FLOAT BorderColor[ 4 ];
            0,//FLOAT MinLOD;
            0//FLOAT MaxLOD;   
        };

        V_RETURN( pd3dDevice->CreateSamplerState( &SamDescShad, &m_pSamShadowAnisotropic16 ) );
        DXUT_SetDebugName( m_pSamShadowAnisotropic16, "VSM Shadow Ansio16" );

        SamDescShad.MaxAnisotropy = 8;
        V_RETURN( pd3dDevice->CreateSamplerState( &SamDescShad, &m_pSamShadowAnisotropic8 ) );
        DXUT_SetDebugName( m_pSamShadowAnisotropic8, "VSM Shadow Ansio8" );

        SamDescShad.MaxAnisotropy = 4;
        V_RETURN( pd3dDevice->CreateSamplerState( &SamDescShad, &m_pSamShadowAnisotropic4 ) );
        DXUT_SetDebugName( m_pSamShadowAnisotropic4, "VSM Shadow Ansio4" );

        SamDescShad.MaxAnisotropy = 2;
        V_RETURN( pd3dDevice->CreateSamplerState( &SamDescShad, &m_pSamShadowAnisotropic2 ) );
        DXUT_SetDebugName( m_pSamShadowAnisotropic2, "VSM Shadow Ansio2" );

        SamDescShad.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        SamDescShad.MaxAnisotropy = 0;
        V_RETURN( pd3dDevice->CreateSamplerState( &SamDescShad, &m_pSamShadowLinear ) );
        DXUT_SetDebugName( m_pSamShadowLinear, "VSM Shadow Linear" );

        SamDescShad.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        V_RETURN( pd3dDevice->CreateSamplerState( &SamDescShad, &m_pSamShadowPoint ) );
        DXUT_SetDebugName( m_pSamShadowPoint, "VSM Shadow Point" );

        for (INT index=0; index < m_CopyOfCascadeConfig.m_nCascadeLevels; ++index ) 
        { 
            m_RenderVP[index].Height = (FLOAT)m_CopyOfCascadeConfig.m_iBufferSize;
            m_RenderVP[index].Width = (FLOAT)m_CopyOfCascadeConfig.m_iBufferSize;
            m_RenderVP[index].MaxDepth = 1.0f;
            m_RenderVP[index].MinDepth = 0.0f;
            m_RenderVP[index].TopLeftX = (FLOAT)( m_CopyOfCascadeConfig.m_iBufferSize * index );
            m_RenderVP[index].TopLeftY = 0;
        }

        m_RenderOneTileVP.Height = (FLOAT)m_CopyOfCascadeConfig.m_iBufferSize;
        m_RenderOneTileVP.Width = (FLOAT)m_CopyOfCascadeConfig.m_iBufferSize;
        m_RenderOneTileVP.MaxDepth = 1.0f;
        m_RenderOneTileVP.MinDepth = 0.0f;
        m_RenderOneTileVP.TopLeftX = 0.0f;
        m_RenderOneTileVP.TopLeftY = 0.0f;

        SAFE_RELEASE( m_pCascadedShadowMapVarianceTextureArray );
        for (int index=0; index < MAX_CASCADES; ++index) {
            SAFE_RELEASE( m_pCascadedShadowMapVarianceRTVArrayAll[index] );
            SAFE_RELEASE( m_pCascadedShadowMapVarianceSRVArrayAll[index] );
        }
        SAFE_RELEASE( m_pCascadedShadowMapVarianceSRVArraySingle );

        SAFE_RELEASE( m_pCascadedShadowMapTempBlurTexture );
        SAFE_RELEASE( m_pCascadedShadowMapTempBlurRTV );
        SAFE_RELEASE( m_pCascadedShadowMapTempBlurSRV );

        SAFE_RELEASE( m_pTemporaryShadowDepthBufferTexture );
        SAFE_RELEASE( m_pTemporaryShadowDepthBufferDSV );

        D3D11_TEXTURE2D_DESC dtd = {
            UINT(m_CopyOfCascadeConfig.m_iBufferSize),//UINT Width;
            UINT(m_CopyOfCascadeConfig.m_iBufferSize),//UINT Height;
            1,//UINT MipLevels;
            UINT(m_CopyOfCascadeConfig.m_nCascadeLevels),//UINT ArraySize;
            m_CopyOfCascadeConfig.m_ShadowBufferFormat,//DXGI_FORMAT Format;
            1,//DXGI_SAMPLE_DESC SampleDesc;
            0,
            D3D11_USAGE_DEFAULT,//D3D11_USAGE Usage;
            D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,//UINT BindFlags;
            0,//UINT CPUAccessFlags;
            0//UINT MiscFlags;    
        };

        V_RETURN( pd3dDevice->CreateTexture2D( &dtd, nullptr, &m_pCascadedShadowMapVarianceTextureArray  ) );
        DXUT_SetDebugName( m_pCascadedShadowMapVarianceTextureArray, "VSM ShadowMap Var Array" );

        dtd.ArraySize = 1;
        V_RETURN( pd3dDevice->CreateTexture2D( &dtd, nullptr, &m_pCascadedShadowMapTempBlurTexture  ) );
        DXUT_SetDebugName( m_pCascadedShadowMapTempBlurTexture, "VSM ShadowMap Temp Blur" );

        dtd.Format = DXGI_FORMAT_R32_TYPELESS;
        dtd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        V_RETURN( pd3dDevice->CreateTexture2D( &dtd, nullptr, &m_pTemporaryShadowDepthBufferTexture  ) );
        DXUT_SetDebugName( m_pTemporaryShadowDepthBufferTexture, "VSM Temp ShadowDepth" );

        D3D11_DEPTH_STENCIL_VIEW_DESC dsvd;
        dsvd.Flags = 0;
        dsvd.Format = DXGI_FORMAT_D32_FLOAT;
        dsvd.Texture2D.MipSlice = 0;
        dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

        V_RETURN( pd3dDevice->CreateDepthStencilView( m_pTemporaryShadowDepthBufferTexture, &dsvd, &m_pTemporaryShadowDepthBufferDSV ) );
        DXUT_SetDebugName( m_pTemporaryShadowDepthBufferDSV, "VSM Temp ShadowDepth DSV" );

        D3D11_RENDER_TARGET_VIEW_DESC drtvd = {
            m_CopyOfCascadeConfig.m_ShadowBufferFormat,
            D3D11_RTV_DIMENSION_TEXTURE2DARRAY
        };

        drtvd.Texture2DArray.MipSlice = 0;

        for( int index = 0; index < m_CopyOfCascadeConfig.m_nCascadeLevels; ++index ) 
        {
            drtvd.Texture2DArray.FirstArraySlice = index;
            drtvd.Texture2DArray.ArraySize = 1;
            V_RETURN( pd3dDevice->CreateRenderTargetView ( m_pCascadedShadowMapVarianceTextureArray, &drtvd,
                &m_pCascadedShadowMapVarianceRTVArrayAll[index] ) );

#if defined(DEBUG) || defined(PROFILE)
            char str[64];
            sprintf_s( str, sizeof(str), "VSM Cascaded Var Array (%d) RTV", index );
            DXUT_SetDebugName( m_pCascadedShadowMapVarianceRTVArrayAll[index], str );
#endif
        }

        drtvd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        drtvd.Texture2D.MipSlice = 0;

        V_RETURN( pd3dDevice->CreateRenderTargetView ( m_pCascadedShadowMapTempBlurTexture, &drtvd,
            &m_pCascadedShadowMapTempBlurRTV ) );
        DXUT_SetDebugName( m_pCascadedShadowMapTempBlurRTV, "VSM Cascaded SM Temp Blur RTV" );

        D3D11_SHADER_RESOURCE_VIEW_DESC dsrvd = {
            m_CopyOfCascadeConfig.m_ShadowBufferFormat,
            D3D11_SRV_DIMENSION_TEXTURE2DARRAY,
        };
        
        dsrvd.Texture2DArray.ArraySize = m_CopyOfCascadeConfig.m_nCascadeLevels;
        dsrvd.Texture2DArray.FirstArraySlice = 0;
        dsrvd.Texture2DArray.MipLevels = 1;
        dsrvd.Texture2DArray.MostDetailedMip = 0;

        V_RETURN( pd3dDevice->CreateShaderResourceView( m_pCascadedShadowMapVarianceTextureArray, 
            &dsrvd, &m_pCascadedShadowMapVarianceSRVArraySingle ) );
        DXUT_SetDebugName( m_pCascadedShadowMapVarianceSRVArraySingle, "VSM Cascaded SM Var Array SRV" );

        for( int index = 0; index < m_CopyOfCascadeConfig.m_nCascadeLevels; ++index ) 
        {
            dsrvd.Texture2DArray.ArraySize = 1;
            dsrvd.Texture2DArray.FirstArraySlice = index;
            dsrvd.Texture2DArray.MipLevels = 1;
            dsrvd.Texture2DArray.MostDetailedMip = 0;
            V_RETURN( pd3dDevice->CreateShaderResourceView( m_pCascadedShadowMapVarianceTextureArray, 
                &dsrvd, &m_pCascadedShadowMapVarianceSRVArrayAll[index] ) );

#if defined(DEBUG) || defined(PROFILE)
            char str[64];
            sprintf_s( str, sizeof(str), "VSM Cascaded SM Var Array (%d) SRV", index );
            DXUT_SetDebugName( m_pCascadedShadowMapVarianceSRVArrayAll[index], str );
#endif
        }

        //dsrvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        //dsrvd.Texture2D.MipLevels = 1;
        //dsrvd.Texture2D.MostDetailedMip = 0;
        dsrvd.Texture2DArray.ArraySize = 1;
        dsrvd.Texture2DArray.FirstArraySlice = 0;
        dsrvd.Texture2DArray.MipLevels = 1;
        dsrvd.Texture2DArray.MostDetailedMip = 0;
        V_RETURN( pd3dDevice->CreateShaderResourceView( m_pCascadedShadowMapTempBlurTexture, 
            &dsrvd, &m_pCascadedShadowMapTempBlurSRV ) );
        DXUT_SetDebugName( m_pCascadedShadowMapTempBlurSRV, "VSM Cascaded SM Temp Blur SRV" );
    }
    return hr;

}

//--------------------------------------------------------------------------------------
// This function takes the camera's projection matrix and returns the 8
// points that make up a view frustum.
// The frustum is scaled to fit within the Begin and End interval paramaters.
//--------------------------------------------------------------------------------------
void VarianceShadowsManager::CreateFrustumPointsFromCascadeInterval( float fCascadeIntervalBegin, 
                                                        FLOAT fCascadeIntervalEnd, 
                                                        CXMMATRIX vProjection,
                                                        XMVECTOR* pvCornerPointsWorld ) 
{
    BoundingFrustum vViewFrust( vProjection );
    vViewFrust.Near = fCascadeIntervalBegin;
    vViewFrust.Far = fCascadeIntervalEnd;

    static const XMVECTORU32 vGrabY = {0x00000000,0xFFFFFFFF,0x00000000,0x00000000};
    static const XMVECTORU32 vGrabX = {0xFFFFFFFF,0x00000000,0x00000000,0x00000000};

    XMVECTORF32 vRightTop = {vViewFrust.RightSlope,vViewFrust.TopSlope,1.0f,1.0f};
    XMVECTORF32 vLeftBottom = {vViewFrust.LeftSlope,vViewFrust.BottomSlope,1.0f,1.0f};
    XMVECTORF32 vNear = {vViewFrust.Near,vViewFrust.Near,vViewFrust.Near,1.0f};
    XMVECTORF32 vFar = {vViewFrust.Far,vViewFrust.Far,vViewFrust.Far,1.0f};
    XMVECTOR vRightTopNear = XMVectorMultiply( vRightTop, vNear );
    XMVECTOR vRightTopFar = XMVectorMultiply( vRightTop, vFar );
    XMVECTOR vLeftBottomNear = XMVectorMultiply( vLeftBottom, vNear );
    XMVECTOR vLeftBottomFar = XMVectorMultiply( vLeftBottom, vFar );

    pvCornerPointsWorld[0] = vRightTopNear;
    pvCornerPointsWorld[1] = XMVectorSelect( vRightTopNear, vLeftBottomNear, vGrabX );
    pvCornerPointsWorld[2] = vLeftBottomNear;
    pvCornerPointsWorld[3] = XMVectorSelect( vRightTopNear, vLeftBottomNear,vGrabY );

    pvCornerPointsWorld[4] = vRightTopFar;
    pvCornerPointsWorld[5] = XMVectorSelect( vRightTopFar, vLeftBottomFar, vGrabX );
    pvCornerPointsWorld[6] = vLeftBottomFar;
    pvCornerPointsWorld[7] = XMVectorSelect( vRightTopFar ,vLeftBottomFar, vGrabY );

}

//--------------------------------------------------------------------------------------
// Used to compute an intersection of the orthographic projection and the Scene AABB
//--------------------------------------------------------------------------------------
struct Triangle 
{
    XMVECTOR pt[3];
    bool culled;
};


//--------------------------------------------------------------------------------------
// Computing an accurate near and flar plane will decrease surface acne and Peter-panning.
// Surface acne is the term for erroneous self shadowing.  Peter-panning is the effect where
// shadows disappear near the base of an object.
// As offsets are generally used with PCF filtering due self shadowing issues, computing the
// correct near and far planes becomes even more important.
// This concept is not complicated, but the intersection code is.
//--------------------------------------------------------------------------------------
void VarianceShadowsManager::ComputeNearAndFar( FLOAT& fNearPlane, 
                                        FLOAT& fFarPlane, 
                                        FXMVECTOR vLightCameraOrthographicMin, 
                                        FXMVECTOR vLightCameraOrthographicMax, 
                                        XMVECTOR* pvPointsInCameraView ) 
{

    // Initialize the near and far planes
    fNearPlane = FLT_MAX;
    fFarPlane = -FLT_MAX;
    
    Triangle triangleList[16];
    INT iTriangleCnt = 1;

    triangleList[0].pt[0] = pvPointsInCameraView[0];
    triangleList[0].pt[1] = pvPointsInCameraView[1];
    triangleList[0].pt[2] = pvPointsInCameraView[2];
    triangleList[0].culled = false;

    // These are the indices used to tesselate an AABB into a list of triangles.
    static const INT iAABBTriIndexes[] = 
    {
        0,1,2,  1,2,3,
        4,5,6,  5,6,7,
        0,2,4,  2,4,6,
        1,3,5,  3,5,7,
        0,1,4,  1,4,5,
        2,3,6,  3,6,7 
    };

    INT iPointPassesCollision[3];

    // At a high level: 
    // 1. Iterate over all 12 triangles of the AABB.  
    // 2. Clip the triangles against each plane. Create new triangles as needed.
    // 3. Find the min and max z values as the near and far plane.
    
    //This is easier because the triangles are in camera spacing making the collisions tests simple comparisions.
    
    float fLightCameraOrthographicMinX = XMVectorGetX( vLightCameraOrthographicMin );
    float fLightCameraOrthographicMaxX = XMVectorGetX( vLightCameraOrthographicMax ); 
    float fLightCameraOrthographicMinY = XMVectorGetY( vLightCameraOrthographicMin );
    float fLightCameraOrthographicMaxY = XMVectorGetY( vLightCameraOrthographicMax );
    
    for( INT AABBTriIter = 0; AABBTriIter < 12; ++AABBTriIter ) 
    {

        triangleList[0].pt[0] = pvPointsInCameraView[ iAABBTriIndexes[ AABBTriIter*3 + 0 ] ];
        triangleList[0].pt[1] = pvPointsInCameraView[ iAABBTriIndexes[ AABBTriIter*3 + 1 ] ];
        triangleList[0].pt[2] = pvPointsInCameraView[ iAABBTriIndexes[ AABBTriIter*3 + 2 ] ];
        iTriangleCnt = 1;
        triangleList[0].culled = FALSE;

        // Clip each invidual triangle against the 4 frustums.  When ever a triangle is clipped into new triangles, 
        //add them to the list.
        for( INT frustumPlaneIter = 0; frustumPlaneIter < 4; ++frustumPlaneIter ) 
        {

            FLOAT fEdge;
            INT iComponent;
            
            if( frustumPlaneIter == 0 ) 
            {
                fEdge = fLightCameraOrthographicMinX; // todo make float temp
                iComponent = 0;
            } 
            else if( frustumPlaneIter == 1 ) 
            {
                fEdge = fLightCameraOrthographicMaxX;
                iComponent = 0;
            } 
            else if( frustumPlaneIter == 2 ) 
            {
                fEdge = fLightCameraOrthographicMinY;
                iComponent = 1;
            } 
            else 
            {
                fEdge = fLightCameraOrthographicMaxY;
                iComponent = 1;
            }

            for( INT triIter=0; triIter < iTriangleCnt; ++triIter ) 
            {
                // We don't delete triangles, so we skip those that have been culled.
                if( !triangleList[triIter].culled ) 
                {
                    INT iInsideVertCount = 0;
                    XMVECTOR tempOrder;
                    // Test against the correct frustum plane.
                    // This could be written more compactly, but it would be harder to understand.
                    
                    if( frustumPlaneIter == 0 ) 
                    {
                        for( INT triPtIter=0; triPtIter < 3; ++triPtIter ) 
                        {
                            if( XMVectorGetX( triangleList[triIter].pt[triPtIter] ) >
                                XMVectorGetX( vLightCameraOrthographicMin ) ) 
                            { 
                                iPointPassesCollision[triPtIter] = 1;
                            }
                            else 
                            {
                                iPointPassesCollision[triPtIter] = 0;
                            }
                            iInsideVertCount += iPointPassesCollision[triPtIter];
                        }
                    }
                    else if( frustumPlaneIter == 1 ) 
                    {
                        for( INT triPtIter=0; triPtIter < 3; ++triPtIter ) 
                        {
                            if( XMVectorGetX( triangleList[triIter].pt[triPtIter] ) < 
                                XMVectorGetX( vLightCameraOrthographicMax ) )
                            {
                                iPointPassesCollision[triPtIter] = 1;
                            }
                            else
                            { 
                                iPointPassesCollision[triPtIter] = 0;
                            }
                            iInsideVertCount += iPointPassesCollision[triPtIter];
                        }
                    }
                    else if( frustumPlaneIter == 2 ) 
                    {
                        for( INT triPtIter=0; triPtIter < 3; ++triPtIter ) 
                        {
                            if( XMVectorGetY( triangleList[triIter].pt[triPtIter] ) > 
                                XMVectorGetY( vLightCameraOrthographicMin ) ) 
                            {
                                iPointPassesCollision[triPtIter] = 1;
                            }
                            else 
                            {
                                iPointPassesCollision[triPtIter] = 0;
                            }
                            iInsideVertCount += iPointPassesCollision[triPtIter];
                        }
                    }
                    else 
                    {
                        for( INT triPtIter=0; triPtIter < 3; ++triPtIter ) 
                        {
                            if( XMVectorGetY( triangleList[triIter].pt[triPtIter] ) < 
                                XMVectorGetY( vLightCameraOrthographicMax ) ) 
                            {
                                iPointPassesCollision[triPtIter] = 1;
                            }
                            else 
                            {
                                iPointPassesCollision[triPtIter] = 0;
                            }
                            iInsideVertCount += iPointPassesCollision[triPtIter];
                        }
                    }

                    // Move the points that pass the frustum test to the begining of the array.
                    if( iPointPassesCollision[1] && !iPointPassesCollision[0] ) 
                    {
                        tempOrder =  triangleList[triIter].pt[0];   
                        triangleList[triIter].pt[0] = triangleList[triIter].pt[1];
                        triangleList[triIter].pt[1] = tempOrder;
                        iPointPassesCollision[0] = TRUE;            
                        iPointPassesCollision[1] = FALSE;            
                    }
                    if( iPointPassesCollision[2] && !iPointPassesCollision[1] ) 
                    {
                        tempOrder =  triangleList[triIter].pt[1];   
                        triangleList[triIter].pt[1] = triangleList[triIter].pt[2];
                        triangleList[triIter].pt[2] = tempOrder;
                        iPointPassesCollision[1] = TRUE;            
                        iPointPassesCollision[2] = FALSE;                        
                    }
                    if( iPointPassesCollision[1] && !iPointPassesCollision[0] ) 
                    {
                        tempOrder =  triangleList[triIter].pt[0];   
                        triangleList[triIter].pt[0] = triangleList[triIter].pt[1];
                        triangleList[triIter].pt[1] = tempOrder;
                        iPointPassesCollision[0] = TRUE;            
                        iPointPassesCollision[1] = FALSE;            
                    }
                    
                    if( iInsideVertCount == 0 ) 
                    { // All points failed. We're done,  
                        triangleList[triIter].culled = true;
                    }
                    else if( iInsideVertCount == 1 ) 
                    {// One point passed. Clip the triangle against the Frustum plane
                        triangleList[triIter].culled = false;
                        
                        // 
                        XMVECTOR vVert0ToVert1 = triangleList[triIter].pt[1] - triangleList[triIter].pt[0];
                        XMVECTOR vVert0ToVert2 = triangleList[triIter].pt[2] - triangleList[triIter].pt[0];
                        
                        // Find the collision ratio.
                        FLOAT fHitPointTimeRatio = fEdge - XMVectorGetByIndex( triangleList[triIter].pt[0], iComponent ) ;
                        // Calculate the distance along the vector as ratio of the hit ratio to the component.
                        FLOAT fDistanceAlongVector01 = fHitPointTimeRatio / XMVectorGetByIndex( vVert0ToVert1, iComponent );
                        FLOAT fDistanceAlongVector02 = fHitPointTimeRatio / XMVectorGetByIndex( vVert0ToVert2, iComponent );
                        // Add the point plus a percentage of the vector.
                        vVert0ToVert1 *= fDistanceAlongVector01;
                        vVert0ToVert1 += triangleList[triIter].pt[0];
                        vVert0ToVert2 *= fDistanceAlongVector02;
                        vVert0ToVert2 += triangleList[triIter].pt[0];

                        triangleList[triIter].pt[1] = vVert0ToVert2;
                        triangleList[triIter].pt[2] = vVert0ToVert1;

                    }
                    else if( iInsideVertCount == 2 ) 
                    { // 2 in  // tesselate into 2 triangles
                        

                        // Copy the triangle\(if it exists) after the current triangle out of
                        // the way so we can override it with the new triangle we're inserting.
                        triangleList[iTriangleCnt] = triangleList[triIter+1];

                        triangleList[triIter].culled = false;
                        triangleList[triIter+1].culled = false;
                        
                        // Get the vector from the outside point into the 2 inside points.
                        XMVECTOR vVert2ToVert0 = triangleList[triIter].pt[0] - triangleList[triIter].pt[2];
                        XMVECTOR vVert2ToVert1 = triangleList[triIter].pt[1] - triangleList[triIter].pt[2];
                        
                        // Get the hit point ratio.
                        FLOAT fHitPointTime_2_0 =  fEdge - XMVectorGetByIndex( triangleList[triIter].pt[2], iComponent );
                        FLOAT fDistanceAlongVector_2_0 = fHitPointTime_2_0 / XMVectorGetByIndex( vVert2ToVert0, iComponent );
                        // Calcaulte the new vert by adding the percentage of the vector plus point 2.
                        vVert2ToVert0 *= fDistanceAlongVector_2_0;
                        vVert2ToVert0 += triangleList[triIter].pt[2];
                        
                        // Add a new triangle.
                        triangleList[triIter+1].pt[0] = triangleList[triIter].pt[0];
                        triangleList[triIter+1].pt[1] = triangleList[triIter].pt[1];
                        triangleList[triIter+1].pt[2] = vVert2ToVert0;
                        
                        //Get the hit point ratio.
                        FLOAT fHitPointTime_2_1 =  fEdge - XMVectorGetByIndex( triangleList[triIter].pt[2], iComponent ) ;
                        FLOAT fDistanceAlongVector_2_1 = fHitPointTime_2_1 / XMVectorGetByIndex( vVert2ToVert1, iComponent );
                        vVert2ToVert1 *= fDistanceAlongVector_2_1;
                        vVert2ToVert1 += triangleList[triIter].pt[2];
                        triangleList[triIter].pt[0] = triangleList[triIter+1].pt[1];
                        triangleList[triIter].pt[1] = triangleList[triIter+1].pt[2];
                        triangleList[triIter].pt[2] = vVert2ToVert1;
                        // Cncrement triangle count and skip the triangle we just inserted.
                        ++iTriangleCnt;
                        ++triIter;

                    
                    }
                    else 
                    { // all in
                        triangleList[triIter].culled = false;

                    }
                }// end if !culled loop            
            }
        }
        for( INT index=0; index < iTriangleCnt; ++index ) 
        {
            if( !triangleList[index].culled ) 
            {
                // Set the near and far plan and the min and max z values respectivly.
                for( int vertind = 0; vertind < 3; ++ vertind ) 
                {
                    float fTriangleCoordZ = XMVectorGetZ( triangleList[index].pt[vertind] );
                    if( fNearPlane > fTriangleCoordZ ) 
                    {
                        fNearPlane = fTriangleCoordZ;
                    }
                    if( fFarPlane  <fTriangleCoordZ ) 
                    {
                        fFarPlane = fTriangleCoordZ;
                    }
                }
            }
        }
    }    

}


//--------------------------------------------------------------------------------------
// This function is where the real work is done. We determin the matricies and constants used in 
// shadow generation and scene generation.
//--------------------------------------------------------------------------------------
HRESULT VarianceShadowsManager::InitFrame ( ID3D11Device* pd3dDevice ) 
{
    ReleaseAndAllocateNewShadowResources( pd3dDevice );

    XMMATRIX matViewCameraProjection = m_pViewerCamera->GetProjMatrix();
    XMMATRIX matViewCameraView = m_pViewerCamera->GetViewMatrix();
    XMMATRIX matLightCameraView = m_pLightCamera->GetViewMatrix();

    XMMATRIX matInverseViewCamera = XMMatrixInverse( nullptr,  matViewCameraView );

    // Convert from min max representation to center extents represnetation.
    // This will make it easier to pull the points out of the transformation.
    BoundingBox bb;
    BoundingBox::CreateFromPoints( bb, m_vSceneAABBMin, m_vSceneAABBMax );

    XMFLOAT3 tmp[8];
    bb.GetCorners( tmp );

    // Transform the scene AABB to Light space.
    XMVECTOR vSceneAABBPointsLightSpace[8];
    for( int index =0; index < 8; ++index ) 
    {
        XMVECTOR v = XMLoadFloat3( &tmp[index] );
        vSceneAABBPointsLightSpace[index] = XMVector3Transform( v, matLightCameraView ); 
    }
    
    FLOAT fFrustumIntervalBegin, fFrustumIntervalEnd;
    XMVECTOR vLightCameraOrthographicMin;  // light space frustrum aabb 
    XMVECTOR vLightCameraOrthographicMax;
    FLOAT fCameraNearFarRange = m_pViewerCamera->GetFarClip() - m_pViewerCamera->GetNearClip();
       
    XMVECTOR vWorldUnitsPerTexel = g_vZero; 

    // We loop over the cascades to calculate the orthographic projection for each cascade.
    for( INT iCascadeIndex=0; iCascadeIndex < m_CopyOfCascadeConfig.m_nCascadeLevels; ++iCascadeIndex ) 
    {
        // Calculate the interval of the View Frustum that this cascade covers. We measure the interval 
        // the cascade covers as a Min and Max distance along the Z Axis.
        if( m_eSelectedCascadesFit == FIT_TO_CASCADES ) 
        {
            // Because we want to fit the orthogrpahic projection tightly around the Cascade, we set the Mimiumum cascade 
            // value to the previous Frustum end Interval
            if( iCascadeIndex==0 ) fFrustumIntervalBegin = 0.0f;
            else fFrustumIntervalBegin = (FLOAT)m_iCascadePartitionsZeroToOne[ iCascadeIndex - 1 ];
        } 
        else 
        {
            // In the FIT_TO_SCENE technique the Cascades overlap eachother.  In other words, interval 1 is coverd by
            // cascades 1 to 8, interval 2 is covered by cascades 2 to 8 and so forth.
            fFrustumIntervalBegin = 0.0f;
        }

        // Scale the intervals between 0 and 1. They are now percentages that we can scale with.
        fFrustumIntervalEnd = (FLOAT)m_iCascadePartitionsZeroToOne[ iCascadeIndex ];        
        fFrustumIntervalBegin/= (FLOAT)m_iCascadePartitionsMax;
        fFrustumIntervalEnd/= (FLOAT)m_iCascadePartitionsMax;
        fFrustumIntervalBegin = fFrustumIntervalBegin * fCameraNearFarRange;
        fFrustumIntervalEnd = fFrustumIntervalEnd * fCameraNearFarRange;
        XMVECTOR vFrustumPoints[8];

        // This function takes the began and end intervals along with the projection matrix and returns the 8
        // points that repreresent the cascade Interval
        CreateFrustumPointsFromCascadeInterval( fFrustumIntervalBegin, fFrustumIntervalEnd, 
            matViewCameraProjection, vFrustumPoints );

        vLightCameraOrthographicMin = g_vFLTMAX;
        vLightCameraOrthographicMax = g_vFLTMIN;

        XMVECTOR vTempTranslatedCornerPoint;
        // This next section of code calculates the min and max values for the orthographic projection.
        for( int icpIndex=0; icpIndex < 8; ++icpIndex ) 
        {
            // Transform the frustum from camera view space to world space.
            vFrustumPoints[icpIndex] = XMVector4Transform ( vFrustumPoints[icpIndex], matInverseViewCamera );
            // Transform the point from world space to Light Camera Space.
            vTempTranslatedCornerPoint = XMVector4Transform ( vFrustumPoints[icpIndex], matLightCameraView );
            // Find the closest point.
            vLightCameraOrthographicMin = XMVectorMin ( vTempTranslatedCornerPoint, vLightCameraOrthographicMin );
            vLightCameraOrthographicMax = XMVectorMax ( vTempTranslatedCornerPoint, vLightCameraOrthographicMax );
        }


        // This code removes the shimmering effect along the edges of shadows due to
        // the light changing to fit the camera.
        if( m_eSelectedCascadesFit == FIT_TO_SCENE ) 
        {
            // Fit the ortho projection to the cascades far plane and a near plane of zero. 
            // Pad the projection to be the size of the diagonal of the Frustum partition. 
            // 
            // To do this, we pad the ortho transform so that it is always big enough to cover 
            // the entire camera view frustum.
            XMVECTOR vDiagonal = vFrustumPoints[0] - vFrustumPoints[6];
            vDiagonal = XMVector3Length( vDiagonal );
            
            // The bound is the length of the diagonal of the frustum interval.
            FLOAT fCascadeBound = XMVectorGetX( vDiagonal );
            
            // The offset calculated will pad the ortho projection so that it is always the same size 
            // and big enough to cover the entire cascade interval.
            XMVECTOR vBoarderOffset = ( vDiagonal - 
                                        ( vLightCameraOrthographicMax - vLightCameraOrthographicMin ) ) 
                                        * g_vHalfVector;
            // Set the Z and W components to zero.
            vBoarderOffset *= g_vMultiplySetzwToZero;
            
            // Add the offsets to the projection.
            vLightCameraOrthographicMax += vBoarderOffset;
            vLightCameraOrthographicMin -= vBoarderOffset;

            // The world units per texel are used to snap the shadow the orthographic projection
            // to texel sized increments.  This keeps the edges of the shadows from shimmering.
            FLOAT fWorldUnitsPerTexel = fCascadeBound / (float)m_CopyOfCascadeConfig.m_iBufferSize;
            vWorldUnitsPerTexel = XMVectorSet( fWorldUnitsPerTexel, fWorldUnitsPerTexel, 0.0f, 0.0f ); 


        } 
        else if( m_eSelectedCascadesFit == FIT_TO_CASCADES ) 
        {

            // We calculate a looser bound based on the size of the PCF blur.  This ensures us that we're 
            // sampling within the correct map.
            float fScaleDuetoBlureAMT = ( (float)( m_iShadowBlurSize * 2 + 1 ) 
                /(float)m_CopyOfCascadeConfig.m_iBufferSize );
            XMVECTORF32 vScaleDuetoBlureAMT = { fScaleDuetoBlureAMT, fScaleDuetoBlureAMT, 0.0f, 0.0f };

            
            float fNormalizeByBufferSize = ( 1.0f / (float)m_CopyOfCascadeConfig.m_iBufferSize );
            XMVECTOR vNormalizeByBufferSize = XMVectorSet( fNormalizeByBufferSize, fNormalizeByBufferSize, 0.0f, 0.0f );

            // We calculate the offsets as a percentage of the bound.
            XMVECTOR vBoarderOffset = vLightCameraOrthographicMax - vLightCameraOrthographicMin;
            vBoarderOffset *= g_vHalfVector;
            vBoarderOffset *= vScaleDuetoBlureAMT;
            vLightCameraOrthographicMax += vBoarderOffset;
            vLightCameraOrthographicMin -= vBoarderOffset;
           
            // The world units per texel are used to snap  the orthographic projection
            // to texel sized increments.  
            // Because we're fitting tighly to the cascades, the shimmering shadow edges will still be present when the 
            // camera rotates.  However, when zooming in or strafing the shadow edge will not shimmer.
            vWorldUnitsPerTexel = vLightCameraOrthographicMax - vLightCameraOrthographicMin;
            vWorldUnitsPerTexel *= vNormalizeByBufferSize;

        }


        if( m_bMoveLightTexelSize ) 
        {

            // We snape the camera to 1 pixel increments so that moving the camera does not cause the shadows to jitter.
            // This is a matter of integer dividing by the world space size of a texel
            vLightCameraOrthographicMin /= vWorldUnitsPerTexel;
            vLightCameraOrthographicMin = XMVectorFloor( vLightCameraOrthographicMin );
            vLightCameraOrthographicMin *= vWorldUnitsPerTexel;
            
            vLightCameraOrthographicMax /= vWorldUnitsPerTexel;
            vLightCameraOrthographicMax = XMVectorFloor( vLightCameraOrthographicMax );
            vLightCameraOrthographicMax *= vWorldUnitsPerTexel;

        }
 
        //These are the unconfigured near and far plane values.  They are purposly awful to show 
        // how important calculating accurate near and far planes is.
        FLOAT fNearPlane = 0.0f;
        FLOAT fFarPlane = 10000.0f;

        if( m_eSelectedNearFarFit == FIT_NEARFAR_AABB ) 
        {

            XMVECTOR vLightSpaceSceneAABBminValue = g_vFLTMAX;  // world space scene aabb 
            XMVECTOR vLightSpaceSceneAABBmaxValue = g_vFLTMIN;       
            // We calculate the min and max vectors of the scene in light space. The min and max "Z" values of the  
            // light space AABB can be used for the near and far plane. This is easier than intersecting the scene with the AABB
            // and in some cases provides similar results.
            for(int index=0; index< 8; ++index) 
            {
                vLightSpaceSceneAABBminValue = XMVectorMin( vSceneAABBPointsLightSpace[index], vLightSpaceSceneAABBminValue );
                vLightSpaceSceneAABBmaxValue = XMVectorMax( vSceneAABBPointsLightSpace[index], vLightSpaceSceneAABBmaxValue );
            }

            // The min and max z values are the near and far planes.
            fNearPlane = XMVectorGetZ( vLightSpaceSceneAABBminValue );
            fFarPlane = XMVectorGetZ( vLightSpaceSceneAABBmaxValue );
        } 
        else if( m_eSelectedNearFarFit == FIT_NEARFAR_SCENE_AABB ) 
        {
            // By intersecting the light frustum with the scene AABB we can get a tighter bound on the near and far plane.
            ComputeNearAndFar( fNearPlane, fFarPlane, vLightCameraOrthographicMin, 
                vLightCameraOrthographicMax, vSceneAABBPointsLightSpace );
        } 
        else 
        {
        
        }
        // Craete the orthographic projection for this cascade.
        m_matShadowProj[ iCascadeIndex ] = XMMatrixOrthographicOffCenterLH( XMVectorGetX( vLightCameraOrthographicMin ), XMVectorGetX( vLightCameraOrthographicMax ), 
                                                                            XMVectorGetY( vLightCameraOrthographicMin ),XMVectorGetY( vLightCameraOrthographicMax ),
                                                                            fNearPlane, fFarPlane );

        m_fCascadePartitionsFrustum[ iCascadeIndex ] = fFrustumIntervalEnd;
    }
    m_matShadowView = m_pLightCamera->GetViewMatrix();
   

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Render the cascades into a texture atlas.
//--------------------------------------------------------------------------------------
HRESULT VarianceShadowsManager::RenderShadowsForAllCascades ( ID3D11DeviceContext* pd3dDeviceContext, CDXUTSDKMesh* pMesh )
{
    HRESULT hr = S_OK;

    bool static first = true;

    pd3dDeviceContext->RSSetState( m_prsShadow );

    for ( int iCurrentCascade=0; iCurrentCascade < m_CopyOfCascadeConfig.m_nCascadeLevels; ++iCurrentCascade ) 
    {
        pd3dDeviceContext->ClearDepthStencilView( m_pTemporaryShadowDepthBufferDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );
        pd3dDeviceContext->OMSetRenderTargets( 1, &m_pCascadedShadowMapVarianceRTVArrayAll[iCurrentCascade], m_pTemporaryShadowDepthBufferDSV );
        pd3dDeviceContext->RSSetViewports( 1, &m_RenderOneTileVP );
        XMMATRIX mWorld = m_pLightCamera->GetWorldMatrix();
        XMMATRIX mProj = m_pLightCamera->GetProjMatrix();

        XMMATRIX mWorldViewProjection =  m_matShadowView * m_matShadowProj[iCurrentCascade];
        
        // VS Per object
        D3D11_MAPPED_SUBRESOURCE MappedResource;
        V( pd3dDeviceContext->Map( m_pcbGlobalConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
        auto pVSPerObject = reinterpret_cast<CB_ALL_SHADOW_DATA*>( MappedResource.pData );
        XMStoreFloat4x4( &pVSPerObject->m_WorldViewProj, XMMatrixTranspose( mWorldViewProjection ) );

        XMMATRIX id = XMMatrixIdentity();
        XMStoreFloat4x4( &pVSPerObject->m_World, id );
        pd3dDeviceContext->Unmap( m_pcbGlobalConstantBuffer, 0 );
        pd3dDeviceContext->IASetInputLayout( m_pVertexLayoutMesh );

        pd3dDeviceContext->VSSetShader( m_pvsRenderVarianceShadow, nullptr, 0 );
        pd3dDeviceContext->PSSetShader( m_ppsRenderVarianceShadow, nullptr, 0 );
        pd3dDeviceContext->GSSetShader( nullptr, nullptr, 0 );
        pd3dDeviceContext->VSSetConstantBuffers( 0, 1, &m_pcbGlobalConstantBuffer );
        pMesh->Render( pd3dDeviceContext, 0, 1 );
    }

    ID3D11DepthStencilView *dsNullview = nullptr;
    pd3dDeviceContext->IASetInputLayout( nullptr );
    pd3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    pd3dDeviceContext->VSSetShader( m_pvsQuadBlur, nullptr, 0 );
    pd3dDeviceContext->PSSetSamplers( 5, 1, &m_pSamShadowPoint );
    ID3D11ShaderResourceView *srvNull = nullptr;

    if (m_iShadowBlurSize > 1 ) {
        INT iKernelShader = ( m_iShadowBlurSize / 2 - 1 ); //3 goes to 0, 5 goes to 1 
        for ( int iCurrentCascade=0; iCurrentCascade < m_CopyOfCascadeConfig.m_nCascadeLevels; ++iCurrentCascade ) 
        {
            pd3dDeviceContext->PSSetShaderResources( 5, 1, &srvNull );
            pd3dDeviceContext->OMSetRenderTargets(1, &m_pCascadedShadowMapTempBlurRTV, dsNullview );
            pd3dDeviceContext->PSSetShaderResources( 5, 1, &m_pCascadedShadowMapVarianceSRVArrayAll[iCurrentCascade] );
            pd3dDeviceContext->PSSetShader( m_ppsQuadBlurX[iKernelShader], nullptr, 0 );
            pd3dDeviceContext->Draw(4, 0);

            pd3dDeviceContext->PSSetShaderResources( 5, 1, &srvNull );
            pd3dDeviceContext->OMSetRenderTargets(1, &m_pCascadedShadowMapVarianceRTVArrayAll[iCurrentCascade], dsNullview );
            pd3dDeviceContext->PSSetShaderResources( 5, 1, &m_pCascadedShadowMapTempBlurSRV );
            pd3dDeviceContext->PSSetShader( m_ppsQuadBlurY[iKernelShader], nullptr, 0 );
            pd3dDeviceContext->Draw(4, 0);

        }
    }
    ID3D11RenderTargetView* nullView = nullptr;
    pd3dDeviceContext->RSSetState( nullptr );
    pd3dDeviceContext->OMSetRenderTargets(1, &nullView, nullptr );
  
    first = false;

    return hr;

}


//--------------------------------------------------------------------------------------
// Render the scene.
//--------------------------------------------------------------------------------------
HRESULT VarianceShadowsManager::RenderScene ( ID3D11DeviceContext* pd3dDeviceContext, 
                                      ID3D11RenderTargetView* prtvBackBuffer, 
                                      ID3D11DepthStencilView* pdsvBackBuffer, 
                                      CDXUTSDKMesh* pMesh,  
                                      CFirstPersonCamera* pActiveCamera,
                                      D3D11_VIEWPORT* dxutViewPort,
                                      bool bVisualize
            )
{
    HRESULT hr = S_OK;
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    // We have a seperate render state for the actual rasterization because of different depth biases and Cull modes.
    pd3dDeviceContext->RSSetState( m_prsScene );
    // 
    pd3dDeviceContext->OMSetRenderTargets( 1, &prtvBackBuffer, pdsvBackBuffer );
    pd3dDeviceContext->RSSetViewports( 1, dxutViewPort );
    pd3dDeviceContext->IASetInputLayout( m_pVertexLayoutMesh );

    XMMATRIX matCameraProj = pActiveCamera->GetProjMatrix();
    XMMATRIX matCameraView = pActiveCamera->GetViewMatrix();

    // The user has the option to view the ortho shadow cameras.
    if( m_eSelectedCamera >= ORTHO_CAMERA1 ) 
    { 
        // In the CAMERA_SELECTION enumeration, value 0 is EYE_CAMERA
        // value 1 is LIGHT_CAMERA and 2 to 10 are the ORTHO_CAMERA values.
        // Subtract to so that we can use the enum to index.
        matCameraProj = m_matShadowProj[(int)m_eSelectedCamera-2];
        matCameraView = m_matShadowView;
    }

    XMMATRIX matWorldViewProjection = matCameraView * matCameraProj;
    
    V( pd3dDeviceContext->Map( m_pcbGlobalConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    auto pcbAllShadowConstants = reinterpret_cast<CB_ALL_SHADOW_DATA*>( MappedResource.pData );
    XMStoreFloat4x4( &pcbAllShadowConstants->m_WorldViewProj, XMMatrixTranspose( matWorldViewProjection ) );
    XMStoreFloat4x4( &pcbAllShadowConstants->m_WorldView, XMMatrixTranspose( matCameraView ) ); 
    // These are the for loop begin end values. 
    // This is a floating point number that is used as the percentage to blur between maps.    
    pcbAllShadowConstants->m_fCascadeBlendArea = m_fBlurBetweenCascadesAmount;
    pcbAllShadowConstants->m_fTexelSize = 1.0f / (float)m_CopyOfCascadeConfig.m_iBufferSize; 
    pcbAllShadowConstants->m_fNativeTexelSizeInX = pcbAllShadowConstants->m_fTexelSize / m_CopyOfCascadeConfig.m_nCascadeLevels;
    XMMATRIX matIdentity = XMMatrixIdentity();
    XMStoreFloat4x4( &pcbAllShadowConstants->m_World, matIdentity );
    XMMATRIX matTextureScale = XMMatrixScaling(  0.5f,-0.5f,1.0f );
    XMMATRIX matTextureTranslation = XMMatrixTranslation( .5,.5,0 );
    XMMATRIX scaleToTile = XMMatrixScaling( 1.0f / (float)m_pCascadeConfig->m_nCascadeLevels, 1.0, 1.0 );
 
    XMStoreFloat4x4( &pcbAllShadowConstants->m_Shadow, XMMatrixTranspose( m_matShadowView ) );
    for(int index=0; index < m_CopyOfCascadeConfig.m_nCascadeLevels; ++index ) 
    {
        XMMATRIX mShadowTexture = m_matShadowProj[index] * matTextureScale * matTextureTranslation;
        pcbAllShadowConstants->m_vCascadeScale[index].x = XMVectorGetX( mShadowTexture.r[0] );
        pcbAllShadowConstants->m_vCascadeScale[index].y = XMVectorGetY( mShadowTexture.r[1] );
        pcbAllShadowConstants->m_vCascadeScale[index].z = XMVectorGetZ( mShadowTexture.r[2] );
        pcbAllShadowConstants->m_vCascadeScale[index].w = 1;

        XMStoreFloat3( reinterpret_cast<XMFLOAT3*>( &pcbAllShadowConstants->m_vCascadeOffset[index] ), mShadowTexture.r[3] );
        pcbAllShadowConstants->m_vCascadeOffset[index].w = 0;

    }
    // Copy intervals for the depth interval selection method.
    memcpy( pcbAllShadowConstants->m_fCascadeFrustumsEyeSpaceDepths, 
            m_fCascadePartitionsFrustum, MAX_CASCADES*4 );
    
    // The border padding values keep the pixel shader from reading the borders during PCF filtering.
    pcbAllShadowConstants->m_fMaxBorderPadding = (float)( m_pCascadeConfig->m_iBufferSize  - 1.0f ) / 
        (float)m_pCascadeConfig->m_iBufferSize;
    pcbAllShadowConstants->m_fMinBorderPadding = (float)( 1.0f ) / 
        (float)m_pCascadeConfig->m_iBufferSize;

    XMVECTOR ep = m_pLightCamera->GetEyePt();
    XMVECTOR lp = m_pLightCamera->GetLookAtPt();
    ep -= lp;
    ep = XMVector3Normalize( ep );

    XMStoreFloat3( reinterpret_cast<XMFLOAT3*>( &pcbAllShadowConstants->m_vLightDir ), ep );
    pcbAllShadowConstants->m_vLightDir.w = 1.0f;
    pcbAllShadowConstants->m_nCascadeLevels = m_CopyOfCascadeConfig.m_nCascadeLevels;
    pcbAllShadowConstants->m_iVisualizeCascades = bVisualize;
    pd3dDeviceContext->Unmap( m_pcbGlobalConstantBuffer, 0 );


    pd3dDeviceContext->PSSetSamplers( 0, 1, &m_pSamLinear );
    pd3dDeviceContext->PSSetSamplers( 1, 1, &m_pSamLinear );

    switch (m_eShadowFilter) 
    {
        case ( SHADOW_FILTER_ANISOTROPIC_16 ):
            pd3dDeviceContext->PSSetSamplers( 5, 1, &m_pSamShadowAnisotropic16 );
        break;
        case ( SHADOW_FILTER_ANISOTROPIC_8 ): 
            pd3dDeviceContext->PSSetSamplers( 5, 1, &m_pSamShadowAnisotropic8 );
        break;
        case ( SHADOW_FILTER_ANISOTROPIC_4 ): 
            pd3dDeviceContext->PSSetSamplers( 5, 1, &m_pSamShadowAnisotropic4 );
        break;
        case ( SHADOW_FILTER_ANISOTROPIC_2 ) :
            pd3dDeviceContext->PSSetSamplers( 5, 1, &m_pSamShadowAnisotropic2 );
        break;
        case ( SHADOW_FILTER_LINEAR ) :
            pd3dDeviceContext->PSSetSamplers( 5, 1, &m_pSamShadowLinear );
        break;
        case ( SHADOW_FILTER_POINT ) :
            pd3dDeviceContext->PSSetSamplers( 5, 1, &m_pSamShadowPoint );
        break;
    }
    /*if ( m_eShadowFilterType == D3D11_FILTER_ANISOTROPIC ) 
    {
        pd3dDeviceContext->PSSetSamplers( 5, 1, &m_pSamShadowAniso );
    }
    else if ( m_eShadowFilterType == D3D11_FILTER_MIN_MAG_MIP_LINEAR )
    {
        pd3dDeviceContext->PSSetSamplers( 5, 1, &m_pSamShadowLinear );
    }
    else if ( m_eShadowFilterType == D3D11_FILTER_MIN_MAG_MIP_LINEAR )
    {
        pd3dDeviceContext->PSSetSamplers( 5, 1, &m_pSamShadowPoint );
    }
    */
    pd3dDeviceContext->GSSetShader( nullptr, nullptr, 0 );

    size_t j = std::max( m_CopyOfCascadeConfig.m_nCascadeLevels-1, 0 );

    pd3dDeviceContext->VSSetShader( m_pvsRenderScene[j], nullptr, 0 );
    
    // There are up to 8 cascades,  blur between cascades, 
    // and two cascade selection maps.  This is a total of 32 permutations of the shader.

    pd3dDeviceContext->PSSetShader(
        m_ppsRenderSceneAllShaders[j]
                                  [m_iBlurBetweenCascades]
                                  [m_eSelectedCascadeSelection],
                                  nullptr, 0 );

    pd3dDeviceContext->PSSetShaderResources( 5,1, &m_pCascadedShadowMapVarianceSRVArraySingle );

    pd3dDeviceContext->VSSetConstantBuffers( 0, 1, &m_pcbGlobalConstantBuffer);
    pd3dDeviceContext->PSSetConstantBuffers( 0, 1, &m_pcbGlobalConstantBuffer);

    pMesh->Render( pd3dDeviceContext, 0, 1 );

    ID3D11ShaderResourceView* nv[] = { nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr};
    pd3dDeviceContext->PSSetShaderResources( 5,8, nv ); 


    return hr;
}


