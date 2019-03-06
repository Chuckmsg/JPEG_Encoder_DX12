//--------------------------------------------------------------------------------------
// Real-Time JPEG Compression using DirectCompute - Demo
//
// Copyright (c) Stefan Petersson 2012. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "JpegEncoderGPU.h"

#include "../../Shared/ComputeShader.h"

class JpegEncoderGPU_422 : public JpegEncoderGPU
{
public:
	JpegEncoderGPU_422(ID3D11Device* d3dDevice,	ID3D11DeviceContext* d3dContext);
	virtual ~JpegEncoderGPU_422();

	virtual bool Init();

private:
	virtual void DoEntropyEncode();
};