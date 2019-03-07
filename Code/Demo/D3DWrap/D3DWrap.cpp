//--------------------------------------------------------------------------------------
// Real-Time JPEG Compression using DirectCompute - Demo
//
// Copyright (c) Stefan Petersson 2012. All rights reserved.
//--------------------------------------------------------------------------------------
#include "D3DWrap.h"

D3D11Wrap::D3D11Wrap()
{
	mSwapChain			= NULL;
	mBackBuffer			= NULL;
	mRenderTargetView	= NULL;
	mBackbufferUAV		= NULL;
	mDevice				= NULL;
	mDeviceContext		= NULL;
}

D3D11Wrap::~D3D11Wrap()
{

}

HRESULT D3D11Wrap::Init(HWND hwnd, int width, int height)
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

void D3D11Wrap::ResetTargets()
{
	mDeviceContext->OMSetRenderTargets( 1, &mRenderTargetView, NULL );
}

void D3D11Wrap::Cleanup()
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

ID3D11Texture2D* D3D11Wrap::GetBackBuffer()
{
	return mBackBuffer;
}

ID3D11Device* D3D11Wrap::GetDevice()
{
	return mDevice;
}

ID3D11DeviceContext* D3D11Wrap::GetDeviceContext()
{
	return mDeviceContext;
}

ID3D11UnorderedAccessView* D3D11Wrap::GetBackBufferUAV()
{
	return mBackbufferUAV;
}

void D3D11Wrap::Clear(float r, float g, float b, float a)
{
	//clear render target
	static float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	GetDeviceContext()->ClearRenderTargetView( mRenderTargetView, ClearColor );
}

HRESULT D3D11Wrap::Present()
{
	return mSwapChain->Present( 0, 0 );
}

HRESULT D3D12Wrap::Init(HWND hwnd, int width, int height)
{
	HRESULT hr = E_FAIL;
	//Debug layer
#ifdef _DEBUG
	hr = D3D12GetDebugInterface(IID_PPV_ARGS(&this->m_debugController));
	if (SUCCEEDED(hr))
		m_debugController->EnableDebugLayer();
	else
		OutputDebugStringA("\nFailed to create D3D12 Debug Interface\n");
#endif // _DEBUG

	//Find a suitable DX12 adapter
	IDXGIFactory5 * factory = NULL;
	IDXGIAdapter1 * adapter = _findDX12Adapter(&factory);

	//create our d3d device or a warp device if no suitable adapter was found
	hr = _createDevice(&factory, &adapter);
	if (FAILED(hr))
	{
		return hr;
	}

	//create command queue
	hr = _createCommandQueue();
	if (FAILED(hr))
	{
		return hr;
	}
	//create command allocator and command list
	hr = _createCmdAllocatorAndList();
	if (FAILED(hr))
	{
		return hr;
	}
	//create the swapchain
	hr = _createSwapChain(hwnd, width, height, &factory);
	if (FAILED(hr))
	{
		return hr;
	}
	//create the render targets
	hr = _createRenderTargets();
	if (FAILED(hr))
	{
		return hr;
	}
	//create resource heap
	hr = _createResourceDescHeap();
	if (FAILED(hr))
	{
		return hr;
	}
	//create the fence
	hr = _createFence();
	if (FAILED(hr))
	{
		return hr;
	}
	//create the root signature
	hr = _createRootSignature();
	if (FAILED(hr))
	{
		return hr;
	}

	SAFE_RELEASE(factory);
	SAFE_RELEASE(adapter);

	//Define the viewport and scissor rect
	{
		this->m_viewPort.TopLeftX = 0.f;
		this->m_viewPort.TopLeftY = 0.f;
		this->m_viewPort.MinDepth = 0.f;
		this->m_viewPort.MaxDepth = 1.f;
		this->m_viewPort.Width = (float)width;
		this->m_viewPort.Height = (float)height;

		this->m_scissorRect.left = (long)this->m_viewPort.TopLeftX;
		this->m_scissorRect.top = (long)this->m_viewPort.TopLeftY;
		this->m_scissorRect.right = (long)this->m_viewPort.Width;
		this->m_scissorRect.bottom = (long)this->m_viewPort.Height;
	}

	return hr;
}

IDXGIAdapter1 * D3D12Wrap::_findDX12Adapter(IDXGIFactory5 ** ppFactory)
{
	IDXGIAdapter1 * adapter = NULL;

	//Create the factory and iterate through adapters, find and return the first adapter that supports DX12
	if (!*ppFactory)
		CreateDXGIFactory(IID_PPV_ARGS(ppFactory));
	assert(*ppFactory);

	for (UINT index = 0;; ++index)
	{
		adapter = NULL;
		if (DXGI_ERROR_NOT_FOUND == (*ppFactory)->EnumAdapters1(index, &adapter))
			break;
		if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_1, __uuidof(ID3D12Device), nullptr)))
			break;

		SAFE_RELEASE(adapter);
	}

	return adapter;
}

HRESULT D3D12Wrap::_createDevice(IDXGIFactory5 ** ppFactory, IDXGIAdapter1 ** ppAdapter)
{
	HRESULT hr = E_FAIL;
	if (*ppAdapter)
	{
		hr = D3D12CreateDevice(*ppAdapter, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&this->m_device));
	}
	else
	{
		(*ppFactory)->EnumWarpAdapter(IID_PPV_ARGS(ppAdapter));
		hr = D3D12CreateDevice(*ppAdapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&this->m_device));
	}
	return hr;
}

HRESULT D3D12Wrap::_createCommandQueue()
{
	//Description
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};
	commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	return  m_device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&m_commandQueue));
}

HRESULT D3D12Wrap::_createCmdAllocatorAndList()
{
	HRESULT hr = E_FAIL;

	hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator));

	if (FAILED(hr))
		return hr;

	hr = m_device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_commandAllocator,
		nullptr,
		IID_PPV_ARGS(&m_gCommandList)
	);

	if (SUCCEEDED(hr))
		m_gCommandList->Close();

	return hr;
}

HRESULT D3D12Wrap::_createSwapChain(HWND hwnd, unsigned int width, unsigned int height, IDXGIFactory5 ** ppFactory)
{
	HRESULT hr = E_FAIL;

	//Description
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 1;
	swapChainDesc.Scaling = DXGI_SCALING_NONE;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.Flags = 0;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;


	hr = (*ppFactory)->CreateSwapChainForHwnd(
		m_commandQueue,
		hwnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		reinterpret_cast<IDXGISwapChain1**>(&m_swapChain)
	);

	return hr;
}

HRESULT D3D12Wrap::_createRenderTargets()
{
	HRESULT hr = E_FAIL;

	//Description for descriptor heap
	D3D12_DESCRIPTOR_HEAP_DESC dhd = {};
	dhd.NumDescriptors = 1;
	dhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

	hr = m_device->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(&m_rtvDescHeap));
	if (FAILED(hr))
		return hr;

	//Create resources for render targets
	m_rtvDescSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	D3D12_CPU_DESCRIPTOR_HANDLE cdh = m_rtvDescHeap->GetCPUDescriptorHandleForHeapStart();

	//one RTV for each swapchain buffer
	for (UINT i = 0; i < 1; i++)
	{
		hr = m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTarget));
		if (SUCCEEDED(hr))
		{
			m_device->CreateRenderTargetView(m_renderTarget, nullptr, cdh);
			cdh.ptr += m_rtvDescSize;
		}
	}
	return hr;
}

HRESULT D3D12Wrap::_createResourceDescHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC heapDescriptorDesc = {};
	heapDescriptorDesc.NumDescriptors = 1;
	heapDescriptorDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDescriptorDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	return m_device->CreateDescriptorHeap(&heapDescriptorDesc, IID_PPV_ARGS(&m_resourceHeap));
}

HRESULT D3D12Wrap::_createFence()
{
	HRESULT hr = E_FAIL;
	hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
	if (FAILED(hr))
		return hr;
	m_fenceValue = 1;

	m_fenceEvent = CreateEvent(0, false, false, 0);

	return hr;
}

HRESULT D3D12Wrap::_createRootSignature()
{
	return E_NOTIMPL;
}
