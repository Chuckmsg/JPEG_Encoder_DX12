//--------------------------------------------------------------------------------------
// Real-Time JPEG Compression using DirectCompute - Demo
//
// Copyright (c) Stefan Petersson 2012. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "JpegEncoderGPU.h"

#include "../../Shared/ComputeShader.h"

class JpegEncoderGPU_420 : public JpegEncoderGPU
{
public:
	JpegEncoderGPU_420(ID3D11Device* d3dDevice,	ID3D11DeviceContext* d3dContext);
	// Using dx12 instead of dx11
	JpegEncoderGPU_420(ID3D12Device* d3dDevice, ID3D12DeviceContext* d3dContext);
	virtual ~JpegEncoderGPU_420();

	virtual bool Init();

private:
	virtual void DoEntropyEncode();
};