//--------------------------------------------------------------------------------------
// Real-Time JPEG Compression using DirectCompute - Demo
//
// Copyright (c) Stefan Petersson 2012. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "../stdafx.h"

struct PreparedSurface
{
	ID3D11ShaderResourceView*	SRV;
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

class SurfacePreperationDX12
{
public:
	SurfacePreperationDX12();
	~SurfacePreperationDX12();

	HRESULT Init(ID3D12Device * device, ID3D12RootSignature * pRoot);

	ID3D12PipelineState * GetPSO() { return m_pso; }

	void Cleanup();
private:
	ID3D12Device * m_device			= NULL;
	ID3D12RootSignature * m_root	= NULL;
	ID3D12PipelineState * m_pso		= NULL;

	ID3D10Blob * m_vertexShaderCode = NULL;
	ID3D10Blob * m_pixelShaderCode = NULL;

private:
	HRESULT _compileVertexShader();
	HRESULT _compilePixelShader();
	HRESULT _createPSO();
};