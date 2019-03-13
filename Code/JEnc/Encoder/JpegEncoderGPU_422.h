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

/*
	JpegEncoderGPU_420 for directx 12
	Christoffer Åleskog 2019
*/
class DX12_JpegEncoderGPU_422 : public DX12_JpegEncoderGPU
{
public:
	DX12_JpegEncoderGPU_422(ID3D12Resource* resource);
	virtual ~DX12_JpegEncoderGPU_422();

	virtual bool Init();

private:
	virtual void DoEntropyEncode();
};