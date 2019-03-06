//--------------------------------------------------------------------------------------
// Backbuffer_CS.fx
//
// Copyright (c) Stefan Petersson, 2012. All rights reserved.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Input and Output Structures
//--------------------------------------------------------------------------------------
Texture2D<float4> inputTex	: register(t0);
RWTexture2D<float4> output : register(u0);

//Constant buffers
cbuffer cbEveryframe : register(b0)
{ 
	float Timer;
};

//Sampler states
SamplerState texSampler : register(s0);

groupshared float GroupIntensity;

//****************************************************************************
[numthreads(32, 32, 1)]
void main( uint3 threadID : SV_DispatchThreadID, uint groupIndex : SV_GroupIndex )
{
	float2 texCoord = float2(threadID.x / 800.0f, threadID.y / 800.0f);
	
	float scalar = Timer;
	texCoord.y += sin(texCoord.y * texCoord.x *10)*0.003 * sin(scalar * 6.28f * 5.0f) +
		cos(texCoord.x * texCoord.y *10)*0.003 * cos(scalar * 6.28f * 5.0f);

	float3 tc = inputTex.SampleLevel(texSampler, texCoord, 0).xyz;
	
	float intensity = abs(sin(Timer)) * (1 - length(threadID.xy - float2(400, 400)) / 800.0f);

	output[threadID.xy] = float4(tc, 1);
}