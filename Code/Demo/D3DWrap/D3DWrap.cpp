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
	hr = _createCommandQueues();
	if (FAILED(hr))
	{
		return hr;
	}
	//create command allocator and command list
	hr = _createCmdAllocatorsAndLists();
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
	hr = _createHeapsAndResources();
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

HRESULT D3D12Wrap::_createCommandQueues()
{
	//Description
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};
	commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	HRESULT hr = m_device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&m_directQ));
	if (FAILED(hr))
		return hr;

	commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
	hr = m_device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&m_copyQ));
	if (FAILED(hr))
		return hr;

	commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	return m_device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&m_computeQ));
}

HRESULT D3D12Wrap::_createCmdAllocatorsAndLists()
{
	HRESULT hr = E_FAIL;

	hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_directAllocator));

	if (FAILED(hr))
		return hr;

	hr = m_device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_directAllocator,
		nullptr,
		IID_PPV_ARGS(&m_gCmdList)
	);

	if (SUCCEEDED(hr))
		m_gCmdList->Close();

	hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&m_copyAllocator));

	if (FAILED(hr))
		return hr;

	hr = m_device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_COPY,
		m_copyAllocator,
		nullptr,
		IID_PPV_ARGS(&m_cpyCmdList)
	);

	if (SUCCEEDED(hr))
		m_cpyCmdList->Close();
	
	hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&m_computeAllocator));

	if (FAILED(hr))
		return hr;

	hr = m_device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_COMPUTE,
		m_computeAllocator,
		nullptr,
		IID_PPV_ARGS(&m_comCmdList)
	);

	if (SUCCEEDED(hr))
		m_comCmdList->Close();

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
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
	swapChainDesc.BufferCount = BUFFER_COUNT;
	swapChainDesc.Scaling = DXGI_SCALING_NONE;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	swapChainDesc.Flags = 0;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;


	hr = (*ppFactory)->CreateSwapChainForHwnd(
		m_directQ,
		hwnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		reinterpret_cast<IDXGISwapChain1**>(&m_swapChain)
	);

	return hr;
}

HRESULT D3D12Wrap::_createHeapsAndResources()
{
	HRESULT hr = E_FAIL;

	//Description for descriptor heap
	D3D12_DESCRIPTOR_HEAP_DESC dhd = {};
	dhd.NumDescriptors = BUFFER_COUNT;
	dhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

	hr = m_device->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(&m_rtvDescHeap));
	if (FAILED(hr))
		return hr;

	D3D12_DESCRIPTOR_HEAP_DESC heapDescriptorDesc = {};
	heapDescriptorDesc.NumDescriptors = BUFFER_COUNT;
	heapDescriptorDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDescriptorDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	hr = m_device->CreateDescriptorHeap(&heapDescriptorDesc, IID_PPV_ARGS(&m_uavHeap));
	if (FAILED(hr))
		return hr;

	heapDescriptorDesc.NumDescriptors = 1; //We only have one Bitmap Image to take care of
	hr = m_device->CreateDescriptorHeap(&heapDescriptorDesc, IID_PPV_ARGS(&m_srvHeap));
	if (FAILED(hr))
		return hr;

	hr = m_device->CreateDescriptorHeap(&heapDescriptorDesc, IID_PPV_ARGS(&m_cbvHeap)); //We only have one CB too AFAIK
	if (FAILED(hr))
		return hr;

	//Create resources for render targets
	m_rtvDescSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	D3D12_CPU_DESCRIPTOR_HANDLE cdh = m_rtvDescHeap->GetCPUDescriptorHandleForHeapStart();

	m_uavDescSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12_CPU_DESCRIPTOR_HANDLE uav_cdh = m_uavHeap->GetCPUDescriptorHandleForHeapStart();

	D3D12_HEAP_PROPERTIES  heapProp;
	ZeroMemory(&heapProp, sizeof(D3D12_HEAP_PROPERTIES));
	heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProp.CreationNodeMask = 0;
	heapProp.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC uavDesc;
	ZeroMemory(&uavDesc, sizeof(D3D12_RESOURCE_DESC));


	//one RTV for each swapchain buffer
	for (UINT i = 0; i < BUFFER_COUNT; i++)
	{
		hr = m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]));
		if (SUCCEEDED(hr))
		{
			m_device->CreateRenderTargetView(m_renderTargets[i], nullptr, cdh);
			cdh.ptr += m_rtvDescSize;

			uavDesc = m_renderTargets[i]->GetDesc();
			uavDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			//Since in DX12 you can no longer directly create a UAV for a backbuffer resource, we need to create new buffer resources for the UAVs
			//And then later we need to copy the backbuffer RTV to the UAV buffer before we can use it in the Compute Shader
			hr = m_device->CreateCommittedResource(
				&heapProp,
				D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES,
				&uavDesc,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				NULL,
				IID_PPV_ARGS(&m_UAVs[i])
			);
			if (SUCCEEDED(hr))
			{
				m_device->CreateUnorderedAccessView(m_UAVs[i], nullptr, nullptr, uav_cdh);
				uav_cdh.ptr += m_uavDescSize;
			}
		}
	}
	return hr;
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

	

	// Create descriptor of static sampler
	D3D12_STATIC_SAMPLER_DESC sampler{};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.MipLODBias = 0.0f;
	sampler.MaxAnisotropy = 16;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	sampler.MinLOD = -D3D11_FLOAT32_MAX;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderRegister = 0;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
}
