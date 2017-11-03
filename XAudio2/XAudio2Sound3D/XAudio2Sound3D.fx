//--------------------------------------------------------------------------------------
// File: XAudio2Sound3D.fx
//
// The effect file for the XAudio Sound 3D sample.  
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
cbuffer cbPerObject : register( b0 )
{
    float4x4 g_mTransform;
}

//--------------------------------------------------------------------------------------
// Vertex shader output structure
//--------------------------------------------------------------------------------------
struct VS_INPUT
{
	float4 vPosition	: SV_Position;
    float4 color        : COLOR;
};

struct VS_OUTPUT
{
    float4 Position : SV_Position;
    float4 Color    : COLOR;
};

//--------------------------------------------------------------------------------------
// This shader computes standard transform and lighting
//--------------------------------------------------------------------------------------
VS_OUTPUT RenderSceneVS( VS_INPUT vIn )
{
    VS_OUTPUT output;
    
    output.Position = mul( vIn.vPosition, g_mTransform );
    
    output.Color = vIn.color;
    
    return output;
}


//--------------------------------------------------------------------------------------
// This shader outputs the pixel's color by modulating the texture's
// color with diffuse material color
//--------------------------------------------------------------------------------------
float4 RenderScenePS( VS_OUTPUT input ) : SV_TARGET
{ 
    return input.Color;
}
