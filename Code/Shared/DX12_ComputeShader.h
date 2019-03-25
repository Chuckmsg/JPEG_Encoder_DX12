//--------------------------------------------------------------------------------------
// DirectX 12 Extension
// Fredrik Olsson 2019
//--------------------------------------------------------------------------------------
#pragma once

#include <d3dcommon.h>
#include <d3d12.h>
#include <d3dcompiler.h>

#include "..\Demo\D3DWrap\D3DWrap.h"
#include "DX_12Helper.h"

#include <tchar.h>

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=NULL; } }
#endif

#ifndef SAFE_DELETE
#define SAFE_DELETE(p)      { if (p) { delete (p); (p)=NULL; } }
#endif

enum DX12_COMPUTE_BUFFER_TYPE
{
	DX12_STRUCTURED_BUFFER,
	DX12_RAW_BUFFER
};

class DX12_ComputeBuffer
{
public:
	ID3D12Resource*				GetResource()
	{
		return m_resource;
	}
	ID3D12Resource*				GetStaging()
	{
		return m_staging;
	}
	ID3D12Resource*			GetResourceView()
	{
		return m_SRV;
	}
	ID3D12Resource*			GetUnorderedAccessView()
	{
		return m_UAV;
	}
	void CopyToStaging()
	{
		m_commandList->CopyResource(m_staging, m_resource);
	}

	template<class T>
	T* Map()
	{
		T* p = NULL;

		// Nullptr range because it might be read (?) Confirm this
		m_staging->Map(0, nullptr, reinterpret_cast<void**>(&p));

		return p;
	}

	void Unmap()
	{
		m_staging->Unmap(0, nullptr);
	}

	explicit DX12_ComputeBuffer()
	{
		m_resource = NULL;
		m_staging = NULL;
		m_commandList = NULL;
		m_descHeap = NULL;
		m_SRV = NULL;
		m_UAV = NULL;
	}

	~DX12_ComputeBuffer()
	{
		Release();
	}
	void Release()
	{
		SAFE_RELEASE(m_resource);
		SAFE_RELEASE(m_staging);
		SAFE_RELEASE(m_commandList);
		SAFE_RELEASE(m_descHeap);
	}

	ID3D12DescriptorHeap* GetHeap()
	{
		return m_descHeap;
	}

private:
	DX12_ComputeBuffer(const DX12_ComputeBuffer & cb) {}

	ID3D12Resource*				m_resource;
	ID3D12Resource*				m_staging;

	ID3D12Resource*				m_SRV;
	ID3D12Resource*				m_UAV;

	ID3D12GraphicsCommandList*	m_commandList;
	ID3D12DescriptorHeap*		m_descHeap;

	friend class DX12_ComputeWrap;
};

class DX12_ComputeTexture
{
public:
	ID3D12Resource*			GetResource()
	{
		return m_resource;
	}
	ID3D12Resource*			GetStaging()
	{
		return m_staging;
	}
	ID3D12Resource*			GetResourceView()
	{
		return m_SRV;
	}
	ID3D12Resource*			GetUnorderedAccessView()
	{
		return m_UAV;
	}

	void CopyToStaging()
	{
		m_commandList->CopyResource(m_staging, m_resource);
	}

	template<class T>
	T* Map()
	{
		T* p = NULL;

		// Nullptr range because it might be read (?) Confirm this
		_Staging->Map(0, nullptr, reinterpret_cast<void**>(&p));

		return p;
	}

	void Unmap()
	{
		m_staging->Unmap(0, nullptr);
	}

	explicit DX12_ComputeTexture()
	{
		m_resource = NULL;
		m_staging = NULL;
		m_commandList = NULL;
		m_descHeap = NULL;
		m_SRV = NULL;
		m_UAV = NULL;
	}

	~DX12_ComputeTexture()
	{
		Release();
	}
	void Release()
	{
		SAFE_RELEASE(m_resource);
		SAFE_RELEASE(m_staging);
		SAFE_RELEASE(m_commandList);
		SAFE_RELEASE(m_descHeap);
	}

	ID3D12DescriptorHeap* GetHeap()
	{
		return m_descHeap;
	}

private:
	DX12_ComputeTexture(const DX12_ComputeBuffer & cb) {}

	ID3D12Resource*				m_resource;
	ID3D12Resource*				m_staging;

	ID3D12Resource*				m_SRV;
	ID3D12Resource*				m_UAV;


	ID3D12GraphicsCommandList*	m_commandList;
	ID3D12DescriptorHeap*		m_descHeap;

	friend class DX12_ComputeWrap;
};

class DX12_ComputeShader
{
	friend class DX12_ComputeWrap;

	ID3D12Device*				m_device;
	// Needed (?)
	ID3D12GraphicsCommandList*	m_commandList;
	ID3DBlob*					m_computeShader;

private:
	explicit DX12_ComputeShader();

	bool Init(TCHAR* shaderFile, char* pFunctionName, D3D_SHADER_MACRO* pDefines,
		ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList);

public:
	~DX12_ComputeShader();

	ID3DBlob* GetShaderCode() { return m_computeShader; }

	// Not required (?)
	//void Set();
	//void Unset();
};

class DX12_ComputeWrap
{
	ID3D12Device*               m_device = NULL;
	ID3D12GraphicsCommandList*  m_commandList = NULL;
	ID3D12DescriptorHeap*		m_descriptorHeap = NULL;
	ID3D12CommandAllocator*		m_cmdAllocator = NULL;
	ID3D12CommandQueue*			m_computeQueue = NULL;
	//ID3D12RootSignature*		m_rootSignature = NULL;

	D3D12Wrap *					m_D3D12Wrap = NULL;

public:
	DX12_ComputeWrap(
		ID3D12Device* pDevice,
		ID3D12GraphicsCommandList* pCommandList,
		ID3D12CommandAllocator* pCmdAllocator,
		ID3D12CommandQueue* pCmdQueue,
		D3D12Wrap* pWrap)
	{
		m_device = pDevice;
		m_commandList = pCommandList;
		m_cmdAllocator = pCmdAllocator;
		m_computeQueue = pCmdQueue;
		m_D3D12Wrap = pWrap;
	}

	DX12_ComputeShader* CreateComputeShader(TCHAR* shaderFile, char* pFunctionName, D3D_SHADER_MACRO* pDefines);

	ID3D12Resource* CreateConstantBuffer(ID3D12DescriptorHeap*& descriptorHeap, UINT uSize, VOID* pInitData, char* debugName = NULL);

	DX12_ComputeBuffer* CreateBuffer(D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle, DX12_COMPUTE_BUFFER_TYPE uType, UINT uElementSize,
		UINT uCount, bool bSRV, bool bUAV, VOID* pInitData, bool bCreateStaging = false, char* debugName = NULL);

	DX12_ComputeTexture* CreateTexture(D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle, DXGI_FORMAT dxFormat, UINT uWidth,
		UINT uHeight, UINT uRowPitch, VOID* pInitData, bool bCreateStaging = false, char* debugName = NULL);

	DX12_ComputeTexture* CreateTextureFromBitmap(D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle, TCHAR* textureFilename, char* debugName = NULL);

private:

	ID3D12Resource* CreateStructuredBuffer(DX12_ComputeBuffer* buffer, D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle, UINT uElementSize, UINT uCount, bool bSRV, bool bUAV, VOID* pInitData);
	ID3D12Resource* CreateRawBuffer(UINT uSize, VOID* pInitData);
	void CreateBufferSRV(DX12_ComputeBuffer* pBuffer, D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle, UINT uElementSize, UINT uCount);
	void CreateBufferUAV(/*ID3D12Resource* */DX12_ComputeBuffer* pBuffer, UINT uElementSize, UINT uCount);
	ID3D12Resource* CreateStagingBuffer(UINT uSize);

	//texture functions
	ID3D12Resource* CreateTextureResource(D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle, DX12_ComputeTexture* texture, DXGI_FORMAT dxFormat,
		UINT uWidth, UINT uHeight, UINT uRowPitch, VOID* pInitData);
	void CreateTextureSRV(D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle, DX12_ComputeTexture* pTexture);
	void CreateTextureUAV(DX12_ComputeTexture* pTexture);
	void CreateStagingTexture(ID3D12Resource* pTexture);

	unsigned char* LoadBitmapFileRGBA(TCHAR *filename, BITMAPINFOHEADER *bitmapInfoHeader);

	//void SetDebugName(ID3D12DeviceChild* object, char* debugName);
};