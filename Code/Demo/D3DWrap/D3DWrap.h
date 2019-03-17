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
public:
	struct DX12Fence
	{
		// Synchronization objects
		UINT m_frameIndex = 0;
		HANDLE m_fenceEvent = NULL;
		ID3D12Fence * m_fence = NULL;
		UINT64 m_fenceValue = 0;
	};

	DX12Fence * GetFence(UINT index) { return &m_fences[index]; }

	inline void WaitForGPUCompletion(ID3D12CommandQueue * pCmdQ, DX12Fence * fence)
	{
		const UINT64 fenceValue = fence->m_fenceValue;
		pCmdQ->Signal(fence->m_fence, fenceValue);
		fence->m_fenceValue++;

		if (fence->m_fence->GetCompletedValue() < fenceValue)
		{
			fence->m_fence->SetEventOnCompletion(fenceValue, fence->m_fenceEvent);
			WaitForSingleObject(fence->m_fenceEvent, INFINITE);
		}
	}

private:
	DX12Fence m_fences[2];
};