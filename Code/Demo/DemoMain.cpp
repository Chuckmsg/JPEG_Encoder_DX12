//--------------------------------------------------------------------------------------
// File: TemplateMain.cpp
//
// BTH-D3D-Tesselation Demo
//
// Copyright (c) Stefan Petersson 2011. All rights reserved.
//--------------------------------------------------------------------------------------
#include "stdafx.h"

#include "D3DWrap\D3DWrap.h"
#include "../Shared/ComputeShader.h"
#include "JEncWrap\MJPEG.h"
#include "Encoders\EncoderJEnc.h"

DX12_EncoderJEnc* jencEncoder = nullptr;
SurfacePreperationDX12 gSurfacePrepDX12;

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
D3D11Wrap				gD3D;
SurfacePreparation		gSurfacePrep;
MotionJpeg				gMJPEG;

Encoder*				gEncJEnc			= NULL;

int						gLockedFrameRate	= 0;
float					gJpegQuality		= 85;
float					gOutputScale		= 1.0f;
CHROMA_SUBSAMPLE		gChromaSubsampling	= CHROMA_SUBSAMPLE_4_2_0;

ComputeWrap*			gComputeWrap		= NULL;
ComputeShader*			gBackbufferShader	= NULL;
ComputeTexture*			gTexture			= NULL;
ID3D11Buffer*			gConstantBuffer		= NULL;
ID3D11SamplerState*		gSamplerState		= NULL;

//DX12 Globals
bool DX12				= false;
D3D12Wrap				gD3D12;
ID3D12PipelineState * gBackBufferPipelineState = NULL;
float * cbData			= NULL;

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HWND	            InitWindow( HINSTANCE hInstance, int nCmdShow, int width, int height );
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
HRESULT				Render(float deltaTime, HWND hwnd);
HRESULT				Update(float deltaTime, HWND hwnd);
HRESULT				Cleanup();

HRESULT				InitDX12(HWND hwnd, unsigned int width, unsigned int height);
HRESULT				RenderDX12(float deltaTime, HWND hwnd);
void						PopulateComputeList(ID3D12GraphicsCommandList * pCompCmdList);
void						PopulateDirectList(ID3D12GraphicsCommandList * pDirectCmdList);
HRESULT				UpdateDX12(float deltaTime, HWND hwnd);
HRESULT				CreateBackBufferPSO();
//--------------------------------------------------------------------------------------
// Create Direct3D device and swap chain
//--------------------------------------------------------------------------------------
HRESULT Init(HWND hwnd, int width, int height)
{
	HRESULT hr = S_OK;

	int response = MessageBoxA(hwnd, "Run in DX12 Mode? Defaults to DX11 otherwise.\n", "API Mode", MB_YESNO | MB_SETFOREGROUND);

	if (response == IDYES)
	{
		if (SUCCEEDED(InitDX12(hwnd, width, height)))
		{
			//if (FAILED(gSurfacePrepDX12.Init(&gD3D12)))
			//	return E_FAIL;

			DX12 = true;

			gComputeWrap = new ComputeWrap(&gD3D12);

			//Compile shader code and create the Compute PSO
			gBackbufferShader = gComputeWrap->CreateComputeShader(_T("../Shaders/Backbuffer_CS.hlsl"), NULL, "main", NULL, DX12);
			hr = CreateBackBufferPSO();

			if (FAILED(hr))
			{
				_com_error err(hr);
				OutputDebugStringW(err.ErrorMessage());
			}
			//Create the texture that is sampled from in the Compute shader
			gTexture = gComputeWrap->CreateTextureFromBitmap(_T("../Images/ssc_800.bmp"), "TEXTURE", DX12);

			//Create the jpeg encoder here
			jencEncoder = myNew DX12_EncoderJEnc(&gSurfacePrepDX12);
			jencEncoder->Init(&gD3D12);
			
			//jencPtr = DX12_CreateJpegEncoderInstance(GPU_ENCODER, JENC_CHROMA_SUBSAMPLE_4_4_4, &gD3D12);

			//D3D12_RESOURCE_DESC& desc = gD3D12.GetBackBufferResource(0)->GetDesc();
			//testTexture = CreateTextureResource(desc.Format, desc.Width, desc.Height, 0, gD3D12.GetBackBufferResource(0)->);

			return S_OK;
		}
	}

	if(SUCCEEDED(hr = gD3D.Init(hwnd, width, height)))
	{
		if(FAILED(gSurfacePrep.Init(gD3D.GetDevice(), gD3D.GetDeviceContext())))
			return E_FAIL;

		gComputeWrap = new ComputeWrap(gD3D.GetDevice(), gD3D.GetDeviceContext());
		gBackbufferShader = gComputeWrap->CreateComputeShader(_T("../Shaders/Backbuffer_CS.hlsl"), NULL, "main", NULL);

		gTexture = gComputeWrap->CreateTextureFromBitmap(_T("../Images/ssc_800.bmp"), "TEXTURE");

		gConstantBuffer = gComputeWrap->CreateConstantBuffer(sizeof(float), NULL, "CB_EVERYFRAME");

		gEncJEnc = myNew EncoderJEnc(&gSurfacePrep);
		gEncJEnc->Init(&gD3D);

		//create sample with default values
		D3D11_SAMPLER_DESC samplerDesc;
		ZeroMemory(&samplerDesc, sizeof(samplerDesc));
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.MipLODBias = 0.0f;
		samplerDesc.MaxAnisotropy = 16;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		samplerDesc.MinLOD = -D3D11_FLOAT32_MAX;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;


		if(FAILED(gD3D.GetDevice()->CreateSamplerState(&samplerDesc, &gSamplerState)))
			return E_FAIL;
	}

	return hr;
}

HRESULT Cleanup()
{

	SAFE_RELEASE(gSamplerState);
	SAFE_RELEASE(gConstantBuffer);

	SAFE_DELETE(jencEncoder);
	SAFE_DELETE(gEncJEnc);
	SAFE_DELETE(gTexture);
	SAFE_DELETE(gBackbufferShader);
	SAFE_DELETE(gComputeWrap);

	if (DX12)
	{
		SAFE_RELEASE(gBackBufferPipelineState);
		gD3D12.Cleanup();
		gSurfacePrepDX12.Cleanup();
	}
	else
	{
		gD3D.Cleanup();
		gSurfacePrep.Cleanup();
	}

	return S_OK;
}

HRESULT Update(float deltaTime, HWND hwnd)
{
	static float timer = 0;

	if (!cbData)
		cbData = &timer;

	timer += deltaTime * 0.1f;

	if (!DX12)
		gD3D.GetDeviceContext()->UpdateSubresource(gConstantBuffer, 0, NULL, &timer, 0, 0);

	if(GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState(VK_ADD))
	{
		gJpegQuality += deltaTime * 10.0f;

		if(gJpegQuality > 100.0f)
			gJpegQuality = 100.0f;
	}

	if(GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState(VK_SUBTRACT))
	{
		gJpegQuality -= deltaTime * 10.0f;

		if(gJpegQuality < 1.0f)
			gJpegQuality = 1.0f;
	}

	if((GetAsyncKeyState(VK_LSHIFT) || GetAsyncKeyState(VK_RSHIFT)) && GetAsyncKeyState(VK_ADD))
	{
		gOutputScale += deltaTime;

		if(gOutputScale > 1.0f)
			gOutputScale = 1.0f;
	}

	if((GetAsyncKeyState(VK_LSHIFT) || GetAsyncKeyState(VK_RSHIFT)) && GetAsyncKeyState(VK_SUBTRACT))
	{
		gOutputScale -= deltaTime;

		if(gOutputScale < 0.1f)
			gOutputScale = 0.1f;
	}

	if(GetAsyncKeyState(VK_NUMPAD1))
		gChromaSubsampling = CHROMA_SUBSAMPLE_4_4_4;
	if(GetAsyncKeyState(VK_NUMPAD2))
		gChromaSubsampling = CHROMA_SUBSAMPLE_4_2_2;
	if(GetAsyncKeyState(VK_NUMPAD3))
		gChromaSubsampling = CHROMA_SUBSAMPLE_4_2_0;

	return S_OK;
}

HRESULT Render(float deltaTime, HWND hwnd)
{
	if (DX12)
		return RenderDX12(deltaTime, hwnd);
	//samplers for surface prep and backbuffer fill, ugly but hey :-)
	gD3D.GetDeviceContext()->PSSetSamplers(0, 1, &gSamplerState);
	gD3D.GetDeviceContext()->CSSetSamplers(0, 1, &gSamplerState);

	ID3D11UnorderedAccessView* uav[] = { gD3D.GetBackBufferUAV() };
	gD3D.GetDeviceContext()->CSSetUnorderedAccessViews(0, 1, uav, NULL);

	ID3D11Buffer* cb[] = { gConstantBuffer };
	gD3D.GetDeviceContext()->CSSetConstantBuffers(0, 1, cb);

	ID3D11ShaderResourceView* srv[] = { gTexture->GetResourceView() };
	gD3D.GetDeviceContext()->CSSetShaderResources(0, 1, srv);

	gBackbufferShader->Set();
	gD3D.GetDeviceContext()->Dispatch(25,25,1);
	gBackbufferShader->Unset();

	cb[0] = NULL;
	gD3D.GetDeviceContext()->CSSetConstantBuffers(0, 1, cb);

	uav[0] = NULL;
	gD3D.GetDeviceContext()->CSSetUnorderedAccessViews(0, 1, uav, NULL);

	EncodeResult res = gEncJEnc->Encode(gD3D.GetBackBuffer(), gChromaSubsampling, gOutputScale, (int)gJpegQuality);

	static int movieNum = 1;
	if(GetAsyncKeyState(VK_F2))
	{
		if(!gMJPEG.IsRecording())
		{
			char filename[100];
			sprintf_s(filename, sizeof(filename), "%s%d.avi", movieNum < 10 ? "00" : movieNum < 100 ? "0" : "", movieNum);

			movieNum++;
			gLockedFrameRate = 24;

			gMJPEG.StartRecording(filename, res.ImageWidth, res.ImageHeight, gLockedFrameRate);
		}
	}

	if(gMJPEG.IsRecording())
	{
		gMJPEG.AppendFrame(res.Bits, res.HeaderSize + res.DataSize);
	}

	if(GetAsyncKeyState(VK_F3))
	{
		if(gMJPEG.IsRecording())
		{
			gMJPEG.StopRecording();

			gLockedFrameRate = 0;
		}
	}

	//////////////////////////////////////////////////////
	static int imgNum = 1;
	static bool bthPressed = false;
	if(!bthPressed && GetAsyncKeyState(VK_F1))
	{
		char filename[100];
		sprintf_s(filename, sizeof(filename), "%s%d.jpg", imgNum < 10 ? "00" : imgNum < 100 ? "0" : "", imgNum);
		FILE* f = NULL;
		fopen_s(&f, filename, "wb");
		fwrite((char*)res.Bits, res.HeaderSize + res.DataSize, 1, f);
		fclose(f);

		imgNum++;
		bthPressed = true;
	}
	else if(!GetAsyncKeyState(VK_F1))
	{
		bthPressed = false;
	}

	TCHAR title[200];
	_stprintf_s(title, sizeof(title) / 2, _T("JPEG DirectCompute Demo | FPS: %.0f | Quality: %d | Output scale: %.2f | Subsampling: %s"),
		1.0f / deltaTime, (int)gJpegQuality, gOutputScale,
		gChromaSubsampling == CHROMA_SUBSAMPLE_4_4_4 ? _T("4:4:4") :
		gChromaSubsampling == CHROMA_SUBSAMPLE_4_2_2 ? _T("4:2:2") : 
		gChromaSubsampling == CHROMA_SUBSAMPLE_4_2_0 ? _T("4:2:0") : _T("Undefined"));
	SetWindowText(hwnd, title);

	gD3D.Present();

	return S_OK;
}

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
	myInitMemoryCheck();

	/*
	Some dual core systems have a problem where the different CPUs return different
	QueryPerformanceCounter values. So when this thread is schedule on the other CPU
	in a later frame, we could even get a negative frameTime. To solve this we force
	the main thread to always run on CPU 0.
	*/
	SetThreadAffinityMask(GetCurrentThread(), 1);

	HWND hwnd = 0;

	HRESULT hr = S_OK;

	if(hr != S_OK)
		return 0;

	if((hwnd = InitWindow(hInstance, nCmdShow, 800, 800)) == 0)
		return 0;

	if(FAILED(Init(hwnd, 800, 800)))
		return 0;

	__int64 cntsPerSec = 0;
	QueryPerformanceFrequency((LARGE_INTEGER*)&cntsPerSec);
	float secsPerCnt = 1.0f / (float)cntsPerSec;

	__int64 prevTimeStamp = 0;
	QueryPerformanceCounter((LARGE_INTEGER*)&prevTimeStamp);

	// Main message loop
	MSG msg = {0};
	int sleep = 0;
	while(WM_QUIT != msg.message)
	{
		if( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE) )
		{
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}
		else
		{
			__int64 currTimeStamp = 0;
			QueryPerformanceCounter((LARGE_INTEGER*)&currTimeStamp);
			float dt, fps;
			// calculate fps
			if(currTimeStamp>=prevTimeStamp)
			{
				fps = (float)((double)cntsPerSec/(double)(currTimeStamp-prevTimeStamp));	
				dt = (currTimeStamp - prevTimeStamp) * secsPerCnt;
			}
			else
			{
				fps = (float)((double)cntsPerSec/(double)(prevTimeStamp-currTimeStamp));
				dt = (prevTimeStamp - currTimeStamp) * secsPerCnt;
			}

			if(gLockedFrameRate > 0)
			{
				if((int)fps < gLockedFrameRate ) 
					sleep--;
				else if((int)fps > gLockedFrameRate ) 
					sleep++;

				// sleep cant be negative
				if(sleep < 0) sleep = 0;

				// sleep
				Sleep(sleep);
			}
			//render
			Update(dt, hwnd);
			Render(dt, hwnd);

			prevTimeStamp = currTimeStamp;
		}
	}

	Cleanup();

	return (int) msg.wParam;
}


//--------------------------------------------------------------------------------------
// Register class and create window
//--------------------------------------------------------------------------------------
HWND InitWindow( HINSTANCE hInstance, int nCmdShow, int width, int height )
{
	HWND hwnd = 0;

	// Register class
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX); 
	wcex.style          = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc    = WndProc;
	wcex.cbClsExtra     = 0;
	wcex.cbWndExtra     = 0;
	wcex.hInstance      = hInstance;
	wcex.hIcon          = 0;
	wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName   = NULL;
	wcex.lpszClassName  = L"DirectComputeJPEG";
	wcex.hIconSm        = 0;
	if( !RegisterClassEx(&wcex) )
		return NULL;

	// Create window
	RECT rc = { 0, 0, width, height };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_OVERLAPPEDWINDOW );

	if(!(hwnd = CreateWindowEx(
							WS_EX_OVERLAPPEDWINDOW,
							L"DirectComputeJPEG",
							L"GPU Pro 4 - JPEG Compression using DirectCompute - (Compiling shaders, please be patient...)",
							WS_OVERLAPPEDWINDOW,
							CW_USEDEFAULT,
							CW_USEDEFAULT,
							rc.right - rc.left,
							rc.bottom - rc.top,
							NULL,
							NULL,
							hInstance,
							NULL)))
	{
		return NULL;
	}

	ShowWindow( hwnd, nCmdShow );
	
	return hwnd;
}


//--------------------------------------------------------------------------------------
// Called every time the application receives a message
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message) 
	{
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_KEYDOWN:

		switch(wParam)
		{
			case VK_ESCAPE:
				PostQuitMessage(0);
				break;
		}
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

HRESULT InitDX12(HWND hwnd, unsigned int width, unsigned int height)
{
	return gD3D12.Init(hwnd, width, height);
}

HRESULT RenderDX12(float deltaTime, HWND hwnd)
{
	//Get the necessary object pointers 
		//Compute stuff
	ID3D12GraphicsCommandList * pCompCmdList = gD3D12.GetComputeCmdList();
	ID3D12CommandQueue * pCompCmdQ = gD3D12.GetComputeQueue();

		//Direct stuff
	ID3D12GraphicsCommandList * pDirectCmdList = gD3D12.GetDirectCmdList();
	ID3D12CommandQueue * pDirectCmdQ = gD3D12.GetDirectQueue();

	//Populate lists with commands
	PopulateComputeList(pCompCmdList);
	PopulateDirectList(pDirectCmdList);

	//Execute command lists // WE CAN EXECUTE THIS IN A THREAD SINCE WE ENSURE R/W ACCESS ON THE UAVs
	ID3D12CommandList* listsToExecute1[] = { pCompCmdList };
	pCompCmdQ->ExecuteCommandLists(1, listsToExecute1);
	gD3D12.WaitForGPUCompletion(pCompCmdQ, gD3D12.GetFence(0));

	ID3D12CommandList* listsToExecute2[] = { pDirectCmdList };
	pDirectCmdQ->ExecuteCommandLists(1, listsToExecute2);
	gD3D12.WaitForGPUCompletion(pDirectCmdQ, gD3D12.GetFence(1));
	//present the back buffer
	DXGI_PRESENT_PARAMETERS pp = {};
	HRESULT hr = gD3D12.GetSwapChain()->Present1(0, 0, &pp);
	
	// Encode
	EncodeResult res = jencEncoder->DX12_Encode(gD3D12.GetBackBufferResource(gD3D12.GetFrameIndex()), gTexture->bits, gChromaSubsampling, gOutputScale, (int)gJpegQuality);

	static int movieNum = 1;
	if (GetAsyncKeyState(VK_F2))
	{
		if (!gMJPEG.IsRecording())
		{
			char filename[100];
			sprintf_s(filename, sizeof(filename), "%s%d.avi", movieNum < 10 ? "00" : movieNum < 100 ? "0" : "", movieNum);

			movieNum++;
			gLockedFrameRate = 24;

			gMJPEG.StartRecording(filename, res.ImageWidth, res.ImageHeight, gLockedFrameRate);
		}
	}

	if (gMJPEG.IsRecording())
	{
		gMJPEG.AppendFrame(res.Bits, res.HeaderSize + res.DataSize);
	}

	if (GetAsyncKeyState(VK_F3))
	{
		if (gMJPEG.IsRecording())
		{
			gMJPEG.StopRecording();

			gLockedFrameRate = 0;
		}
	}

	//////////////////////////////////////////////////////
	static int imgNum = 1;
	static bool bthPressed = false;
	if (!bthPressed && GetAsyncKeyState(VK_F1))
	{
		char filename[100];
		sprintf_s(filename, sizeof(filename), "%s%d.jpg", imgNum < 10 ? "00" : imgNum < 100 ? "0" : "", imgNum);
		FILE* f = NULL;
		fopen_s(&f, filename, "wb");
		fwrite((char*)res.Bits, res.HeaderSize + res.DataSize, 1, f);
		fclose(f);

		imgNum++;
		bthPressed = true;
	}
	else if (!GetAsyncKeyState(VK_F1))
	{
		bthPressed = false;
	}

	TCHAR title[200];
	_stprintf_s(title, sizeof(title) / 2, _T("JPEG DirectCompute Demo | FPS: %.0f | Quality: %d | Output scale: %.2f | Subsampling: %s"),
		1.0f / deltaTime, (int)gJpegQuality, gOutputScale,
		gChromaSubsampling == CHROMA_SUBSAMPLE_4_4_4 ? _T("4:4:4") :
		gChromaSubsampling == CHROMA_SUBSAMPLE_4_2_2 ? _T("4:2:2") :
		gChromaSubsampling == CHROMA_SUBSAMPLE_4_2_0 ? _T("4:2:0") : _T("Undefined"));
	SetWindowText(hwnd, title);

	return hr;
}

void PopulateComputeList(ID3D12GraphicsCommandList * pCompCmdList)
{
	ID3D12CommandAllocator * pCompCmdAllo = gD3D12.GetComputeAllocator();
	ID3D12CommandQueue * pCompCmdQ = gD3D12.GetComputeQueue();
	ID3D12DescriptorHeap * pSRVHeap = gD3D12.GetSRVHeap();
	ID3D12DescriptorHeap * pUAVHeap = gD3D12.GetUAVHeap();

	UINT frameIndex = gD3D12.GetFrameIndex();
	auto uav_gdh = pUAVHeap->GetGPUDescriptorHandleForHeapStart();
	uav_gdh.ptr += gD3D12.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * frameIndex;

	pCompCmdAllo->Reset();
	pCompCmdList->Reset(pCompCmdAllo, gBackBufferPipelineState);
	//Set the Root Signature
	pCompCmdList->SetComputeRootSignature(gD3D12.GetRootSignature());

	//Set the SRV and UAV descriptor heaps
	ID3D12DescriptorHeap * srvHeap[] = { pSRVHeap };
	pCompCmdList->SetDescriptorHeaps(1, srvHeap);
	pCompCmdList->SetComputeRootDescriptorTable(0, pSRVHeap->GetGPUDescriptorHandleForHeapStart());
	ID3D12DescriptorHeap * uavHeap[] = { pUAVHeap };
	pCompCmdList->SetDescriptorHeaps(1, uavHeap);
	pCompCmdList->SetComputeRootDescriptorTable(1, uav_gdh);

	//Update the "constant buffer" 
	pCompCmdList->SetComputeRoot32BitConstants(2, 1, (void*)cbData, 0);
	//Set a Resource Barrier for the active UAV so that the copy queue waits until all operations are completed
	D3D12_RESOURCE_BARRIER barrier{}; //This barrier ensure that all R/W operations complete before any future R/W operations can begin
	MakeResourceBarrier(
		barrier,
		D3D12_RESOURCE_BARRIER_TYPE_UAV,
		gD3D12.GetUnorderedAccessResource(0),
		D3D12_RESOURCE_STATES(0), //UAV does not need transition states
		D3D12_RESOURCE_STATES(0)
	);
	//Everything is set now
	pCompCmdList->Dispatch(25, 25, 1);
	pCompCmdList->ResourceBarrier(1, &barrier);

	pCompCmdList->Close();
}

void PopulateDirectList(ID3D12GraphicsCommandList * pDirectCmdList)
{
	ID3D12CommandAllocator * pDirectCmdAllo = gD3D12.GetDirectAllocator();
	ID3D12CommandQueue * pDirectCmdQ = gD3D12.GetDirectQueue();

	UINT frameIndex = gD3D12.GetFrameIndex();

	pDirectCmdAllo->Reset();
	pDirectCmdList->Reset(pDirectCmdAllo, nullptr);
	pDirectCmdList->SetComputeRootSignature(gD3D12.GetRootSignature());

	//prepare the backbuffer for copy
	D3D12_RESOURCE_BARRIER cpyBarrierDest{};
	MakeResourceBarrier(
		cpyBarrierDest,
		D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
		gD3D12.GetBackBufferResource(frameIndex),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_COPY_DEST
	);
	pDirectCmdList->ResourceBarrier(1, &cpyBarrierDest);
	//Copy the uav resource to the backbuffer resource
	pDirectCmdList->CopyResource(gD3D12.GetBackBufferResource(frameIndex), gD3D12.GetUnorderedAccessResource(frameIndex));
	//Set the the last barrier so that the backbuffer is in present state
	D3D12_RESOURCE_BARRIER cpyBarrierDone{};
	MakeResourceBarrier(
		cpyBarrierDone,
		D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
		gD3D12.GetBackBufferResource(frameIndex),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_PRESENT
	);
	pDirectCmdList->ResourceBarrier(1, &cpyBarrierDone);

	pDirectCmdList->Close();
}

HRESULT UpdateDX12(float deltaTime, HWND hwnd)
{
	return E_NOTIMPL;
}

HRESULT CreateBackBufferPSO()
{
	ID3D10Blob * cs = gBackbufferShader->GetShaderCode();

	D3D12_COMPUTE_PIPELINE_STATE_DESC cpsDesc;
	ZeroMemory(&cpsDesc, sizeof(D3D12_COMPUTE_PIPELINE_STATE_DESC));

	cpsDesc.pRootSignature = gD3D12.GetRootSignature();
	cpsDesc.CS = { cs->GetBufferPointer(), cs->GetBufferSize() };
	cpsDesc.NodeMask = 0;
	cpsDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;


	return gD3D12.GetDevice()->CreateComputePipelineState(&cpsDesc, IID_PPV_ARGS(&gBackBufferPipelineState));

}
