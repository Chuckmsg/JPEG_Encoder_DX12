//--------------------------------------------------------------------------------------
// Real-Time JPEG Compression using DirectCompute - Demo
//
// Copyright (c) Stefan Petersson 2012. All rights reserved.
//--------------------------------------------------------------------------------------
#include "SurfacePreparation.h"

SurfacePreparation::SurfacePreparation()
{
	mTextureWidth		= 0;
	mTextureHeight		= 0;
	mComputationWidth	= 0;
	mComputationHeight	= 0;

	mRescaleWidth		= 0;
	mRescaleHeight		= 0;

	mDevice				= NULL;
	mDeviceContext		= NULL;

	mVertexShader		= NULL;
	mPixelShader		= NULL;

	mCopyTextureGPU		= NULL;
	mCopyTextureCPU		= NULL;
	mCopySRV			= NULL;
	mRescaleRTV			= NULL;
	mRescaleTextureGPU	= NULL;
	mRescaleTextureCPU	= NULL;
	mRescaleSRV			= NULL;

	mMappedResource		= NULL;
}

SurfacePreparation::~SurfacePreparation()
{
	SAFE_RELEASE(mCopySRV);
	SAFE_RELEASE(mCopyTextureGPU);
	SAFE_RELEASE(mCopyTextureCPU);
	SAFE_RELEASE(mRescaleRTV);
	SAFE_RELEASE(mRescaleSRV);
	SAFE_RELEASE(mRescaleTextureGPU);
	SAFE_RELEASE(mRescaleTextureCPU);

//	SAFE_RELEASE(mDevice);
	SAFE_RELEASE(mVertexShader);
	SAFE_RELEASE(mPixelShader);
}

HRESULT SurfacePreparation::Init(ID3D11Device* device, ID3D11DeviceContext* deviceContext)
{
	mDevice = device;
	mDeviceContext = deviceContext;

	if(FAILED(CreateVertexShader()))
		return E_FAIL;

	if(FAILED(CreatePixelShader()))
		return E_FAIL;

	return S_OK;
}

HRESULT SurfacePreparation::InitRescaleTexture()
{
	// create a texture
	D3D11_TEXTURE2D_DESC texDesc;
	memset(&texDesc, 0, sizeof(texDesc));
	texDesc.Width = mRescaleWidth;
	texDesc.Height = mRescaleHeight;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;

	SAFE_RELEASE(mRescaleTextureGPU);
	if(FAILED(mDevice->CreateTexture2D(&texDesc, NULL, &mRescaleTextureGPU)))
	{
		MessageBoxA(0, "Unable to create RescaleTexture-GPU", "Creation error!", 0);
		return E_FAIL;
	}

	InitSRV(mRescaleTextureGPU, texDesc.Format, &mRescaleSRV);

	//create render target view from texture
	D3D11_RENDER_TARGET_VIEW_DESC DescRT;
	DescRT.Format = texDesc.Format;
	DescRT.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	DescRT.Texture2DArray.MipSlice = 0;

	SAFE_RELEASE(mRescaleRTV);
	if(FAILED(mDevice->CreateRenderTargetView(mRescaleTextureGPU, &DescRT, &mRescaleRTV)))
	{
		MessageBoxA(0, "Unable to create RescaleTexture-RT", "Creation error!", 0);
		return E_FAIL;
	}


	// create a texture
	texDesc.Usage = D3D11_USAGE_STAGING;
	texDesc.BindFlags = 0;
	texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

	SAFE_RELEASE(mRescaleTextureCPU);
	if(FAILED(mDevice->CreateTexture2D(&texDesc, NULL, &mRescaleTextureCPU)))
	{
		MessageBoxA(0, "Unable to create RescaleTexture-CPU", "Creation error!", 0);
		return E_FAIL;
	}

	return S_OK;
}

HRESULT SurfacePreparation::InitCopyTexture(int width, int height)
{
	memset(&mCopyDescription, 0, sizeof(mCopyDescription));
	mCopyDescription.Width = width;
	mCopyDescription.Height = height;
	mCopyDescription.MipLevels = 1;
	mCopyDescription.ArraySize = 1;
	mCopyDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	mCopyDescription.SampleDesc.Count = 1;
	mCopyDescription.Usage = D3D11_USAGE_DEFAULT;
	mCopyDescription.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	SAFE_RELEASE(mCopyTextureGPU);
	if(FAILED(mDevice->CreateTexture2D(&mCopyDescription, NULL, &mCopyTextureGPU)))
		return E_FAIL;

	// create a texture
	mCopyDescription.Usage = D3D11_USAGE_STAGING;
	mCopyDescription.BindFlags = 0;
	mCopyDescription.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

	SAFE_RELEASE(mCopyTextureCPU);
	if(FAILED(mDevice->CreateTexture2D(&mCopyDescription, NULL, &mCopyTextureCPU)))
	{
		MessageBoxA(0, "Unable to create CopyTextureGPU", "Creation error!", 0);
		return E_FAIL;
	}

	return S_OK;
}

HRESULT SurfacePreparation::InitSRV(ID3D11Texture2D* shaderResource, DXGI_FORMAT format, ID3D11ShaderResourceView** outSRV)
{
	if(shaderResource == NULL)
		return E_FAIL;

	if(*outSRV)
		SAFE_RELEASE(*outSRV);

	// Create the shader-resource view from the texture
	D3D11_SHADER_RESOURCE_VIEW_DESC srDesc;
	srDesc.Format = format; //DXGI_FORMAT_R8G8B8A8_UNORM;
	srDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srDesc.Texture2D.MostDetailedMip = 0;
	srDesc.Texture2D.MipLevels = 1;

	return mDevice->CreateShaderResourceView(shaderResource, &srDesc, outSRV);
}

void SurfacePreparation::RenderCopyToRescaleTarget()
{
	ID3D11ShaderResourceView* srv[] = { mCopySRV };
	mDeviceContext->PSSetShaderResources(0, 1, srv);

	mDeviceContext->VSSetShader(mVertexShader, NULL, 0);
	mDeviceContext->HSSetShader(NULL, NULL, 0);
	mDeviceContext->GSSetShader(NULL, NULL, 0);
	mDeviceContext->DSSetShader(NULL, NULL, 0);
	mDeviceContext->PSSetShader(mPixelShader, NULL, 0);

	mDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		
	UINT num = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
	D3D11_VIEWPORT tmpVP[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
	mDeviceContext->RSGetViewports(&num, tmpVP);

	ID3D11RenderTargetView* tmpRTV[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = { NULL };
	ID3D11DepthStencilView* tmpDSV = NULL;
	mDeviceContext->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, tmpRTV, &tmpDSV);
		
	ID3D11RenderTargetView* rtv[] = { mRescaleRTV };
	mDeviceContext->OMSetRenderTargets( 1, rtv, NULL);

    D3D11_VIEWPORT vp;
    vp.Width = (float)mRescaleWidth;
    vp.Height = (float)mRescaleHeight;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    mDeviceContext->RSSetViewports( 1, &vp );

	mDeviceContext->Draw(4, 0);

	//reset viewports and rendertargets
	mDeviceContext->RSSetViewports(D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE, tmpVP);
	mDeviceContext->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, tmpRTV, tmpDSV);

	//have to release all RTVs and DSV since refcount is incremented by OMGetRenderTargets
	SAFE_RELEASE(tmpDSV);
	for(int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
		SAFE_RELEASE(tmpRTV[i]);

	mDeviceContext->VSSetShader(NULL, NULL, 0);
	mDeviceContext->PSSetShader(NULL, NULL, 0);
}

void SurfacePreparation::Cleanup()
{
	SAFE_RELEASE(mVertexShader);
	SAFE_RELEASE(mPixelShader);

	SAFE_RELEASE(mRescaleRTV);
	SAFE_RELEASE(mRescaleSRV);
	SAFE_RELEASE(mRescaleTextureGPU);
	SAFE_RELEASE(mRescaleTextureCPU);

	SAFE_RELEASE(mCopySRV);
	SAFE_RELEASE(mCopyTextureGPU);
}

PreparedSurface SurfacePreparation::GetValidSurface(ID3D11Texture2D* texture, float outputScale)
{
	PreparedSurface result;

	if(texture)
	{
		D3D11_TEXTURE2D_DESC textureDesc;
		texture->GetDesc(&textureDesc);

		if(((textureDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE) > 0) && (textureDesc.SampleDesc.Count == 1))
		{
			SAFE_RELEASE(mCopyTextureGPU);
			HRESULT hr = InitSRV(texture, textureDesc.Format, &mCopySRV);
		}
		else
		{
			if(mCopyDescription.Width != textureDesc.Width || mCopyDescription.Height != textureDesc.Height)
			{
				if(FAILED(InitCopyTexture(textureDesc.Width, textureDesc.Height)))
				{
					result.SRV = NULL;
					return result;
				}

				InitSRV(mCopyTextureGPU, textureDesc.Format, &mCopySRV);
			}

			//check if texture is multisampled
			if(textureDesc.SampleDesc.Count > 1)
			{
				mDeviceContext->ResolveSubresource(mCopyTextureGPU, 0, texture, 0, textureDesc.Format);
			}
			else
			{
				mDeviceContext->CopyResource(mCopyTextureGPU, texture);
			}
		}

		if(outputScale != 1.0f)
		{
			int rescaledWidth = int(textureDesc.Width * outputScale);
			int rescaledHeight = int(textureDesc.Height * outputScale);
			
			if(mRescaleWidth != rescaledWidth || mRescaleHeight != rescaledHeight)
			{
				mRescaleWidth = rescaledWidth;
				mRescaleHeight = rescaledHeight;

				if(FAILED(InitRescaleTexture()))
				{
					result.SRV = NULL;
					return result;
				}
			}

			RenderCopyToRescaleTarget();

			result.SRV = mRescaleSRV;
			result.Width = rescaledWidth;
			result.Height = rescaledHeight;
		}
		else
		{
			result.SRV = mCopySRV;
			result.Width = textureDesc.Width;
			result.Height = textureDesc.Height;
		}
	}

	return result;
}

PreparedSurfaceBits SurfacePreparation::GetValidSurfaceBits(ID3D11Texture2D* texture, float outputScale)
{
	PreparedSurfaceBits result;
	if(texture)
	{
		D3D11_TEXTURE2D_DESC textureDesc;
		texture->GetDesc(&textureDesc);

		if(((textureDesc.Usage & D3D11_BIND_SHADER_RESOURCE) > 0) && (textureDesc.SampleDesc.Count == 1))
		{
			SAFE_RELEASE(mCopyTextureGPU);
			InitSRV(texture, textureDesc.Format, &mCopySRV);
		}
		else
		{
			if(mCopyDescription.Width != textureDesc.Width || mCopyDescription.Height != textureDesc.Height)
			{
				if(FAILED(InitCopyTexture(textureDesc.Width, textureDesc.Height)))
				{
					result.Bits = NULL;
					return result;
				}

				InitSRV(mCopyTextureGPU, textureDesc.Format, &mCopySRV);
			}

			//check if texture is multisampled
			if(textureDesc.SampleDesc.Count > 1)
			{
				mDeviceContext->ResolveSubresource(mCopyTextureGPU, 0, texture, 0, textureDesc.Format);
			}
			else
			{
				mDeviceContext->CopyResource(mCopyTextureGPU, texture);
			}
		}


		if(outputScale != 1.0f)
		{
			int rescaledWidth = int(textureDesc.Width * outputScale);
			int rescaledHeight = int(textureDesc.Height * outputScale);
			
			if(mRescaleWidth != rescaledWidth || mRescaleHeight != rescaledHeight)
			{
				mRescaleWidth = rescaledWidth;
				mRescaleHeight = rescaledHeight;

				if(FAILED(InitRescaleTexture()))
				{
					result.Bits = NULL;
					return result;
				}
			}

			RenderCopyToRescaleTarget();

			mDeviceContext->CopyResource(mRescaleTextureCPU, mRescaleTextureGPU);

			D3D11_MAPPED_SUBRESOURCE mapRes;
			if(SUCCEEDED(mDeviceContext->Map(mRescaleTextureCPU, D3D11CalcSubresource(0, 0, 1), D3D11_MAP_READ, 0, &mapRes)))
			{
				result.Bits = mapRes.pData;
				result.RowPitch = mapRes.RowPitch;
				result.Width = mRescaleWidth;
				result.Height = mRescaleHeight;

				mMappedResource = mRescaleTextureCPU;
			}
			else
			{
				result.Bits = NULL;
			}
		}
		else
		{
			mDeviceContext->CopyResource(mCopyTextureCPU, mCopyTextureGPU);

			D3D11_MAPPED_SUBRESOURCE mapRes;
			if(SUCCEEDED(mDeviceContext->Map(mCopyTextureCPU, D3D11CalcSubresource(0, 0, 1), D3D11_MAP_READ, 0, &mapRes)))
			{
				result.Bits = mapRes.pData;
				result.RowPitch = mapRes.RowPitch;
				result.Width = textureDesc.Width;
				result.Height = textureDesc.Height;

				mMappedResource = mCopyTextureCPU;
			}
			else
			{
				result.Bits = NULL;
			}
		}
	}

	return result;
}

void SurfacePreparation::Unmap()
{
	if(mMappedResource)
		mDeviceContext->Unmap(mMappedResource, D3D11CalcSubresource(0, 0, 1));
}

HRESULT SurfacePreparation::CreateVertexShader()
{
	char szVertexShader[] = "struct VS_OUT"
	"{"
	"    float4 pos    : SV_Position;"
	"    float2 tex    : TEXCOORD0;"
	"};"

	"VS_OUT main( uint vertexId : SV_VertexID )"
	"{"
	"	VS_OUT output = (VS_OUT)0;"
			
	"	if(vertexId == 3)"
	"		output.pos = float4(1.0, 1.0, 0.5, 1.0);"
	"	else if(vertexId == 2)"
	"		output.pos = float4(1.0, -1.0, 0.5, 1.0);"
	"	else if(vertexId == 1)"
	"		output.pos = float4(-1.0, 1.0, 0.5, 1.0);"
	"	else if(vertexId == 0)"
	"		output.pos = float4(-1.0, -1.0, 0.5, 1.0);"
		
	"	output.tex = float2(0.5f, -0.5f) * output.pos.xy + 0.5f.xx;"
		
	"	return output;"
	"}";
	
	DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
	ID3D10Blob* pShader = NULL;
	ID3D10Blob* pErrors = NULL;
	HRESULT hr = D3DCompile( szVertexShader, sizeof(szVertexShader), NULL,
		NULL, NULL, "main", "vs_4_0", dwShaderFlags, 0, &pShader, &pErrors );

	if(FAILED(hr))
	{
		MessageBoxA(0, (char*)pErrors->GetBufferPointer(), "VS Compile error!", 0);
	}

	// Create the vertex shader
	hr = mDevice->CreateVertexShader((DWORD*)pShader->GetBufferPointer(),
		pShader->GetBufferSize(), NULL, &mVertexShader );

	if(FAILED(hr))
	{
		MessageBoxA(0, "Unable to create vertex shader", "VS creation error!", 0);
	}

	// Release the pointer to the compiled shader once we are done with it
	if(pShader)
		pShader->Release();

	return hr;
}

HRESULT SurfacePreparation::CreatePixelShader()
{
	char szPixelShader[] = 
	"Texture2D txDiffuse : register( t0 );"
	"SamplerState samLinear"
	"{"
	"	Filter = MIN_MAG_MIP_LINEAR;"
	"	AddressU = Wrap;"
	"	AddressV = Wrap;"
	"};"
			
	"struct VS_OUT"
	"{"
	"    float4 pos    : SV_Position;"
	"    float2 tex    : TEXCOORD0;"
	"};"

	"float4 main( VS_OUT input ) : SV_Target"
	"{"
	//flip v-coord?
	"return float4(txDiffuse.Sample(samLinear, float2(input.tex.x, input.tex.y)).rgb, 1);"
	"}";

	DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
	ID3D10Blob* pShader = NULL;
	ID3D10Blob* pErrors = NULL;
	HRESULT hr = D3DCompile( szPixelShader, sizeof(szPixelShader), NULL,
		NULL, NULL, "main", "ps_4_0", dwShaderFlags, 0, &pShader, &pErrors );

	if(FAILED(hr))
	{
		MessageBoxA(0, (char*)pErrors->GetBufferPointer(), "PS Compile error!", 0);
	}

	// Create the pixel shader
	hr = mDevice->CreatePixelShader( (DWORD*)pShader->GetBufferPointer(),
		pShader->GetBufferSize(), NULL, &mPixelShader );

	if(FAILED(hr))
	{
		MessageBoxA(0, "Unable to create pixel shader", "PS creation error!", 0);
	}

	// Release the pointer to the compiled shader once we are done with it
	if(pShader)
		pShader->Release();

	return hr;
}

SurfacePreperationDX12::SurfacePreperationDX12()
{
}

SurfacePreperationDX12::~SurfacePreperationDX12()
{
}

HRESULT SurfacePreperationDX12::Init(D3D12Wrap * pD3D12Wrap)
{
	m_device = pD3D12Wrap->GetDevice();

	HRESULT hr = _compileVertexShader();
	if (FAILED(hr))
		return hr;

	hr = _compilePixelShader();
	if (FAILED(hr))
		return hr;

	hr = _createRootSignature();
	if (FAILED(hr))
		return hr;

	hr = _createPSO();
	if (FAILED(hr))
		return hr;

	hr = _createListAllocQueue();
	if (FAILED(hr))
		return hr;

	hr = _createFence();
	if (FAILED(hr))
		return hr;

	mRescaleWidth = 0;
	mRescaleHeight = 0;

	return hr;
}

void SurfacePreperationDX12::Cleanup()
{
	SAFE_RELEASE(m_vertexShaderCode);
	SAFE_RELEASE(m_pixelShaderCode);
	SAFE_RELEASE(m_PSO);
	SAFE_RELEASE(descHeap1);
	SAFE_RELEASE(descHeap2);

	SAFE_RELEASE(rtvDescHeap1);
	SAFE_RELEASE(rtvDescHeap2);
	SAFE_RELEASE(srvDescHeap1);
	SAFE_RELEASE(srvDescHeap2);

	SAFE_RELEASE(copyTexture1);
	SAFE_RELEASE(rtvTexture1);
	SAFE_RELEASE(copyTexture2);
	SAFE_RELEASE(rtvTexture2);

	SAFE_RELEASE(m_depthBuffer);
	SAFE_RELEASE(m_depthHeap);

	SAFE_RELEASE(m_commandList);
	SAFE_RELEASE(m_cmdAllocator);
	SAFE_RELEASE(m_commandQueue);
	SAFE_RELEASE(m_rootSignature);

	SAFE_RELEASE(m_fence.m_fence);
	CloseHandle(m_fence.m_fenceEvent);
}

DX12_PreparedSurface SurfacePreperationDX12::GetValidSurface(ID3D12Device * device, ID3D12Resource * texture, float outputScale)
{
	DX12_PreparedSurface result;

	if (shaderResource2 != texture && shaderResource1 == nullptr)
	{
		shaderResource1 = texture;
		SAFE_RELEASE(descHeap1);
		createSRV(device, texture, descHeap1);
	}
	else if (shaderResource1 != texture && shaderResource2 == nullptr)
	{
		shaderResource2 = texture;
		SAFE_RELEASE(descHeap2);
		createSRV(device, texture, descHeap2);
	}

	result.Width = texture->GetDesc().Width;
	result.Height = texture->GetDesc().Height;

	if (outputScale != 1.0f)
	{
		int rescaledWidth = int(result.Width * outputScale);
		int rescaledHeight = int(result.Height * outputScale);

		if (mRescaleWidth != rescaledWidth || mRescaleHeight != rescaledHeight)
		{
			mRescaleWidth = rescaledWidth;
			mRescaleHeight = rescaledHeight;

			SAFE_RELEASE(m_depthBuffer);
			SAFE_RELEASE(m_depthHeap);
			_createDepthBuffer(mRescaleWidth, mRescaleHeight, m_depthBuffer, m_depthHeap);
		}

		if (mRescaleWidth1 != mRescaleWidth || mRescaleHeight1 != mRescaleHeight ||
			mRescaleWidth2 != mRescaleWidth || mRescaleHeight2 != mRescaleHeight)
		{
			if (shaderResource1 == texture && (mRescaleWidth1 != mRescaleWidth || mRescaleHeight1 != mRescaleHeight))
			{
				mRescaleWidth1 = mRescaleWidth;
				mRescaleHeight1 = mRescaleHeight;

				SAFE_RELEASE(copyTexture1);
				SAFE_RELEASE(rtvTexture1);
				SAFE_RELEASE(srvDescHeap1);
				SAFE_RELEASE(rtvDescHeap1);
				if (FAILED(InitRescaleTexture(copyTexture1, rtvTexture1, texture->GetDesc().Format, srvDescHeap1, rtvDescHeap1)))
				{
					result.Heap = NULL;
					return result;
				}
			}
			if (shaderResource2 == texture && (mRescaleWidth2 != mRescaleWidth || mRescaleHeight2 != mRescaleHeight))
			{
				mRescaleWidth2 = mRescaleWidth;
				mRescaleHeight2 = mRescaleHeight;

				SAFE_RELEASE(copyTexture2);
				SAFE_RELEASE(rtvTexture2);
				SAFE_RELEASE(srvDescHeap2);
				SAFE_RELEASE(rtvDescHeap2);
				if (FAILED(InitRescaleTexture(copyTexture2, rtvTexture2, texture->GetDesc().Format, srvDescHeap2, rtvDescHeap2)))
				{
					result.Heap = NULL;
					return result;
				}
			}
		}
		
		result.Width = rescaledWidth;
		result.Height = rescaledHeight;
		if (shaderResource1 == texture)
		{
			render(rtvTexture1, rtvDescHeap1, descHeap1);

			copyTexture(copyTexture1, rtvTexture1);

			result.Heap = srvDescHeap1;
		}
		if (shaderResource2 == texture)
		{
			render(rtvTexture2, rtvDescHeap2, descHeap2);

			copyTexture(copyTexture2, rtvTexture2);

			result.Heap = srvDescHeap2;
		}
	}
	else
	{
		if (shaderResource1 == texture)
		{
			result.Heap = descHeap1;
		}
		if (shaderResource2 == texture)
		{
			result.Heap = descHeap2;
		}
	}

	return result;
}

HRESULT SurfacePreperationDX12::_createListAllocQueue()
{
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};
	commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	HRESULT hr = m_device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&m_commandQueue));
	if (FAILED(hr))
		return hr;
	m_commandQueue->SetName(L"Rescale commandQueue");

	hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_cmdAllocator));
	if (FAILED(hr))
		return hr;
	m_cmdAllocator->SetName(L"Rescale commandAllocator");

	hr = m_device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_cmdAllocator,
		nullptr,
		IID_PPV_ARGS(&m_commandList)
	);
	m_commandList->SetName(L"Rescale commandList");
	if (SUCCEEDED(hr))
		m_commandList->Close();

	return hr;
}

HRESULT SurfacePreperationDX12::_compileVertexShader()
{
	char szVertexShader[] = "struct VS_OUT"
		"{"
		"    float4 pos    : SV_Position;"
		"    float2 tex    : TEXCOORD0;"
		"};"

		"VS_OUT main( uint vertexId : SV_VertexID )"
		"{"
		"	VS_OUT output = (VS_OUT)0;"

		"	if(vertexId == 5)"
		"		output.pos = float4(1.0, 1.0, 0.5, 1.0);"
		"	else if(vertexId == 4)"
		"		output.pos = float4(-1.0, 1.0, 0.5, 1.0);"
		"	else if(vertexId == 3)"
		"		output.pos = float4(1.0, -1.0, 0.5, 1.0);"
		"	else if(vertexId == 2)"
		"		output.pos = float4(1.0, -1.0, 0.5, 1.0);"
		"	else if(vertexId == 1)"
		"		output.pos = float4(-1.0, 1.0, 0.5, 1.0);"
		"	else if(vertexId == 0)"
		"		output.pos = float4(-1.0, -1.0, 0.5, 1.0);"

		"	output.tex = float2(0.5f, -0.5f) * output.pos.xy + 0.5f.xx;"

		"	return output;"
		"}";

	ID3D10Blob* pErrors = NULL;
	HRESULT hr = D3DCompile(szVertexShader, sizeof(szVertexShader), NULL,
		NULL, NULL, "main", "vs_5_0", 0, 0, &m_vertexShaderCode, &pErrors);

	if (FAILED(hr))
	{
		MessageBoxA(0, (char*)pErrors->GetBufferPointer(), "VS Compile error!", 0);
	}

	return hr;
}

HRESULT SurfacePreperationDX12::_compilePixelShader()
{
	char szPixelShader[] =
		"Texture2D txDiffuse : register( t0 );"
		"SamplerState samLinear"
		"{"
		"	Filter = MIN_MAG_MIP_LINEAR;"
		"	AddressU = Wrap;"
		"	AddressV = Wrap;"
		"};"

		"struct VS_OUT"
		"{"
		"    float4 pos    : SV_Position;"
		"    float2 tex    : TEXCOORD0;"
		"};"

		"float4 main( VS_OUT input ) : SV_Target"
		"{"
		//flip v-coord?
		"return float4(txDiffuse.Sample(samLinear, float2(input.tex.x, input.tex.y)).rgb, 1);"
		"}";

	ID3D10Blob* pShader = NULL;
	ID3D10Blob* pErrors = NULL;
	HRESULT hr = D3DCompile(szPixelShader, sizeof(szPixelShader), NULL,
		NULL, NULL, "main", "ps_5_0", 0, 0, &m_pixelShaderCode, &pErrors);

	if (FAILED(hr))
	{
		MessageBoxA(0, (char*)pErrors->GetBufferPointer(), "PS Compile error!", 0);
	}



	return hr;
}

HRESULT SurfacePreperationDX12::_createPSO()
{
	HRESULT hr = S_OK;

	D3D12_RASTERIZER_DESC rsDesc = {};
	rsDesc.AntialiasedLineEnable = TRUE;
	rsDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	rsDesc.CullMode = D3D12_CULL_MODE_NONE;
	rsDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	rsDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	rsDesc.DepthClipEnable = FALSE;
	rsDesc.ForcedSampleCount = 0;
	rsDesc.FrontCounterClockwise = FALSE;
	rsDesc.MultisampleEnable = FALSE;
	rsDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	rsDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// Define the vertex input layout. BASED ON IA.h macros and Number of Vertex buffers for correct input slot
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }/*,
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }*/
	};

	//Define the blend state as default
	D3D12_BLEND_DESC blendDesc = {};
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
	{
		blendDesc.RenderTarget[i].BlendEnable = FALSE;
		blendDesc.RenderTarget[i].LogicOpEnable = FALSE;
		blendDesc.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
		blendDesc.RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;
		blendDesc.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
		blendDesc.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
		blendDesc.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
		blendDesc.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		blendDesc.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
		blendDesc.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	}

	D3D12_DEPTH_STENCIL_DESC dsDesc = {};
	dsDesc.DepthEnable = TRUE;
	dsDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	dsDesc.StencilEnable = FALSE;
	dsDesc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	dsDesc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp = {
		D3D12_STENCIL_OP_KEEP,
		D3D12_STENCIL_OP_KEEP,
		D3D12_STENCIL_OP_KEEP,
		D3D12_COMPARISON_FUNC_ALWAYS };
	dsDesc.FrontFace = defaultStencilOp;
	dsDesc.BackFace = defaultStencilOp;

	// Create compute pipeline state for the Y component
	D3D12_GRAPHICS_PIPELINE_STATE_DESC descPSO = {};
	descPSO.InputLayout = { nullptr, 0 };//{ inputElementDescs, _ARRAYSIZE(inputElementDescs) };
	descPSO.pRootSignature = m_rootSignature;
	descPSO.VS.pShaderBytecode = m_vertexShaderCode->GetBufferPointer();
	descPSO.VS.BytecodeLength = m_vertexShaderCode->GetBufferSize();
	descPSO.PS.pShaderBytecode = m_pixelShaderCode->GetBufferPointer();
	descPSO.PS.BytecodeLength = m_pixelShaderCode->GetBufferSize();
	descPSO.RasterizerState = rsDesc;
	descPSO.BlendState = blendDesc;
	descPSO.DepthStencilState = dsDesc;
	descPSO.SampleMask = UINT_MAX;
	descPSO.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	descPSO.NumRenderTargets = 1;
	descPSO.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	descPSO.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	descPSO.SampleDesc.Count = 1;
	
	hr = m_device->CreateGraphicsPipelineState(&descPSO, IID_PPV_ARGS(&m_PSO));
	if (FAILED(hr))
	{
		MessageBoxA(0, "Failed to create PSO in SurfacePreperationDX12!", "Error!", 0);
		exit(-1);
	}
	m_PSO->SetName(L"Rescale PSO");
	
	return hr;
}

HRESULT SurfacePreperationDX12::_createRootSignature()
{
	D3D12_DESCRIPTOR_RANGE descRanges[1];
	{
		{
			descRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			descRanges[0].NumDescriptors = 1;
			descRanges[0].BaseShaderRegister = 0;
			descRanges[0].RegisterSpace = 0;
			descRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		}
	}

	//Create necessary Descriptor Table
	D3D12_ROOT_DESCRIPTOR_TABLE descTables[1];
	{
		//1 Descriptor Table for the SRV descriptor = 1 DWORD
		{
			descTables[0].NumDescriptorRanges = _ARRAYSIZE(descRanges);
			descTables[0].pDescriptorRanges = &descRanges[0];
		}
	}

	//We only have defined one element to insert (SRV Descriptor table)
	D3D12_ROOT_PARAMETER rootParams[1];
	{
		// [1] - Descriptor table for SRV descriptor
		{
			rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParams[0].DescriptorTable = descTables[0];
			rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		}
	}

	// Create descriptor of static sampler
	D3D12_STATIC_SAMPLER_DESC sampler{};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.MipLODBias = 0;
	sampler.MaxAnisotropy = 0;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	sampler.MinLOD = 0.0f;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderRegister = 0;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	//Create the descriptions of the root signature
	D3D12_ROOT_SIGNATURE_DESC rsDesc;
	{
		rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
		rsDesc.NumParameters = _ARRAYSIZE(rootParams);
		rsDesc.pParameters = rootParams;
		rsDesc.NumStaticSamplers = 1;
		rsDesc.pStaticSamplers = &sampler;
	}

	//Serialize the root signature (no error blob)
	ID3DBlob * pSerBlob = NULL;
	D3D12SerializeRootSignature(
		&rsDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&pSerBlob,
		nullptr
	);

	UINT nodeMask = 0;
	return this->m_device->CreateRootSignature(nodeMask, pSerBlob->GetBufferPointer(), pSerBlob->GetBufferSize(), IID_PPV_ARGS(&this->m_rootSignature));
}

HRESULT SurfacePreperationDX12::_createFence()
{
	HRESULT hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence.m_fence));
	if (FAILED(hr))
		return hr;
	m_fence.m_fenceValue = 1;
	m_fence.m_fence->SetName(L"Rescale Fence");
	m_fence.m_fenceEvent = CreateEvent(0, false, false, 0);

	return hr;
}

HRESULT SurfacePreperationDX12::createSRV(ID3D12Device * device, ID3D12Resource * shaderResource, ID3D12DescriptorHeap*& outDescriptorHeap)
{
	D3D12_DESCRIPTOR_HEAP_DESC dhd = {};
	dhd.NumDescriptors = 1;
	dhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	dhd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	if (FAILED(device->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(&outDescriptorHeap))))
		return E_FAIL;
	outDescriptorHeap->SetName(L"Descriptor heap in EncoderJEnc");

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = shaderResource->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	device->CreateShaderResourceView(shaderResource, &srvDesc, outDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	return E_NOTIMPL;
}

HRESULT SurfacePreperationDX12::InitRescaleTexture(ID3D12Resource*& copyTexture, ID3D12Resource*& rtvTexture, DXGI_FORMAT format, ID3D12DescriptorHeap*& outDescriptorHeap, ID3D12DescriptorHeap*& rtvDescriptorHeap)
{
	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.MipLevels = 1;
	textureDesc.Format = format;
	textureDesc.Width = mRescaleWidth;
	textureDesc.Height = mRescaleWidth;
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

	if (FAILED(m_device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&copyTexture))))
	{
		return E_FAIL;
	}
	copyTexture->SetName(L"Rescale copyTexture");

	// Describe and create an SRV for the texture
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = textureDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_DESCRIPTOR_HEAP_DESC dhd = {};
	dhd.NumDescriptors = 1;
	dhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	dhd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	if (FAILED(m_device->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(&outDescriptorHeap))))
		return E_FAIL;
	outDescriptorHeap->SetName(L"Descriptor heap Rescale SRV");

	m_device->CreateShaderResourceView(copyTexture, &srvDesc, outDescriptorHeap->GetCPUDescriptorHandleForHeapStart());


	D3D12_RESOURCE_DESC textureDesc2 = {};
	textureDesc2.MipLevels = 1;
	textureDesc2.Format = format;
	textureDesc2.Width = mRescaleWidth;
	textureDesc2.Height = mRescaleWidth;
	textureDesc2.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	textureDesc2.DepthOrArraySize = 1;
	textureDesc2.SampleDesc.Count = 1;
	textureDesc2.SampleDesc.Quality = 0;
	textureDesc2.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureDesc2.Alignment = 0;
	textureDesc2.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	D3D12_HEAP_PROPERTIES heapProperties2 = {};
	heapProperties2.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProperties2.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties2.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties2.CreationNodeMask = 1;
	heapProperties2.VisibleNodeMask = 1;

	if (FAILED(m_device->CreateCommittedResource(
		&heapProperties2,
		D3D12_HEAP_FLAG_NONE,
		&textureDesc2,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		nullptr,
		IID_PPV_ARGS(&rtvTexture))))
	{
		return E_FAIL;
	}
	rtvTexture->SetName(L"Rescale rtvTexture");

	D3D12_DESCRIPTOR_HEAP_DESC dhd2 = {};
	dhd2.NumDescriptors = 1;
	dhd2.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	if (FAILED(m_device->CreateDescriptorHeap(&dhd2, IID_PPV_ARGS(&rtvDescriptorHeap))))
		return E_FAIL;
	rtvDescriptorHeap->SetName(L"Descriptor heap Rescale RTV");

	m_device->CreateRenderTargetView(rtvTexture, nullptr, rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	return S_OK;
}

void SurfacePreperationDX12::render(ID3D12Resource*& rtvTexture, ID3D12DescriptorHeap*& rtvDescriptorHeap, ID3D12DescriptorHeap*& textureDescriptorHeap)
{
	ThrowIfFailed(m_cmdAllocator->Reset());
	ThrowIfFailed(m_commandList->Reset(m_cmdAllocator, m_PSO));

	// Set necessary state.
	m_commandList->SetGraphicsRootSignature(m_rootSignature);

	D3D12_VIEWPORT vp;
	vp.Width = (float)mRescaleWidth;
	vp.Height = (float)mRescaleHeight;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	m_commandList->RSSetViewports(1, &vp);

	D3D12_RECT scissorRect;
	scissorRect.left = (long)vp.TopLeftX;
	scissorRect.top = (long)vp.TopLeftY;
	scissorRect.right = (long)vp.Width;
	scissorRect.bottom = (long)vp.Height;
	m_commandList->RSSetScissorRects(1, &scissorRect);

	m_commandList->OMSetRenderTargets(1, &rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), TRUE, &m_depthHeap->GetCPUDescriptorHandleForHeapStart());

	static const float clearColor[4] = { 1.0f, 1.0f, 0.0f, 1.0f };
	m_commandList->ClearRenderTargetView(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), clearColor, 0, nullptr);
	m_commandList->ClearDepthStencilView(m_depthHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_commandList->SetDescriptorHeaps(1, &textureDescriptorHeap);
	m_commandList->SetGraphicsRootDescriptorTable(0, textureDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	m_commandList->SetPipelineState(m_PSO);
	//m_commandList->IASetVertexBuffers(0, 1, &_vertexBuffer_View);

	// Record commands.
	m_commandList->DrawInstanced(6, 1, 0, 0);

	m_commandList->Close();

	ID3D12CommandList* listsToExecute[] = { m_commandList };
	m_commandQueue->ExecuteCommandLists(_ARRAYSIZE(listsToExecute), listsToExecute);

	WaitForGPUCompletion(m_commandQueue, &m_fence);
}

HRESULT SurfacePreperationDX12::_createDepthBuffer(float width, float height, ID3D12Resource *& shaderResource, ID3D12DescriptorHeap*& outDescriptorHeap)
{
	HRESULT hr = E_FAIL;

	// create a depth stencil descriptor heap so we can get a pointer to the depth stencil buffer
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	hr = m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&outDescriptorHeap));
	if (FAILED(hr))
		return hr;

	outDescriptorHeap->SetName(L"Rescale Depth Resource Heap");

	D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
	depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

	D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
	depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
	depthOptimizedClearValue.DepthStencil.Stencil = 0;

	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.MipLevels = 1;
	textureDesc.Format = DXGI_FORMAT_D32_FLOAT;
	textureDesc.Width = width;
	textureDesc.Height = height;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	textureDesc.DepthOrArraySize = 1;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	textureDesc.Alignment = 0;
	textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	hr = m_device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthOptimizedClearValue,
		IID_PPV_ARGS(&shaderResource)
	);

	if (FAILED(hr))
		return hr;

	shaderResource->SetName(L"Rescale DepthBufferResource");

	m_device->CreateDepthStencilView(shaderResource, &depthStencilDesc, outDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	return hr;
}

void SurfacePreperationDX12::copyTexture(ID3D12Resource *& destTexture, ID3D12Resource *& TextureToCopy)
{
	D3D12_RESOURCE_BARRIER barrier{};
	MakeResourceBarrier(
		barrier,
		D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
		TextureToCopy,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_COPY_SOURCE
	);

	m_cmdAllocator->Reset();
	m_commandList->Reset(m_cmdAllocator, nullptr);

	//Fill the cmd q with commands
	m_commandList->ResourceBarrier(1, &barrier);
	m_commandList->CopyResource(destTexture, TextureToCopy);

	D3D12_RESOURCE_BARRIER barrier2{};
	MakeResourceBarrier(
		barrier2,
		D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
		TextureToCopy,
		D3D12_RESOURCE_STATE_COPY_SOURCE,
		D3D12_RESOURCE_STATE_RENDER_TARGET
	);

	m_commandList->ResourceBarrier(1, &barrier2);

	//Close the q and execute it
	m_commandList->Close();
	ID3D12CommandList* ppCommandLists[] = { m_commandList };
	m_commandQueue->ExecuteCommandLists(_ARRAYSIZE(ppCommandLists), ppCommandLists);

	WaitForGPUCompletion(m_commandQueue, &m_fence);
}
