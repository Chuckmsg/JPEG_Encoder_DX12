//--------------------------------------------------------------------------------------
// File: Jpeg_CS.hlsl
//
// HLSL part of the baseline JPEG encoder
//
// Copyright (c) 2012 Stefan Petersson
//--------------------------------------------------------------------------------------

#pragma warning( disable : 3078 )

static const uint ZigZagIndices[64] =
{
	0,1,8,16,9,2,3,10,
	17,24,32,25,18,11,4,5,
	12,19,26,33,40,48,41,34,
	27,20,13,6,7,14,21,28,
	35,42,49,56,57,50,43,36,
	29,22,15,23,30,37,44,51,
	58,59,52,45,38,31,39,46,
	53,60,61,54,47,55,62,63
};

typedef int BitString;

struct BSResult
{
	int NumEntropyBits;
	BitString BS[6];
};

groupshared float TransformedPixelData[64];
groupshared float DCT_MatrixTmp[64];
groupshared float DCT_Coefficients[64];

groupshared int QuantizedComponents[64];
groupshared uint ZeroMask[64];
groupshared uint ScanArray[64];

groupshared int RemappedValues[64];
groupshared uint PrevIndex[64];

groupshared uint EntropyResult[32];



//Input buffers
StructuredBuffer<float> DCT_matrix				: register(t0);
StructuredBuffer<float> DCT_matrix_transpose	: register(t1);
Texture2D<float4> InputTex						: register(t2);

StructuredBuffer<float> Quantization_Table		: register(t3);
StructuredBuffer<BitString> AC_Huffman			: register(t4);

//Output buffer
RWStructuredBuffer<int> EntropyOut			: register(u0);

//Sampler states
SamplerState PointSampler						: register(s0);

//Constant buffers
cbuffer cbImageInfo								: register(b0)
{ 
	float ImageWidth;
	float ImageHeight;

	uint numBlocksX;
	uint numBlocksY;

	uint EntropyBlockSize;
};

float Get_YCbCr_Component_From_RGB(float3 RGB)
{
#ifdef COMPONENT_Y
	return dot(RGB,float3(0.299,0.587,0.114)) * 255.0f;
#elif COMPONENT_CB
	return dot(RGB,float3(-0.168736,-0.331264,0.5)) * 255.0f + 128.0f;
#elif COMPONENT_CR
	return dot(RGB,float3(0.5,-0.418688,-0.081312)) * 255.0f + 128.0f;
#endif
}

uint GetOutputIndex(uint GroupIndex : SV_GroupIndex, uint3 GroupID : SV_GroupID)
{
	int EBS = EntropyBlockSize;
	int outputIndex = 0;

#ifdef COMPONENT_Y
	#ifdef C_4_4_4
		outputIndex = (GroupID.y * numBlocksX + GroupID.x) * EBS * 3 + GroupIndex;
	#elif C_4_2_2
		int row_pitch = EBS * numBlocksX * 2;
		outputIndex = GroupID.y * row_pitch + (GroupID.x / 2) * EBS * 4 + (GroupID.x % 2) * EBS + GroupIndex;
	#elif C_4_2_0
		int row_pitch = EBS * numBlocksX *3;
		outputIndex = (GroupID.y / 2) * row_pitch + (GroupID.x / 2) * EBS * 6 + (GroupID.x % 2) * EBS + (EBS*2 * (GroupID.y%2)) + GroupIndex;
	#endif
#elif COMPONENT_CB
	#ifdef C_4_4_4
		outputIndex = (GroupID.y * numBlocksX + GroupID.x) * EBS * 3 + GroupIndex + EBS;
	#elif C_4_2_2
		outputIndex = EBS*2 + numBlocksX * 4 * EBS * GroupID.y + GroupID.x * (EBS*4) + GroupIndex;
	#elif C_4_2_0
		outputIndex = EBS*4 + numBlocksX * 6 * EBS * GroupID.y + GroupID.x * EBS * 6 + GroupIndex;
	#endif
#elif COMPONENT_CR
	#ifdef C_4_4_4
		outputIndex = (GroupID.y * numBlocksX + GroupID.x) * EBS * 3 + GroupIndex + EBS * 2;
	#elif C_4_2_2
		outputIndex = EBS*2 + numBlocksX * 4 * EBS * GroupID.y + GroupID.x * (EBS*4) + GroupIndex + EBS;
	#elif C_4_2_0
		outputIndex = EBS*4 + numBlocksX * 6 * EBS * GroupID.y + GroupID.x * EBS * 6 + GroupIndex + EBS;
	#endif
#endif

	return outputIndex;
}

float2 GetTexCoord(uint3 DispatchThreadID)
{
    return float2(DispatchThreadID.x / ImageWidth, DispatchThreadID.y / ImageHeight);
}

float Get_Transformed_YCbCr_Component(uint3 DispatchThreadID)
{
	float yuv_component = 0;

#ifdef COMPONENT_Y
	//float3 color = InputTex.SampleLevel(PointSampler, float2((float)DispatchThreadID.x / ImageWidth, (float)DispatchThreadID.y / ImageHeight), 0).xyz;

	float3 color = InputTex.SampleLevel(PointSampler, GetTexCoord(DispatchThreadID), 0).xyz;
	
	yuv_component = Get_YCbCr_Component_From_RGB(color);
#elif COMPONENT_CB
	#ifdef C_4_4_4
		float3 color = InputTex.SampleLevel(PointSampler, GetTexCoord(DispatchThreadID), 0).xyz;
		yuv_component = Get_YCbCr_Component_From_RGB(color);
	#endif

	#ifdef C_4_2_2
		float3 color = InputTex.SampleLevel(PointSampler, float2((float)DispatchThreadID.x * 2 / ImageWidth, (float)DispatchThreadID.y / ImageHeight), 0).xyz;
		float3 color2 = InputTex.SampleLevel(PointSampler, float2((float)DispatchThreadID.x * 2 / ImageWidth + 1.0f / ImageWidth, (float)DispatchThreadID.y / ImageHeight), 0).xyz;

		yuv_component = Get_YCbCr_Component_From_RGB(color);
		float yuv2 = Get_YCbCr_Component_From_RGB(color2);

		yuv_component = (yuv_component + yuv2) / 2;
	#endif

	#ifdef C_4_2_0
		float2 texCoord = float2((float)(DispatchThreadID.x * 2) / ImageWidth, (float)(DispatchThreadID.y * 2) / ImageHeight);
		float2 dx = float2(1.0f / ImageWidth, 0);
		float2 dy = float2(0, 1.0f / ImageHeight);
		float3 color = InputTex.SampleLevel(PointSampler, texCoord, 0).xyz;
		float3 color2 = InputTex.SampleLevel(PointSampler, texCoord + dx, 0).xyz;
		float3 color3 = InputTex.SampleLevel(PointSampler, texCoord + dy, 0).xyz;
		float3 color4 = InputTex.SampleLevel(PointSampler, texCoord + dx + dy, 0).xyz;

		yuv_component = Get_YCbCr_Component_From_RGB(color);
		float yuv2 = Get_YCbCr_Component_From_RGB(color2);
		float yuv3 = Get_YCbCr_Component_From_RGB(color3);
		float yuv4 = Get_YCbCr_Component_From_RGB(color4);

		yuv_component = (yuv_component + yuv2 + yuv3 + yuv4) / 4;
	#endif
#elif COMPONENT_CR
	#ifdef C_4_4_4
		float3 color = InputTex.SampleLevel(PointSampler, GetTexCoord(DispatchThreadID), 0).xyz;
		yuv_component = Get_YCbCr_Component_From_RGB(color);
	#endif

	#ifdef C_4_2_2
		float3 color = InputTex.SampleLevel(PointSampler, float2((float)DispatchThreadID.x * 2 / ImageWidth, (float)DispatchThreadID.y / ImageHeight), 0).xyz;
		float3 color2 = InputTex.SampleLevel(PointSampler, float2((float)DispatchThreadID.x * 2 / ImageWidth + 1.0f / ImageWidth, (float)DispatchThreadID.y / ImageHeight), 0).xyz;

		yuv_component = Get_YCbCr_Component_From_RGB(color);
		float yuv2 = Get_YCbCr_Component_From_RGB(color2);

		yuv_component = (yuv_component + yuv2) / 2;
	#endif

	#ifdef C_4_2_0
		float2 texCoord = float2((float)(DispatchThreadID.x * 2) / ImageWidth, (float)(DispatchThreadID.y * 2) / ImageHeight);
		float2 dx = float2(1.0f / ImageWidth, 0);
		float2 dy = float2(0, 1.0f / ImageHeight);
		float3 color = InputTex.SampleLevel(PointSampler, texCoord, 0).xyz;
		float3 color2 = InputTex.SampleLevel(PointSampler, texCoord + dx, 0).xyz;
		float3 color3 = InputTex.SampleLevel(PointSampler, texCoord + dy, 0).xyz;
		float3 color4 = InputTex.SampleLevel(PointSampler, texCoord + dx + dy, 0).xyz;

		yuv_component = Get_YCbCr_Component_From_RGB(color);
		float yuv2 = Get_YCbCr_Component_From_RGB(color2);
		float yuv3 = Get_YCbCr_Component_From_RGB(color3);
		float yuv4 = Get_YCbCr_Component_From_RGB(color4);

		yuv_component = (yuv_component + yuv2 + yuv3 + yuv4) / 4;
	#endif
#endif

	return max(-128.0f, min(127.0f, yuv_component - 128.0f));
}

void Sum(int thid)
{
	int n = 64;
    int offset = 1;

    // build the sum in place up the tree
    for(int d = n>>1; d > 0; d >>= 1)
    {
        GroupMemoryBarrierWithGroupSync();

        if (thid < d)      
        {
            int ai = offset*(2*thid+1)-1;
            int bi = offset*(2*thid+2)-1;

            ScanArray[bi] += ScanArray[ai];
        }

        offset *= 2;
    }

    // scan back down the tree

    // clear the last element
    if (thid == 0)
    {
        ScanArray[n - 1] = 0;
    }   

    // traverse down the tree building the scan in place
    for(int d = 1; d < n; d *= 2)
    {
        offset >>= 1;
        GroupMemoryBarrierWithGroupSync();

        if (thid < d)
        {
            int ai = offset*(2*thid+1)-1;
            int bi = offset*(2*thid+2)-1;

            float t   = ScanArray[ai];
            ScanArray[ai]  = ScanArray[bi];
            ScanArray[bi] += t;
        }
    }

	GroupMemoryBarrierWithGroupSync();
}


void put_bits_atomic(uint kc, uint startbit, uint numbits, uint codeword)
{
	uint cw32 = codeword;
	uint restbits = 32 - startbit - numbits;

	if ((startbit == 0) && (restbits == 0)) EntropyResult[kc] = cw32;
		else InterlockedOr(EntropyResult[kc], cw32 << restbits);
}

void Write(BitString bs, uint bitpos)
{
	int cw = (bs & 0x0000ffff);
	int cwlen = bs >> 16;

	uint kc = bitpos / 32;
	uint startbit = bitpos % 32;

	bool overflow = false;
	int numbits = cwlen;
	int cwpart = cw;

	if(startbit + cwlen > 32)
	{
		overflow = true;
		numbits = 32 - startbit;
		cwpart = cw >> (cwlen - numbits);
	}

	put_bits_atomic(kc, startbit, numbits, cwpart);

	if(overflow)
	{
		startbit = (startbit + numbits) % 32;
		cw &= ((1 << (cwlen - numbits)) - 1);

		//"next iteration"
		numbits = cwlen - numbits;
		cwpart = cw;

		put_bits_atomic(kc + 1, startbit, numbits, cwpart);
	}
}

uint ConvertEndian(uint value)
{
	return ((value >> 24) & 0xff) | ((value << 8) & 0xff0000) | ((value >> 8) & 0xff00) | ((value << 24) & 0xff000000);
}

void InitSharedMemory(uint GroupIndex)
{
	RemappedValues[GroupIndex] = 0;
	PrevIndex[GroupIndex] = 0;

	EntropyResult[GroupIndex*2] = 0;
	EntropyResult[GroupIndex*2+1] = 0;
}

void ComputeColorTransform(uint GroupIndex, uint3 DispatchThreadID)
{
	TransformedPixelData[GroupIndex] = Get_Transformed_YCbCr_Component(DispatchThreadID);

	GroupMemoryBarrierWithGroupSync();
}

void ComputeFDCT(uint GroupIndex, uint3 GroupThreadID)
{
	DCT_MatrixTmp[GroupIndex] = 0;
	[unroll] for(int k = 0; k < 8; k++)
		DCT_MatrixTmp[GroupIndex] += DCT_matrix[GroupThreadID.y*8+k] * TransformedPixelData[k*8+GroupThreadID.x];

	GroupMemoryBarrierWithGroupSync();

	DCT_Coefficients[GroupIndex] = 0;
	[unroll] for(int k = 0; k < 8; k++)
		DCT_Coefficients[GroupIndex] += DCT_MatrixTmp[GroupThreadID.y*8+k] * DCT_matrix_transpose[k*8+GroupThreadID.x];

	GroupMemoryBarrierWithGroupSync();
}

void ComputeQuantization(int GroupIndex)
{
	//divide and round to nearest integer
	QuantizedComponents[GroupIndex] =
		round(DCT_Coefficients[ZigZagIndices[GroupIndex]] /
			Quantization_Table[GroupIndex]);
}

void StreamCompactQuantizedData(int GroupIndex)
{
	//set to 0 when DC component or AC == 0
	if(GroupIndex > 0 && QuantizedComponents[GroupIndex] != 0)
		ScanArray[GroupIndex] = 1;
	else
		ScanArray[GroupIndex] = 0;

	GroupMemoryBarrierWithGroupSync();

	Sum(GroupIndex);

	if(GroupIndex > 0 && QuantizedComponents[GroupIndex] != 0)
	{
		RemappedValues[ScanArray[GroupIndex]] = QuantizedComponents[GroupIndex];
		PrevIndex[ScanArray[GroupIndex]] = GroupIndex;
	}
	
	GroupMemoryBarrierWithGroupSync();
}

BSResult BuildBitStrings(int GroupIndex)
{
	BSResult result = (BSResult)0;

	static const uint mask[] = {1,2,4,8,16,32,64,128,256,
								512,1024,2048,4096,8192,16384,32768};

	//special marker symbols
	BitString M_16Z = AC_Huffman[0xF0];
	BitString M_EOB = AC_Huffman[0x00];

	if(GroupIndex == 0 && RemappedValues[0] == 0)
	{
		result.BS[5] = M_EOB;
	}
	else if(RemappedValues[GroupIndex] != 0)
	{
		uint PrecedingZeroes = (PrevIndex[GroupIndex] -
								PrevIndex[GroupIndex-1] - 1);

		//append 16 zeroes markers
		[unroll] for(uint i = 0; i < PrecedingZeroes / 16; i++)
		{
			result.BS[i] = M_16Z;
		}

		int tmp = RemappedValues[GroupIndex];

		//get number of bits to represent number
		uint nbits = firstbithigh(abs(tmp)) + 1;
		
		//AC symbol 1
		result.BS[3] = AC_Huffman[((PrecedingZeroes % 16) << 4) + nbits];
		
		//AC symbol 2
		if(tmp < 0) tmp--;
		result.BS[4] = (nbits << 16) | (tmp & (mask[nbits] - 1));

		//insert End of Block (EOB) symbol?
		if(PrevIndex[GroupIndex] != 63 && RemappedValues[GroupIndex+1] == 0)
		{
			result.BS[5] = M_EOB;
		}
	}

	//calculate total bit count
	[unroll] for(int i = 0; i < 6; i++)
	{
		if(result.BS[i] != 0)
		{
			result.NumEntropyBits += (result.BS[i] >> 16);
		}
	}

	return result;
}

void EntropyCodeAC(int GroupIndex, BSResult result)
{
	ScanArray[GroupIndex] = result.NumEntropyBits;

	Sum(GroupIndex);

	uint bitpos = ScanArray[GroupIndex];
	[unroll] for(int i = 0; i < 6; i++)
	{
		if(result.BS[i] != 0)
		{
			Write(result.BS[i], bitpos);
			bitpos += result.BS[i] >> 16;
		}
	}

	GroupMemoryBarrierWithGroupSync();
}

void CopyToDeviceMemory(int GroupIndex, uint3 GroupID, BSResult result)
{
	uint outIndex = GetOutputIndex(GroupIndex, GroupID);

	if(GroupIndex > 0 && GroupIndex < (int)EntropyBlockSize-1)
		EntropyOut[outIndex] = ConvertEndian(EntropyResult[GroupIndex-1]);

	else if(GroupIndex == 0)
		EntropyOut[outIndex] = QuantizedComponents[0];
	else if(GroupIndex == 63)
		EntropyOut[outIndex - 64 + EntropyBlockSize] = ScanArray[GroupIndex];
}

[numthreads(8, 8, 1)]
void ComputeJPEG(
	uint3 DispatchThreadID	: SV_DispatchThreadID,
	uint3 GroupThreadID		: SV_GroupThreadID,
	uint3 GroupID			: SV_GroupID,
	uint GroupIndex			: SV_GroupIndex)
{
	InitSharedMemory(GroupIndex);

	//RGB -> YCbCr component and level shift
	ComputeColorTransform(GroupIndex, DispatchThreadID);

	//Apply Forward Discrete Cosine Transform
	ComputeFDCT(GroupIndex, GroupThreadID);

	//Quantize DCT coefficients
	ComputeQuantization(GroupIndex);

	//Move non-zero quantized values to
	//beginning of shared memory array
	//to be able to calculate preceding zeroes
	StreamCompactQuantizedData(GroupIndex);
	
	//Initiate bitstrings, calculate number of
	//bits to occupy and identify if thread represents EOB
	BSResult result = BuildBitStrings(GroupIndex);

	//Do entropy coding to shared memory
	//using atomic operation
	EntropyCodeAC(GroupIndex, result);

	//move result from shared memory to device memory
	CopyToDeviceMemory(GroupIndex, GroupID, result);
}