//--------------------------------------------------------------------------------------
// Real-Time JPEG Compression using DirectCompute - Demo
//
// Copyright (c) Stefan Petersson 2012. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "../stdafx.h"
#include "../D3DWrap/D3DWrap.h"

struct PreparedSurface
{
	ID3D11ShaderResourceView*	SRV;
	int							Width;
	int							Height;
};

struct DX12_PreparedSurface
{
	ID3D12DescriptorHeap*		Heap;
	int							Width;
	int							Height;
};

struct PreparedSurfaceBits
{
	void*	Bits;
	int		Width;
	int		Height;
	int		RowPitch;
};

class SurfacePreparation
{
	int						mTextureWidth;
	int						mTextureHeight;
	int						mComputationWidth;
	int						mComputationHeight;

	int						mRescaleWidth;
	int						mRescaleHeight;

	ID3D11Device*			mDevice;
	ID3D11DeviceContext*	mDeviceContext;

	ID3D11VertexShader*		mVertexShader;
	ID3D11PixelShader*		mPixelShader;


	HRESULT CreateVertexShader();
	HRESULT CreatePixelShader();

	D3D11_TEXTURE2D_DESC		mRescaleDescription;
	ID3D11RenderTargetView*		mRescaleRTV;
	ID3D11ShaderResourceView*	mRescaleSRV;
	ID3D11Texture2D*			mRescaleTextureGPU;
	ID3D11Texture2D*			mRescaleTextureCPU;

	D3D11_TEXTURE2D_DESC		mCopyDescription;
	ID3D11ShaderResourceView*	mCopySRV;
	ID3D11Texture2D*			mCopyTextureGPU;
	ID3D11Texture2D*			mCopyTextureCPU;


	ID3D11Texture2D*			mMappedResource;

	HRESULT InitCopyTexture(int width, int height);
	HRESULT InitRescaleTexture();
	HRESULT InitSRV(ID3D11Texture2D* shaderResource, DXGI_FORMAT format, ID3D11ShaderResourceView** outSRV);

	void RenderCopyToRescaleTarget();
public:

	SurfacePreparation();
	~SurfacePreparation();

	HRESULT Init(ID3D11Device* device, ID3D11DeviceContext* deviceContext);
	void Cleanup();

	PreparedSurface GetValidSurface(ID3D11Texture2D* texture, float outputScale);
	PreparedSurfaceBits GetValidSurfaceBits(ID3D11Texture2D* texture, float outputScale);
	void Unmap();
};


/*
	Christoffer Åleskog
	DirectX 12 version of class SurfacePreperation
*/
class SurfacePreperationDX12
{
public:
	SurfacePreperationDX12();
	~SurfacePreperationDX12();

	HRESULT Init(D3D12Wrap * pD3D12Wrap);

	ID3D12PipelineState * GetPSO() { return m_PSO; }

	void Cleanup();

	DX12_PreparedSurface GetValidSurface(ID3D12Device * device, ID3D12Resource* texture, float outputScale);

private:
	ID3D12Device * m_device			= NULL;
	ID3D12RootSignature * m_rootSignature = NULL;
	ID3D12PipelineState * m_PSO		= NULL;

	ID3D10Blob * m_vertexShaderCode = NULL;
	ID3D10Blob * m_pixelShaderCode = NULL;

	ID3D12DescriptorHeap* descHeap1 = nullptr;
	ID3D12DescriptorHeap* descHeap2 = nullptr;
	ID3D12Resource * shaderResource1 = nullptr;
	ID3D12Resource * shaderResource2 = nullptr;

	int mRescaleWidth;
	int mRescaleHeight;

	int mRescaleWidth1;
	int mRescaleHeight1;
	int mRescaleWidth2;
	int mRescaleHeight2;

	ID3D12GraphicsCommandList*  m_commandList = nullptr;
	ID3D12CommandAllocator*		m_cmdAllocator = nullptr;
	ID3D12CommandQueue*			m_commandQueue = nullptr;
	ID3D12DescriptorHeap* rtvDescHeap1 = nullptr;
	ID3D12DescriptorHeap* rtvDescHeap2 = nullptr;
	ID3D12DescriptorHeap* srvDescHeap1 = nullptr;
	ID3D12DescriptorHeap* srvDescHeap2 = nullptr;

	ID3D12DescriptorHeap* m_depthHeap = nullptr;
	ID3D12Resource* m_depthBuffer = nullptr;
	ID3D12Resource* copyTexture1 = nullptr;
	ID3D12Resource* rtvTexture1 = nullptr;
	ID3D12Resource* copyTexture2 = nullptr;
	ID3D12Resource* rtvTexture2 = nullptr;

	struct DX12Fence
	{
		UINT m_frameIndex = 0;
		HANDLE m_fenceEvent = NULL;
		ID3D12Fence* m_fence = NULL;
		UINT64 m_fenceValue = 0;
	};

	DX12Fence m_fence;

private:
	HRESULT _createListAllocQueue();
	HRESULT _compileVertexShader();
	HRESULT _compilePixelShader();
	HRESULT _createPSO();
	HRESULT _createRootSignature();
	HRESULT _createFence();

	HRESULT createSRV(ID3D12Device * device, ID3D12Resource * shaderResource, ID3D12DescriptorHeap*& outDescriptorHeap);
	HRESULT InitRescaleTexture(ID3D12Resource*& copyTexture, ID3D12Resource*& rtvTexture, DXGI_FORMAT format, ID3D12DescriptorHeap*& outDescriptorHeap, ID3D12DescriptorHeap*& rtvDescriptorHeap);
	void render(ID3D12Resource*& rtvTexture, ID3D12DescriptorHeap*& rtvDescriptorHeap, ID3D12DescriptorHeap*& textureDescriptorHeap);
	HRESULT _createDepthBuffer(float width, float height, ID3D12Resource *& shaderResource, ID3D12DescriptorHeap*& outDescriptorHeap);
	void copyTexture(ID3D12Resource*& destTexture, ID3D12Resource*& TextureToCopy);

	inline void WaitForGPUCompletion(ID3D12CommandQueue * pCmdQ, DX12Fence * fence)
	{
		const UINT64 fenceValue = fence->m_fenceValue;
		pCmdQ->Signal(fence->m_fence, fenceValue);
		fence->m_fenceValue++;

		auto lol = fence->m_fence->GetCompletedValue();
		if (lol < fenceValue)
		{
			fence->m_fence->SetEventOnCompletion(fenceValue, fence->m_fenceEvent);
			WaitForSingleObject(fence->m_fenceEvent, INFINITE);
		}
	}
};