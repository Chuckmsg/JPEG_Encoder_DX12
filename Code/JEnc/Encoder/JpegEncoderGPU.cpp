//--------------------------------------------------------------------------------------
// Real-Time JPEG Compression using DirectCompute - Demo
//
// Copyright (c) Stefan Petersson 2012. All rights reserved.
//--------------------------------------------------------------------------------------
#include "JpegEncoderGPU.h"

#include <tchar.h>

JpegEncoderGPU::JpegEncoderGPU(ID3D11Device* d3dDevice,	ID3D11DeviceContext* d3dContext)
	: mD3DDevice(d3dDevice), mD3DDeviceContext(d3dContext)
{
	_tcscpy_s(mComputeShaderFile, _T("../Shaders/Jpeg_CS.hlsl"));

	mCB_ImageData_Y = NULL;
	mCB_ImageData_CbCr = NULL;

	mCT_RGBA = NULL;

	mCB_EntropyResult = NULL;

	mCB_Huff_Y_AC = NULL;
	mCB_Huff_CbCr_AC = NULL;

	mCB_DCT_Matrix = NULL;
	mCB_DCT_Matrix_Transpose = NULL;
	mCB_Y_Quantization_Table = NULL;
	mCB_CbCr_Quantization_Table = NULL;

	mCB_SamplerState_PointClamp = NULL;

	mImageWidth = 0;
	mImageHeight = 0;
	mEntropyBlockSize = 0;

	mDoCreateBuffers = true;

	mComputeSys	= new ComputeWrap(d3dDevice, d3dContext);
	mShader_Y_Component = NULL;
	mShader_Cb_Component = NULL;
	mShader_Cr_Component = NULL;
}

JpegEncoderGPU::~JpegEncoderGPU()
{
	ReleaseQuantizationBuffers();
	ReleaseBuffers();
	ReleaseShaders();
}

void JpegEncoderGPU::ReleaseShaders()
{
	SAFE_DELETE(mComputeSys);
	SAFE_DELETE(mShader_Y_Component);
	SAFE_DELETE(mShader_Cb_Component);
	SAFE_DELETE(mShader_Cr_Component);
}

void JpegEncoderGPU::ReleaseBuffers()
{
	SAFE_RELEASE(mCB_ImageData_Y);
	SAFE_RELEASE(mCB_ImageData_CbCr);
	SAFE_DELETE(mCT_RGBA);

	SAFE_DELETE(mCB_EntropyResult);

	SAFE_DELETE(mCB_Huff_Y_AC);
	SAFE_DELETE(mCB_Huff_CbCr_AC);

	SAFE_DELETE(mCB_DCT_Matrix);
	SAFE_DELETE(mCB_DCT_Matrix_Transpose);

	SAFE_RELEASE(mCB_SamplerState_PointClamp);
}

void JpegEncoderGPU::ReleaseQuantizationBuffers()
{
	SAFE_DELETE(mCB_Y_Quantization_Table);
	SAFE_DELETE(mCB_CbCr_Quantization_Table);
}

void JpegEncoderGPU::DoHuffmanEncoding(int* DU, short& prevDC, BitString* HTDC)
{
	static const unsigned short mask[] = {1,2,4,8,16,32,64,128,256,512,1024,2048,4096,8192,16384,32768};
	static BitString bs;
    short tmp1, tmp2;

    // appeend DC bits
	tmp1 = tmp2 = (short)(DU[0] - prevDC);
    prevDC = DU[0];

	if(tmp1 < 0)
	{
		tmp1 = -tmp1;
		tmp2--;
	}

	//count numberof bits
	int nbits = NumBitsInUShort[tmp1];

	WriteBits(HTDC[nbits]);

	if(nbits)
    {
		bs.value = tmp2 & (mask[nbits] - 1); 
		bs.length = nbits;
		WriteBits(bs);
    }


	// append ac bits generated by the GPU
	BYTE* ac_entropy_data = (BYTE*)&DU[1];
	int num_ac_bits = DU[mEntropyBlockSize-1];
	
	bs.length = 8;
	int num_bytes = num_ac_bits / 8;
	while(num_bytes-- > 0)
	{
		bs.value = *ac_entropy_data++;
		WriteBits(bs);
	}

	//append last bits
	num_ac_bits = num_ac_bits % 8;
	if(num_ac_bits > 0)
	{
		bs.value = *ac_entropy_data++ >> (8 - num_ac_bits);
		bs.length = num_ac_bits;
		WriteBits(bs);
	}
}

void JpegEncoderGPU::Dispatch()
{
	ID3D11UnorderedAccessView* aUAVViews[] = { mCB_EntropyResult->GetUnorderedAccessView() };
	mD3DDeviceContext->CSSetUnorderedAccessViews( 0, sizeof(aUAVViews) / sizeof(aUAVViews[0]), aUAVViews, NULL );


	ID3D11ShaderResourceView* srv_Huffman_Y[] =	{ mCB_Y_Quantization_Table->GetResourceView(), mCB_Huff_Y_AC->GetResourceView() };
	mD3DDeviceContext->CSSetShaderResources( 3, 2, srv_Huffman_Y );

	mD3DDeviceContext->CSSetConstantBuffers(0, 1, &mCB_ImageData_Y);

	mShader_Y_Component->Set();
	mD3DDeviceContext->Dispatch( mNumComputationBlocks_Y[0], mNumComputationBlocks_Y[1], 1 );
	mShader_Y_Component->Unset();



	ID3D11ShaderResourceView* srv_Huffman_CbCr[] =	{ mCB_CbCr_Quantization_Table->GetResourceView(), mCB_Huff_CbCr_AC->GetResourceView() };
	mD3DDeviceContext->CSSetShaderResources( 3, 2, srv_Huffman_CbCr );

	mD3DDeviceContext->CSSetConstantBuffers(0, 1, &mCB_ImageData_CbCr);

	mShader_Cb_Component->Set();
	mD3DDeviceContext->Dispatch( mNumComputationBlocks_CbCr[0], mNumComputationBlocks_CbCr[1], 1 );
	mShader_Cb_Component->Unset();

	mShader_Cr_Component->Set();
	mD3DDeviceContext->Dispatch( mNumComputationBlocks_CbCr[0], mNumComputationBlocks_CbCr[1], 1 );
	mShader_Cr_Component->Unset();
}

void JpegEncoderGPU::QuantizationTablesChanged()
{
	for(int i = 0; i < 64; i++)
	{
		Y_Quantization_Table_Float[i] = (float)Y_Quantization_Table[i];
		CbCr_Quantization_Table_Float[i] = (float)CbCr_Quantization_Table[i];
	}

    D3D11_BOX box;
    box.left = 0;
    box.right = sizeof(float) * 64;
    box.top = 0;
    box.bottom = 1;
    box.front = 0;
    box.back = 1;


	ReleaseQuantizationBuffers();
	
	if(mCB_Y_Quantization_Table == NULL)
		mCB_Y_Quantization_Table = mComputeSys->CreateBuffer(COMPUTE_BUFFER_TYPE::STRUCTURED_BUFFER, sizeof(float), 64, true, false, Y_Quantization_Table_Float, false, "mCB_Y_Quantization_Table");
	else
		mD3DDeviceContext->UpdateSubresource(mCB_Y_Quantization_Table->GetResource(), 0, &box, Y_Quantization_Table_Float, 0, 0);

	if(mCB_CbCr_Quantization_Table == NULL)
		mCB_CbCr_Quantization_Table = mComputeSys->CreateBuffer(COMPUTE_BUFFER_TYPE::STRUCTURED_BUFFER, sizeof(float), 64, true, false, CbCr_Quantization_Table_Float, false, "mCB_CbCr_Quantization_Table");
	else
		mD3DDeviceContext->UpdateSubresource(mCB_CbCr_Quantization_Table->GetResource(), 0, &box, CbCr_Quantization_Table_Float, 0, 0);

	mDoCreateBuffers = true;
}

void JpegEncoderGPU::DoQuantization(ID3D11ShaderResourceView* pSRV)
{
	ID3D11ShaderResourceView* aRViews[] =
	{
		mCB_DCT_Matrix->GetResourceView(),
		mCB_DCT_Matrix_Transpose->GetResourceView(),
		pSRV ? pSRV : mCT_RGBA ? mCT_RGBA->GetResourceView() : NULL
	};

	mD3DDeviceContext->CSSetShaderResources( 0, 3, aRViews );

	ID3D11SamplerState* samplers[] = { mCB_SamplerState_PointClamp };
	mD3DDeviceContext->CSSetSamplers(0, 1, samplers);
	
	Dispatch();

	samplers[0] = NULL;
	mD3DDeviceContext->CSSetSamplers(0, 1, samplers);

	mD3DDeviceContext->CSSetShader( NULL, NULL, 0 );

	ID3D11UnorderedAccessView* ppUAViewNULL[8] = { NULL };
	mD3DDeviceContext->CSSetUnorderedAccessViews( 0, 8, ppUAViewNULL, NULL );

	ID3D11ShaderResourceView* ppSRVNULL[5] = { NULL };
	mD3DDeviceContext->CSSetShaderResources( 0, 5, ppSRVNULL );	
}

int JpegEncoderGPU::CalculateBufferSize(int quality)
{
	/*
		NOTE!!
		This calculation is not proved to work on all images.
		If the encoded image is broken, then please try other values here.
	*/
	if(quality < 12)
		quality = 12;
	
	int size = quality / 3 + 2;

	return size;
}

HRESULT JpegEncoderGPU::CreateBuffers()
{
	ReleaseBuffers();

	int width = mComputationWidthY;
	int height = mComputationHeightY;

	//recalculate buffer size
	mEntropyBlockSize = CalculateBufferSize(mQualitySetting);

	mCB_EntropyResult = mComputeSys->CreateBuffer(
		COMPUTE_BUFFER_TYPE::STRUCTURED_BUFFER,
		sizeof(int),
		(mNumComputationBlocks_Y[0] * mNumComputationBlocks_Y[1] + mNumComputationBlocks_CbCr[0] * mNumComputationBlocks_CbCr[1] * 2) * mEntropyBlockSize,
		false,
		true,
		NULL,
		true,
		"mCB_EntropyResult");

	mCB_Huff_Y_AC = mComputeSys->CreateBuffer(COMPUTE_BUFFER_TYPE::STRUCTURED_BUFFER, sizeof(BitString), 256, true, false,  Y_AC_Huffman_Table, false, "mCB_Huff_Y_AC");
	mCB_Huff_CbCr_AC = mComputeSys->CreateBuffer(COMPUTE_BUFFER_TYPE::STRUCTURED_BUFFER, sizeof(BitString), 256, true, false,  Cb_AC_Huffman_Table, false, "mCB_Huff_CbCr_AC");
		
	mCB_DCT_Matrix = mComputeSys->CreateBuffer(COMPUTE_BUFFER_TYPE::STRUCTURED_BUFFER, sizeof(float), 64, true, false, DCT_matrix, false, "mCB_DCT_Matrix");
	mCB_DCT_Matrix_Transpose = mComputeSys->CreateBuffer(COMPUTE_BUFFER_TYPE::STRUCTURED_BUFFER, sizeof(float), 64, true, false,  DCT_matrix_transpose, false, "mCB_DCT_Matrix_Transpose");
	
	ImageData id;
	id.ImageWidth = (float)mImageWidth;
	id.ImageHeight = (float)mImageHeight;
	id.EntropyBlockSize = mEntropyBlockSize;

	id.NumBlocksX = mNumComputationBlocks_Y[0];
	id.NumBlocksY = mNumComputationBlocks_Y[1];

	mCB_ImageData_Y = mComputeSys->CreateConstantBuffer(sizeof(ImageData), &id, "mCB_ImageData_Y");

	id.NumBlocksX = mNumComputationBlocks_CbCr[0];
	id.NumBlocksY = mNumComputationBlocks_CbCr[1];
	mCB_ImageData_CbCr = mComputeSys->CreateConstantBuffer(sizeof(ImageData), &id, "mCB_ImageData_CbCr");


	D3D11_SAMPLER_DESC samplerDesc;
	ZeroMemory(&samplerDesc, sizeof(samplerDesc));
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER; //ALWAYS;
	samplerDesc.MinLOD = 0; //-D3D11_FLOAT32_MAX;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;


	HRESULT hr = mD3DDevice->CreateSamplerState(&samplerDesc, &mCB_SamplerState_PointClamp);

	return S_OK;
}

void JpegEncoderGPU::ComputationDimensionsChanged()
{
	mDoCreateBuffers = true;
}

void JpegEncoderGPU::FinalizeData()
{
	//write any remaining bits to complete last block
	BitString bs;
	bs.length = 7;
	bs.value = 0;
	WriteBits(bs);

	//Write End of Image Marker
	WriteHex(0xFFD9);
}

void JpegEncoderGPU::WriteImageData(JEncRGBDataDesc rgbDataDesc)
{
	if(!mCT_RGBA)
	{
		mCT_RGBA = mComputeSys->CreateTexture(DXGI_FORMAT_R8G8B8A8_UNORM,
			rgbDataDesc.Width, rgbDataDesc.Height, rgbDataDesc.RowPitch, rgbDataDesc.Data);
	}

	if(mDoCreateBuffers)
	{
		CreateBuffers();
		mDoCreateBuffers = false;
	}

	DoQuantization(NULL);

	DoEntropyEncode();

	FinalizeData();
}

void JpegEncoderGPU::WriteImageData(JEncD3DDataDesc d3dDataDesc)
{
	if(mDoCreateBuffers)
	{
		CreateBuffers();
		mDoCreateBuffers = false;
	}

	DoQuantization(d3dDataDesc.ResourceView);

	DoEntropyEncode();

	FinalizeData();
}

/*
	JpegEncoderGPU for directx 12
	Christoffer �leskog 2019
*/

HRESULT DX12_JpegEncoderGPU::CreateBuffers()
{
	ReleaseBuffers();

	int width = mComputationWidthY;
	int height = mComputationHeightY;

	//recalculate buffer size
	mEntropyBlockSize = CalculateBufferSize(mQualitySetting);

	mCB_EntropyResult = mComputeSys->CreateBuffer(
		DX12_COMPUTE_BUFFER_TYPE::STRUCTURED_BUFFER,
		sizeof(int),
		(mNumComputationBlocks_Y[0] * mNumComputationBlocks_Y[1] + mNumComputationBlocks_CbCr[0] * mNumComputationBlocks_CbCr[1] * 2) * mEntropyBlockSize,
		false, // SRV
		true, // UAV
		NULL,
		true,
		"mCB_EntropyResult");

	mCB_Huff_Y_AC = mComputeSys->CreateBuffer(DX12_COMPUTE_BUFFER_TYPE::STRUCTURED_BUFFER, sizeof(BitString), 256, true, false, Y_AC_Huffman_Table, false, "mCB_Huff_Y_AC");
	mCB_Huff_CbCr_AC = mComputeSys->CreateBuffer(DX12_COMPUTE_BUFFER_TYPE::STRUCTURED_BUFFER, sizeof(BitString), 256, true, false, Cb_AC_Huffman_Table, false, "mCB_Huff_CbCr_AC");

	mCB_DCT_Matrix = mComputeSys->CreateBuffer(DX12_COMPUTE_BUFFER_TYPE::STRUCTURED_BUFFER, sizeof(float), 64, true, false, DCT_matrix, false, "mCB_DCT_Matrix");
	mCB_DCT_Matrix_Transpose = mComputeSys->CreateBuffer(DX12_COMPUTE_BUFFER_TYPE::STRUCTURED_BUFFER, sizeof(float), 64, true, false, DCT_matrix_transpose, false, "mCB_DCT_Matrix_Transpose");

	ImageData id;
	id.ImageWidth = (float)mImageWidth;
	id.ImageHeight = (float)mImageHeight;
	id.EntropyBlockSize = mEntropyBlockSize;

	id.NumBlocksX = mNumComputationBlocks_Y[0];
	id.NumBlocksY = mNumComputationBlocks_Y[1];

	mCB_ImageData_Y = mComputeSys->CreateConstantBuffer(mCB_ImageData_Y_Heap, sizeof(ImageData), &id, "mCB_ImageData_Y");

	id.NumBlocksX = mNumComputationBlocks_CbCr[0];
	id.NumBlocksY = mNumComputationBlocks_CbCr[1];
	mCB_ImageData_CbCr = mComputeSys->CreateConstantBuffer(mCB_ImageData_CbCr_Heap, sizeof(ImageData), &id, "mCB_ImageData_CbCr");


	D3D12_SAMPLER_DESC samplerDesc;
	ZeroMemory(&samplerDesc, sizeof(samplerDesc));
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; //ALWAYS;
	samplerDesc.MinLOD = 0; //-D3D11_FLOAT32_MAX;
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

	mD3DDevice->CreateSampler(&samplerDesc, mCB_SamplerState_PointClamp->GetCPUDescriptorHandleForHeapStart());
	
	return S_OK;
}

void DX12_JpegEncoderGPU::ComputationDimensionsChanged()
{
	mDoCreateBuffers = true;
}

void DX12_JpegEncoderGPU::QuantizationTablesChanged() // nr:1
{
	for (int i = 0; i < 64; i++)
	{
		Y_Quantization_Table_Float[i] = (float)Y_Quantization_Table[i];
		CbCr_Quantization_Table_Float[i] = (float)CbCr_Quantization_Table[i];
	}

	ReleaseQuantizationBuffers();

	if (mCB_Y_Quantization_Table == NULL)
		mCB_Y_Quantization_Table = mComputeSys->CreateBuffer(DX12_COMPUTE_BUFFER_TYPE::STRUCTURED_BUFFER, sizeof(float), 64, true, false, Y_Quantization_Table_Float, false, "mCB_Y_Quantization_Table");
	else
	{
		UpdateQuantizationTable(mCB_Y_Quantization_Table, Y_Quantization_Table_Float);
	}

	if (mCB_CbCr_Quantization_Table == NULL)
		mCB_CbCr_Quantization_Table = mComputeSys->CreateBuffer(DX12_COMPUTE_BUFFER_TYPE::STRUCTURED_BUFFER, sizeof(float), 64, true, false, CbCr_Quantization_Table_Float, false, "mCB_CbCr_Quantization_Table");
	else
	{
		UpdateQuantizationTable(mCB_Y_Quantization_Table, CbCr_Quantization_Table_Float);
	}
	

	mDoCreateBuffers = true;
}

int DX12_JpegEncoderGPU::CalculateBufferSize(int quality)
{
	/*
	NOTE!!
	This calculation is not proved to work on all images.
	If the encoded image is broken, then please try other values here.
	*/
	if (quality < 12)
		quality = 12;

	int size = quality / 3 + 2;

	return size;
}

HRESULT DX12_JpegEncoderGPU::createRootSignature()
{
	HRESULT hr = E_FAIL;

	/*
		descriptors:
		CBV		[mCB_ImageData_Y]				---|
		CBV		[mCB_ImageData_CbCr]			---| only take up one descriptor, register(b0)

		UAV		[mCB_EntropyResult]												  register(u0)

		SRV		[mCB_DCT_Matrix]												  register(t0)
		SRV		[mCB_DCT_Matrix_Transpose]										  register(t1)
		SRV		[mCT_RGBA or DX12_JEncD3DDataDesc]								  register(t2)
		SRV		[mCB_Y_Quantization_Table]		---|
		SRV		[mCB_CbCr_Quantization_Table]	---| only take up one descriptor, register(t3)
		SRV		[mCB_Huff_Y_AC]					---|
		SRV		[mCB_Huff_CbCr_AC]				---| only take up one descriptor, register(t4)

	*/

	// Descriptor range for sampler
	D3D12_DESCRIPTOR_RANGE descRangesSampler[1]; //Lets start with one
	{
		//Sampler descriptor
		{
			descRangesSampler[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
			descRangesSampler[0].NumDescriptors = 1; //Only one sampler
			descRangesSampler[0].BaseShaderRegister = 0; // register s0
			descRangesSampler[0].RegisterSpace = 0;
			descRangesSampler[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		}
	}

	//	Descriptor ranges for SRV, UAV and CBV
	D3D12_DESCRIPTOR_RANGE descRangesSRVUAV[3];
	{
		//CBV, b0
		{
			descRangesSRVUAV[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
			descRangesSRVUAV[0].NumDescriptors = 1;
			descRangesSRVUAV[0].BaseShaderRegister = 0;
			descRangesSRVUAV[0].RegisterSpace = 0;
			descRangesSRVUAV[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		}
		//UAV, u0
		{
			descRangesSRVUAV[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
			descRangesSRVUAV[1].NumDescriptors = 1;
			descRangesSRVUAV[1].BaseShaderRegister = 0;
			descRangesSRVUAV[1].RegisterSpace = 0;
			descRangesSRVUAV[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		}
		//SRV, t0 - t4
		{
			descRangesSRVUAV[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			descRangesSRVUAV[2].NumDescriptors = 5; // Do not know if this works
			descRangesSRVUAV[2].BaseShaderRegister = 0;
			descRangesSRVUAV[2].RegisterSpace = 0;
			descRangesSRVUAV[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		}
	}
	//Create necessary Descriptor Tables
	D3D12_ROOT_DESCRIPTOR_TABLE descTables[2];
	{
		// Descriptor Table for the sampler
		{
			descTables[0].NumDescriptorRanges = _ARRAYSIZE(descRangesSampler); //how many descriptors for this table
			descTables[0].pDescriptorRanges = &descRangesSampler[0]; //pointer to descriptor array
		}
		// Descriptor Table for the SRV, UAV and CBV descriptors
		{
			descTables[1].NumDescriptorRanges = _ARRAYSIZE(descRangesSRVUAV); //how many descriptors for this table
			descTables[1].pDescriptorRanges = &descRangesSRVUAV[0]; //pointer to descriptor array
		}
	}

	//Create the root parameters. Only two descriptor tables, one for sampler and the other for the SRV, UAV and CBV descriptors
	D3D12_ROOT_PARAMETER rootParams[2];
	{
		// [0] - Descriptor table for smpler descriptors
		{
			rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParams[0].DescriptorTable = descTables[0];
			rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		}
		// [1] - Descriptor table for SRV, UAV and CBV descriptors
		{
			rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParams[1].DescriptorTable = descTables[1];
			rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		}
	}


	//Create the descriptions of the root signature
	D3D12_ROOT_SIGNATURE_DESC rsDesc;
	{
		rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE; // Do not know if this is correct
		rsDesc.NumParameters = _ARRAYSIZE(rootParams);
		rsDesc.pParameters = rootParams;
		rsDesc.NumStaticSamplers = 0;
		rsDesc.pStaticSamplers = NULL;
	}

	//Serialize the root signature (no error blob)
	ID3DBlob * pSerBlob = NULL;
	D3D12SerializeRootSignature(
		&rsDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&pSerBlob,
		nullptr
	);

	//Use d3d12 device to create the root signature
	UINT nodeMask = 0;
	return mD3DDevice->CreateRootSignature(nodeMask, pSerBlob->GetBufferPointer(), pSerBlob->GetBufferSize(), IID_PPV_ARGS(&mRootSignature));
}

HRESULT DX12_JpegEncoderGPU::createAllocatorQueueList()
{
	// Create direct allocator, command queue and command list
	D3D12_COMMAND_QUEUE_DESC descDirectQueue = { D3D12_COMMAND_LIST_TYPE_DIRECT, 0, D3D12_COMMAND_QUEUE_FLAG_NONE };
	
	mD3DDevice->CreateCommandQueue(&descDirectQueue, IID_PPV_ARGS(&mDirectQueue));

	mD3DDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mDirectAllocator));

	mD3DDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		mDirectAllocator,
		mPSO_Y_Component,
		IID_PPV_ARGS(&mDirectList)
	);

	// Create copy allocator, command queue and command list
	D3D12_COMMAND_QUEUE_DESC descCopyQueue = { D3D12_COMMAND_LIST_TYPE_COPY, 0, D3D12_COMMAND_QUEUE_FLAG_NONE };

	mD3DDevice->CreateCommandQueue(&descCopyQueue, IID_PPV_ARGS(&mCopyQueue));

	mD3DDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&mCopyAllocator));

	mD3DDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_COPY,
		mCopyAllocator,
		mPSO_Y_Component,
		IID_PPV_ARGS(&mCopyList)
	);

	return E_NOTIMPL;
}

HRESULT DX12_JpegEncoderGPU::createPiplineStateObjects()
{
	HRESULT hr = S_OK;
	HWND wHnd = GetActiveWindow();
	assert(wHnd);

	// Create compute pipeline state for the Y component
	D3D12_COMPUTE_PIPELINE_STATE_DESC descComputePSO_Y = {};
	descComputePSO_Y.pRootSignature = mRootSignature;
	descComputePSO_Y.CS.pShaderBytecode = mShader_Y_Component->GetShaderCode().data();
	descComputePSO_Y.CS.BytecodeLength = mShader_Y_Component->GetShaderCode().size();

	hr = mD3DDevice->CreateComputePipelineState(&descComputePSO_Y, IID_PPV_ARGS(&mPSO_Y_Component));
	if (FAILED(hr))
	{
		PostMessageBoxOnError(hr, L"Failed to create compute PSO for the Y component: ", L"Fatal error", MB_ICONERROR, wHnd);
		exit(-1);
	}
	mPSO_Y_Component->SetName(L"Compute PSO Y Component");

	// Create compute pipeline state for the Cb component
	D3D12_COMPUTE_PIPELINE_STATE_DESC descComputePSO_Cb = {};
	descComputePSO_Cb.pRootSignature = mRootSignature;
	descComputePSO_Cb.CS.pShaderBytecode = mShader_Cb_Component->GetShaderCode().data();
	descComputePSO_Cb.CS.BytecodeLength = mShader_Cb_Component->GetShaderCode().size();

	hr = mD3DDevice->CreateComputePipelineState(&descComputePSO_Cb, IID_PPV_ARGS(&mPSO_Cb_Component));
	if (FAILED(hr))
	{
		PostMessageBoxOnError(hr, L"Failed to create compute PSO for the Cb component: ", L"Fatal error", MB_ICONERROR, wHnd);
		exit(-1);
	}
	mPSO_Cb_Component->SetName(L"Compute PSO Y Component");

	// Create compute pipeline state for the Cr component
	D3D12_COMPUTE_PIPELINE_STATE_DESC descComputePSO_Cr = {};
	descComputePSO_Cr.pRootSignature = mRootSignature;
	descComputePSO_Cr.CS.pShaderBytecode = mShader_Cr_Component->GetShaderCode().data();
	descComputePSO_Cr.CS.BytecodeLength = mShader_Cr_Component->GetShaderCode().size();

	hr = mD3DDevice->CreateComputePipelineState(&descComputePSO_Cr, IID_PPV_ARGS(&mPSO_Cr_Component));
	if (FAILED(hr))
	{
		PostMessageBoxOnError(hr, L"Failed to create compute PSO for the Cr component: ", L"Fatal error", MB_ICONERROR, wHnd);
		exit(-1);
	}
	mPSO_Cr_Component->SetName(L"Compute PSO Y Component");

	return S_OK;
}

void DX12_JpegEncoderGPU::UpdateQuantizationTable(DX12_ComputeBuffer * quantizationTable, float* quantizationTableFloat)
{
	// Create the GPU upload buffer
	UINT64 uploadBufferSize = 0;
	{
		mD3DDevice->GetCopyableFootprints(
			&quantizationTable->GetResource()->GetDesc(),
			0,
			1,
			0,
			nullptr,
			nullptr,
			nullptr,
			&uploadBufferSize);
	}

	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC rdBuffer = {};
	rdBuffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	rdBuffer.Alignment = 0;
	rdBuffer.Width = uploadBufferSize;
	rdBuffer.Height = 1;
	rdBuffer.DepthOrArraySize = 1;
	rdBuffer.MipLevels = 1;
	rdBuffer.Format = DXGI_FORMAT_UNKNOWN;
	rdBuffer.SampleDesc.Count = 1;
	rdBuffer.SampleDesc.Quality = 0;
	rdBuffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	rdBuffer.Flags = D3D12_RESOURCE_FLAG_NONE;

	ID3D12Resource* uploadHeap;
	ThrowIfFailed(mD3DDevice->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&rdBuffer,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&uploadHeap)
	));

	// Copy data to the intermediate upload heap and then schedule a copy
	// from the upload heap to the Quantization Table(DX12_ComputeBuffer)
	D3D12_SUBRESOURCE_DATA data = {};
	data.pData = quantizationTableFloat;
	data.RowPitch = 64 * sizeof(unsigned char);
	data.SlicePitch = data.RowPitch * 1;

	D3D12_RESOURCE_BARRIER barrier{};
	MakeResourceBarrier(
		barrier,
		D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
		mCB_CbCr_Quantization_Table->GetResource(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_COPY_SOURCE // Do not know if this is correct
	);

	ThrowIfFailed(mDirectAllocator->Reset());
	ThrowIfFailed(mDirectList->Reset(mDirectAllocator, nullptr));
	//mDirectList->SetGraphicsRootSignature(mRootSignature); Do not know if this is correct

	UpdateSubresources(mDirectList, quantizationTable->GetResource(), uploadHeap, 0, 0, 1, &data);
	mDirectList->ResourceBarrier(1, &barrier);

	// Execute command list?
	ID3D12CommandList* listsToExecute[] = { mDirectList };
	mDirectQueue->ExecuteCommandLists(_ARRAYSIZE(listsToExecute), listsToExecute);
}

void DX12_JpegEncoderGPU::shutdown()
{
	SafeRelease(&mRootSignature);

	// Image resource
	SafeRelease(&imageResource);

	// Pipeline State object
	SafeRelease(&mPSO_Y_Component);
	SafeRelease(&mPSO_Cb_Component);
	SafeRelease(&mPSO_Cr_Component);

	// Command lists
	SafeRelease(&mDirectQueue);
	SafeRelease(&mCopyQueue);
	SafeRelease(&mDirectAllocator);
	SafeRelease(&mCopyAllocator);
	SafeRelease(&mDirectList);
	SafeRelease(&mCopyList);
}

DX12_JpegEncoderGPU::DX12_JpegEncoderGPU(ID3D12Resource* resource) // nr:0
	: imageResource(resource)
{
	imageResource->GetDevice(__uuidof(ID3D12Device), (void**)(&mD3DDevice));

	_tcscpy_s(mComputeShaderFile, _T("../Shaders/Jpeg_CS.hlsl"));

	mCB_ImageData_Y = NULL;
	mCB_ImageData_CbCr = NULL;

	mCT_RGBA = NULL;

	mCB_EntropyResult = NULL;

	mCB_Huff_Y_AC = NULL;
	mCB_Huff_CbCr_AC = NULL;

	mCB_DCT_Matrix = NULL;
	mCB_DCT_Matrix_Transpose = NULL;
	mCB_Y_Quantization_Table = NULL;
	mCB_CbCr_Quantization_Table = NULL;

	mCB_SamplerState_PointClamp = NULL;

	mImageWidth = 0;
	mImageHeight = 0;
	mEntropyBlockSize = 0;

	mDoCreateBuffers = true;

	mComputeSys = new DX12_ComputeWrap(mD3DDevice, mDirectList, mDirectAllocator, mDirectQueue);
	mShader_Y_Component = NULL;
	mShader_Cb_Component = NULL;
	mShader_Cr_Component = NULL;

	HWND wHnd = GetActiveWindow();
	assert(wHnd);

	//create the root signature
	HRESULT hr = createRootSignature();
	if (FAILED(hr))
	{
		PostMessageBoxOnError(hr, L"Failed to create Root Signature: ", L"Fatal error", MB_ICONERROR, wHnd);
		exit(-1);
	}

	// Create the PSOs
	hr = createPiplineStateObjects();
	if (FAILED(hr))
	{
		PostMessageBoxOnError(hr, L"Failed to create PSOs: ", L"Fatal error", MB_ICONERROR, wHnd);
		exit(-1);
	}

	// Create the allocators, queues and lists
	hr = createAllocatorQueueList();
	if (FAILED(hr))
	{
		PostMessageBoxOnError(hr, L"Failed to create Allocators, Queues and Lists: ", L"Fatal error", MB_ICONERROR, wHnd);
		exit(-1);
	}
}

DX12_JpegEncoderGPU::~DX12_JpegEncoderGPU()
{
	ReleaseQuantizationBuffers();
	ReleaseBuffers();
	ReleaseShaders();
	shutdown();
}

void DX12_JpegEncoderGPU::ReleaseBuffers()
{
	SAFE_RELEASE(mCB_ImageData_Y);
	SAFE_RELEASE(mCB_ImageData_CbCr);
	SAFE_DELETE(mCT_RGBA);

	SAFE_DELETE(mCB_EntropyResult);

	SAFE_DELETE(mCB_Huff_Y_AC);
	SAFE_DELETE(mCB_Huff_CbCr_AC);

	SAFE_DELETE(mCB_DCT_Matrix);
	SAFE_DELETE(mCB_DCT_Matrix_Transpose);
	
	SAFE_RELEASE(mCB_SamplerState_PointClamp);

	// Might not be necessary
	SAFE_RELEASE(mCB_ImageData_Y_Heap);
	SAFE_RELEASE(mCB_ImageData_CbCr_Heap);
}

void DX12_JpegEncoderGPU::ReleaseQuantizationBuffers()
{
	SAFE_DELETE(mCB_Y_Quantization_Table);
	SAFE_DELETE(mCB_CbCr_Quantization_Table);
}

void DX12_JpegEncoderGPU::ReleaseShaders()
{
	SAFE_DELETE(mComputeSys);
	SAFE_DELETE(mShader_Y_Component);
	SAFE_DELETE(mShader_Cb_Component);
	SAFE_DELETE(mShader_Cr_Component);
}

void DX12_JpegEncoderGPU::DoHuffmanEncoding(int * DU, short & prevDC, BitString * HTDC)
{
	static const unsigned short mask[] = { 1,2,4,8,16,32,64,128,256,512,1024,2048,4096,8192,16384,32768 };
	static BitString bs;
	short tmp1, tmp2;

	// appeend DC bits
	tmp1 = tmp2 = (short)(DU[0] - prevDC);
	prevDC = DU[0];

	if (tmp1 < 0)
	{
		tmp1 = -tmp1;
		tmp2--;
	}

	//count numberof bits
	int nbits = NumBitsInUShort[tmp1];

	WriteBits(HTDC[nbits]);

	if (nbits)
	{
		bs.value = tmp2 & (mask[nbits] - 1);
		bs.length = nbits;
		WriteBits(bs);
	}


	// append ac bits generated by the GPU
	BYTE* ac_entropy_data = (BYTE*)&DU[1];
	int num_ac_bits = DU[mEntropyBlockSize - 1];

	bs.length = 8;
	int num_bytes = num_ac_bits / 8;
	while (num_bytes-- > 0)
	{
		bs.value = *ac_entropy_data++;
		WriteBits(bs);
	}

	//append last bits
	num_ac_bits = num_ac_bits % 8;
	if (num_ac_bits > 0)
	{
		bs.value = *ac_entropy_data++ >> (8 - num_ac_bits);
		bs.length = num_ac_bits;
		WriteBits(bs);
	}
}

void DX12_JpegEncoderGPU::WriteImageData(JEncRGBDataDesc rgbDataDesc)
{
	if (!mCT_RGBA)
	{
		mCT_RGBA = mComputeSys->CreateTexture(DXGI_FORMAT_R8G8B8A8_UNORM,
			rgbDataDesc.Width, rgbDataDesc.Height, rgbDataDesc.RowPitch, rgbDataDesc.Data);
	}

	if (mDoCreateBuffers)
	{
		CreateBuffers();
		mDoCreateBuffers = false;
	}

	DoQuantization(NULL);

	DoEntropyEncode();

	FinalizeData();
}

void DX12_JpegEncoderGPU::WriteImageData(DX12_JEncD3DDataDesc d3dDataDesc) // nr:2 Start point
{
	if (mDoCreateBuffers)
	{
		CreateBuffers();
		mDoCreateBuffers = false;
	}

	DoQuantization(d3dDataDesc.DescriptorHeap);

	DoEntropyEncode();

	FinalizeData();
}

void DX12_JpegEncoderGPU::DoQuantization(ID3D12DescriptorHeap * pSRV)
{
	ThrowIfFailed(mDirectAllocator->Reset());
	ThrowIfFailed(mDirectList->Reset(mDirectAllocator, mPSO_Y_Component));
	
	// Set necessary state.
	mDirectList->SetComputeRootSignature(mRootSignature);

	//Set the SRV and UAV descriptor heaps
	ID3D12DescriptorHeap * srvHeap[] = { 
		mCB_DCT_Matrix->GetHeap(),
		mCB_DCT_Matrix_Transpose->GetHeap(),
		pSRV ? pSRV : mCT_RGBA ? mCT_RGBA->GetHeap() : NULL
	};
	mDirectList->SetDescriptorHeaps(3, srvHeap);
	mDirectList->SetComputeRootDescriptorTable(0, srvHeap[0]->GetGPUDescriptorHandleForHeapStart()); // t0
	mDirectList->SetComputeRootDescriptorTable(1, srvHeap[1]->GetGPUDescriptorHandleForHeapStart()); // t1
	mDirectList->SetComputeRootDescriptorTable(2, srvHeap[2]->GetGPUDescriptorHandleForHeapStart()); // t2
	
	// Set sampler descriptor heap
	ID3D12DescriptorHeap * samplerHeap[] = { mCB_SamplerState_PointClamp };
	mDirectList->SetDescriptorHeaps(1, samplerHeap);
	mDirectList->SetComputeRootDescriptorTable(0, mCB_SamplerState_PointClamp->GetGPUDescriptorHandleForHeapStart()); // s0

	// Dispatch to GPU compute shader
	Dispatch();

	ID3D12CommandList* listsToExecute[] = { mDirectList };
	mDirectQueue->ExecuteCommandLists(_ARRAYSIZE(listsToExecute), listsToExecute);

	/*ID3D11ShaderResourceView* aRViews[] =
	{
		mCB_DCT_Matrix->GetResourceView(),
		mCB_DCT_Matrix_Transpose->GetResourceView(),
		pSRV ? pSRV : mCT_RGBA ? mCT_RGBA->GetResourceView() : NULL
	};

	mD3DDeviceContext->CSSetShaderResources(0, 3, aRViews);

	ID3D11SamplerState* samplers[] = { mCB_SamplerState_PointClamp };
	mD3DDeviceContext->CSSetSamplers(0, 1, samplers);

	Dispatch();

	samplers[0] = NULL;
	mD3DDeviceContext->CSSetSamplers(0, 1, samplers);

	mD3DDeviceContext->CSSetShader(NULL, NULL, 0);

	ID3D11UnorderedAccessView* ppUAViewNULL[8] = { NULL };
	mD3DDeviceContext->CSSetUnorderedAccessViews(0, 8, ppUAViewNULL, NULL);

	ID3D11ShaderResourceView* ppSRVNULL[5] = { NULL };
	mD3DDeviceContext->CSSetShaderResources(0, 5, ppSRVNULL);*/
}

void DX12_JpegEncoderGPU::Dispatch()
{
	ID3D12DescriptorHeap * uavHeap[] = { mCB_EntropyResult->GetHeap() };
	mDirectList->SetDescriptorHeaps(1, uavHeap);
	mDirectList->SetComputeRootDescriptorTable(0, uavHeap[0]->GetGPUDescriptorHandleForHeapStart()); // u0

	ID3D12DescriptorHeap * srv_Huffman_Y[] = { 
		mCB_Y_Quantization_Table->GetHeap(),
		mCB_Huff_Y_AC->GetHeap()
	};
	mDirectList->SetDescriptorHeaps(2, srv_Huffman_Y);
	mDirectList->SetComputeRootDescriptorTable(3, srv_Huffman_Y[0]->GetGPUDescriptorHandleForHeapStart()); // t3
	mDirectList->SetComputeRootDescriptorTable(4, srv_Huffman_Y[1]->GetGPUDescriptorHandleForHeapStart()); // t4

	ID3D12DescriptorHeap * cb_ImageData_Y[] = { mCB_ImageData_Y_Heap };
	mDirectList->SetDescriptorHeaps(1, cb_ImageData_Y);
	mDirectList->SetComputeRootDescriptorTable(0, cb_ImageData_Y[0]->GetGPUDescriptorHandleForHeapStart()); // b0

	//Set a Resource Barrier for the active UAV so that the copy queue waits until all operations are completed
	D3D12_RESOURCE_BARRIER barrier{};
	MakeResourceBarrier(
		barrier,
		D3D12_RESOURCE_BARRIER_TYPE_UAV,
		uavHeap[0],
		D3D12_RESOURCE_STATES(0), //UAV does not need transition states
		D3D12_RESOURCE_STATES(0)
	);
	mDirectList->ResourceBarrier(1, &barrier);
	
	// Dispatch Y component
	mDirectList->SetPipelineState(mPSO_Y_Component);
	mShader_Y_Component->Set();
	mDirectList->Dispatch(mNumComputationBlocks_Y[0], mNumComputationBlocks_Y[1], 1);
	mShader_Y_Component->Unset();

	ID3D12DescriptorHeap* srv_Huffman_CbCr[] = { 
		mCB_CbCr_Quantization_Table->GetHeap(),
		mCB_Huff_CbCr_AC->GetHeap()
	};
	mDirectList->SetDescriptorHeaps(2, srv_Huffman_CbCr);
	mDirectList->SetComputeRootDescriptorTable(3, srv_Huffman_CbCr[0]->GetGPUDescriptorHandleForHeapStart()); // t3
	mDirectList->SetComputeRootDescriptorTable(4, srv_Huffman_CbCr[1]->GetGPUDescriptorHandleForHeapStart()); // t4

	ID3D12DescriptorHeap * constantBuffer[] = { mCB_ImageData_CbCr_Heap };
	mDirectList->SetDescriptorHeaps(1, constantBuffer);
	mDirectList->SetComputeRootDescriptorTable(0, constantBuffer[0]->GetGPUDescriptorHandleForHeapStart()); // bo

	// Dispatch Cb component
	mDirectList->SetPipelineState(mPSO_Cb_Component);
	mShader_Cb_Component->Set();
	mDirectList->Dispatch(mNumComputationBlocks_CbCr[0], mNumComputationBlocks_CbCr[1], 1);
	mShader_Cb_Component->Unset();

	// Dispatch Cr component
	mDirectList->SetPipelineState(mPSO_Cr_Component);
	mShader_Cr_Component->Set();
	mDirectList->Dispatch(mNumComputationBlocks_CbCr[0], mNumComputationBlocks_CbCr[1], 1);
	mShader_Cr_Component->Unset();

	D3D12_RESOURCE_BARRIER cpyBarrierSrc{};
	MakeResourceBarrier(
		cpyBarrierSrc,
		D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
		uavHeap[0],
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_COPY_SOURCE
	);
	mDirectList->ResourceBarrier(1, &cpyBarrierSrc);
	mDirectList->Close();
}

void DX12_JpegEncoderGPU::FinalizeData()
{
	//write any remaining bits to complete last block
	BitString bs;
	bs.length = 7;
	bs.value = 0;
	WriteBits(bs);

	//Write End of Image Marker
	WriteHex(0xFFD9);
}
