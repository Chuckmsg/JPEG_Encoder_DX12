//--------------------------------------------------------------------------------------
// Real-Time JPEG Compression using DirectCompute - Demo
//
// Copyright (c) Stefan Petersson 2012. All rights reserved.
//--------------------------------------------------------------------------------------
#include "ComputeShader.h"
#include <cstdio>

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib") 

ComputeShader::ComputeShader()
	: mD3DDevice(NULL), mD3DDeviceContext(NULL), mShader(NULL)
{

}

ComputeShader::~ComputeShader()
{
	SAFE_RELEASE(mShader);
	SAFE_RELEASE(pCompiledShader);
	//SAFE_RELEASE(mD3DDevice);
	//SAFE_RELEASE(mD3DDeviceContext);
}

bool ComputeShader::Init(TCHAR* shaderFile, char* blobFileAppendix, char* pFunctionName, D3D10_SHADER_MACRO* pDefines,
	ID3D11Device* d3dDevice, ID3D11DeviceContext*d3dContext, bool dx12)
{
	HRESULT hr = S_OK;
	mD3DDevice = d3dDevice;
	mD3DDeviceContext = d3dContext;

	
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
	{
		OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
	}

	if(hr == S_OK)
	{
		/*
		ID3D11ShaderReflection* pReflector = NULL;
		hr = D3DReflect( pCompiledShader->GetBufferPointer(), 
			pCompiledShader->GetBufferSize(), IID_ID3D11ShaderReflection, 
			(void**) &pReflector);
		*/
		if(hr == S_OK && !dx12)
		{
			hr = mD3DDevice->CreateComputeShader(pCompiledShader->GetBufferPointer(),
				pCompiledShader->GetBufferSize(), NULL, &mShader);
		}
	}

	SAFE_RELEASE(pErrorBlob);
	

    return (hr == S_OK);
}

void ComputeShader::Set()
{
	mD3DDeviceContext->CSSetShader( mShader, NULL, 0 );
}

void ComputeShader::Unset()
{
	mD3DDeviceContext->CSSetShader( NULL, NULL, 0 );
}

ComputeBuffer* ComputeWrap::CreateBuffer(COMPUTE_BUFFER_TYPE uType,
	UINT uElementSize, UINT uCount, bool bSRV, bool bUAV, VOID* pInitData, bool bCreateStaging, char* debugName)
{
	ComputeBuffer* buffer = new ComputeBuffer();
	buffer->_D3DContext = mD3DDeviceContext;

	if(uType == STRUCTURED_BUFFER)
		buffer->_Resource = CreateStructuredBuffer(uElementSize, uCount, bSRV, bUAV, pInitData);
	else if(uType == RAW_BUFFER)
		buffer->_Resource = CreateRawBuffer(uElementSize * uCount, pInitData);

	if(buffer->_Resource != NULL)
	{
		if(bSRV)
			buffer->_ResourceView = CreateBufferSRV(buffer->_Resource);
		if(bUAV)
			buffer->_UnorderedAccessView = CreateBufferUAV(buffer->_Resource);
		
		if(bCreateStaging)
			buffer->_Staging = CreateStagingBuffer(uElementSize * uCount);
	}

	if(debugName)
	{
		if(buffer->_Resource)				SetDebugName(buffer->_Resource, debugName);
		if(buffer->_Staging)				SetDebugName(buffer->_Staging, debugName);
		if(buffer->_ResourceView)			SetDebugName(buffer->_ResourceView, debugName);
		if(buffer->_UnorderedAccessView)	SetDebugName(buffer->_UnorderedAccessView, debugName);
	}

	return buffer; //return shallow copy
}

ID3D11Buffer* ComputeWrap::CreateStructuredBuffer(UINT uElementSize, UINT uCount,
									bool bSRV, bool bUAV, VOID* pInitData)
{
    ID3D11Buffer* pBufOut = NULL;

    D3D11_BUFFER_DESC desc;
    ZeroMemory( &desc, sizeof(desc) );
    desc.BindFlags = 0;
	
	if(bUAV)	desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
	if(bSRV)	desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
    
	UINT bufferSize = uElementSize * uCount;
	desc.ByteWidth = bufferSize < 16 ? 16 : bufferSize;
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride = uElementSize;

    if ( pInitData )
    {
        D3D11_SUBRESOURCE_DATA InitData;
        InitData.pSysMem = pInitData;
		mD3DDevice->CreateBuffer( &desc, &InitData, &pBufOut);
    }
	else
	{
		mD3DDevice->CreateBuffer(&desc, NULL, &pBufOut);
	}

	return pBufOut;
}

ID3D11Buffer* ComputeWrap::CreateRawBuffer(UINT uSize, VOID* pInitData)
{
    ID3D11Buffer* pBufOut = NULL;

    D3D11_BUFFER_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_INDEX_BUFFER | D3D11_BIND_VERTEX_BUFFER;
    desc.ByteWidth = uSize;
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;

    if ( pInitData )
    {
        D3D11_SUBRESOURCE_DATA InitData;
        InitData.pSysMem = pInitData;
        mD3DDevice->CreateBuffer(&desc, &InitData, &pBufOut);
    }
	else
	{
        mD3DDevice->CreateBuffer(&desc, NULL, &pBufOut);
	}

	return pBufOut;
}

ID3D11ShaderResourceView* ComputeWrap::CreateBufferSRV(ID3D11Buffer* pBuffer)
{
	ID3D11ShaderResourceView* pSRVOut = NULL;

    D3D11_BUFFER_DESC descBuf;
    ZeroMemory(&descBuf, sizeof(descBuf));
    pBuffer->GetDesc(&descBuf);

    D3D11_SHADER_RESOURCE_VIEW_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
    desc.BufferEx.FirstElement = 0;

    if(descBuf.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS)
    {
        // This is a Raw Buffer
        desc.Format = DXGI_FORMAT_R32_TYPELESS;
        desc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
        desc.BufferEx.NumElements = descBuf.ByteWidth / 4;
    }
	else if(descBuf.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED)
    {
        // This is a Structured Buffer
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.BufferEx.NumElements = descBuf.ByteWidth / descBuf.StructureByteStride;
    }
	else
	{
		return NULL;
	}

    mD3DDevice->CreateShaderResourceView(pBuffer, &desc, &pSRVOut);

	return pSRVOut;
}

ID3D11UnorderedAccessView* ComputeWrap::CreateBufferUAV(ID3D11Buffer* pBuffer)
{
	ID3D11UnorderedAccessView* pUAVOut = NULL;

	D3D11_BUFFER_DESC descBuf;
    ZeroMemory(&descBuf, sizeof(descBuf));
    pBuffer->GetDesc(&descBuf);
        
    D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    desc.Buffer.FirstElement = 0;

    if (descBuf.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS)
    {
        // This is a Raw Buffer
        desc.Format = DXGI_FORMAT_R32_TYPELESS; // Format must be DXGI_FORMAT_R32_TYPELESS, when creating Raw Unordered Access View
        desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
        desc.Buffer.NumElements = descBuf.ByteWidth / 4; 
    }
	else if(descBuf.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED)
    {
        // This is a Structured Buffer
        desc.Format = DXGI_FORMAT_UNKNOWN;      // Format must be must be DXGI_FORMAT_UNKNOWN, when creating a View of a Structured Buffer
        desc.Buffer.NumElements = descBuf.ByteWidth / descBuf.StructureByteStride; 
    }
	else
	{
		return NULL;
	}
    
	mD3DDevice->CreateUnorderedAccessView(pBuffer, &desc, &pUAVOut);

	return pUAVOut;
}

ID3D11Buffer* ComputeWrap::CreateStagingBuffer(UINT uSize)
{
    ID3D11Buffer* debugbuf = NULL;

    D3D11_BUFFER_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
	desc.ByteWidth = uSize;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;
    
	mD3DDevice->CreateBuffer(&desc, NULL, &debugbuf);

    return debugbuf;
}



// TEXTURE FUNCTIONS
ComputeTexture* ComputeWrap::CreateTexture(DXGI_FORMAT dxFormat, UINT uWidth,
	UINT uHeight, UINT uRowPitch, VOID* pInitData, bool bCreateStaging, char* debugName)
{
	ComputeTexture* texture = new ComputeTexture();
	texture->_D3DContext = mD3DDeviceContext;

	texture->_Resource = CreateTextureResource(dxFormat, uWidth, uHeight, uRowPitch, pInitData);

	if(texture->_Resource != NULL)
	{
		texture->_ResourceView = CreateTextureSRV(texture->_Resource);
		texture->_UnorderedAccessView = CreateTextureUAV(texture->_Resource);
		
		if(bCreateStaging)
			texture->_Staging = CreateStagingTexture(texture->_Resource);
	}

	if(debugName)
	{
		if(texture->_Resource)				SetDebugName(texture->_Resource, debugName);
		if(texture->_Staging)				SetDebugName(texture->_Staging, debugName);
		if(texture->_ResourceView)			SetDebugName(texture->_ResourceView, debugName);
		if(texture->_UnorderedAccessView)	SetDebugName(texture->_UnorderedAccessView, debugName);
	}

	return texture;
}

ComputeTexture* ComputeWrap::CreateTextureFromBitmap(TCHAR* textureFilename, char* debugName, bool dx12)
{
	ComputeTexture* texture = new ComputeTexture();
	texture->_D3DContext = mD3DDeviceContext;

	BITMAPINFOHEADER bitmapInfoHeader;
	unsigned char* bits = LoadBitmapFileRGBA(textureFilename, &bitmapInfoHeader);

	if(bits != NULL)
	{
		if (dx12)
		{
			texture->m_DX12Resource = CreateTextureResourceDX12(
				DXGI_FORMAT_R8G8B8A8_UNORM,
				bitmapInfoHeader.biWidth,
				bitmapInfoHeader.biHeight,
				bitmapInfoHeader.biWidth * 4,
				bits
			);

			SAFE_DELETE(bits);
		}
		else
		{
			texture->_Resource = CreateTextureResource(
				DXGI_FORMAT_R8G8B8A8_UNORM,
				bitmapInfoHeader.biWidth,
				bitmapInfoHeader.biHeight,
				bitmapInfoHeader.biWidth * 4,
				bits);

			SAFE_DELETE(bits);

			if(texture->_Resource)
			{
				texture->_ResourceView = CreateTextureSRV(texture->_Resource);
		
				if(debugName)
				{
					if(texture->_Resource)				SetDebugName(texture->_Resource, debugName);
					if(texture->_Staging)				SetDebugName(texture->_Staging, debugName);
					if(texture->_ResourceView)			SetDebugName(texture->_ResourceView, debugName);
					if(texture->_UnorderedAccessView)	SetDebugName(texture->_UnorderedAccessView, debugName);
				}
			}
		}
	}
	return texture;
}

ID3D11Texture2D* ComputeWrap::CreateTextureResource(DXGI_FORMAT dxFormat,
	UINT uWidth, UINT uHeight, UINT uRowPitch, VOID* pInitData)
{
	ID3D11Texture2D* pTexture = NULL;

	D3D11_TEXTURE2D_DESC desc;
	desc.Width = uWidth;
	desc.Height = uHeight;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = dxFormat;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA data;
	data.pSysMem = pInitData;
	data.SysMemPitch = uRowPitch; //uWidth * 4;

	if(FAILED(mD3DDevice->CreateTexture2D( &desc, pInitData ? &data : NULL, &pTexture )))
	{

	}

	return pTexture;
}

ID3D12Resource * ComputeWrap::CreateTextureResourceDX12(DXGI_FORMAT dxFormat, UINT uWidth, UINT uHeight, UINT uRowPitch, VOID * pInitData)
{
	ID3D12Resource * pTexture = NULL;

	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.MipLevels = 1;
	textureDesc.Format = dxFormat;
	textureDesc.Width = uWidth;
	textureDesc.Height = uHeight;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	textureDesc.DepthOrArraySize = 1;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureDesc.Alignment = 0;
	textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	HRESULT hr = m_D3D12Wrap->GetDevice()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&pTexture)
	);

	if (FAILED(hr))
		return nullptr;

	pTexture->SetName(L"TextureResource");

	//Create GPU upload buffer
	UINT64 uploadBufferSize = 0;
	{
		m_D3D12Wrap->GetDevice()->GetCopyableFootprints(
			&textureDesc,
			0,
			1,
			0,
			nullptr,
			nullptr,
			nullptr,
			&uploadBufferSize);
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
	rdBuffer.SampleDesc.Count = 1;
	rdBuffer.SampleDesc.Quality = 0;
	rdBuffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	rdBuffer.Flags = D3D12_RESOURCE_FLAG_NONE;

	ID3D12Resource* _textureUploadHeap = nullptr;

	hr = m_D3D12Wrap->GetDevice()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&rdBuffer,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&_textureUploadHeap)
	);

	// Copy data to the intermediate upload heap and then schedule a copy
		// from the upload heap to the Texture2D
	D3D12_SUBRESOURCE_DATA textureData = {};
	textureData.pData = pInitData;
	textureData.RowPitch = uWidth * 4 * sizeof(unsigned char); // 4 = size of pixel: rgba.
	textureData.SlicePitch = textureData.RowPitch * uHeight;

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = pTexture;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE; //since we access the srv in compute shader
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	//Get all the necessary D3D12 object pointers
	ID3D12CommandAllocator * pCmdAllo = m_D3D12Wrap->GetComputeAllocator();
	ID3D12GraphicsCommandList * pCompCmdList = m_D3D12Wrap->GetComputeCmdList();
	ID3D12CommandQueue * pCompCmdQ = m_D3D12Wrap->GetComputeQueue();
	ID3D12DescriptorHeap * pSRVHeap = m_D3D12Wrap->GetSRVHeap();

	pCmdAllo->Reset();
	pCompCmdList->Reset(pCmdAllo, nullptr);

	//Fill the cmd q with commands
	pCompCmdList->SetComputeRootSignature(m_D3D12Wrap->GetRootSignature());
	UpdateSubresources(pCompCmdList, pTexture, _textureUploadHeap, 0, 0, 1, &textureData);
	pCompCmdList->ResourceBarrier(1, &barrier);

	// Describe and create a SRV for the texture
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = textureDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	m_D3D12Wrap->GetDevice()->CreateShaderResourceView(
		pTexture,
		&srvDesc,
		pSRVHeap->GetCPUDescriptorHandleForHeapStart()
	);

	// set the descriptor heap
	ID3D12DescriptorHeap* descriptorHeaps[] = { pSRVHeap };
	pCompCmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	// set the descriptor table to the descriptor heap (parameter 1, as constant buffer root descriptor is parameter index 0)
	pCompCmdList->SetComputeRootDescriptorTable(0, pSRVHeap->GetGPUDescriptorHandleForHeapStart());

	//Close the q and execute it
	pCompCmdList->Close();
	ID3D12CommandList* ppCommandLists[] = { pCompCmdList };
	pCompCmdQ->ExecuteCommandLists(_ARRAYSIZE(ppCommandLists), ppCommandLists);

	m_D3D12Wrap->WaitForGPUCompletion(pCompCmdQ, m_D3D12Wrap->GetFence(0));

	SAFE_RELEASE(_textureUploadHeap);

	return pTexture;
}

ID3D11ShaderResourceView* ComputeWrap::CreateTextureSRV(ID3D11Texture2D* pTexture)
{
	ID3D11ShaderResourceView* pSRV = NULL;

	D3D11_TEXTURE2D_DESC td;
	pTexture->GetDesc(&td);

	//init view description
	D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc; 
	ZeroMemory( &viewDesc, sizeof(viewDesc) ); 
	
	viewDesc.Format					= td.Format;
	viewDesc.ViewDimension			= D3D11_SRV_DIMENSION_TEXTURE2D;
	viewDesc.Texture2D.MipLevels	= td.MipLevels;

	if(FAILED(mD3DDevice->CreateShaderResourceView(pTexture, &viewDesc, &pSRV)))
	{
		//MessageBox(0, "Unable to create shader resource view", "Error!", 0);
	}

	return pSRV;
}

ID3D11UnorderedAccessView* ComputeWrap::CreateTextureUAV(ID3D11Texture2D* pTexture)
{
	ID3D11UnorderedAccessView* pUAV = NULL;

	mD3DDevice->CreateUnorderedAccessView( pTexture, NULL, &pUAV );
	pTexture->Release();

	return pUAV;
}

ID3D11Texture2D* ComputeWrap::CreateStagingTexture(ID3D11Texture2D* pTexture)
{
    ID3D11Texture2D* pStagingTex = NULL;

    D3D11_TEXTURE2D_DESC desc;
	pTexture->GetDesc(&desc);
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;
    
	mD3DDevice->CreateTexture2D(&desc, NULL, &pStagingTex);

    return pStagingTex;
}

unsigned char* ComputeWrap::LoadBitmapFileRGBA(TCHAR *filename, BITMAPINFOHEADER *bitmapInfoHeader)
{
	FILE* filePtr;
	BITMAPFILEHEADER bitmapFileHeader;
	unsigned char* bitmapImage;
	unsigned char* flippedBitmapImage;
	int imageIdx = 0;
	unsigned char tempRGB;

	//open filename in read binary mode
	_tfopen_s(&filePtr, filename,_T("rb"));
	if (filePtr == NULL)
		return NULL;

	//read the bitmap file header
	fread(&bitmapFileHeader, sizeof(BITMAPFILEHEADER),1,filePtr);

	//verify that this is a bmp file by check bitmap id ('B' and 'M')
	if (bitmapFileHeader.bfType != 0x4D42)
	{
		fclose(filePtr);
		return NULL;
	}

	//read the bitmap info header
	fread(bitmapInfoHeader, sizeof(BITMAPINFOHEADER),1,filePtr);

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
	fread(bitmapImage,bitmapInfoHeader->biSizeImage,1,filePtr);

	//make sure bitmap image data was read
	if (bitmapImage == NULL)
	{
		fclose(filePtr);
		return NULL;
	}

	//swap the r and b values to get RGB (bitmap is BGR)
	for (imageIdx = 0;imageIdx < (int)bitmapInfoHeader->biSizeImage;imageIdx+=3)
	{
		tempRGB = bitmapImage[imageIdx];
		bitmapImage[imageIdx] = bitmapImage[imageIdx + 2];
		bitmapImage[imageIdx + 2] = tempRGB;
	}

	//flip image
	for(int y = 0; y < bitmapInfoHeader->biHeight; y++)
	{
		unsigned char* src = &bitmapImage[(bitmapInfoHeader->biHeight - 1 - y) * bitmapInfoHeader->biWidth * 3];
		unsigned char* dst = &flippedBitmapImage[bitmapInfoHeader->biWidth * y * 4];
		for(int x = 0; x < bitmapInfoHeader->biWidth; x++)
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

ID3D11Buffer* ComputeWrap::CreateConstantBuffer(UINT uSize, VOID* pInitData, char* debugName)
{
	ID3D11Buffer* pBuffer = NULL;

	// setup creation information
	D3D11_BUFFER_DESC cbDesc;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.ByteWidth = uSize + (16 - uSize % 16);
	cbDesc.CPUAccessFlags = 0;
	cbDesc.MiscFlags = 0;
	cbDesc.StructureByteStride = 0;
	cbDesc.Usage = D3D11_USAGE_DEFAULT;

    if(pInitData)
    {
        D3D11_SUBRESOURCE_DATA InitData;
        InitData.pSysMem = pInitData;
        mD3DDevice->CreateBuffer(&cbDesc, &InitData, &pBuffer);
    }
	else
	{
        mD3DDevice->CreateBuffer(&cbDesc, NULL, &pBuffer);
	}

	if(debugName && pBuffer)
	{
		SetDebugName(pBuffer, debugName);
	}

	return pBuffer;
}

void ComputeWrap::SetDebugName(ID3D11DeviceChild* object, char* debugName)
{
#if defined( DEBUG ) || defined( _DEBUG )
	// Only works if device is created with the D3D10 or D3D11 debug layer, or when attached to PIX for Windows
	object->SetPrivateData( WKPDID_D3DDebugObjectName, (unsigned int)strlen(debugName), debugName );
#endif
}

ComputeShader* ComputeWrap::CreateComputeShader(TCHAR* shaderFile, char* blobFileAppendix, char* pFunctionName, D3D10_SHADER_MACRO* pDefines, bool dx12)
{
	ComputeShader* cs = new ComputeShader();

	if(cs && !cs->Init(
		shaderFile,
		blobFileAppendix,
		pFunctionName,
		pDefines,
		mD3DDevice,
		mD3DDeviceContext,
		dx12))
	{
		SAFE_DELETE(cs);
	}

	return cs;
}

