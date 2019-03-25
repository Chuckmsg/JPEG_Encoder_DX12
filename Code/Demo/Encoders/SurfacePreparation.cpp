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
	m_root = pD3D12Wrap->GetRootSignature();
	m_D3D12Wrap = pD3D12Wrap;

	HRESULT hr = _compileVertexShader();
	if (FAILED(hr))
		return hr;

	hr = _compilePixelShader();
	if (FAILED(hr))
		return hr;

	hr = _createPSO();

	return hr;
}

void SurfacePreperationDX12::Cleanup()
{
	SAFE_RELEASE(m_vertexShaderCode);
	SAFE_RELEASE(m_pixelShaderCode);
	SAFE_RELEASE(m_pso);
}

DX12_PreparedSurface SurfacePreperationDX12::GetValidSurface(ID3D12Device * device, ID3D12Resource * texture, float outputScale)
{
	DX12_PreparedSurface result;
	
	//SAFE_RELEASE(m_heap);
	//InitSRV(texture, texture->GetDesc().Format, m_heap);

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
	//Description for descriptor heap
	D3D12_DESCRIPTOR_HEAP_DESC dhd = {};
	dhd.NumDescriptors = 1;
	dhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	device->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(&m_heap));
	m_heap->SetName(L"Testing resource HEAP for JPeg Encoder");
	InitSRV(device, texture, texture->GetDesc().Format, m_heap);
	
	//m_heap = m_D3D12Wrap->GetSRVHeap();
	result.Width = texture->GetDesc().Width;
	result.Height = texture->GetDesc().Height;
	result.Heap = m_heap;

	return result;
	/*if (texture)
	{
		//D3D11_TEXTURE2D_DESC textureDesc;
		D3D12_RESOURCE_DESC textureDesc;
		textureDesc = texture->GetDesc();
		
		if (textureDesc.SampleDesc.Count == 1)
		{
			SAFE_RELEASE(mCopyTextureGPU);
			HRESULT hr = InitSRV(texture, textureDesc.Format, &mCopySRV);
		}
		else
		{
			if (mCopyDescription.Width != textureDesc.Width || mCopyDescription.Height != textureDesc.Height)
			{
				if (FAILED(InitCopyTexture(textureDesc.Width, textureDesc.Height)))
				{
					result.Heap = NULL;
					return result;
				}

				InitSRV(mCopyTextureGPU, textureDesc.Format, &mCopySRV);
			}

			//check if texture is multisampled
			if (textureDesc.SampleDesc.Count > 1)
			{
				mDeviceContext->ResolveSubresource(mCopyTextureGPU, 0, texture, 0, textureDesc.Format);
			}
			else
			{
				mDeviceContext->CopyResource(mCopyTextureGPU, texture);
			}
		}

		if (outputScale != 1.0f)
		{
			int rescaledWidth = int(textureDesc.Width * outputScale);
			int rescaledHeight = int(textureDesc.Height * outputScale);

			if (mRescaleWidth != rescaledWidth || mRescaleHeight != rescaledHeight)
			{
				mRescaleWidth = rescaledWidth;
				mRescaleHeight = rescaledHeight;

				if (FAILED(InitRescaleTexture()))
				{
					result.Heap = NULL;
					return result;
				}
			}

			RenderCopyToRescaleTarget();

			result.Heap = mRescaleSRV;
			result.Width = rescaledWidth;
			result.Height = rescaledHeight;
		}
		else
		{
			result.Heap = mCopySRV;
			result.Width = textureDesc.Width;
			result.Height = textureDesc.Height;
		}
	}

	return result;*/
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
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
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
	descPSO.InputLayout = { inputElementDescs, _ARRAYSIZE(inputElementDescs) };
	descPSO.pRootSignature = m_root;
	descPSO.VS.pShaderBytecode = m_vertexShaderCode->GetBufferPointer();
	descPSO.VS.BytecodeLength = m_vertexShaderCode->GetBufferSize();
	descPSO.PS.pShaderBytecode = m_pixelShaderCode->GetBufferPointer();
	descPSO.PS.BytecodeLength = m_pixelShaderCode->GetBufferSize();
	descPSO.RasterizerState = rsDesc;
	descPSO.BlendState = blendDesc;
	descPSO.DepthStencilState = dsDesc;
	descPSO.SampleMask = UINT_MAX;
	descPSO.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	descPSO.NumRenderTargets = 2;
	descPSO.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	descPSO.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
	descPSO.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	descPSO.SampleDesc.Count = 1;
	
	hr = m_device->CreateGraphicsPipelineState(&descPSO, IID_PPV_ARGS(&m_pso));
	if (FAILED(hr))
	{
		MessageBoxA(0, "Failed to create PSO in SurfacePreperationDX12!", "Error!", 0);
		exit(-1);
	}
	m_pso->SetName(L"Compute PSO Y Component");
	
	return hr;
}

HRESULT SurfacePreperationDX12::InitSRV(ID3D12Device * device, ID3D12Resource * shaderResource, DXGI_FORMAT format, ID3D12DescriptorHeap *& outDescriptorHeap)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC desc;
	desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	desc.Format = format;
	desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	desc.Texture2D.MostDetailedMip = 0;
	desc.Texture2D.MipLevels = 1;

	device->CreateShaderResourceView(shaderResource, &desc, outDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	return S_OK;
}
