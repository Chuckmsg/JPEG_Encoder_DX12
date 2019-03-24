//--------------------------------------------------------------------------------------
// Real-Time JPEG Compression using DirectCompute - Demo
//
// Copyright (c) Stefan Petersson 2012. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "../stdafx.h"
#include "../Shared/DX_12Helper.h"

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

//-------------------------------------------------------------------------------
// DirectX 12 Extension
// Max Tlatlik 2019
// ------------------------------------------------------------------------------
class D3D12Wrap
{
public:
	struct DX12Fence
	{
		//Synchronization objects
		UINT m_frameIndex = 0;
		HANDLE m_fenceEvent = NULL;
		ID3D12Fence* m_fence = NULL;
		UINT64 m_fenceValue = 0;
	};

	DX12Fence * GetFence(UINT index) { return &Fences[index]; }
	DX12Fence * GetTestFence() { return &TestFence; }

	HRESULT Init(HWND hwnd, int width, int height);
	void Cleanup();

	ID3D12Device * GetDevice()														{ return m_device; }
	IDXGISwapChain3 * GetSwapChain()											{ return m_swapChain; }

	ID3D12GraphicsCommandList * GetComputeCmdList()				{ return m_comCmdList; }
	ID3D12GraphicsCommandList * GetDirectCmdList()					{ return m_gCmdList; }

	ID3D12DescriptorHeap * GetRTVHeap()			{ return m_rtvHeap; }
	ID3D12DescriptorHeap * GetUAVHeap()		{ return m_uavHeap; }
	ID3D12DescriptorHeap * GetSRVHeap()			{ return m_srvHeap; }

	ID3D12Resource * GetBackBufferResource(unsigned int index)					{ return m_renderTargets[index]; }
	ID3D12Resource * GetUnorderedAccessResource(unsigned int index)		{ return m_UAVs[index]; }

	UINT GetFrameIndex() { return m_swapChain->GetCurrentBackBufferIndex(); }

	ID3D12CommandAllocator * GetDirectAllocator() { return m_directAllocator; }
	ID3D12CommandAllocator * GetComputeAllocator() { return m_computeAllocator; }

	ID3D12CommandQueue * GetDirectQueue() { return m_directQ; }
	ID3D12CommandQueue * GetComputeQueue() { return m_computeQ; }

	ID3D12RootSignature * GetRootSignature() { return m_rootSignature; }

	//DX12Fence * GetFence(UINT index) { return &m_fences[index]; }

	inline void WaitForGPUCompletion(ID3D12CommandQueue * pCmdQ, DX12Fence * fence)
	{
		const UINT64 fenceValue = fence->m_fenceValue ;
		pCmdQ->Signal(fence->m_fence, fenceValue);
		fence->m_fenceValue++;
		
		auto lol = fence->m_fence->GetCompletedValue();
		if (lol < fenceValue)
		{
			fence->m_fence->SetEventOnCompletion(fenceValue, fence->m_fenceEvent);
			WaitForSingleObject(fence->m_fenceEvent, INFINITE);
		}
	}
private:
	static const unsigned int BUFFER_COUNT	 = 2;
	//TODO: Add all relevant D3D components here	       
	//D3D12 Debug														    
	ID3D12Debug* m_debugController								= NULL;
	//D3D12										    
	ID3D12Device* m_device												= NULL;
	IDXGISwapChain3* m_swapChain									= NULL;

	ID3D12CommandQueue * m_directQ								= NULL;
	ID3D12CommandQueue * m_computeQ						= NULL;

	ID3D12CommandAllocator * m_directAllocator				= NULL;
	ID3D12CommandAllocator * m_computeAllocator		= NULL;
	//Command lists
	ID3D12GraphicsCommandList * m_gCmdList					= NULL; //For copying to backbuffer
	ID3D12GraphicsCommandList * m_comCmdList			= NULL; //For compute

	ID3D12Resource* m_renderTargets[BUFFER_COUNT]	= { NULL, NULL };
	ID3D12Resource* m_UAVs[BUFFER_COUNT]					= { NULL, NULL };

	ID3D12RootSignature* m_rootSignature						= NULL;
	//Heaps
	ID3D12DescriptorHeap* m_rtvHeap								= NULL;
	ID3D12DescriptorHeap* m_uavHeap								= NULL;
	ID3D12DescriptorHeap* m_srvHeap								= NULL;
	//Fences
	DX12Fence Fences[2]														= {};
	DX12Fence TestFence;
private:
	IDXGIAdapter1 * _findDX12Adapter(IDXGIFactory5 ** ppFactory);
	HRESULT _createDevice(IDXGIFactory5 ** ppFactory, IDXGIAdapter1 ** ppAdapter);
	HRESULT _createCommandQueues();
	HRESULT _createCmdAllocatorsAndLists();
	HRESULT _createSwapChain(HWND hwnd, unsigned int width, unsigned int height, IDXGIFactory5 ** ppFactory);
	HRESULT _createHeapsAndResources();
	HRESULT _createFences();
	HRESULT _createRootSignature();
};

