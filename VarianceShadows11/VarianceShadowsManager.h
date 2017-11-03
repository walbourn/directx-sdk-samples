//--------------------------------------------------------------------------------------
// File: CascadedShadowManager.h
//
// This is where the shadows are calcaulted and rendered.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "ShadowSampleMisc.h"

class CFirstPersonCamera;
class CDXUTSDKMesh;

#pragma warning(push)
#pragma warning(disable: 4324)

__declspec(align(16)) class VarianceShadowsManager 
{
public:
    VarianceShadowsManager();
    ~VarianceShadowsManager();
    
    // This runs when the application is initialized.
    HRESULT Init( ID3D11Device* pd3dDevice, 
                  CDXUTSDKMesh* pMesh, 
                  CFirstPersonCamera* pViewerCamera,
                  CFirstPersonCamera* pLightCamera,
                  CascadeConfig* pCascadeConfig
                );
    
    HRESULT DestroyAndDeallocateShadowResources();

    // This runs per frame.  This data could be cached when the cameras do not move.
    HRESULT InitFrame( ID3D11Device* pd3dDevice ) ;

    HRESULT RenderShadowsForAllCascades( ID3D11DeviceContext* pd3dDeviceContext, CDXUTSDKMesh* pMesh );

    HRESULT RenderScene ( ID3D11DeviceContext* pd3dDeviceContext, 
                          ID3D11RenderTargetView* prtvBackBuffer, 
                          ID3D11DepthStencilView* pdsvBackBuffer, 
                          CDXUTSDKMesh* pMesh,  
                          CFirstPersonCamera* pActiveCamera,
                          D3D11_VIEWPORT* dxutViewPort,
                          bool bVisualize
                        );

    DirectX::XMVECTOR GetSceneAABBMin() const { return m_vSceneAABBMin; };
    DirectX::XMVECTOR GetSceneAABBMax() const { return m_vSceneAABBMax; };

    
    INT                                 m_iCascadePartitionsMax;
    FLOAT                               m_fCascadePartitionsFrustum[MAX_CASCADES]; // Values are  between near and far
    INT                                 m_iCascadePartitionsZeroToOne[MAX_CASCADES]; // Values are 0 to 100 and represent a percent of the frstum
    INT                                 m_iShadowBlurSize;
    INT                                 m_iBlurBetweenCascades;
    FLOAT                               m_fBlurBetweenCascadesAmount;

    bool                                m_bMoveLightTexelSize;
    CAMERA_SELECTION                    m_eSelectedCamera;
    FIT_PROJECTION_TO_CASCADES          m_eSelectedCascadesFit;
    FIT_TO_NEAR_FAR                     m_eSelectedNearFarFit;
    CASCADE_SELECTION                   m_eSelectedCascadeSelection;
    SHADOW_FILTER                       m_eShadowFilter;

private:

    // Compute the near and far plane by intersecting an Ortho Projection with the Scenes AABB.
    void ComputeNearAndFar( FLOAT& fNearPlane, 
                            FLOAT& fFarPlane, 
                            DirectX::FXMVECTOR vLightCameraOrthographicMin, 
                            DirectX::FXMVECTOR vLightCameraOrthographicMax, 
                            DirectX::XMVECTOR* pvPointsInCameraView 
                          );
    

    void CreateFrustumPointsFromCascadeInterval ( FLOAT fCascadeIntervalBegin, 
                                                  FLOAT fCascadeIntervalEnd, 
                                                  DirectX::CXMMATRIX vProjection,
                                                  DirectX::XMVECTOR* pvCornerPointsWorld
                                                );

    HRESULT ReleaseAndAllocateNewShadowResources( ID3D11Device* pd3dDevice );  // This is called when cascade config changes. 

    DirectX::XMVECTOR                   m_vSceneAABBMin;
    DirectX::XMVECTOR                   m_vSceneAABBMax;
                                                                               // For example: when the shadow buffer size changes.
    char                                m_cvsModel[31];
    char                                m_cpsModel[31];
    char                                m_cgsModel[31];
    DirectX::XMMATRIX                   m_matShadowProj[MAX_CASCADES]; 
    DirectX::XMMATRIX                   m_matShadowView;
    CascadeConfig                       m_CopyOfCascadeConfig;      // This copy is used to determine when settings change. 
                                                                    //Some of these settings require new buffer allocations.
    CascadeConfig*                      m_pCascadeConfig;           // Pointer to the most recent setting.

// D3D11 variables
    ID3D11VertexShader*                 m_pvsQuadBlur;
    ID3DBlob*                           m_pvsQuadBlurBlob;
    
    ID3D11PixelShader*                  m_ppsQuadBlurX[MAXIMUM_BLUR_LEVELS];
    ID3DBlob*                           m_ppsQuadBlurXBlob[MAXIMUM_BLUR_LEVELS];
    ID3D11PixelShader*                  m_ppsQuadBlurY[MAXIMUM_BLUR_LEVELS];
    ID3DBlob*                           m_ppsQuadBlurYBlob[MAXIMUM_BLUR_LEVELS];

    ID3D11VertexShader*                 m_pvsRenderVarianceShadow;
    ID3DBlob*                           m_pvsRenderVarianceShadowBlob;
    ID3D11PixelShader*                  m_ppsRenderVarianceShadow;
    ID3DBlob*                           m_ppsRenderVarianceShadowBlob;

    ID3D11InputLayout*                  m_pVertexLayoutMesh;
    ID3D11VertexShader*                 m_pvsRenderScene[MAX_CASCADES];
    ID3DBlob*                           m_pvsRenderSceneBlob[MAX_CASCADES];
    ID3D11PixelShader*                  m_ppsRenderSceneAllShaders[MAX_CASCADES][2][2];
    ID3DBlob*                           m_ppsRenderSceneAllShadersBlob[MAX_CASCADES][2][2];

    ID3D11Texture2D*                    m_pCascadedShadowMapVarianceTextureArray;
    ID3D11RenderTargetView*             m_pCascadedShadowMapVarianceRTVArrayAll[MAX_CASCADES];
    ID3D11ShaderResourceView*           m_pCascadedShadowMapVarianceSRVArrayAll[MAX_CASCADES];
    ID3D11ShaderResourceView*           m_pCascadedShadowMapVarianceSRVArraySingle;
    
    ID3D11Texture2D*                    m_pTemporaryShadowDepthBufferTexture;
    ID3D11DepthStencilView*             m_pTemporaryShadowDepthBufferDSV;

    ID3D11Texture2D*                    m_pCascadedShadowMapTempBlurTexture;
    ID3D11RenderTargetView*             m_pCascadedShadowMapTempBlurRTV;
    ID3D11ShaderResourceView*           m_pCascadedShadowMapTempBlurSRV;

    ID3D11Buffer*                       m_pcbGlobalConstantBuffer; // All VS and PS constants are in the same buffer.  
                                                          // An actual title would break this up into multiple 
                                                          // buffers updated based on frequency of variable changes

    ID3D11RasterizerState*              m_prsScene;
    ID3D11RasterizerState*              m_prsShadow;
    
    D3D11_VIEWPORT                      m_RenderVP[MAX_CASCADES];
    D3D11_VIEWPORT                      m_RenderOneTileVP;

    CFirstPersonCamera*                 m_pViewerCamera;         
    CFirstPersonCamera*                 m_pLightCamera;         

    ID3D11SamplerState*                 m_pSamLinear;
    ID3D11SamplerState*                 m_pSamShadowPoint;
    ID3D11SamplerState*                 m_pSamShadowLinear;
    ID3D11SamplerState*                 m_pSamShadowAnisotropic16;
    ID3D11SamplerState*                 m_pSamShadowAnisotropic8;
    ID3D11SamplerState*                 m_pSamShadowAnisotropic4;
    ID3D11SamplerState*                 m_pSamShadowAnisotropic2;
};

#pragma warning(pop)
