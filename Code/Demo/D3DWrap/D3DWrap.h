//--------------------------------------------------------------------------------------
// Real-Time JPEG Compression using DirectCompute - Demo
//
// Copyright (c) Stefan Petersson 2012. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "../stdafx.h"

class D3D11Wrap
{
	IDXGISwapChain*				mSwapChain;
	ID3D11Texture2D*			mBackBuffer;
	ID3D11RenderTargetView*		mRenderTargetView;
	ID3D11UnorderedAccessView*  mBackbufferUAV; 
	ID3D11Device*				mDevice;	
	ID3D11DeviceContext*		mDeviceContext;

public:
	D3D11Wrap();
	~D3D11Wrap();

	HRESULT Init(HWND hwnd, int width, int height);
	void Cleanup();

	void Clear(float r, float g, float b, float a);
	HRESULT Present();

	ID3D11Texture2D*		GetBackBuffer();
	ID3D11Device*			GetDevice();
	ID3D11DeviceContext*	GetDeviceContext();

	ID3D11UnorderedAccessView* GetBackBufferUAV();

	void ResetTargets();
};

class D3D12Wrap
{

};