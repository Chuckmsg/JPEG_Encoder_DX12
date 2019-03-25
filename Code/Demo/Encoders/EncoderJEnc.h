//--------------------------------------------------------------------------------------
// Real-Time JPEG Compression using DirectCompute - Demo
//
// Copyright (c) Stefan Petersson 2012. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "Encoder.h"

#include "../../JEnc/Include/JEnc.h"

class EncoderJEnc : public Encoder
{
	JEnc*		jEncoder444;
	JEnc*		jEncoder422;
	JEnc*		jEncoder420;

public:
	EncoderJEnc(SurfacePreparation* surfacePrep);
	virtual ~EncoderJEnc();

	virtual EncodeResult Encode(	ID3D11Texture2D* textureResource,
									CHROMA_SUBSAMPLE subsampleType,
									float outputScale,
									int jpegQuality);

	virtual HRESULT Init(D3D11Wrap* d3d);

	virtual char* Name() { return "JEnc"; }
};

/*
	DX12 class
*/

class DX12_EncoderJEnc : public DX12_Encoder
{
	JEnc*		jEncoder444;
	JEnc*		jEncoder422;
	JEnc*		jEncoder420;

public:
	DX12_EncoderJEnc(SurfacePreperationDX12* surfacePrep);
	virtual ~DX12_EncoderJEnc();

	virtual EncodeResult DX12_Encode(ID3D12Resource* textureResource, unsigned char* Data,
		CHROMA_SUBSAMPLE subsampleType,
		float outputScale,
		int jpegQuality);

	virtual HRESULT Init(D3D12Wrap* d3d);

	virtual char* Name() { return "JEnc"; }

private:
	D3D12Wrap * mD3D12Wrap;
	ID3D12Resource * copyTexture = nullptr;
	ID3D12Resource * currentTextureResourcePtr = nullptr;
	ID3D12DescriptorHeap* descHeap = nullptr;

private:

	HRESULT CreateTextureResource(D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle, ID3D12Resource * textureResource);
	HRESULT CopyTexture(ID3D12Resource * textureResource);
};