//--------------------------------------------------------------------------------------
// File: ShadowSampleMisc.h
//
// This sample demonstrates cascaded shadow maps.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include <D3DCompiler.h>
#include <DirectXMath.h>

#define MAX_CASCADES 8

// Used to do selection of the shadow buffer format.
enum SHADOW_TEXTURE_FORMAT 
{
    CASCADE_DXGI_FORMAT_R32_TYPELESS,
    CASCADE_DXGI_FORMAT_R24G8_TYPELESS,
    CASCADE_DXGI_FORMAT_R16_TYPELESS,
    CASCADE_DXGI_FORMAT_R8_TYPELESS
};

enum SCENE_SELECTION 
{
    POWER_PLANT_SCENE,
    TEST_SCENE
};

enum FIT_PROJECTION_TO_CASCADES 
{
    FIT_TO_CASCADES,
    FIT_TO_SCENE
};

enum FIT_TO_NEAR_FAR 
{
    FIT_NEARFAR_PANCAKING,
    FIT_NEARFAR_ZERO_ONE,
    FIT_NEARFAR_AABB,
    FIT_NEARFAR_SCENE_AABB
};

enum CASCADE_SELECTION 
{
    CASCADE_SELECTION_MAP,
    CASCADE_SELECTION_INTERVAL
};

enum CAMERA_SELECTION 
{
    EYE_CAMERA,
    LIGHT_CAMERA,
    ORTHO_CAMERA1,
    ORTHO_CAMERA2,
    ORTHO_CAMERA3,
    ORTHO_CAMERA4,
    ORTHO_CAMERA5,
    ORTHO_CAMERA6,
    ORTHO_CAMERA7,
    ORTHO_CAMERA8
};
// when these paramters change, we must reallocate the shadow resources.
struct CascadeConfig 
{
    INT m_nCascadeLevels;
    SHADOW_TEXTURE_FORMAT m_ShadowBufferFormat;
    INT m_iBufferSize;
};


struct CB_ALL_SHADOW_DATA
{
    DirectX::XMFLOAT4X4  m_WorldViewProj;
    DirectX::XMFLOAT4X4  m_World;
    DirectX::XMFLOAT4X4  m_WorldView;
    DirectX::XMFLOAT4X4  m_Shadow;
    DirectX::XMFLOAT4 m_vCascadeOffset[8];
    DirectX::XMFLOAT4 m_vCascadeScale[8];

    INT         m_nCascadeLevels; // Number of Cascades
    INT         m_iVisualizeCascades; // 1 is to visualize the cascades in different colors. 0 is to just draw the scene.
    INT         m_iPCFBlurForLoopStart; // For loop begin value. For a 5x5 kernal this would be -2.
    INT         m_iPCFBlurForLoopEnd; // For loop end value. For a 5x5 kernel this would be 3.

    // For Map based selection scheme, this keeps the pixels inside of the the valid range.
    // When there is no boarder, these values are 0 and 1 respectivley.
    FLOAT       m_fMinBorderPadding;     
    FLOAT       m_fMaxBorderPadding;
    FLOAT       m_fShadowBiasFromGUI;  // A shadow map offset to deal with self shadow artifacts.  
                                        //These artifacts are aggravated by PCF.
    FLOAT       m_fShadowPartitionSize; 
    FLOAT       m_fCascadeBlendArea; // Amount to overlap when blending between cascades.
    FLOAT       m_fTexelSize; // Shadow map texel size.
    FLOAT       m_fNativeTexelSizeInX; // Texel size in native map ( textures are packed ).
    FLOAT       m_fPaddingForCB3;// Padding variables CBs must be a multiple of 16 bytes.
    FLOAT       m_fCascadeFrustumsEyeSpaceDepths[8]; // The values along Z that seperate the cascades.
    DirectX::XMFLOAT4 m_fCascadeFrustumsEyeSpaceDepthsFloat4[8];// the values along Z that separte the cascades.  
                                                          // Wastefully stored in float4 so they are array indexable :(
    DirectX::XMFLOAT4 m_vLightDir;
};