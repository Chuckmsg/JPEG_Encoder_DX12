//--------------------------------------------------------------------------------------
// Real-Time JPEG Compression using DirectCompute - Demo
//
// Copyright (c) Stefan Petersson 2012. All rights reserved.
//--------------------------------------------------------------------------------------
#include "JpegEncoderGPU_422.h"

#include <tchar.h>

JpegEncoderGPU_422::JpegEncoderGPU_422(ID3D11Device* d3dDevice, ID3D11DeviceContext* d3dContext)
	: JpegEncoderGPU(d3dDevice, d3dContext)
{
	mSubsampleType = JENC_CHROMA_SUBSAMPLE_4_2_2;
}

JpegEncoderGPU_422::~JpegEncoderGPU_422()
{

}

bool JpegEncoderGPU_422::Init()
{
	D3D10_SHADER_MACRO shaderDefines_Y[] = {
		{ "C_4_2_2", "1" },
		{ "COMPONENT_Y", "1" },
		{ NULL, NULL}
	};

	mShader_Y_Component = mComputeSys->CreateComputeShader(mComputeShaderFile, "422_Y", "ComputeJPEG", shaderDefines_Y);
	if(!mShader_Y_Component)
	{
		return false;
	}

	D3D10_SHADER_MACRO shaderDefines_Cb[] = {
		{ "C_4_2_2", "1" },
		{ "COMPONENT_CB", "1" },
		{ NULL, NULL}
	};

	mShader_Cb_Component = mComputeSys->CreateComputeShader(mComputeShaderFile, "422_Cb", "ComputeJPEG", shaderDefines_Cb);
	if(!mShader_Cb_Component)
	{
		return false;
	}

	D3D10_SHADER_MACRO shaderDefines_Cr[] = {
		{ "C_4_2_2", "1" },
		{ "COMPONENT_CR", "1" },
		{ NULL, NULL}
	};

	mShader_Cr_Component = mComputeSys->CreateComputeShader(mComputeShaderFile, "422_Cr", "ComputeJPEG", shaderDefines_Cr);
	if(!mShader_Cr_Component)
	{
		return false;
	}

	return true;
}

void JpegEncoderGPU_422::DoEntropyEncode()
{
	short prev_DC_Y = 0;
	short prev_DC_Cb = 0;
	short prev_DC_Cr = 0;

	int Width = mComputationWidthY;
	int Height = mComputationHeightY;

	mCB_EntropyResult->CopyToStaging();
	int* pEntropyData = mCB_EntropyResult->Map<int>();

	int iterations = mComputationWidthY / 16 * mComputationHeightY / 8;
	while(iterations-- > 0)
	{
		DoHuffmanEncoding(pEntropyData, prev_DC_Y, Y_DC_Huffman_Table);
		DoHuffmanEncoding(pEntropyData+mEntropyBlockSize, prev_DC_Y, Y_DC_Huffman_Table);
		DoHuffmanEncoding(pEntropyData+mEntropyBlockSize*2, prev_DC_Cb, Cb_DC_Huffman_Table);
		DoHuffmanEncoding(pEntropyData+mEntropyBlockSize*3, prev_DC_Cr, Cb_DC_Huffman_Table);

		pEntropyData += mEntropyBlockSize*4;
	}

	mCB_EntropyResult->Unmap();
}

DX12_JpegEncoderGPU_422::DX12_JpegEncoderGPU_422(ID3D12Resource* resource)
	: DX12_JpegEncoderGPU(resource)
{
	mSubsampleType = JENC_CHROMA_SUBSAMPLE_4_2_2;
}

DX12_JpegEncoderGPU_422::~DX12_JpegEncoderGPU_422()
{
}

bool DX12_JpegEncoderGPU_422::Init()
{
	D3D10_SHADER_MACRO shaderDefines_Y[] = {
		{ "C_4_2_2", "1" },
	{ "COMPONENT_Y", "1" },
	{ NULL, NULL }
	};

	mShader_Y_Component = mComputeSys->CreateComputeShader(mComputeShaderFile, "422_Y", "ComputeJPEG", shaderDefines_Y);
	if (!mShader_Y_Component)
	{
		return false;
	}

	D3D10_SHADER_MACRO shaderDefines_Cb[] = {
		{ "C_4_2_2", "1" },
	{ "COMPONENT_CB", "1" },
	{ NULL, NULL }
	};

	mShader_Cb_Component = mComputeSys->CreateComputeShader(mComputeShaderFile, "422_Cb", "ComputeJPEG", shaderDefines_Cb);
	if (!mShader_Cb_Component)
	{
		return false;
	}

	D3D10_SHADER_MACRO shaderDefines_Cr[] = {
		{ "C_4_2_2", "1" },
	{ "COMPONENT_CR", "1" },
	{ NULL, NULL }
	};

	mShader_Cr_Component = mComputeSys->CreateComputeShader(mComputeShaderFile, "422_Cr", "ComputeJPEG", shaderDefines_Cr);
	if (!mShader_Cr_Component)
	{
		return false;
	}

	return true;
}

void DX12_JpegEncoderGPU_422::DoEntropyEncode()
{
	short prev_DC_Y = 0;
	short prev_DC_Cb = 0;
	short prev_DC_Cr = 0;

	int Width = mComputationWidthY;
	int Height = mComputationHeightY;

	mCB_EntropyResult->CopyToStaging();
	int* pEntropyData = mCB_EntropyResult->Map<int>();

	int iterations = mComputationWidthY / 16 * mComputationHeightY / 8;
	while (iterations-- > 0)
	{
		DoHuffmanEncoding(pEntropyData, prev_DC_Y, Y_DC_Huffman_Table);
		DoHuffmanEncoding(pEntropyData + mEntropyBlockSize, prev_DC_Y, Y_DC_Huffman_Table);
		DoHuffmanEncoding(pEntropyData + mEntropyBlockSize * 2, prev_DC_Cb, Cb_DC_Huffman_Table);
		DoHuffmanEncoding(pEntropyData + mEntropyBlockSize * 3, prev_DC_Cr, Cb_DC_Huffman_Table);

		pEntropyData += mEntropyBlockSize * 4;
	}

	mCB_EntropyResult->Unmap();
}
