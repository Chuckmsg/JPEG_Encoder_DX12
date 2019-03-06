//--------------------------------------------------------------------------------------
// Real-Time JPEG Compression using DirectCompute - Demo
//
// Copyright (c) Stefan Petersson 2012. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "SurfacePreparation.h"
#include "../D3DWrap/D3DWrap.h"

enum CHROMA_SUBSAMPLE
{
	CHROMA_SUBSAMPLE_4_4_4,
	CHROMA_SUBSAMPLE_4_2_2,
	CHROMA_SUBSAMPLE_4_2_0
};

struct EncodeResult
{
	void* Bits;
	unsigned int HeaderSize;
	unsigned int DataSize;

	unsigned int ImageWidth;
	unsigned int ImageHeight;

	double	GPUCopyToCPU;
	double	GPUTime;
	double	CPUTime;
};

class Encoder
{
protected:
	BYTE* destinationBuffer;
	UINT destinationBufferSize;

	int imageWidth;
	int imageHeight;

protected:
	SurfacePreparation*	surfacePreparation;
public:
	Encoder(SurfacePreparation* surfacePrep)
	{
		destinationBuffer = NULL;
		destinationBufferSize = 0;
		imageWidth = 0;
		imageHeight = 0;

		surfacePreparation = surfacePrep;
	}
	virtual ~Encoder() { SAFE_DELETE_ARRAY(destinationBuffer); };

	void VerifyDestinationBuffer(int w, int h)
	{
		if(imageWidth < w || imageHeight < h)
		{
			imageWidth = w;
			imageHeight = h;
			destinationBufferSize = imageWidth * imageHeight * 4;

			SAFE_DELETE_ARRAY(destinationBuffer);
			destinationBuffer = myNew BYTE[destinationBufferSize]; //will be big enough :-)
		}
	}

	virtual EncodeResult Encode(	ID3D11Texture2D* textureResource,
									CHROMA_SUBSAMPLE subsampleType,
									float outputScale,
									int jpegQuality) = 0;

	virtual HRESULT Init(D3DWrap* d3d) = 0;

	virtual char* Name() = 0;
};