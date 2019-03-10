//--------------------------------------------------------------------------------------
// Real-Time JPEG Compression using DirectCompute - Demo
//
// Copyright (c) Stefan Petersson 2012. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include <d3dcommon.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#include <d3d12.h>

#include <tchar.h>

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=NULL; } }
#endif

#ifndef SAFE_DELETE
#define SAFE_DELETE(p)      { if (p) { delete (p); (p)=NULL; } }
#endif

enum COMPUTE_BUFFER_TYPE
{
	STRUCTURED_BUFFER,
	RAW_BUFFER
};

class ComputeBuffer
{
public:
	ID3D11Buffer*				GetResource()
	{ return _Resource; }
	ID3D11ShaderResourceView*	GetResourceView()
	{ return _ResourceView; }
	ID3D11UnorderedAccessView*	GetUnorderedAccessView()
	{ return _UnorderedAccessView; }
	ID3D11Buffer*				GetStaging()
	{ return _Staging; }
	void CopyToStaging()
	{ _D3DContext->CopyResource(_Staging, _Resource); }

	template<class T>
	T* Map()
	{
		D3D11_MAPPED_SUBRESOURCE MappedResource; 
		T* p = NULL;
		if(SUCCEEDED(_D3DContext->Map( _Staging, 0, D3D11_MAP_READ, 0, &MappedResource )))
			p = (T*)MappedResource.pData;

		return p;
	}

	void Unmap()
	{ 	_D3DContext->Unmap( _Staging, 0 ); }

	explicit ComputeBuffer()
	{
		_Resource = NULL;
		_ResourceView = NULL;
		_UnorderedAccessView = NULL;
		_Staging = NULL;	
	}

	~ComputeBuffer()
	{ Release(); }
	void Release()
	{
		SAFE_RELEASE(_Resource);
		SAFE_RELEASE(_ResourceView);
		SAFE_RELEASE(_UnorderedAccessView);
		SAFE_RELEASE(_Staging);
	}
private:
	ComputeBuffer(const ComputeBuffer& cb) {}

	ID3D11Buffer*				_Resource;
	ID3D11ShaderResourceView*	_ResourceView;
	ID3D11UnorderedAccessView*	_UnorderedAccessView;
	ID3D11Buffer*				_Staging;

	ID3D11DeviceContext*        _D3DContext;

	friend class ComputeWrap;
};

class ComputeTexture
{
public:
	ID3D11Texture2D*			GetResource()
	{ return _Resource; }
	ID3D11ShaderResourceView*	GetResourceView()
	{ return _ResourceView; }
	ID3D11UnorderedAccessView*	GetUnorderedAccessView()
	{ return _UnorderedAccessView; }
	ID3D11Texture2D*			GetStaging()
	{ return _Staging; }
	void CopyToStaging()
	{ _D3DContext->CopyResource(_Staging, _Resource); }

	template<class T>
	T* Map()
	{
		D3D11_MAPPED_SUBRESOURCE MappedResource; 
		T* p = NULL;
		if(SUCCEEDED(_D3DContext->Map( _Staging, 0, D3D11_MAP_READ, 0, &MappedResource )))
			p = (T*)MappedResource.pData;

		return p;
	}

	void Unmap()
	{ 	_D3DContext->Unmap( _Staging, 0 ); }

	explicit ComputeTexture()
	{
		_Resource = NULL;
		_ResourceView = NULL;
		_UnorderedAccessView = NULL;
		_Staging = NULL;	
	}

	~ComputeTexture()
	{ Release(); }
	void Release()
	{
		SAFE_RELEASE(_Resource);
		SAFE_RELEASE(_ResourceView);
		SAFE_RELEASE(_UnorderedAccessView);
		SAFE_RELEASE(_Staging);
	}
private:
	ComputeTexture(const ComputeBuffer& cb) {}

	ID3D11Texture2D*			_Resource;
	ID3D11ShaderResourceView*	_ResourceView;
	ID3D11UnorderedAccessView*	_UnorderedAccessView;
	ID3D11Texture2D*			_Staging;

	ID3D11DeviceContext*        _D3DContext;

	friend class ComputeWrap;
};

class ComputeShader
{
	friend class ComputeWrap;

	ID3D11Device*               mD3DDevice;
	ID3D11DeviceContext*        mD3DDeviceContext;

	ID3D11ComputeShader*		mShader;
private:
	explicit ComputeShader();

	bool Init(TCHAR* shaderFile, char* blobFileAppendix, char* pFunctionName, D3D10_SHADER_MACRO* pDefines,
		ID3D11Device* d3dDevice, ID3D11DeviceContext*d3dContext);

public:
	~ComputeShader();

	void Set();
	void Unset();
};

class ComputeWrap
{
	ID3D11Device*               mD3DDevice;
	ID3D11DeviceContext*        mD3DDeviceContext;

public:
	ComputeWrap(ID3D11Device* d3dDevice, ID3D11DeviceContext* d3dContext)
	{
		mD3DDevice = d3dDevice;
		mD3DDeviceContext = d3dContext;
	}

	ComputeShader* CreateComputeShader(TCHAR* shaderFile, char* blobFileAppendix, char* pFunctionName, D3D10_SHADER_MACRO* pDefines);

	ID3D11Buffer* CreateConstantBuffer(UINT uSize, VOID* pInitData, char* debugName = NULL);

	ComputeBuffer* CreateBuffer(COMPUTE_BUFFER_TYPE uType, UINT uElementSize,
		UINT uCount, bool bSRV, bool bUAV, VOID* pInitData, bool bCreateStaging = false, char* debugName = NULL);

	ComputeTexture* CreateTexture(DXGI_FORMAT dxFormat,	UINT uWidth,
		UINT uHeight, UINT uRowPitch, VOID* pInitData, bool bCreateStaging = false, char* debugName = NULL);

	ComputeTexture* CreateTextureFromBitmap(TCHAR* textureFilename, char* debugName = NULL);

private:
	ID3D11Buffer* CreateStructuredBuffer(UINT uElementSize, UINT uCount, bool bSRV, bool bUAV, VOID* pInitData);
	ID3D11Buffer* CreateRawBuffer(UINT uSize, VOID* pInitData);
	ID3D11ShaderResourceView* CreateBufferSRV(ID3D11Buffer* pBuffer);
	ID3D11UnorderedAccessView* CreateBufferUAV(ID3D11Buffer* pBuffer);
	ID3D11Buffer* CreateStagingBuffer(UINT uSize);

	//texture functions
	ID3D11Texture2D* CreateTextureResource(DXGI_FORMAT dxFormat,
		UINT uWidth, UINT uHeight, UINT uRowPitch, VOID* pInitData);
	//ID3D11Buffer* CreateRawBuffer(UINT uSize, VOID* pInitData);
	ID3D11ShaderResourceView* CreateTextureSRV(ID3D11Texture2D* pTexture);
	ID3D11UnorderedAccessView* CreateTextureUAV(ID3D11Texture2D* pTexture);
	ID3D11Texture2D* CreateStagingTexture(ID3D11Texture2D* pTexture);
	
	unsigned char* LoadBitmapFileRGBA(TCHAR *filename, BITMAPINFOHEADER *bitmapInfoHeader);

	void SetDebugName(ID3D11DeviceChild* object, char* debugName);
};

//--------------------------------------------------------------------------------------
// DirectX 12 Extension
// Fredrik Olsson 2019
//--------------------------------------------------------------------------------------

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
	/*ID3D11ShaderResourceView*	GetResourceView()
	{
		return _ResourceView;
	}
	ID3D11UnorderedAccessView*	GetUnorderedAccessView()
	{
		return _UnorderedAccessView;
	}*/
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

	explicit DX12_ComputeBuffer()
	{
		m_resource = NULL;
		m_staging = NULL;
		m_commandList = NULL;
	}

	~DX12_ComputeBuffer()
	{
		Release();
	}
	void Release()
	{
		SAFE_RELEASE(m_resource);
		SAFE_RELEASE(m_staging);
		// Maybe not necessary
		SAFE_RELEASE(m_commandList);
	}

private:
	DX12_ComputeBuffer(const DX12_ComputeBuffer & cb) {}

	ID3D12Resource*				m_resource;
	ID3D12Resource*				m_staging;
	ID3D12GraphicsCommandList*	m_commandList;
	//ID3D12DescriptorHeap*		m_descHeap;
	//ID3D11ShaderResourceView*	_ResourceView;
	//ID3D11UnorderedAccessView*	_UnorderedAccessView;

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
	/*ID3D11ShaderResourceView*	GetResourceView()
	{
		return _ResourceView;
	}
	ID3D11UnorderedAccessView*	GetUnorderedAccessView()
	{
		return _UnorderedAccessView;
	}*/
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
	}

	~DX12_ComputeTexture()
	{
		Release();
	}
	void Release()
	{
		SAFE_RELEASE(m_resource);
		SAFE_RELEASE(m_staging);
		// Maybe not necessary
		SAFE_RELEASE(m_commandList);
	}

private:
	DX12_ComputeTexture(const DX12_ComputeBuffer & cb) {}

	ID3D12Resource*				m_resource;
	ID3D12Resource*				m_staging;
	ID3D12GraphicsCommandList*	m_commandList;
	//ID3D11ShaderResourceView*	_ResourceView;
	//ID3D11UnorderedAccessView*	_UnorderedAccessView;

	friend class DX12_ComputeWrap;
};

class DX12_ComputeShader
{
	friend class DX12_ComputeWrap;

	ID3D12Device*				m_device;
	ID3D12GraphicsCommandList*	m_commandList;
	ID3DBlob*					m_computeShader;

private:
	explicit DX12_ComputeShader();

	bool Init(TCHAR* shaderFile, char* pFunctionName, D3D_SHADER_MACRO* pDefines,
		ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList);

public:
	~DX12_ComputeShader();

	void Set();
	void Unset();
};

class DX12_ComputeWrap
{
	ID3D12Device*               m_device;
	ID3D12GraphicsCommandList*  m_commandList;

public:
	DX12_ComputeWrap(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList)
	{
		m_device = pDevice;
		m_commandList = pCommandList;
	}

	DX12_ComputeShader* CreateComputeShader(TCHAR* shaderFile, char* blobFileAppendix, char* pFunctionName, D3D_SHADER_MACRO* pDefines);

	ID3D12Resource* CreateConstantBuffer(UINT uSize, VOID* pInitData, char* debugName = NULL);

	DX12_ComputeBuffer* CreateBuffer(COMPUTE_BUFFER_TYPE uType, UINT uElementSize,
		UINT uCount, bool bSRV, bool bUAV, VOID* pInitData, bool bCreateStaging = false, char* debugName = NULL);

	DX12_ComputeTexture* CreateTexture(DXGI_FORMAT dxFormat, UINT uWidth,
		UINT uHeight, UINT uRowPitch, VOID* pInitData, bool bCreateStaging = false, char* debugName = NULL);

	DX12_ComputeTexture* CreateTextureFromBitmap(TCHAR* textureFilename, char* debugName = NULL);

private:
	ID3D12Resource* CreateStructuredBuffer(UINT uElementSize, UINT uCount, bool bSRV, bool bUAV, VOID* pInitData);
	ID3D12Resource* CreateRawBuffer(UINT uSize, VOID* pInitData);
	ID3D11ShaderResourceView* CreateBufferSRV(ID3D11Buffer* pBuffer);
	ID3D11UnorderedAccessView* CreateBufferUAV(ID3D11Buffer* pBuffer);
	ID3D12Resource* CreateStagingBuffer(UINT uSize);

	//texture functions
	ID3D12Resource* CreateTextureResource(DXGI_FORMAT dxFormat,
		UINT uWidth, UINT uHeight, UINT uRowPitch, VOID* pInitData);
	ID3D11ShaderResourceView* CreateTextureSRV(ID3D12Resource* pTexture);
	ID3D11UnorderedAccessView* CreateTextureUAV(ID3D12Resource* pTexture);
	ID3D12Resource* CreateStagingTexture(ID3D12Resource* pTexture);

	unsigned char* LoadBitmapFileRGBA(TCHAR *filename, BITMAPINFOHEADER *bitmapInfoHeader);

	void SetDebugName(ID3D12DeviceChild* object, char* debugName);
};