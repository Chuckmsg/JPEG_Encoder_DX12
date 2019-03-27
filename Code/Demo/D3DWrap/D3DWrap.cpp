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
	sd.BufferDesc.RefreshRate.Numerator = 0;
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
	hr = _createFences();
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

	return hr;
}

void D3D12Wrap::Cleanup()
{
#ifdef _DEBUG
	SAFE_RELEASE(m_debugController);
#endif // _DEBUG
	//Explicitly flush all work on the GPU
	WaitForGPUCompletion(m_computeQ, &Fences[0]);
	WaitForGPUCompletion(m_directQ, &Fences[1]);

	SAFE_RELEASE(m_device);
	SAFE_RELEASE(m_swapChain);
	SAFE_RELEASE(m_directQ);
	SAFE_RELEASE(m_computeQ);
	SAFE_RELEASE(m_directAllocator);
	SAFE_RELEASE(m_computeAllocator);
	SAFE_RELEASE(m_gCmdList);
	SAFE_RELEASE(m_comCmdList);
	SAFE_RELEASE(m_renderTargets[0]);
	SAFE_RELEASE(m_renderTargets[1]);
	SAFE_RELEASE(m_UAVs[0]);
	SAFE_RELEASE(m_UAVs[1]);
	SAFE_RELEASE(m_rootSignature);
	SAFE_RELEASE(m_rtvHeap);
	SAFE_RELEASE(m_uavHeap);
	SAFE_RELEASE(m_srvHeap);
	SAFE_RELEASE(Fences[0].m_fence);
	SAFE_RELEASE(Fences[1].m_fence);
	SAFE_RELEASE(TestFence.m_fence);

	CloseHandle(Fences[0].m_fenceEvent);
	CloseHandle(Fences[1].m_fenceEvent);
	CloseHandle(TestFence.m_fenceEvent);
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
		m_device->SetName(L"DX12_DEVICE");
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
	m_directQ->SetName(L"Direct Command Queue");

	commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	hr = m_device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&m_computeQ));
	m_computeQ->SetName(L"Compute Command Queue");
	return hr;
}

HRESULT D3D12Wrap::_createCmdAllocatorsAndLists()
{
	HRESULT hr = E_FAIL;

	hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_directAllocator));

	if (FAILED(hr))
		return hr;
	m_directAllocator->SetName(L"Direct Allocator");

	hr = m_device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_directAllocator,
		nullptr,
		IID_PPV_ARGS(&m_gCmdList)
	);

	if (SUCCEEDED(hr))
		m_gCmdList->Close();

	m_gCmdList->SetName(L"Direct Command List");

	hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&m_computeAllocator));

	if (FAILED(hr))
		return hr;
	m_computeAllocator->SetName(L"Compute Allocator");
	hr = m_device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_COMPUTE,
		m_computeAllocator,
		nullptr,
		IID_PPV_ARGS(&m_comCmdList)
	);

	if (SUCCEEDED(hr))
		m_comCmdList->Close();
	m_comCmdList->SetName(L"Compute Command List");

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

	hr = m_device->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(&m_rtvHeap));
	if (FAILED(hr))
		return hr;
	m_rtvHeap->SetName(L"RTV HEAP");

	D3D12_DESCRIPTOR_HEAP_DESC heapDescriptorDesc = {};
	heapDescriptorDesc.NumDescriptors = BUFFER_COUNT;
	heapDescriptorDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDescriptorDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	hr = m_device->CreateDescriptorHeap(&heapDescriptorDesc, IID_PPV_ARGS(&m_uavHeap));
	if (FAILED(hr))
		return hr;
	m_uavHeap->SetName(L"UAV HEAP");

	heapDescriptorDesc.NumDescriptors = 1; //We only have one Bitmap Image to take care of
	hr = m_device->CreateDescriptorHeap(&heapDescriptorDesc, IID_PPV_ARGS(&m_srvHeap));
	if (FAILED(hr))
		return hr;
	m_srvHeap->SetName(L"SRV HEAP");

	//Create resources for render targets
	UINT m_rtvDescSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	D3D12_CPU_DESCRIPTOR_HANDLE cdh = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

	UINT m_uavDescSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
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
		m_renderTargets[i]->SetName((std::wstring(L"BackBuffer_") + std::to_wstring(i)).c_str());
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
				D3D12_RESOURCE_STATE_COPY_SOURCE, //this state should never be changed since all we do with this is copying
				NULL,
				IID_PPV_ARGS(&m_UAVs[i])
			);
			if (SUCCEEDED(hr))
			{
				m_device->CreateUnorderedAccessView(m_UAVs[i], nullptr, nullptr, uav_cdh);
				uav_cdh.ptr += m_uavDescSize;
				m_UAVs[i]->SetName((std::wstring(L"UAV_") + std::to_wstring(i)).c_str());
			}
		}
	}
	return hr;
}


HRESULT D3D12Wrap::_createFences()
{
	HRESULT hr = E_FAIL;
	hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Fences[0].m_fence));
	if (FAILED(hr))
		return hr;
	Fences[0].m_fenceValue = 1;
	Fences[0].m_fence->SetName(L"Compute_Fence");
	Fences[0].m_fenceEvent = CreateEvent(0, false, false, 0);

	hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Fences[1].m_fence));
	if (FAILED(hr))
		return hr;
	Fences[1].m_fenceValue = 1;
	Fences[1].m_fence->SetName(L"Direct_Fence");
	Fences[1].m_fenceEvent = CreateEvent(0, false, false, 0);

	hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&TestFence.m_fence));
	if (FAILED(hr))
		return hr;
	TestFence.m_fenceValue = 1;
	TestFence.m_fence->SetName(L"Encoder Fence");
	TestFence.m_fenceEvent = CreateEvent(0, false, false, 0);

	return hr;
}

HRESULT D3D12Wrap::_createRootSignature()
{
	//Important things to identify: 
	// 1. How many root constants does this program need? (1 DWORD, 0 indirections)
	// 2. How many Root Descriptors does this program need/have (CBV, SRV, UAV)? (2 DWORDs, 1 indirection)
	// 3. How many Descriptor Tables do we need/want? (1 DWORD, 2 indirections)
	// 4. How many DWORDs did we use (Max size of 64, or 63 with enabled IA)

	//Start with defining descriptor ranges
	D3D12_DESCRIPTOR_RANGE descRanges[2];
	{
		//1 SRV descriptor = 2 DWORDs
		{
			descRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			descRanges[0].NumDescriptors = 1; //Only assmuing one texture so far
			descRanges[0].BaseShaderRegister = 0; // register t0
			descRanges[0].RegisterSpace = 0; //register(t0, space0);
			descRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			descRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
			descRanges[1].NumDescriptors = 1; //Only assmuing one UAV so far
			descRanges[1].BaseShaderRegister = 0; // register u0
			descRanges[1].RegisterSpace = 0; //register(u0, space0);
			descRanges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		}
	}
	//Create necessary Descriptor Tables
	D3D12_ROOT_DESCRIPTOR_TABLE descTables[2]; //"Only need one so far"?
	{

		//1 Descriptor Table for the SRV descriptor = 1 DWORD
		{
			descTables[0].NumDescriptorRanges = 1;//_ARRAYSIZE(descRanges); //how many descriptors for this table
			descTables[0].pDescriptorRanges = &descRanges[0]; //pointer to descriptor array
		}
		//1 Descriptor table for the UAV descriptor
		{
			descTables[1].NumDescriptorRanges = 1;//_ARRAYSIZE(descRanges); //how many descriptors for this table
			descTables[1].pDescriptorRanges = &descRanges[1]; //pointer to descriptor array
		}
	}

	//This is the constant buffer
	D3D12_ROOT_CONSTANTS rootConstants[1];
	rootConstants[0].Num32BitValues = 1;
	rootConstants[0].RegisterSpace = 0;
	rootConstants[0].ShaderRegister = 0;

	D3D12_ROOT_PARAMETER rootParams[3];
	{
		rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[0].DescriptorTable = descTables[0];
		rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[1].DescriptorTable = descTables[1];
		rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParams[2].Constants = rootConstants[0];
		rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		//Total of 3 DWORD
	}


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

	D3D12_ROOT_SIGNATURE_DESC rsDesc{};
	rsDesc.NumParameters = _ARRAYSIZE(rootParams); //How many entries?
	rsDesc.pParameters = rootParams; //Pointer to array of table entries
	rsDesc.NumStaticSamplers = 1;  //One static samplers were defined
	rsDesc.pStaticSamplers = &sampler; // The static sampler
	rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	//Serialize the root signature (no error blob)
	ID3DBlob * pSerBlob = NULL;
	ID3DBlob * pError = NULL;
	HRESULT hr = D3D12SerializeRootSignature(
		&rsDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&pSerBlob,
		&pError
	);

	if (FAILED(hr))
	{
		_com_error err(hr);
		OutputDebugStringW(err.ErrorMessage());
	}
	//Use d3d12 device to create the root signature
	UINT nodeMask = 0;
	
	return this->m_device->CreateRootSignature(nodeMask, pSerBlob->GetBufferPointer(), pSerBlob->GetBufferSize(), IID_PPV_ARGS(&this->m_rootSignature));
}

