//--------------------------------------------------------------------------------------
// Real-Time JPEG Compression using DirectCompute - Demo
//
// Copyright (c) Stefan Petersson 2012. All rights reserved.
//--------------------------------------------------------------------------------------
#include "EncoderJEnc.h"

#if defined( DEBUG ) || defined( _DEBUG )
#pragma comment(lib, "JEncD.lib")
#else
#pragma comment(lib, "JEnc.lib")
#endif

EncoderJEnc::EncoderJEnc(SurfacePreparation* surfacePrep)
	: Encoder(surfacePrep)	
{
	jEncoder444 = NULL;
	jEncoder422 = NULL;
	jEncoder420 = NULL;
}

EncoderJEnc::~EncoderJEnc()
{
	SAFE_DELETE(jEncoder444);
	SAFE_DELETE(jEncoder422);
	SAFE_DELETE(jEncoder420);
}

EncodeResult EncoderJEnc::Encode(	ID3D11Texture2D* textureResource,
									CHROMA_SUBSAMPLE subsampleType,
									float outputScale,
									int jpegQuality)
{
	EncodeResult result;
	PreparedSurface ps = surfacePreparation->GetValidSurface(textureResource, outputScale);
		
	JEncD3DDataDesc jD3D;
	jD3D.Width = ps.Width;
	jD3D.Height = ps.Height;
	jD3D.ResourceView = ps.SRV;

	VerifyDestinationBuffer(ps.Width, ps.Height);

	jD3D.TargetMemory = destinationBuffer;

	JEncResult jRes;
	
		 if(subsampleType == CHROMA_SUBSAMPLE_4_4_4)	jRes = jEncoder444->Encode(jD3D, jpegQuality);
	else if(subsampleType == CHROMA_SUBSAMPLE_4_2_2)	jRes = jEncoder422->Encode(jD3D, jpegQuality);
	else if(subsampleType == CHROMA_SUBSAMPLE_4_2_0)	jRes = jEncoder420->Encode(jD3D, jpegQuality);

	result.Bits = jRes.Bits;
	result.DataSize = jRes.DataSize;
	result.HeaderSize = jRes.HeaderSize;
	result.ImageWidth = ps.Width;
	result.ImageHeight = ps.Height;

	return result;
}

HRESULT EncoderJEnc::Init(D3D11Wrap* d3d)
{
	jEncoder444 = CreateJpegEncoderInstance(GPU_ENCODER, JENC_CHROMA_SUBSAMPLE_4_4_4, d3d->GetDevice(), d3d->GetDeviceContext());
	jEncoder422 = CreateJpegEncoderInstance(GPU_ENCODER, JENC_CHROMA_SUBSAMPLE_4_2_2, d3d->GetDevice(), d3d->GetDeviceContext());
	jEncoder420 = CreateJpegEncoderInstance(GPU_ENCODER, JENC_CHROMA_SUBSAMPLE_4_2_0, d3d->GetDevice(), d3d->GetDeviceContext());

	if(jEncoder444 && jEncoder422 && jEncoder420)
		return S_OK;

	return E_FAIL;
}

/*
	DX12 class
*/

DX12_EncoderJEnc::DX12_EncoderJEnc(SurfacePreperationDX12 * surfacePrep)
	: DX12_Encoder(surfacePrep)
{
	jEncoder444 = NULL;
	jEncoder422 = NULL;
	jEncoder420 = NULL;
}

DX12_EncoderJEnc::~DX12_EncoderJEnc()
{
	SAFE_DELETE(jEncoder444);
	SAFE_DELETE(jEncoder422);
	SAFE_DELETE(jEncoder420);
	SAFE_RELEASE(copyTexture);
}

EncodeResult DX12_EncoderJEnc::DX12_Encode(ID3D12Resource * textureResource, unsigned char* Data, CHROMA_SUBSAMPLE subsampleType, float outputScale, int jpegQuality)
{
	EncodeResult result;
	
	/*
	// Copy the intermediate render target to the cross-adapter shared resource.
	// Transition barriers are not required since there are fences guarding against
	// concurrent read/write access to the shared heap.
	if (m_crossAdapterTextureSupport)
	{
		// If cross-adapter row-major textures are supported by the adapter,
		// simply copy the texture into the cross-adapter texture.
		m_copyCommandList->CopyResource(m_crossAdapterResources[adapter][m_frameIndex].Get(), m_renderTargets[adapter][m_frameIndex].Get());
	}
	else
	{
		// If cross-adapter row-major textures are not supported by the adapter,
		// the texture will be copied over as a buffer so that the texture row
		// pitch can be explicitly managed.


		// Copy the intermediate render target into the shared buffer using the
		// memory layout prescribed by the render target.
		D3D12_RESOURCE_DESC renderTargetDesc = m_renderTargets[adapter][m_frameIndex]->GetDesc();
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT renderTargetLayout;


		m_devices[adapter]->GetCopyableFootprints(&renderTargetDesc, 0, 1, 0, &renderTargetLayout, nullptr, nullptr, nullptr);


		CD3DX12_TEXTURE_COPY_LOCATION dest(m_crossAdapterResources[adapter][m_frameIndex].Get(), renderTargetLayout);
		CD3DX12_TEXTURE_COPY_LOCATION src(m_renderTargets[adapter][m_frameIndex].Get(), 0);
		CD3DX12_BOX box(0, 0, m_width, m_height);


		m_copyCommandList->CopyTextureRegion(&dest, 0, 0, 0, &src, &box);
	}
	*/

	//JEncRGBDataDesc jD3D;
	//jD3D.Data = Data;
	//jD3D.Width = textureResource->GetDesc().Width;
	//jD3D.Height = textureResource->GetDesc().Height;
	//jD3D.RowPitch = textureResource->GetDesc().Width * 4;

	DX12_PreparedSurface ps = surfacePreparation->GetValidSurface(mD3D12Wrap->GetDevice(), textureResource, outputScale);
	DX12_JEncD3DDataDesc jD3D;
	jD3D.DescriptorHeap = ps.Heap;
	jD3D.Width = ps.Width;
	jD3D.Height = ps.Height;

	VerifyDestinationBuffer(jD3D.Width, jD3D.Height);

	jD3D.TargetMemory = destinationBuffer;

	JEncResult jRes;

	if (subsampleType == CHROMA_SUBSAMPLE_4_4_4)	jRes = jEncoder444->Encode(jD3D, jpegQuality);
	else if (subsampleType == CHROMA_SUBSAMPLE_4_2_2)	jRes = jEncoder422->Encode(jD3D, jpegQuality);
	else if (subsampleType == CHROMA_SUBSAMPLE_4_2_0)	jRes = jEncoder420->Encode(jD3D, jpegQuality);

	result.Bits = jRes.Bits;
	result.DataSize = jRes.DataSize;
	result.HeaderSize = jRes.HeaderSize;
	result.ImageWidth = jD3D.Width;
	result.ImageHeight = jD3D.Height;

	return result;
}

HRESULT DX12_EncoderJEnc::Init(D3D12Wrap * d3d)
{
	mD3D12Wrap = d3d;
	jEncoder444 = DX12_CreateJpegEncoderInstance(GPU_ENCODER, JENC_CHROMA_SUBSAMPLE_4_4_4, d3d);
	jEncoder422 = DX12_CreateJpegEncoderInstance(GPU_ENCODER, JENC_CHROMA_SUBSAMPLE_4_2_2, d3d);
	jEncoder420 = DX12_CreateJpegEncoderInstance(GPU_ENCODER, JENC_CHROMA_SUBSAMPLE_4_2_0, d3d);

	if (jEncoder444 && jEncoder422 && jEncoder420)
		return S_OK;

	return E_FAIL;
}

HRESULT DX12_EncoderJEnc::CreateTextureResource(D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle, ID3D12Resource * textureResource)
{
	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.MipLevels = 1;
	textureDesc.Format = textureResource->GetDesc().Format;
	textureDesc.Width = textureResource->GetDesc().Width;
	textureDesc.Height = textureResource->GetDesc().Height;
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

	if (FAILED(mD3D12Wrap->GetDevice()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&copyTexture))))
	{
		return E_FAIL;
	}

	// Describe and create an SRV for the texture
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = textureDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	mD3D12Wrap->GetDevice()->CreateShaderResourceView(copyTexture, &srvDesc, cpuDescHandle);

	return S_OK;
}

HRESULT DX12_EncoderJEnc::CopyTexture(ID3D12Resource * textureResource)
{
	//Get all the necessary D3D12 object pointers
	ID3D12CommandAllocator * pCmdAllo = mD3D12Wrap->GetDirectAllocator();
	ID3D12GraphicsCommandList * pCompCmdList = mD3D12Wrap->GetDirectCmdList();
	ID3D12CommandQueue * pCompCmdQ = mD3D12Wrap->GetDirectQueue();

	pCmdAllo->Reset();
	pCompCmdList->Reset(pCmdAllo, nullptr);

	//Fill the cmd q with commands
	pCompCmdList->CopyResource(copyTexture, textureResource);

	//Close the q and execute it
	pCompCmdList->Close();
	ID3D12CommandList* ppCommandLists[] = { pCompCmdList };
	pCompCmdQ->ExecuteCommandLists(_ARRAYSIZE(ppCommandLists), ppCommandLists);

	mD3D12Wrap->WaitForGPUCompletion(pCompCmdQ, mD3D12Wrap->GetFence(1));

	return S_OK;
}
