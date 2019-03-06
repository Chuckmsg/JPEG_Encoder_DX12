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