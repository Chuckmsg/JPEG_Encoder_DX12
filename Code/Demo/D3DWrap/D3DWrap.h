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

//-------------------------------------------------------------------------------
// DirectX 12 Extension
// Max Tlatlik 2019
// ------------------------------------------------------------------------------
class D3D12Wrap
{
public:
	HRESULT Init(HWND hwnd, int width, int height);
	void Cleanup();
	void Clear(float r, float g, float b, float a);
	HRESULT Present();

	ID3D12Device * GetDevice()											{ return m_device; }
	ID3D12CommandList * GetComputeCmdList()				{ return m_comCommandList; }
	ID3D12CommandList * GetCopyCmdList()						{ return m_cpyCommandList; }
	ID3D12GraphicsCommandList * getGraphicsCmdList()	{ return m_gCommandList; }
private:
	//TODO: Add all relevant D3D components here	       
	//D3D12 Debug														    
	ID3D12Debug* m_debugController								= NULL;
	//D3D12/Pipeline objects										    
	ID3D12Device* m_device												= NULL;
	ID3D12CommandQueue* m_commandQueue				= NULL;
	ID3D12CommandAllocator * m_commandAllocator		= NULL;
	IDXGISwapChain3* m_swapChain									= NULL;
	ID3D12Resource* m_renderTarget									= NULL;
	D3D12_VIEWPORT m_viewPort										= {};
	D3D12_RECT m_scissorRect											= {};
	ID3D12RootSignature* m_rootSignature						= NULL;
	UINT m_rtvDescSize														= 0;
	//Command lists
	ID3D12GraphicsCommandList* m_gCommandList			= NULL; //For rendering
	ID3D12CommandList * m_comCommandList					= NULL; //For compute
	ID3D12CommandList * m_cpyCommandList					= NULL; //For copy
	//Heaps
	ID3D12DescriptorHeap* m_rtvDescHeap						= NULL;
	ID3D12DescriptorHeap* m_resourceHeap						= NULL;
	//Synchronization objects
	UINT m_frameIndex															= 0;
	HANDLE m_fenceEvent													= NULL;
	ID3D12Fence* m_fence													= NULL;
	UINT64 m_fenceValue														= 0;

private:
	IDXGIAdapter1 * _findDX12Adapter(IDXGIFactory5 ** ppFactory);
	HRESULT _createDevice(IDXGIFactory5 ** ppFactory, IDXGIAdapter1 ** ppAdapter);
	HRESULT _createCommandQueue();
	HRESULT _createCmdAllocatorAndList();
	HRESULT _createSwapChain(HWND hwnd, unsigned int width, unsigned int height, IDXGIFactory5 ** ppFactory);
	HRESULT _createRenderTargets();
	HRESULT _createResourceDescHeap();
	HRESULT _createFence();
	HRESULT _createRootSignature();
};