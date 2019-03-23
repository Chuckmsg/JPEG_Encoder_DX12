//--------------------------------------------------------------------------------------
// Real-Time JPEG Compression using DirectCompute - Demo
//
// Copyright (c) Stefan Petersson 2012. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "JpegEncoderGPU.h"

#include "../../Shared/ComputeShader.h"
#include "../../Demo/D3DWrap/D3DWrap.h"

class JpegEncoderGPU_420 : public JpegEncoderGPU
{
public:
	JpegEncoderGPU_420(ID3D11Device* d3dDevice,	ID3D11DeviceContext* d3dContext);
	virtual ~JpegEncoderGPU_420();

	virtual bool Init();

private:
	virtual void DoEntropyEncode();
};

/*
	JpegEncoderGPU_420 for directx 12
	Christoffer Åleskog 2019
*/
class DX12_JpegEncoderGPU_420 : public DX12_JpegEncoderGPU
{
public:
	DX12_JpegEncoderGPU_420(D3D12Wrap* d3dWrap);
	virtual ~DX12_JpegEncoderGPU_420();

	virtual bool Init();

private:
	virtual void DoEntropyEncode();
};