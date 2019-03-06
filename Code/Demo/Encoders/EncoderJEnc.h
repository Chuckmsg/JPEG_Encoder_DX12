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