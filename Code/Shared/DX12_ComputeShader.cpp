#include "DX12_ComputeShader.h"
#include <cstdio>

//--------------------------------------------------------------------------------------
// Copyright (c) Stefan Petersson 2012. All rights reserved.
//--------------------------------------------------------------------------------------
unsigned char * DX12_ComputeWrap::LoadBitmapFileRGBA(TCHAR * filename, BITMAPINFOHEADER * bitmapInfoHeader)
{
	FILE* filePtr;
	BITMAPFILEHEADER bitmapFileHeader;
	unsigned char* bitmapImage;
	unsigned char* flippedBitmapImage;
	int imageIdx = 0;
	unsigned char tempRGB;

	//open filename in read binary mode
	_tfopen_s(&filePtr, filename, _T("rb"));
	if (filePtr == NULL)
		return NULL;

	//read the bitmap file header
	fread(&bitmapFileHeader, sizeof(BITMAPFILEHEADER), 1, filePtr);

	//verify that this is a bmp file by check bitmap id ('B' and 'M')
	if (bitmapFileHeader.bfType != 0x4D42)
	{
		fclose(filePtr);
		return NULL;
	}

	//read the bitmap info header
	fread(bitmapInfoHeader, sizeof(BITMAPINFOHEADER), 1, filePtr);

	//move file point to the begging of bitmap data
	fseek(filePtr, bitmapFileHeader.bfOffBits, SEEK_SET);

	//allocate enough memory for the bitmap image data
	bitmapImage = new unsigned char[bitmapInfoHeader->biSizeImage];

	size_t memSize = bitmapInfoHeader->biWidth * bitmapInfoHeader->biHeight * 4;
	flippedBitmapImage = new unsigned char[memSize];

	//verify memory allocation
	if (!bitmapImage || !flippedBitmapImage)
	{
		SAFE_DELETE(bitmapImage);
		SAFE_DELETE(flippedBitmapImage);
		fclose(filePtr);
		return NULL;
	}

	//read in the bitmap image data
	fread(bitmapImage, bitmapInfoHeader->biSizeImage, 1, filePtr);

	//make sure bitmap image data was read
	if (bitmapImage == NULL)
	{
		fclose(filePtr);
		return NULL;
	}

	//swap the r and b values to get RGB (bitmap is BGR)
	for (imageIdx = 0; imageIdx < (int)bitmapInfoHeader->biSizeImage; imageIdx += 3)
	{
		tempRGB = bitmapImage[imageIdx];
		bitmapImage[imageIdx] = bitmapImage[imageIdx + 2];
		bitmapImage[imageIdx + 2] = tempRGB;
	}

	//flip image
	for (int y = 0; y < bitmapInfoHeader->biHeight; y++)
	{
		unsigned char* src = &bitmapImage[(bitmapInfoHeader->biHeight - 1 - y) * bitmapInfoHeader->biWidth * 3];
		unsigned char* dst = &flippedBitmapImage[bitmapInfoHeader->biWidth * y * 4];
		for (int x = 0; x < bitmapInfoHeader->biWidth; x++)
		{
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
			dst[3] = 255; //full alpha

			dst += 4;
			src += 3;
		}
	}

	SAFE_DELETE(bitmapImage);

	//close file and return bitmap iamge data
	fclose(filePtr);

	return flippedBitmapImage;
}

//--------------------------------------------------------------------------------------
// DirectX 12 Extension
// Fredrik Olsson 2019
//--------------------------------------------------------------------------------------

DX12_ComputeShader::DX12_ComputeShader()
	: m_device(NULL), m_commandList(NULL), m_computeShader(NULL)
{
}

bool DX12_ComputeShader::Init(TCHAR * shaderFile, char * pFunctionName, D3D_SHADER_MACRO * pDefines, ID3D12Device * pDevice, ID3D12GraphicsCommandList * pCommandList)
{
	HRESULT hr = S_OK;

	m_device = pDevice;
	m_commandList = pCommandList;

	ID3DBlob* pCompiledShader = NULL;
	ID3DBlob* pErrorBlob = NULL;
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(DEBUG) || defined(_DEBUG)
	dwShaderFlags |= D3DCOMPILE_DEBUG;
	dwShaderFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL0;
#else
	dwShaderFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL0;
#endif

	hr = D3DCompileFromFile(shaderFile, pDefines, NULL, pFunctionName, "cs_5_0",
		dwShaderFlags, NULL, &pCompiledShader, &pErrorBlob);

	if (pErrorBlob)
		OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());

	if (S_OK == hr)
		m_computeShader = pCompiledShader;

	SAFE_RELEASE(pErrorBlob);
	SAFE_RELEASE(pCompiledShader);

	return (hr == S_OK);
}

DX12_ComputeShader::~DX12_ComputeShader()
{
	SAFE_RELEASE(m_computeShader);
	SAFE_RELEASE(m_commandList);
}

DX12_ComputeShader * DX12_ComputeWrap::CreateComputeShader(TCHAR * shaderFile, char * pFunctionName, D3D_SHADER_MACRO * pDefines)
{
	DX12_ComputeShader * cs = new DX12_ComputeShader();

	if (cs && !cs->Init(shaderFile, pFunctionName, pDefines, m_device, m_commandList))
		SAFE_DELETE(cs);

	return cs;
}

ID3D12Resource * DX12_ComputeWrap::CreateConstantBuffer(UINT uSize, VOID * pInitData, char * debugName)
{
	ID3D12Resource * pBuffer = NULL;

	D3D12_RESOURCE_DESC cbDesc;
	cbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	cbDesc.Alignment = 0;
	cbDesc.Width = uSize;
	cbDesc.Height = 1;
	cbDesc.DepthOrArraySize = 1;
	cbDesc.MipLevels = 1;
	cbDesc.Format = DXGI_FORMAT_UNKNOWN;
	cbDesc.SampleDesc.Count = 1;;
	cbDesc.SampleDesc.Quality = 0;
	cbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	cbDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	if (FAILED(m_device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&cbDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&pBuffer))))
	{
		return nullptr;
	}

	if (pInitData)
	{
		D3D12_SUBRESOURCE_DATA resourceData;
		resourceData.pData = pInitData;
		resourceData.RowPitch = sizeof(pInitData);
		resourceData.SlicePitch = resourceData.RowPitch;

		// Create GPU upload buffer
		ID3D12Resource * intermediateResource = NULL;

		UINT64 uploadBufferSize = 0;
		{
			m_device->GetCopyableFootprints(
				&cbDesc,
				0,
				1,
				0,
				nullptr,
				nullptr,
				nullptr,
				&uploadBufferSize
			);
		}

		heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC rdBuffer = {};
		rdBuffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		rdBuffer.Alignment = 0;
		rdBuffer.Width = uploadBufferSize;
		rdBuffer.Height = 1;
		rdBuffer.DepthOrArraySize = 1;
		rdBuffer.MipLevels = 1;
		rdBuffer.Format = DXGI_FORMAT_UNKNOWN;
		rdBuffer.SampleDesc.Count = 1;;
		rdBuffer.SampleDesc.Quality = 0;
		rdBuffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		rdBuffer.Flags = D3D12_RESOURCE_FLAG_NONE;

		if (FAILED(m_device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&rdBuffer,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&intermediateResource))))
		{
			return nullptr;
		}

		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = pBuffer;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER; // Since we access the cb in the compute shader
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		if (FAILED(m_cmdAllocator->Reset()))
			return nullptr;
		if (FAILED(m_commandList->Reset(m_cmdAllocator, nullptr)))
			return nullptr;

		// Populate the command queue
		m_commandList->SetGraphicsRootSignature(m_rootSignature);
		UpdateSubresources(m_commandList, pBuffer, intermediateResource, 0, 0, 1, &resourceData);
		m_commandList->ResourceBarrier(1, &barrier);

		D3D12_CONSTANT_BUFFER_VIEW_DESC constBuffDesc;
		constBuffDesc.BufferLocation = pBuffer->GetGPUVirtualAddress();
		constBuffDesc.SizeInBytes = uploadBufferSize;

		m_device->CreateConstantBufferView(&constBuffDesc, m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());

		// Close the command list and execute it
		m_commandList->Close();
		ID3D12CommandList* ppCommandLists[] = { m_commandList };
		m_computeQueue->ExecuteCommandLists(_ARRAYSIZE(ppCommandLists), ppCommandLists);

		m_D3D12Wrap->WaitForGPUCompletion(m_computeQueue, m_D3D12Wrap->GetFence(0));

		SAFE_RELEASE(intermediateResource);
	}

	if (debugName && pBuffer)
		pBuffer->SetName((LPCWSTR)debugName);

	return pBuffer;
}

DX12_ComputeBuffer * DX12_ComputeWrap::CreateBuffer(COMPUTE_BUFFER_TYPE uType, UINT uElementSize, UINT uCount,
	bool bSRV, bool bUAV, VOID * pInitData, bool bCreateStaging, char * debugName)
{
	DX12_ComputeBuffer* buffer = new DX12_ComputeBuffer();

	if (STRUCTURED_BUFFER == uType)
		buffer->m_resource = CreateStructuredBuffer(uElementSize, uCount, bSRV, bUAV, pInitData);
	else if (RAW_BUFFER == uType)
		buffer->m_resource = CreateRawBuffer(uElementSize * uCount, pInitData);

	if (NULL != buffer->m_resource)
	{
		if (bSRV)
			CreateBufferSRV(buffer->m_SRV, uElementSize, uCount);
		if (bUAV)
			CreateBufferUAV(buffer->m_UAV, uElementSize, uCount);

		if (bCreateStaging)
			buffer->m_staging = CreateStagingBuffer(uElementSize * uCount);
	}

	if (debugName)
	{
		if (buffer->m_resource)
			buffer->m_resource->SetName(LPCWSTR(debugName));
		if (buffer->m_staging)
			buffer->m_staging->SetName(LPCWSTR(debugName));
		if (buffer->m_SRV)
			buffer->m_SRV->SetName(LPCWSTR(debugName));
		if (buffer->m_UAV)
			buffer->m_UAV->SetName(LPCWSTR(debugName));
	}

	return buffer;
}

DX12_ComputeTexture * DX12_ComputeWrap::CreateTexture(DXGI_FORMAT dxFormat, UINT uWidth, UINT uHeight, UINT uRowPitch, VOID * pInitData, bool bCreateStaging, char * debugName)
{
	DX12_ComputeTexture* texture = new DX12_ComputeTexture();
	
	texture->m_resource = CreateTextureResource(dxFormat, uWidth, uHeight, uRowPitch, pInitData);

	if (NULL != texture->m_resource)
	{
		CreateTextureSRV(texture->m_SRV);
		CreateTextureUAV(texture->m_UAV);

		if (bCreateStaging)
			CreateStagingTexture(texture->m_resource);
	}

	if (debugName)
	{
		if (texture->m_resource)
			texture->m_resource->SetName(LPCWSTR(debugName));
		if (texture->m_staging)
			texture->m_staging->SetName(LPCWSTR(debugName));
		if (texture->m_SRV)
			texture->m_SRV->SetName(LPCWSTR(debugName));
		if (texture->m_UAV)
			texture->m_UAV->SetName(LPCWSTR(debugName));
	}

	return texture;
}

DX12_ComputeTexture * DX12_ComputeWrap::CreateTextureFromBitmap(TCHAR * textureFilename, char * debugName)
{
	DX12_ComputeTexture* texture = new DX12_ComputeTexture();
	BITMAPINFOHEADER bitmapInfoHeader;
	unsigned char* bits = LoadBitmapFileRGBA(textureFilename, &bitmapInfoHeader);

	if (NULL != bits)
	{
		texture->m_resource = CreateTextureResource(
			DXGI_FORMAT_R8G8B8A8_UNORM,
			bitmapInfoHeader.biWidth,
			bitmapInfoHeader.biHeight,
			bitmapInfoHeader.biWidth * 4,
			bits);

		SAFE_DELETE(bits);

		if (texture->m_resource)
		{
			CreateTextureSRV(texture->m_SRV);

			if (debugName)
			{
				if (texture->m_resource)
					texture->m_resource->SetName(LPCWSTR(debugName));
				if (texture->m_staging)
					texture->m_staging->SetName(LPCWSTR(debugName));
				if (texture->m_SRV)
					texture->m_SRV->SetName(LPCWSTR(debugName));
				if (texture->m_UAV)
					texture->m_UAV->SetName(LPCWSTR(debugName));
			}
		}
	}

	return texture;
}

ID3D12Resource * DX12_ComputeWrap::CreateStructuredBuffer(UINT uElementSize, UINT uCount, bool bSRV, bool bUAV, VOID * pInitData)
{
	ID3D12Resource* pResource = NULL;

	D3D12_RESOURCE_DESC desc = {};
	ZeroMemory(&desc, sizeof(desc));

	desc.Flags = D3D12_RESOURCE_FLAG_NONE;

	UINT bufferSize = uElementSize * uCount;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Alignment = 0;
	desc.Width = bufferSize;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	if (bUAV)
		desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	if (FAILED(m_device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&pResource))))
	{
		return nullptr;
	}

	if (pInitData)
	{
		D3D12_SUBRESOURCE_DATA resourceData;
		resourceData.pData = pInitData;
		resourceData.RowPitch = sizeof(pInitData);
		resourceData.SlicePitch = resourceData.RowPitch;

		// Create GPU upload buffer
		ID3D12Resource * intermediateResource = NULL;

		UINT64 uploadBufferSize = 0;
		{
			m_device->GetCopyableFootprints(
				&desc,
				0,
				1,
				0,
				nullptr,
				nullptr,
				nullptr,
				&uploadBufferSize
			);
		}

		heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC rdBuffer = {};
		rdBuffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		rdBuffer.Alignment = 0;
		rdBuffer.Width = uploadBufferSize;
		rdBuffer.Height = 1;
		rdBuffer.DepthOrArraySize = 1;
		rdBuffer.MipLevels = 1;
		rdBuffer.Format = DXGI_FORMAT_UNKNOWN;
		rdBuffer.SampleDesc.Count = 1;;
		rdBuffer.SampleDesc.Quality = 0;
		rdBuffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		rdBuffer.Flags = D3D12_RESOURCE_FLAG_NONE; // Maybe change to desc.Flags (?)

		if (FAILED(m_device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&rdBuffer,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&intermediateResource))))
		{
			return nullptr;
		}

		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = pResource;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		if (bUAV)
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		else if (bSRV)
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		else
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

		if (FAILED(m_cmdAllocator->Reset()))
			return nullptr;
		if (FAILED(m_commandList->Reset(m_cmdAllocator, nullptr)))
			return nullptr;

		// Populate the command queue
		m_commandList->SetGraphicsRootSignature(m_rootSignature);
		UpdateSubresources(m_commandList, pResource, intermediateResource, 0, 0, 1, &resourceData);
		m_commandList->ResourceBarrier(1, &barrier);

		// Close the command list and execute it
		m_commandList->Close();
		ID3D12CommandList* ppCommandLists[] = { m_commandList };
		m_computeQueue->ExecuteCommandLists(_ARRAYSIZE(ppCommandLists), ppCommandLists);

		m_D3D12Wrap->WaitForGPUCompletion(m_computeQueue, m_D3D12Wrap->GetFence(0));

		SAFE_RELEASE(intermediateResource);
	}

	return pResource;
}

// This is wrong. Havn't found a way to define Raw yet
ID3D12Resource * DX12_ComputeWrap::CreateRawBuffer(UINT uSize, VOID * pInitData)
{
	ID3D12Resource* pResource = NULL;

	D3D12_RESOURCE_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Alignment = 0;
	desc.Width = uSize;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	// Change generic read
	if (FAILED(m_device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&pResource))))
	{
		return nullptr;
	}

	if (pInitData)
	{
		D3D12_SUBRESOURCE_DATA resourceData;
		resourceData.pData = pInitData;
		resourceData.RowPitch = sizeof(pInitData);
		resourceData.SlicePitch = resourceData.RowPitch;

		// Create GPU upload buffer
		ID3D12Resource * intermediateResource = NULL;

		UINT64 uploadBufferSize = 0;
		{
			m_device->GetCopyableFootprints(
				&desc,
				0,
				1,
				0,
				nullptr,
				nullptr,
				nullptr,
				&uploadBufferSize
			);
		}

		heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC rdBuffer = {};
		rdBuffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		rdBuffer.Alignment = 0;
		rdBuffer.Width = uSize;
		rdBuffer.Height = 1;
		rdBuffer.DepthOrArraySize = 1;
		rdBuffer.MipLevels = 1;
		rdBuffer.Format = DXGI_FORMAT_UNKNOWN;
		rdBuffer.SampleDesc.Count = 1;;
		rdBuffer.SampleDesc.Quality = 0;
		rdBuffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		rdBuffer.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		if (FAILED(m_device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&rdBuffer,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&intermediateResource))))
		{
			return nullptr;
		}

		UpdateSubresources(m_commandList, pResource, intermediateResource, 0, 0, 1, &resourceData);

		SAFE_RELEASE(intermediateResource);
	}

	return pResource;
}

void DX12_ComputeWrap::CreateBufferSRV(ID3D12Resource * pBuffer, UINT uElementSize, UINT uCount)
{
	/*D3D12_RESOURCE_DESC descBuff;
	ZeroMemory(&descBuff, sizeof(descBuff));
	descBuff = pBuffer->GetDesc();*/

	D3D12_SHADER_RESOURCE_VIEW_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	desc.Buffer.FirstElement = 0;

	// if raw do smth

	// else if structured...
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.Buffer.NumElements = uCount;
	desc.Buffer.StructureByteStride = uElementSize;
	
	// else NULL

	m_device->CreateShaderResourceView(pBuffer, &desc, m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
}

void DX12_ComputeWrap::CreateBufferUAV(ID3D12Resource * pBuffer, UINT uElementSize, UINT uCount)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	desc.Buffer.FirstElement = 0;
	
	// if else wooho

	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.Buffer.NumElements = uCount;
	desc.Buffer.StructureByteStride = uElementSize;

	// else NULL

	m_device->CreateUnorderedAccessView(pBuffer, NULL, &desc, m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
}

// Not yet implemented
ID3D12Resource * DX12_ComputeWrap::CreateStagingBuffer(UINT uSize)
{
	return nullptr;
}

ID3D12Resource * DX12_ComputeWrap::CreateTextureResource(DXGI_FORMAT dxFormat, UINT uWidth, UINT uHeight, UINT uRowPitch, VOID * pInitData)
{
	ID3D12Resource * pTexture = NULL;

	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.Width = uWidth;
	textureDesc.Height = uHeight;
	textureDesc.MipLevels = 1;
	textureDesc.DepthOrArraySize = 1;
	textureDesc.Format = dxFormat;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureDesc.Alignment = 0;
	textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	if (FAILED(m_device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&pTexture))))
	{
		return nullptr;
	}

	// Create GPU upload buffer
	UINT64 uploadBufferSize = 0;
	{
		m_device->GetCopyableFootprints(
			&textureDesc,
			0,
			1,
			0,
			nullptr,
			nullptr,
			nullptr,
			&uploadBufferSize
		);
	}

	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC resourceDescBuffer = {};
	resourceDescBuffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDescBuffer.Alignment = 0;
	resourceDescBuffer.Width = uploadBufferSize;
	resourceDescBuffer.Height = 1;
	resourceDescBuffer.DepthOrArraySize = 1;
	resourceDescBuffer.MipLevels = 1;
	resourceDescBuffer.Format = DXGI_FORMAT_UNKNOWN;
	resourceDescBuffer.SampleDesc.Count = 1;
	resourceDescBuffer.SampleDesc.Quality = 0;
	resourceDescBuffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDescBuffer.Flags = D3D12_RESOURCE_FLAG_NONE;

	ID3D12Resource * textureUploadHeap = nullptr;

	if (FAILED(m_device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&textureUploadHeap))))
	{
		return nullptr;
	}

	// Copy data to the intermediate upload heap and then
	// schedule a copy from the upload heap to the Texture2D
	D3D12_SUBRESOURCE_DATA textureData = {};
	textureData.pData = pInitData;
	textureData.RowPitch = uWidth * 4 * sizeof(unsigned char); // 4 = size of pixel: rgba.
	textureData.SlicePitch = textureData.RowPitch * uHeight;

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = pTexture;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE; // Since we access the srv in the compute shader
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	if (FAILED(m_cmdAllocator->Reset()))
		return nullptr;
	if (FAILED(m_commandList->Reset(m_cmdAllocator, nullptr)))
		return nullptr;

	// Populate the command queue
	m_commandList->SetGraphicsRootSignature(m_rootSignature);
	UpdateSubresources(m_commandList, pTexture, textureUploadHeap, 0, 0, 1, &textureData);
	m_commandList->ResourceBarrier(1, &barrier);

	// Describe and create an SRV for the texture
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = textureDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	m_device->CreateShaderResourceView(pTexture, &srvDesc, m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());

	// Close the command list and execute it
	m_commandList->Close();
	ID3D12CommandList* ppCommandLists[] = { m_commandList };
	m_computeQueue->ExecuteCommandLists(_ARRAYSIZE(ppCommandLists), ppCommandLists);

	m_D3D12Wrap->WaitForGPUCompletion(m_computeQueue, m_D3D12Wrap->GetFence(0));

	SAFE_RELEASE(textureUploadHeap);

	pTexture->SetName(L"Resource - CreateTextureResource");

	return pTexture;
}

void DX12_ComputeWrap::CreateTextureSRV(ID3D12Resource * pTexture)
{
	D3D12_RESOURCE_DESC desc;
	desc = pTexture->GetDesc();

	// Init view description
	D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc;
	ZeroMemory(&viewDesc, sizeof(viewDesc));

	viewDesc.Format =				desc.Format;
	viewDesc.ViewDimension =		D3D12_SRV_DIMENSION_TEXTURE2D;
	viewDesc.Texture2D.MipLevels =	desc.MipLevels;
	
	m_device->CreateShaderResourceView(pTexture, &viewDesc, m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
}

void DX12_ComputeWrap::CreateTextureUAV(ID3D12Resource * pTexture)
{
	m_device->CreateUnorderedAccessView(pTexture, NULL, NULL, m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
	pTexture->Release();
}

// Not yet implemented
void DX12_ComputeWrap::CreateStagingTexture(ID3D12Resource * pTexture)
{
}
