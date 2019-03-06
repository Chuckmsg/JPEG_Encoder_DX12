//--------------------------------------------------------------------------------------
// Real-Time JPEG Compression using DirectCompute - Demo
//
// Copyright (c) Stefan Petersson 2012. All rights reserved.
//--------------------------------------------------------------------------------------
#ifndef __jenc_common_h__
#define __jenc_common_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#if defined DLL_EXPORT
#define DECLDIR __declspec(dllexport)
#else
#define DECLDIR __declspec(dllimport)
#endif

#include <string>
#include <cmath>

enum JENC_TYPE
{
	CPU_ENCODER,
	GPU_ENCODER
};

enum JENC_OPTIONS
{
//	COUNT_ZEROES_ON_GPU = 1	//will only work with GPU_ENCODER type
};

enum JENC_CHROMA_SUBSAMPLE
{
	JENC_CHROMA_SUBSAMPLE_4_4_4,
	JENC_CHROMA_SUBSAMPLE_4_2_2,
	JENC_CHROMA_SUBSAMPLE_4_2_0
};

struct JEncRGBDataDesc
{
	unsigned char* Data;
	unsigned int Width;
	unsigned int Height;
	unsigned int RowPitch;
	unsigned char* TargetMemory;

	JEncRGBDataDesc()
	{
		memset(this, 0, sizeof(JEncRGBDataDesc));
	}
};

struct JEncD3DDataDesc
{
	struct ID3D11ShaderResourceView* ResourceView;
	unsigned int Width;
	unsigned int Height;
	unsigned char* TargetMemory;
	
	JEncD3DDataDesc()
	{
		memset(this, 0, sizeof(JEncD3DDataDesc));
	}
};

struct JEncResult
{
	void* Bits;
	unsigned int HeaderSize;
	unsigned int DataSize;
};

#endif