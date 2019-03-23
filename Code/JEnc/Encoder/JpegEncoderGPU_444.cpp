//--------------------------------------------------------------------------------------
// Real-Time JPEG Compression using DirectCompute - Demo
//
// Copyright (c) Stefan Petersson 2012. All rights reserved.
//--------------------------------------------------------------------------------------
#include "JpegEncoderGPU_444.h"

#include <tchar.h>

JpegEncoderGPU_444::JpegEncoderGPU_444(ID3D11Device* d3dDevice, ID3D11DeviceContext* d3dContext)
	: JpegEncoderGPU(d3dDevice, d3dContext)
{
	mSubsampleType = JENC_CHROMA_SUBSAMPLE_4_4_4;
}

JpegEncoderGPU_444::~JpegEncoderGPU_444()
{

}

bool JpegEncoderGPU_444::Init()
{
	D3D10_SHADER_MACRO shaderDefines_Y[] = {
		{ "C_4_4_4", "1" },
		{ "COMPONENT_Y", "1" },
		{ NULL, NULL}
	};

	mShader_Y_Component = mComputeSys->CreateComputeShader(mComputeShaderFile, "444_Y", "ComputeJPEG", shaderDefines_Y);
	if(!mShader_Y_Component)
	{
		return false;
	}

	D3D10_SHADER_MACRO shaderDefines_Cb[] = {
		{ "C_4_4_4", "1" },
		{ "COMPONENT_CB", "1" },
		{ NULL, NULL}
	};

	mShader_Cb_Component = mComputeSys->CreateComputeShader(mComputeShaderFile,	"444_Cb", "ComputeJPEG", shaderDefines_Cb);
	if(!mShader_Cb_Component)
	{
		return false;
	}

	D3D10_SHADER_MACRO shaderDefines_Cr[] = {
		{ "C_4_4_4", "1" },
		{ "COMPONENT_CR", "1" },
		{ NULL, NULL}
	};

	mShader_Cr_Component = mComputeSys->CreateComputeShader(mComputeShaderFile,	"444_Cr", "ComputeJPEG", shaderDefines_Cr);
	if(!mShader_Cr_Component)
	{
		return false;
	}

	return true;
}

void JpegEncoderGPU_444::DoEntropyEncode()
{
	short prev_DC_Y = 0;
	short prev_DC_Cb = 0;
	short prev_DC_Cr = 0;

	int Width = mComputationWidthY;
	int Height = mComputationHeightY;

	mCB_EntropyResult->CopyToStaging();
	int* pEntropyData = mCB_EntropyResult->Map<int>();

	int iterations = mComputationWidthY / 8 * mComputationHeightY / 8;
	while(iterations-- > 0)
	{
		DoHuffmanEncoding(pEntropyData, prev_DC_Y, Y_DC_Huffman_Table);
		DoHuffmanEncoding(pEntropyData + mEntropyBlockSize, prev_DC_Cb, Cb_DC_Huffman_Table);
		DoHuffmanEncoding(pEntropyData + mEntropyBlockSize*2, prev_DC_Cr, Cb_DC_Huffman_Table);

		pEntropyData += mEntropyBlockSize*3;
	}

	mCB_EntropyResult->Unmap();
}

DX12_JpegEncoderGPU_444::DX12_JpegEncoderGPU_444(D3D12Wrap* d3dWrap)
	: DX12_JpegEncoderGPU(d3dWrap)
{
	mSubsampleType = JENC_CHROMA_SUBSAMPLE_4_4_4;
}

DX12_JpegEncoderGPU_444::~DX12_JpegEncoderGPU_444()
{
}

bool DX12_JpegEncoderGPU_444::Init()
{
	D3D10_SHADER_MACRO shaderDefines_Y[] = {
		{ "C_4_4_4", "1" },
	{ "COMPONENT_Y", "1" },
	{ NULL, NULL }
	};

	mShader_Y_Component = mComputeSys->CreateComputeShader(mComputeShaderFile, "ComputeJPEG", shaderDefines_Y);
	if (!mShader_Y_Component)
	{
		return false;
	}

	D3D10_SHADER_MACRO shaderDefines_Cb[] = {
		{ "C_4_4_4", "1" },
	{ "COMPONENT_CB", "1" },
	{ NULL, NULL }
	};

	mShader_Cb_Component = mComputeSys->CreateComputeShader(mComputeShaderFile, "ComputeJPEG", shaderDefines_Cb);
	if (!mShader_Cb_Component)
	{
		return false;
	}

	D3D10_SHADER_MACRO shaderDefines_Cr[] = {
		{ "C_4_4_4", "1" },
	{ "COMPONENT_CR", "1" },
	{ NULL, NULL }
	};

	mShader_Cr_Component = mComputeSys->CreateComputeShader(mComputeShaderFile, "ComputeJPEG", shaderDefines_Cr);
	if (!mShader_Cr_Component)
	{
		return false;
	}

	// Create the PSOs
	if (FAILED(createPiplineStateObjects()))
	{
		//PostMessageBoxOnError(0, L"Failed to create PSOs: ", L"Fatal error", MB_ICONERROR, wHnd);
		exit(-1);
	}

	return true;
}

void DX12_JpegEncoderGPU_444::DoEntropyEncode()
{
	short prev_DC_Y = 0;
	short prev_DC_Cb = 0;
	short prev_DC_Cr = 0;

	int Width = mComputationWidthY;
	int Height = mComputationHeightY;

	mCB_EntropyResult->CopyToStaging();
	int* pEntropyData = mCB_EntropyResult->Map<int>();

	int iterations = mComputationWidthY / 8 * mComputationHeightY / 8;
	while (iterations-- > 0)
	{
		DoHuffmanEncoding(pEntropyData, prev_DC_Y, Y_DC_Huffman_Table);
		DoHuffmanEncoding(pEntropyData + mEntropyBlockSize, prev_DC_Cb, Cb_DC_Huffman_Table);
		DoHuffmanEncoding(pEntropyData + mEntropyBlockSize * 2, prev_DC_Cr, Cb_DC_Huffman_Table);

		pEntropyData += mEntropyBlockSize * 3;
	}

	mCB_EntropyResult->Unmap();
}
