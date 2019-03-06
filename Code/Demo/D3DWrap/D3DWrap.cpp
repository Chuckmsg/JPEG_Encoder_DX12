//--------------------------------------------------------------------------------------
// Real-Time JPEG Compression using DirectCompute - Demo
//
// Copyright (c) Stefan Petersson 2012. All rights reserved.
//--------------------------------------------------------------------------------------
#include "D3DWrap.h"

D3DWrap::D3DWrap()
{
	mSwapChain			= NULL;
	mBackBuffer			= NULL;
	mRenderTargetView	= NULL;
	mBackbufferUAV		= NULL;
	mDevice				= NULL;
	mDeviceContext		= NULL;
}

D3DWrap::~D3DWrap()
{

}

HRESULT D3DWrap::Init(HWND hwnd, int width, int height)
{
	HRESULT hr = S_OK;

	UINT createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_DRIVER_TYPE driverType;

	D3D_DRIVER_TYPE driverTypes[] = 
	{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_REFERENCE,
	};
	UINT numDriverTypes = sizeof(driverTypes) / sizeof(driverTypes[0]);

	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory( &sd, sizeof(sd) );
	sd.BufferCount = 1;
	sd.BufferDesc.Width = width;
	sd.BufferDesc.Height = height;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_UNORDERED_ACCESS;
	sd.OutputWindow = hwnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;

	D3D_FEATURE_LEVEL featureLevel;

	for( UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++ )
	{
		driverType = driverTypes[driverTypeIndex];
		if(SUCCEEDED(hr = D3D11CreateDeviceAndSwapChain(
			NULL,
			driverType,
			NULL,
			createDeviceFlags,
			NULL,
			NULL,
			D3D11_SDK_VERSION,
			&sd,
			&mSwapChain,
			&mDevice,
			&featureLevel,
			&mDeviceContext)))
		{
			break;
		}
	}

	if( SUCCEEDED(hr) )
	{
		// Create a render target view
		if(FAILED(hr = mSwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), (LPVOID*)&mBackBuffer )))
			return hr;

		// create shader unordered access view on back buffer for compute shader to write into texture
		if(FAILED(hr = mDevice->CreateUnorderedAccessView( mBackBuffer, NULL, &mBackbufferUAV )))
			return hr;

		if(FAILED(hr = mDevice->CreateRenderTargetView( mBackBuffer, NULL, &mRenderTargetView )))
			return hr;

		//ResetTargets();

		// Setup the viewport
		D3D11_VIEWPORT vp;
		vp.Width = (float)width;
		vp.Height = (float)height;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = 0;
		vp.TopLeftY = 0;
		mDeviceContext->RSSetViewports( 1, &vp );
	}

	return hr;
}

void D3DWrap::ResetTargets()
{
	mDeviceContext->OMSetRenderTargets( 1, &mRenderTargetView, NULL );
}

void D3DWrap::Cleanup()
{
	//force to free up 0 ref objects
	mDeviceContext->ClearState();
	mDeviceContext->Flush();

	SAFE_RELEASE(mBackbufferUAV);
	SAFE_RELEASE(mSwapChain);
	SAFE_RELEASE(mBackBuffer);
	SAFE_RELEASE(mRenderTargetView);
	SAFE_RELEASE(mDeviceContext);
	SAFE_RELEASE(mDevice);
}

ID3D11Texture2D* D3DWrap::GetBackBuffer()
{
	return mBackBuffer;
}

ID3D11Device* D3DWrap::GetDevice()
{
	return mDevice;
}

ID3D11DeviceContext* D3DWrap::GetDeviceContext()
{
	return mDeviceContext;
}

ID3D11UnorderedAccessView* D3DWrap::GetBackBufferUAV()
{
	return mBackbufferUAV;
}

void D3DWrap::Clear(float r, float g, float b, float a)
{
	//clear render target
	static float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	GetDeviceContext()->ClearRenderTargetView( mRenderTargetView, ClearColor );
}

HRESULT D3DWrap::Present()
{
	return mSwapChain->Present( 0, 0 );
}