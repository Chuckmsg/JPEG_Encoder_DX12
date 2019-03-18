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

/*void DX12_ComputeWrap::SetDebugName(ID3D12DeviceChild * object, char * debugName)
{
#if defined( DEBUG ) || defined( _DEBUG )
	// Only works if device is created with the D3D10 or D3D11 debug layer, or when attached to PIX for Windows
	object->SetPrivateData(WKPDID_D3DDebugObjectName, (unsigned int)strlen(debugName), debugName);
#endif
}*/

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

	// Change to cs_6_0 (?)
	hr = D3DCompileFromFile(shaderFile, pDefines, NULL, pFunctionName, "cs_5_0",
		dwShaderFlags, NULL, &pCompiledShader, &pErrorBlob);

	if (pErrorBlob)
		OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());

	// Does this work (?)
	if (S_OK == hr)
		m_computeShader = pCompiledShader;

	SAFE_RELEASE(pErrorBlob);
	// Necessary (?)
	SAFE_RELEASE(pCompiledShader);

	return (hr == S_OK);
}

DX12_ComputeShader::~DX12_ComputeShader()
{
	SAFE_RELEASE(m_computeShader);
	// Might not necessary
	SAFE_RELEASE(m_commandList);
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

	pTexture->SetName(L"Resource - CreateTextureResource");

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

	m_cmdAllocator->Reset();
	m_commandList->Reset(m_cmdAllocator, nullptr);

	// Populate the command queue
	m_commandList->SetComputeRootSignature(m_rootSignature);
	UpdateSubresources(m_commandList, pTexture, textureUploadHeap, 0, 0, 1, &textureData);
	m_commandList->ResourceBarrier(1, &barrier);

	// Describe and create an SRV for the texture
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = textureDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	m_device->CreateShaderResourceView(pTexture, &srvDesc, m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());

	// Set the descriptor heap
	ID3D12DescriptorHeap* descriptorHeaps[] = { m_descriptorHeap };
	m_commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	// Set the descriptor table to the descriptor heap
	m_commandList->SetComputeRootDescriptorTable(0, m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());

	m_commandList->Close();
	ID3D12CommandList* ppCommandLists[] = { m_commandList };
	m_computeQueue->ExecuteCommandLists(_ARRAYSIZE(ppCommandLists), ppCommandLists);

	m_D3D12Wrap->WaitForGPUCompletion(m_computeQueue, m_D3D12Wrap->GetFence(0));

	SAFE_RELEASE(textureUploadHeap);

	return pTexture;
}

DX12_ComputeShader * DX12_ComputeWrap::CreateComputeShader(TCHAR * shaderFile, char * pFunctionName, D3D_SHADER_MACRO * pDefines)
{
	DX12_ComputeShader * cs = new DX12_ComputeShader();

	if (cs && !cs->Init(shaderFile, pFunctionName, pDefines, m_device, m_commandList))
		SAFE_DELETE(cs);

	return cs;
}

ID3D12Resource * DX12_ComputeWrap::CreateConstantBuffer(ID3D12DescriptorHeap * pHeap, UINT uSize, VOID * pInitData, char * debugName)
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
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&pBuffer))))
	{
		return nullptr;
	}

	if (pInitData)
	{
		D3D12_SUBRESOURCE_DATA resourceData;
		resourceData.pData = pInitData;
		resourceData.RowPitch = uSize * sizeof(ConstantBuffer); // Multiply the size with the constant buffer struct size
		resourceData.SlicePitch = resourceData.RowPitch;

		// Create GPU upload buffer
		ID3D12Resource * intermediateResource = NULL;

		/*UINT64 uploadBufferSize = 0;
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
		}*/

		heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC rdBuffer = {};
		rdBuffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		cbDesc.Alignment = 0;
		cbDesc.Width = uSize;
		//cbDesc.Width = uploadBufferSize;
		cbDesc.Height = 1;
		cbDesc.DepthOrArraySize = 1;
		cbDesc.MipLevels = 1;
		cbDesc.Format = DXGI_FORMAT_UNKNOWN;
		cbDesc.SampleDesc.Count = 1;;
		cbDesc.SampleDesc.Quality = 0;
		cbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		cbDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

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

		UpdateSubresources(m_commandList, pBuffer, intermediateResource, 0, 0, 1, &resourceData);

		//textureData.RowPitch = uWidth * 4 * sizeof(unsigned char); // 4 = size of pixel: rgba.
		//textureData.SlicePitch = textureData.RowPitch * uHeight;
	}

	if (debugName && pBuffer)
		pBuffer->SetName((LPCWSTR)debugName);

	return pBuffer;
}
